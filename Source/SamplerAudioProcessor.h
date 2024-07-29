#pragma once

#include <array>
#include <atomic>
#include <memory>
#include <vector>
#include <tuple>
#include <iomanip>
#include <sstream>
#include <functional>
#include <mutex>

#include "Command.h"
#include "Helper.h"

struct ProcessorState
{
    int synthVoices;
    bool legacyModeEnabled;
    Range<int> legacyChannels;
    int legacyPitchbendRange;
    bool voiceStealingEnabled;
    MPEZoneLayout mpeZoneLayout;
    std::unique_ptr<AudioFormatReaderFactory> readerFactory;
    double centreFrequencyHz;
};

//=====================================================

class SamplerAudioProcessor final : public AudioProcessor
{
public:
    SamplerAudioProcessor();

    void prepareToPlay (double sampleRate, int) override
    {
        synthesiser.setCurrentPlaybackSampleRate (sampleRate);
    }

    void releaseResources() override {}

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override
    {
        return layouts.getMainOutputChannelSet() == AudioChannelSet::mono()
            || layouts.getMainOutputChannelSet() == AudioChannelSet::stereo();
    }

    AudioProcessorEditor* createEditor() override;

    bool hasEditor() const override                                       { return true; }

    const String getName() const override                                 { return "SamplerPlugin"; }
    bool acceptsMidi() const override                                     { return true; }
    bool producesMidi() const override                                    { return false; }
    bool isMidiEffect() const override                                    { return false; }
    double getTailLengthSeconds() const override                          { return 0.0; }

    int getNumPrograms() override                                         { return 1; }
    int getCurrentProgram() override                                      { return 0; }
    void setCurrentProgram (int) override                                 {}
    const String getProgramName (int) override                            { return "None"; }
    void changeProgramName (int, const String&) override                  {}

    void getStateInformation (MemoryBlock&) override                      {}
    void setStateInformation (const void*, int) override                  {}

    void processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi) override
    {
        process (buffer, midi);
    }

    void processBlock (AudioBuffer<double>& buffer, MidiBuffer& midi) override
    {
        process (buffer, midi);
    }

    // These should be called from the GUI thread, and will block until the
    // command buffer has enough room to accept a command.
    void setSample (std::unique_ptr<AudioFormatReaderFactory> fact, AudioFormatManager& formatManager);
    void setCentreFrequency (double centreFrequency);
    void setMPEZoneLayout (MPEZoneLayout layout);
    void setLegacyModeEnabled (int pitchbendRange, Range<int> channelRange);
    void setVoiceStealingEnabled (bool voiceStealingEnabled);
    void setNumberOfVoices (int numberOfVoices);

    // These accessors are just for an 'overview' and won't give the exact
    // state of the audio engine at a particular point in time.
    // If you call getNumVoices(), get the result '10', and then call
    // getPlaybackPosiiton (9), there's a chance the audio engine will have
    // been updated to remove some voices in the meantime, so the returned
    // value won't correspond to an existing voice.
    int getNumVoices() const                    { return synthesiser.getNumVoices(); }
    float getPlaybackPosition (int voice) const { return playbackPositions.at ((size_t) voice); }

private:
    template <typename Element>
    void process (AudioBuffer<Element>& buffer, MidiBuffer& midiMessages);

    CommandFifo<SamplerAudioProcessor> commands;

    MemoryBlock mb;
    std::unique_ptr<AudioFormatReaderFactory> readerFactory;
    std::shared_ptr<MPESamplerSound> samplerSound = std::make_shared<MPESamplerSound>();
    MPESynthesiser synthesiser;

    // This mutex is used to ensure we don't modify the processor state during
    // a call to createEditor, which would cause the UI to become desynched
    // with the real state of the processor.
    SpinLock commandQueueMutex;

    enum { maxVoices = 20 };

    // This is used for visualising the current playback position of each voice.
    std::array<std::atomic<float>, maxVoices> playbackPositions;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SamplerAudioProcessor)
};

//==========================================================

template<typename Element>
void SamplerAudioProcessor::process (AudioBuffer<Element>& buffer, MidiBuffer& midiMessages)
{
    // Try to acquire a lock on the command queue.
    // If we were successful, we pop all pending commands off the queue and
    // apply them to the processor.
    // If we weren't able to acquire the lock, it's because someone called
    // createEditor, which requires that the processor data model stays in
    // a valid state for the duration of the call.
    const GenericScopedTryLock<SpinLock> lock (commandQueueMutex);

    if (lock.isLocked ())
        commands.call (*this);

    synthesiser.renderNextBlock (buffer, midiMessages, 0, buffer.getNumSamples ());

    auto loadedSamplerSound = samplerSound;

    if (loadedSamplerSound->getSample () == nullptr)
        return;

    auto numVoices = synthesiser.getNumVoices ();

    // Update the current playback positions
    for (auto i = 0; i < maxVoices; ++i)
    {
        auto* voicePtr = dynamic_cast<MPESamplerVoice*> (synthesiser.getVoice (i));

        if (i < numVoices && voicePtr != nullptr)
            playbackPositions[(size_t) i] = static_cast<float> (voicePtr->getCurrentSamplePosition () / loadedSamplerSound->getSample ()->getSampleRate ());
        else
            playbackPositions[(size_t) i] = 0.0f;
    }
}
