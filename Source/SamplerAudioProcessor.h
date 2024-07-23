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
#include "MpeSettingsComponent.h"
#include "GUI/LoopPointMarker.h"
#include "GUI/MainSamplerView.h"

struct ProcessorState
{
    int synthVoices;
    bool legacyModeEnabled;
    Range<int> legacyChannels;
    int legacyPitchbendRange;
    bool voiceStealingEnabled;
    MPEZoneLayout mpeZoneLayout;
    std::unique_ptr<AudioFormatReaderFactory> readerFactory;
    Range<double> loopPointsSeconds;
    double centreFrequencyHz;
    LoopMode loopMode;
};

inline File getExamplesDirectory () noexcept
{
#ifdef PIP_JUCE_EXAMPLES_DIRECTORY
    MemoryOutputStream mo;

    auto success = Base64::convertFromBase64 (mo, JUCE_STRINGIFY (PIP_JUCE_EXAMPLES_DIRECTORY));
    ignoreUnused (success);
    jassert (success);

    return mo.toString ();
#elif defined PIP_JUCE_EXAMPLES_DIRECTORY_STRING
    return File { CharPointer_UTF8 { PIP_JUCE_EXAMPLES_DIRECTORY_STRING } };
#else
    auto currentFile = File::getSpecialLocation (File::SpecialLocationType::currentApplicationFile);
    auto exampleDir = currentFile.getParentDirectory ().getChildFile ("examples");

    if (exampleDir.exists ())
        return exampleDir;

    // keep track of the number of parent directories so we don't go on endlessly
    for (int numTries = 0; numTries < 15; ++numTries)
    {
        if (currentFile.getFileName () == "examples")
            return currentFile;

        const auto sibling = currentFile.getSiblingFile ("examples");

        if (sibling.exists ())
            return sibling;

        currentFile = currentFile.getParentDirectory ();
    }

    return currentFile;
#endif
}

inline std::unique_ptr<InputStream> createAssetInputStream (const char* resourcePath)
{
#if JUCE_ANDROID
    ZipFile apkZip (File::getSpecialLocation (File::invokedExecutableFile));
    const auto fileIndex = apkZip.getIndexOfFileName ("assets/" + String (resourcePath));

    if (fileIndex == -1)
    {
        jassert (assertExists == AssertAssetExists::no);
        return {};
    }

    return std::unique_ptr<InputStream> (apkZip.createStreamForEntry (fileIndex));
#else
#if JUCE_IOS
    auto assetsDir = File::getSpecialLocation (File::currentExecutableFile)
        .getParentDirectory ().getChildFile ("Assets");
#elif JUCE_MAC
    auto assetsDir = File::getSpecialLocation (File::currentExecutableFile)
        .getParentDirectory ().getParentDirectory ().getChildFile ("Resources").getChildFile ("Assets");

    if (! assetsDir.exists ())
        assetsDir = getExamplesDirectory ().getChildFile ("Assets");
#else
    auto assetsDir = getExamplesDirectory ().getChildFile ("Assets");
#endif

    auto resourceFile = assetsDir.getChildFile (resourcePath);

    if (! resourceFile.existsAsFile ())
    {
        jassertfalse;
        return {};
    }

    return resourceFile.createInputStream ();
#endif
}

//==============================================================================
class SamplerAudioProcessor final : public AudioProcessor
{
public:
    SamplerAudioProcessor()
        : AudioProcessor (BusesProperties().withOutput ("Output", AudioChannelSet::stereo(), true))
    {
        if (auto inputStream = createAssetInputStream ("cello.wav"))
        {
            inputStream->readIntoMemoryBlock (mb);
            readerFactory.reset (new MemoryAudioFormatReaderFactory (mb.getData(), mb.getSize()));
        }

        // Set up initial sample, which we load from a binary resource
        AudioFormatManager manager;
        manager.registerBasicFormats();
        auto reader = readerFactory->make (manager);
        jassert (reader != nullptr); // Failed to load resource!

        auto sound = samplerSound;
        auto sample = std::unique_ptr<Sample> (new Sample (*reader, 10.0));
        auto lengthInSeconds = sample->getLength() / sample->getSampleRate();
        sound->setLoopPointsInSeconds ({lengthInSeconds * 0.1, lengthInSeconds * 0.9 });
        sound->setSample (std::move (sample));

        // Start with the max number of voices
        for (auto i = 0; i != maxVoices; ++i)
            synthesiser.addVoice (new MPESamplerVoice (sound));
    }

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

    //==============================================================================
    AudioProcessorEditor* createEditor() override
    {
        // This function will be called from the message thread. We lock the command
        // queue to ensure that no messages are processed for the duration of this
        // call.
        SpinLock::ScopedLockType lock (commandQueueMutex);

        ProcessorState state;
        state.synthVoices          = synthesiser.getNumVoices();
        state.legacyModeEnabled    = synthesiser.isLegacyModeEnabled();
        state.legacyChannels       = synthesiser.getLegacyModeChannelRange();
        state.legacyPitchbendRange = synthesiser.getLegacyModePitchbendRange();
        state.voiceStealingEnabled = synthesiser.isVoiceStealingEnabled();
        state.mpeZoneLayout        = synthesiser.getZoneLayout();
        state.readerFactory        = readerFactory == nullptr ? nullptr : readerFactory->clone();

        auto sound = samplerSound;
        state.loopPointsSeconds = sound->getLoopPointsInSeconds();
        state.centreFrequencyHz = sound->getCentreFrequencyInHz();
        state.loopMode          = sound->getLoopMode();

        return new SamplerAudioProcessorEditor (*this, std::move (state));
    }

    bool hasEditor() const override                                       { return true; }

    //==============================================================================
    const String getName() const override                                 { return "SamplerPlugin"; }
    bool acceptsMidi() const override                                     { return true; }
    bool producesMidi() const override                                    { return false; }
    bool isMidiEffect() const override                                    { return false; }
    double getTailLengthSeconds() const override                          { return 0.0; }

    //==============================================================================
    int getNumPrograms() override                                         { return 1; }
    int getCurrentProgram() override                                      { return 0; }
    void setCurrentProgram (int) override                                 {}
    const String getProgramName (int) override                            { return "None"; }
    void changeProgramName (int, const String&) override                  {}

    //==============================================================================
    void getStateInformation (MemoryBlock&) override                      {}
    void setStateInformation (const void*, int) override                  {}

    //==============================================================================
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
    void setSample (std::unique_ptr<AudioFormatReaderFactory> fact, AudioFormatManager& formatManager)
    {
        class SetSampleCommand
        {
        public:
            SetSampleCommand (std::unique_ptr<AudioFormatReaderFactory> r,
                              std::unique_ptr<Sample> sampleIn,
                              std::vector<std::unique_ptr<MPESamplerVoice>> newVoicesIn)
                : readerFactory (std::move (r)),
                  sample (std::move (sampleIn)),
                  newVoices (std::move (newVoicesIn))
            {}

            void operator() (SamplerAudioProcessor& proc)
            {
                proc.readerFactory = std::move (readerFactory);
                auto sound = proc.samplerSound;
                sound->setSample (std::move (sample));
                auto numberOfVoices = proc.synthesiser.getNumVoices();
                proc.synthesiser.clearVoices();

                for (auto it = begin (newVoices); proc.synthesiser.getNumVoices() < numberOfVoices; ++it)
                {
                    proc.synthesiser.addVoice (it->release());
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

        if (fact == nullptr)
        {
            commands.push (SetSampleCommand (std::move (fact),
                                             nullptr,
                                             std::move (newSamplerVoices)));
        }
        else if (auto reader = fact->make (formatManager))
        {
            commands.push (SetSampleCommand (std::move (fact),
                                             std::unique_ptr<Sample> (new Sample (*reader, 10.0)),
                                             std::move (newSamplerVoices)));
        }
    }

    void setCentreFrequency (double centreFrequency)
    {
        commands.push ([centreFrequency] (SamplerAudioProcessor& proc)
                       {
                           auto loaded = proc.samplerSound;
                           if (loaded != nullptr)
                               loaded->setCentreFrequencyInHz (centreFrequency);
                       });
    }

    void setLoopMode (LoopMode loopMode)
    {
        commands.push ([loopMode] (SamplerAudioProcessor& proc)
                       {
                           auto loaded = proc.samplerSound;
                           if (loaded != nullptr)
                               loaded->setLoopMode (loopMode);
                       });
    }

    void setLoopPoints (Range<double> loopPoints)
    {
        commands.push ([loopPoints] (SamplerAudioProcessor& proc)
                       {
                           auto loaded = proc.samplerSound;
                           if (loaded != nullptr)
                               loaded->setLoopPointsInSeconds (loopPoints);
                       });
    }

    void setMPEZoneLayout (MPEZoneLayout layout)
    {
        commands.push ([layout] (SamplerAudioProcessor& proc)
                       {
                           // setZoneLayout will lock internally, so we don't care too much about
                           // ensuring that the layout doesn't get copied or destroyed on the
                           // audio thread. If the audio glitches while updating midi settings
                           // it doesn't matter too much.
                           proc.synthesiser.setZoneLayout (layout);
                       });
    }

    void setLegacyModeEnabled (int pitchbendRange, Range<int> channelRange)
    {
        commands.push ([pitchbendRange, channelRange] (SamplerAudioProcessor& proc)
                       {
                           proc.synthesiser.enableLegacyMode (pitchbendRange, channelRange);
                       });
    }

    void setVoiceStealingEnabled (bool voiceStealingEnabled)
    {
        commands.push ([voiceStealingEnabled] (SamplerAudioProcessor& proc)
                       {
                           proc.synthesiser.setVoiceStealingEnabled (voiceStealingEnabled);
                       });
    }

    void setNumberOfVoices (int numberOfVoices)
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
            {}

            void operator() (SamplerAudioProcessor& proc)
            {
                if ((int) newVoices.size() < proc.synthesiser.getNumVoices())
                    proc.synthesiser.reduceNumVoices (int (newVoices.size()));
                else
                    for (auto it = begin (newVoices); (size_t) proc.synthesiser.getNumVoices() < newVoices.size(); ++it)
                        proc.synthesiser.addVoice (it->release());
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

    // These accessors are just for an 'overview' and won't give the exact
    // state of the audio engine at a particular point in time.
    // If you call getNumVoices(), get the result '10', and then call
    // getPlaybackPosiiton (9), there's a chance the audio engine will have
    // been updated to remove some voices in the meantime, so the returned
    // value won't correspond to an existing voice.
    int getNumVoices() const                    { return synthesiser.getNumVoices(); }
    float getPlaybackPosition (int voice) const { return playbackPositions.at ((size_t) voice); }

private:
    //==============================================================================
    class SamplerAudioProcessorEditor final : public AudioProcessorEditor,
                                              public FileDragAndDropTarget,
                                              private DataModel::Listener,
                                              private MPESettingsDataModel::Listener
    {
    public:
        SamplerAudioProcessorEditor (SamplerAudioProcessor& p, ProcessorState state)
            : AudioProcessorEditor (&p),
              samplerAudioProcessor (p),
              mainSamplerView (dataModel,
                               [&p]
                               {
                                   std::vector<float> ret;
                                   auto voices = p.getNumVoices();
                                   ret.reserve ((size_t) voices);

                                   for (auto i = 0; i != voices; ++i)
                                       ret.emplace_back (p.getPlaybackPosition (i));

                                   return ret;
                               },
                               undoManager)
        {
            dataModel.addListener (*this);
            mpeSettings.addListener (*this);

            formatManager.registerBasicFormats();

            addAndMakeVisible (tabbedComponent);

            auto lookFeel = dynamic_cast<LookAndFeel_V4*> (&getLookAndFeel());
            auto bg = lookFeel->getCurrentColourScheme()
                               .getUIColour (LookAndFeel_V4::ColourScheme::UIColour::widgetBackground);

            tabbedComponent.addTab ("Sample Editor", bg, &mainSamplerView, false);
            tabbedComponent.addTab ("MPE Settings", bg, &settingsComponent, false);

            mpeSettings.setSynthVoices          (state.synthVoices,               nullptr);
            mpeSettings.setLegacyModeEnabled    (state.legacyModeEnabled,         nullptr);
            mpeSettings.setLegacyFirstChannel   (state.legacyChannels.getStart(), nullptr);
            mpeSettings.setLegacyLastChannel    (state.legacyChannels.getEnd(),   nullptr);
            mpeSettings.setLegacyPitchbendRange (state.legacyPitchbendRange,      nullptr);
            mpeSettings.setVoiceStealingEnabled (state.voiceStealingEnabled,      nullptr);
            mpeSettings.setMPEZoneLayout        (state.mpeZoneLayout,             nullptr);

            dataModel.setSampleReader (std::move (state.readerFactory), nullptr);

            dataModel.setLoopPointsSeconds  (state.loopPointsSeconds, nullptr);
            dataModel.setCentreFrequencyHz  (state.centreFrequencyHz, nullptr);
            dataModel.setLoopMode           (state.loopMode,          nullptr);

            // Make sure that before the constructor has finished, you've set the
            // editor's size to whatever you need it to be.
            setResizable (true, true);
            setResizeLimits (640, 480, 2560, 1440);
            setSize (640, 480);
        }

    private:
        void resized() override
        {
            tabbedComponent.setBounds (getLocalBounds());
        }

        bool keyPressed (const KeyPress& key) override
        {
            if (key == KeyPress ('z', ModifierKeys::commandModifier, 0))
            {
                undoManager.undo();
                return true;
            }

            if (key == KeyPress ('z', ModifierKeys::commandModifier | ModifierKeys::shiftModifier, 0))
            {
                undoManager.redo();
                return true;
            }

            return Component::keyPressed (key);
        }

        bool isInterestedInFileDrag (const StringArray& files) override
        {
            WildcardFileFilter filter (formatManager.getWildcardForAllFormats(), {}, "Known Audio Formats");
            return files.size() == 1 && filter.isFileSuitable (files[0]);
        }

        void filesDropped (const StringArray& files, int, int) override
        {
            jassert (files.size() == 1);
            undoManager.beginNewTransaction();
            auto r = new FileAudioFormatReaderFactory (files[0]);
            dataModel.setSampleReader (std::unique_ptr<AudioFormatReaderFactory> (r),
                                       &undoManager);

        }

        void sampleReaderChanged (std::shared_ptr<AudioFormatReaderFactory> value) override
        {
            samplerAudioProcessor.setSample (value == nullptr ? nullptr : value->clone(),
                                             dataModel.getAudioFormatManager());
        }

        void centreFrequencyHzChanged (double value) override
        {
            samplerAudioProcessor.setCentreFrequency (value);
        }

        void loopPointsSecondsChanged (Range<double> value) override
        {
            samplerAudioProcessor.setLoopPoints (value);
        }

        void loopModeChanged (LoopMode value) override
        {
            samplerAudioProcessor.setLoopMode (value);
        }

        void synthVoicesChanged (int value) override
        {
            samplerAudioProcessor.setNumberOfVoices (value);
        }

        void voiceStealingEnabledChanged (bool value) override
        {
            samplerAudioProcessor.setVoiceStealingEnabled (value);
        }

        void legacyModeEnabledChanged (bool value) override
        {
            if (value)
                setProcessorLegacyMode();
            else
                setProcessorMPEMode();
        }

        void mpeZoneLayoutChanged (const MPEZoneLayout&) override
        {
            setProcessorMPEMode();
        }

        void legacyFirstChannelChanged (int) override
        {
            setProcessorLegacyMode();
        }

        void legacyLastChannelChanged (int) override
        {
            setProcessorLegacyMode();
        }

        void legacyPitchbendRangeChanged (int) override
        {
            setProcessorLegacyMode();
        }

        void setProcessorLegacyMode()
        {
            samplerAudioProcessor.setLegacyModeEnabled (mpeSettings.getLegacyPitchbendRange(),
                                                        Range<int> (mpeSettings.getLegacyFirstChannel(),
                                                        mpeSettings.getLegacyLastChannel()));
        }

        void setProcessorMPEMode()
        {
            samplerAudioProcessor.setMPEZoneLayout (mpeSettings.getMPEZoneLayout());
        }

        SamplerAudioProcessor& samplerAudioProcessor;
        AudioFormatManager formatManager;
        DataModel dataModel { formatManager };
        UndoManager undoManager;
        MPESettingsDataModel mpeSettings { dataModel.mpeSettings() };

        TabbedComponent tabbedComponent { TabbedButtonBar::Orientation::TabsAtTop };
        MPESettingsComponent settingsComponent { dataModel.mpeSettings(), undoManager };
        MainSamplerView mainSamplerView;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SamplerAudioProcessorEditor)
    };

    //==============================================================================
    template <typename Element>
    void process (AudioBuffer<Element>& buffer, MidiBuffer& midiMessages)
    {
        // Try to acquire a lock on the command queue.
        // If we were successful, we pop all pending commands off the queue and
        // apply them to the processor.
        // If we weren't able to acquire the lock, it's because someone called
        // createEditor, which requires that the processor data model stays in
        // a valid state for the duration of the call.
        const GenericScopedTryLock<SpinLock> lock (commandQueueMutex);

        if (lock.isLocked())
            commands.call (*this);

        synthesiser.renderNextBlock (buffer, midiMessages, 0, buffer.getNumSamples());

        auto loadedSamplerSound = samplerSound;

        if (loadedSamplerSound->getSample() == nullptr)
            return;

        auto numVoices = synthesiser.getNumVoices();

        // Update the current playback positions
        for (auto i = 0; i < maxVoices; ++i)
        {
            auto* voicePtr = dynamic_cast<MPESamplerVoice*> (synthesiser.getVoice (i));

            if (i < numVoices && voicePtr != nullptr)
                playbackPositions[(size_t) i] = static_cast<float> (voicePtr->getCurrentSamplePosition() / loadedSamplerSound->getSample()->getSampleRate());
            else
                playbackPositions[(size_t) i] = 0.0f;
        }

    }

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
