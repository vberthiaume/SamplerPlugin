#include "SamplerAudioProcessor.h"
#include "GUI/SamplerAudioEditor.h"

SamplerAudioProcessor::SamplerAudioProcessor ()
    : AudioProcessor (BusesProperties ().withOutput ("Output", AudioChannelSet::stereo (), true))
{
    const juce::File celloWav ("C:/Users/barth/Documents/git/JUCE/examples/Assets/cello.wav");
    if (auto inputStream = celloWav.createInputStream())
    {
        inputStream->readIntoMemoryBlock (mb);
        readerFactory.reset (new MemoryAudioFormatReaderFactory (mb.getData (), mb.getSize ()));
    }

    // Set up initial sample, which we load from a binary resource
    AudioFormatManager manager;
    manager.registerBasicFormats ();
    auto reader = readerFactory->make (manager);
    jassert (reader != nullptr); // Failed to load resource!

    auto sound = samplerSound;
    auto sample = std::unique_ptr<Sample> (new Sample (*reader, 10.0));
    sound->setSample (std::move (sample));

    // Start with the max number of voices
    for (auto i = 0; i != maxVoices; ++i)
        synthesiser.addVoice (new MPESamplerVoice (sound));
}

AudioProcessorEditor* SamplerAudioProcessor::createEditor ()
{
    // This function will be called from the message thread, so lock the command
    // queue to ensure that no messages are processed while in here
    SpinLock::ScopedLockType lock (commandQueueMutex);

    ProcessorState state;
    state.synthVoices = synthesiser.getNumVoices ();
    state.legacyModeEnabled = synthesiser.isLegacyModeEnabled ();
    state.legacyChannels = synthesiser.getLegacyModeChannelRange ();
    state.legacyPitchbendRange = synthesiser.getLegacyModePitchbendRange ();
    state.voiceStealingEnabled = synthesiser.isVoiceStealingEnabled ();
    state.mpeZoneLayout = synthesiser.getZoneLayout ();
    state.readerFactory = readerFactory == nullptr ? nullptr : readerFactory->clone ();

    auto sound = samplerSound;
    state.centreFrequencyHz = sound->getCentreFrequencyInHz ();

    return new SamplerAudioProcessorEditor (*this, std::move (state));
}

void SamplerAudioProcessor::setSample (std::unique_ptr<AudioFormatReaderFactory> factory,
                                       AudioFormatManager& formatManager)
{
    class SetSampleCommand
    {
    public:
        SetSampleCommand (std::unique_ptr<AudioFormatReaderFactory> r,
                          std::unique_ptr<Sample> sampleIn,
                          std::vector<std::unique_ptr<MPESamplerVoice>> newVoicesIn) :
            readerFactory (std::move (r)),
            sample (std::move (sampleIn)),
            newVoices (std::move (newVoicesIn))
        {
        }

        void operator() (SamplerAudioProcessor& proc)
        {
            proc.readerFactory = std::move (readerFactory);
            auto sound = proc.samplerSound;
            sound->setSample (std::move (sample));
            auto numberOfVoices = proc.synthesiser.getNumVoices ();
            proc.synthesiser.clearVoices ();

            for (auto it = begin (newVoices); proc.synthesiser.getNumVoices () < numberOfVoices; ++it)
            {
                proc.synthesiser.addVoice (it->release ());
            }
        }

    private:
        std::unique_ptr<AudioFormatReaderFactory> readerFactory;
        std::unique_ptr<Sample> sample;
        std::vector<std::unique_ptr<MPESamplerVoice>> newVoices;
    };

    // Note that all allocation happens here, on the main message thread. Then,
    // we transfer ownership across to the audio thread.
    auto loadedSamplerSound = samplerSound;
    std::vector<std::unique_ptr<MPESamplerVoice>> newSamplerVoices;
    newSamplerVoices.reserve (maxVoices);

    for (auto i = 0; i != maxVoices; ++i)
        newSamplerVoices.emplace_back (new MPESamplerVoice (loadedSamplerSound));

    if (factory == nullptr)
    {
        commands.push (SetSampleCommand (std::move (factory),
                                         nullptr,
                                         std::move (newSamplerVoices)));
    }
    else if (auto reader = factory->make (formatManager))
    {
        commands.push (SetSampleCommand (std::move (factory),
                                         std::unique_ptr<Sample> (new Sample (*reader, 10.0)),
                                         std::move (newSamplerVoices)));
    }
}

void SamplerAudioProcessor::setCentreFrequency (double centreFrequency)
{
    commands.push ([centreFrequency](SamplerAudioProcessor& proc)
                   {
                       auto loaded = proc.samplerSound;
                       if (loaded != nullptr)
                           loaded->setCentreFrequencyInHz (centreFrequency);
                   });
}

void SamplerAudioProcessor::setMPEZoneLayout (MPEZoneLayout layout)
{
    commands.push ([layout](SamplerAudioProcessor& proc)
                   {
                       // setZoneLayout will lock internally, so we don't care too much about
                       // ensuring that the layout doesn't get copied or destroyed on the
                       // audio thread. If the audio glitches while updating midi settings
                       // it doesn't matter too much.
                       proc.synthesiser.setZoneLayout (layout);
                   });
}

void SamplerAudioProcessor::setLegacyModeEnabled (int pitchbendRange, Range<int> channelRange)
{
    commands.push ([pitchbendRange, channelRange](SamplerAudioProcessor& proc)
                   {
                       proc.synthesiser.enableLegacyMode (pitchbendRange, channelRange);
                   });
}

void SamplerAudioProcessor::setVoiceStealingEnabled (bool voiceStealingEnabled)
{
    commands.push ([voiceStealingEnabled](SamplerAudioProcessor& proc)
                   {
                       proc.synthesiser.setVoiceStealingEnabled (voiceStealingEnabled);
                   });
}

void SamplerAudioProcessor::setNumberOfVoices (int numberOfVoices)
{
    // We don't want to call 'new' on the audio thread. Normally, we'd
    // construct things here, on the GUI thread, and then move them into the
    // command lambda. Unfortunately, C++11 doesn't have extended lambda
    // capture, so we use a custom struct instead.

    class SetNumVoicesCommand
    {
    public:
        SetNumVoicesCommand (std::vector<std::unique_ptr<MPESamplerVoice>> newVoicesIn)
            : newVoices (std::move (newVoicesIn))
        {
        }

        void operator() (SamplerAudioProcessor& proc)
        {
            if ((int) newVoices.size () < proc.synthesiser.getNumVoices ())
                proc.synthesiser.reduceNumVoices (int (newVoices.size ()));
            else
                for (auto it = begin (newVoices); (size_t) proc.synthesiser.getNumVoices () < newVoices.size (); ++it)
                    proc.synthesiser.addVoice (it->release ());
        }

    private:
        std::vector<std::unique_ptr<MPESamplerVoice>> newVoices;
    };

    numberOfVoices = std::min ((int) maxVoices, numberOfVoices);
    auto loadedSamplerSound = samplerSound;
    std::vector<std::unique_ptr<MPESamplerVoice>> newSamplerVoices;
    newSamplerVoices.reserve ((size_t) numberOfVoices);

    for (auto i = 0; i != numberOfVoices; ++i)
        newSamplerVoices.emplace_back (new MPESamplerVoice (loadedSamplerSound));

    commands.push (SetNumVoicesCommand (std::move (newSamplerVoices)));
}
