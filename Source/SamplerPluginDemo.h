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
#include "LoopPointMarker.h"

class Ruler final : public Component,
                    private VisibleRangeDataModel::Listener
{
public:
    explicit Ruler (const VisibleRangeDataModel& model)
        : visibleRange (model)
    {
        visibleRange.addListener (*this);
        setMouseCursor (MouseCursor::LeftRightResizeCursor);
    }

private:
    void paint (Graphics& g) override
    {
        auto minDivisionWidth = 50.0f;
        auto maxDivisions     = (float) getWidth() / minDivisionWidth;

        auto lookFeel = dynamic_cast<LookAndFeel_V4*> (&getLookAndFeel());
        auto bg = lookFeel->getCurrentColourScheme()
                           .getUIColour (LookAndFeel_V4::ColourScheme::UIColour::widgetBackground);

        g.setGradientFill (ColourGradient (bg.brighter(),
                                           0,
                                           0,
                                           bg.darker(),
                                           0,
                                           (float) getHeight(),
                                           false));

        g.fillAll();
        g.setColour (bg.brighter());
        g.drawHorizontalLine (0, 0.0f, (float) getWidth());
        g.setColour (bg.darker());
        g.drawHorizontalLine (1, 0.0f, (float) getWidth());
        g.setColour (Colours::lightgrey);

        auto minLog = std::ceil (std::log10 (visibleRange.getVisibleRange().getLength() / maxDivisions));
        auto precision = 2 + std::abs (minLog);
        auto divisionMagnitude = std::pow (10, minLog);
        auto startingDivision = std::ceil (visibleRange.getVisibleRange().getStart() / divisionMagnitude);

        for (auto div = startingDivision; div * divisionMagnitude < visibleRange.getVisibleRange().getEnd(); ++div)
        {
            auto time = div * divisionMagnitude;
            auto xPos = (time - visibleRange.getVisibleRange().getStart()) * getWidth()
                              / visibleRange.getVisibleRange().getLength();

            std::ostringstream outStream;
            outStream << std::setprecision (roundToInt (precision)) << time;

            const auto bounds = Rectangle<int> (Point<int> (roundToInt (xPos) + 3, 0),
                                                Point<int> (roundToInt (xPos + minDivisionWidth), getHeight()));

            g.drawText (outStream.str(), bounds, Justification::centredLeft, false);

            g.drawVerticalLine (roundToInt (xPos), 2.0f, (float) getHeight());
        }
    }

    void mouseDown (const MouseEvent& e) override
    {
        visibleRangeOnMouseDown = visibleRange.getVisibleRange();
        timeOnMouseDown = visibleRange.getVisibleRange().getStart()
                       + (visibleRange.getVisibleRange().getLength() * e.getMouseDownX()) / getWidth();
    }

    void mouseDrag (const MouseEvent& e) override
    {
        // Work out the scale of the new range
        auto unitDistance = 100.0f;
        auto scaleFactor  = 1.0 / std::pow (2, (float) e.getDistanceFromDragStartY() / unitDistance);

        // Now position it so that the mouse continues to point at the same
        // place on the ruler.
        auto visibleLength = std::max (0.12, visibleRangeOnMouseDown.getLength() * scaleFactor);
        auto rangeBegin = timeOnMouseDown - visibleLength * e.x / getWidth();
        const Range<double> range (rangeBegin, rangeBegin + visibleLength);
        visibleRange.setVisibleRange (range, nullptr);
    }

    void visibleRangeChanged (Range<double>) override
    {
        repaint();
    }

    VisibleRangeDataModel visibleRange;
    Range<double> visibleRangeOnMouseDown;
    double timeOnMouseDown;
};

//==============================================================================
class LoopPointsOverlay final : public Component,
                                private DataModel::Listener,
                                private VisibleRangeDataModel::Listener
{
public:
    LoopPointsOverlay (const DataModel& dModel,
                       const VisibleRangeDataModel& vModel,
                       UndoManager& undoManagerIn)
        : dataModel (dModel),
          visibleRange (vModel),
          beginMarker ("B",
                       [this] (LoopPointMarker& m, const MouseEvent& e) { this->loopPointMouseDown (m, e); },
                       [this] (LoopPointMarker& m, const MouseEvent& e) { this->loopPointDragged   (m, e); },
                       [this] (LoopPointMarker& m, const MouseEvent& e) { this->loopPointMouseUp   (m, e); }),
          endMarker   ("E",
                       [this] (LoopPointMarker& m, const MouseEvent& e) { this->loopPointMouseDown (m, e); },
                       [this] (LoopPointMarker& m, const MouseEvent& e) { this->loopPointDragged   (m, e); },
                       [this] (LoopPointMarker& m, const MouseEvent& e) { this->loopPointMouseUp   (m, e); }),
          undoManager (&undoManagerIn)
    {
        dataModel   .addListener (*this);
        visibleRange.addListener (*this);

        for (auto ptr : { &beginMarker, &endMarker })
            addAndMakeVisible (ptr);
    }

private:
    void resized() override
    {
        positionLoopPointMarkers();
    }

    void loopPointMouseDown (LoopPointMarker&, const MouseEvent&)
    {
        loopPointsOnMouseDown = dataModel.getLoopPointsSeconds();
        undoManager->beginNewTransaction();
    }

    void loopPointDragged (LoopPointMarker& marker, const MouseEvent& e)
    {
        auto x = xPositionToTime (e.getEventRelativeTo (this).position.x);
        const Range<double> newLoopRange (&marker == &beginMarker ? x : loopPointsOnMouseDown.getStart(),
                                          &marker == &endMarker   ? x : loopPointsOnMouseDown.getEnd());

        dataModel.setLoopPointsSeconds (newLoopRange, undoManager);
    }

    void loopPointMouseUp (LoopPointMarker& marker, const MouseEvent& e)
    {
        auto x = xPositionToTime (e.getEventRelativeTo (this).position.x);
        const Range<double> newLoopRange (&marker == &beginMarker ? x : loopPointsOnMouseDown.getStart(),
                                          &marker == &endMarker   ? x : loopPointsOnMouseDown.getEnd());

        dataModel.setLoopPointsSeconds (newLoopRange, undoManager);
    }

    void loopPointsSecondsChanged (Range<double>) override
    {
        positionLoopPointMarkers();
    }

    void visibleRangeChanged (Range<double>) override
    {
        positionLoopPointMarkers();
    }

    double timeToXPosition (double time) const
    {
        return (time - visibleRange.getVisibleRange().getStart()) * getWidth()
                     / visibleRange.getVisibleRange().getLength();
    }

    double xPositionToTime (double xPosition) const
    {
        return ((xPosition * visibleRange.getVisibleRange().getLength()) / getWidth())
                           + visibleRange.getVisibleRange().getStart();
    }

    void positionLoopPointMarkers()
    {
        auto halfMarkerWidth = 7;

        for (auto tup : { std::make_tuple (&beginMarker, dataModel.getLoopPointsSeconds().getStart()),
                          std::make_tuple (&endMarker,   dataModel.getLoopPointsSeconds().getEnd()) })
        {
            auto ptr  = std::get<0> (tup);
            auto time = std::get<1> (tup);
            ptr->setSize (halfMarkerWidth * 2, getHeight());
            ptr->setTopLeftPosition (roundToInt (timeToXPosition (time) - halfMarkerWidth), 0);
        }
    }

    DataModel dataModel;
    VisibleRangeDataModel visibleRange;
    Range<double> loopPointsOnMouseDown;
    LoopPointMarker beginMarker, endMarker;
    UndoManager* undoManager;
};

//==============================================================================
class PlaybackPositionOverlay final : public Component,
                                      private Timer,
                                      private VisibleRangeDataModel::Listener
{
public:
    using Provider = std::function<std::vector<float>()>;
    PlaybackPositionOverlay (const VisibleRangeDataModel& model,
                             Provider providerIn)
        : visibleRange (model),
          provider (std::move (providerIn))
    {
        visibleRange.addListener (*this);
        startTimer (16);
    }

private:
    void paint (Graphics& g) override
    {
        g.setColour (Colours::red);

        for (auto position : provider())
        {
            g.drawVerticalLine (roundToInt (timeToXPosition (position)), 0.0f, (float) getHeight());
        }
    }

    void timerCallback() override
    {
        repaint();
    }

    void visibleRangeChanged (Range<double>) override
    {
        repaint();
    }

    double timeToXPosition (double time) const
    {
        return (time - visibleRange.getVisibleRange().getStart()) * getWidth()
                     / visibleRange.getVisibleRange().getLength();
    }

    VisibleRangeDataModel visibleRange;
    Provider provider;
};

//==============================================================================
class WaveformView final : public Component,
                           private ChangeListener,
                           private DataModel::Listener,
                           private VisibleRangeDataModel::Listener
{
public:
    WaveformView (const DataModel& model,
                  const VisibleRangeDataModel& vr)
        : dataModel (model),
          visibleRange (vr),
          thumbnailCache (4),
          thumbnail (4, dataModel.getAudioFormatManager(), thumbnailCache)
    {
        dataModel   .addListener (*this);
        visibleRange.addListener (*this);
        thumbnail   .addChangeListener (this);
    }

private:
    void paint (Graphics& g) override
    {
        // Draw the waveforms
        g.fillAll (Colours::black);
        auto numChannels = thumbnail.getNumChannels();

        if (numChannels == 0)
        {
            g.setColour (Colours::white);
            g.drawFittedText ("No File Loaded", getLocalBounds(), Justification::centred, 1);
            return;
        }

        auto bounds = getLocalBounds();
        auto channelHeight = bounds.getHeight() / numChannels;

        for (auto i = 0; i != numChannels; ++i)
        {
            drawChannel (g, i, bounds.removeFromTop (channelHeight));
        }
    }

    void changeListenerCallback (ChangeBroadcaster* source) override
    {
        if (source == &thumbnail)
            repaint();
    }

    void sampleReaderChanged (std::shared_ptr<AudioFormatReaderFactory> value) override
    {
        if (value != nullptr)
        {
            if (auto reader = value->make (dataModel.getAudioFormatManager()))
            {
                thumbnail.setReader (reader.release(), currentHashCode);
                currentHashCode += 1;

                return;
            }
        }

        thumbnail.clear();
    }

    void visibleRangeChanged (Range<double>) override
    {
        repaint();
    }

    void drawChannel (Graphics& g, int channel, Rectangle<int> bounds)
    {
        g.setGradientFill (ColourGradient (Colours::lightblue,
                                           bounds.getTopLeft().toFloat(),
                                           Colours::darkgrey,
                                           bounds.getBottomLeft().toFloat(),
                                           false));
        thumbnail.drawChannel (g,
                               bounds,
                               visibleRange.getVisibleRange().getStart(),
                               visibleRange.getVisibleRange().getEnd(),
                               channel,
                               1.0f);
    }

    DataModel dataModel;
    VisibleRangeDataModel visibleRange;
    AudioThumbnailCache thumbnailCache;
    AudioThumbnail thumbnail;
    int64 currentHashCode = 0;
};

//==============================================================================
class WaveformEditor final : public Component,
                             private DataModel::Listener
{
public:
    WaveformEditor (const DataModel& model,
                    PlaybackPositionOverlay::Provider provider,
                    UndoManager& undoManager)
        : dataModel (model),
          waveformView (model, visibleRange),
          playbackOverlay (visibleRange, std::move (provider)),
          loopPoints (dataModel, visibleRange, undoManager),
          ruler (visibleRange)
    {
        dataModel.addListener (*this);

        addAndMakeVisible (waveformView);
        addAndMakeVisible (playbackOverlay);
        addChildComponent (loopPoints);
        loopPoints.setAlwaysOnTop (true);

        waveformView.toBack();

        addAndMakeVisible (ruler);
    }

private:
    void resized() override
    {
        auto bounds = getLocalBounds();
        ruler          .setBounds (bounds.removeFromTop (25));
        waveformView   .setBounds (bounds);
        playbackOverlay.setBounds (bounds);
        loopPoints     .setBounds (bounds);
    }

    void loopModeChanged (LoopMode value) override
    {
        loopPoints.setVisible (value != LoopMode::none);
    }

    void sampleReaderChanged (std::shared_ptr<AudioFormatReaderFactory>) override
    {
        auto lengthInSeconds = dataModel.getSampleLengthSeconds();
        visibleRange.setTotalRange   (Range<double> (0, lengthInSeconds), nullptr);
        visibleRange.setVisibleRange (Range<double> (0, lengthInSeconds), nullptr);
    }

    DataModel dataModel;
    VisibleRangeDataModel visibleRange;
    WaveformView waveformView;
    PlaybackPositionOverlay playbackOverlay;
    LoopPointsOverlay loopPoints;
    Ruler ruler;
};

//==============================================================================
class MainSamplerView final : public Component,
                              private DataModel::Listener,
                              private ChangeListener
{
public:
    MainSamplerView (const DataModel& model,
                     PlaybackPositionOverlay::Provider provider,
                     UndoManager& um)
        : dataModel (model),
          waveformEditor (dataModel, std::move (provider), um),
          undoManager (um)
    {
        dataModel.addListener (*this);

        addAndMakeVisible (waveformEditor);
        addAndMakeVisible (loadNewSampleButton);
        addAndMakeVisible (undoButton);
        addAndMakeVisible (redoButton);

        auto setReader = [this] (const FileChooser& fc)
        {
            const auto result = fc.getResult();

            if (result != File())
            {
                undoManager.beginNewTransaction();
                auto readerFactory = new FileAudioFormatReaderFactory (result);
                dataModel.setSampleReader (std::unique_ptr<AudioFormatReaderFactory> (readerFactory),
                                           &undoManager);
            }
        };

        loadNewSampleButton.onClick = [this, setReader]
        {
            fileChooser.launchAsync (FileBrowserComponent::FileChooserFlags::openMode |
                                     FileBrowserComponent::FileChooserFlags::canSelectFiles,
                                     setReader);
        };

        addAndMakeVisible (centreFrequency);
        centreFrequency.onValueChange = [this]
        {
            undoManager.beginNewTransaction();
            dataModel.setCentreFrequencyHz (centreFrequency.getValue(),
                                            centreFrequency.isMouseButtonDown() ? nullptr : &undoManager);
        };

        centreFrequency.setRange (20, 20000, 1);
        centreFrequency.setSliderStyle (Slider::SliderStyle::IncDecButtons);
        centreFrequency.setIncDecButtonsMode (Slider::IncDecButtonMode::incDecButtonsDraggable_Vertical);

        auto radioGroupId = 1;

        for (auto buttonPtr : { &loopKindNone, &loopKindForward, &loopKindPingpong })
        {
            addAndMakeVisible (buttonPtr);
            buttonPtr->setRadioGroupId (radioGroupId, dontSendNotification);
            buttonPtr->setClickingTogglesState (true);
        }

        loopKindNone.onClick = [this]
        {
            if (loopKindNone.getToggleState())
            {
                undoManager.beginNewTransaction();
                dataModel.setLoopMode (LoopMode::none, &undoManager);
            }
        };

        loopKindForward.onClick = [this]
        {
            if (loopKindForward.getToggleState())
            {
                undoManager.beginNewTransaction();
                dataModel.setLoopMode (LoopMode::forward, &undoManager);
            }
        };

        loopKindPingpong.onClick = [this]
        {
            if (loopKindPingpong.getToggleState())
            {
                undoManager.beginNewTransaction();
                dataModel.setLoopMode (LoopMode::pingpong, &undoManager);
            }
        };

        undoButton.onClick = [this] { undoManager.undo(); };
        redoButton.onClick = [this] { undoManager.redo(); };

        addAndMakeVisible (centreFrequencyLabel);
        addAndMakeVisible (loopKindLabel);

        changeListenerCallback (&undoManager);
        undoManager.addChangeListener (this);
    }

    ~MainSamplerView() override
    {
        undoManager.removeChangeListener (this);
    }

private:
    void changeListenerCallback (ChangeBroadcaster* source) override
    {
        if (source == &undoManager)
        {
            undoButton.setEnabled (undoManager.canUndo());
            redoButton.setEnabled (undoManager.canRedo());
        }
    }

    void resized() override
    {
        auto bounds = getLocalBounds();

        auto topBar = bounds.removeFromTop (50);
        auto padding = 4;
        loadNewSampleButton .setBounds (topBar.removeFromRight (100).reduced (padding));
        redoButton          .setBounds (topBar.removeFromRight (100).reduced (padding));
        undoButton          .setBounds (topBar.removeFromRight (100).reduced (padding));
        centreFrequencyLabel.setBounds (topBar.removeFromLeft  (100).reduced (padding));
        centreFrequency     .setBounds (topBar.removeFromLeft  (100).reduced (padding));

        auto bottomBar = bounds.removeFromBottom (50);
        loopKindLabel   .setBounds (bottomBar.removeFromLeft (100).reduced (padding));
        loopKindNone    .setBounds (bottomBar.removeFromLeft (80) .reduced (padding));
        loopKindForward .setBounds (bottomBar.removeFromLeft (80) .reduced (padding));
        loopKindPingpong.setBounds (bottomBar.removeFromLeft (80) .reduced (padding));

        waveformEditor.setBounds (bounds);
    }

    void loopModeChanged (LoopMode value) override
    {
        switch (value)
        {
            case LoopMode::none:
                loopKindNone.setToggleState (true, dontSendNotification);
                break;
            case LoopMode::forward:
                loopKindForward.setToggleState (true, dontSendNotification);
                break;
            case LoopMode::pingpong:
                loopKindPingpong.setToggleState (true, dontSendNotification);
                break;

            default:
                break;
        }
    }

    void centreFrequencyHzChanged (double value) override
    {
        centreFrequency.setValue (value, dontSendNotification);
    }

    DataModel dataModel;
    WaveformEditor waveformEditor;
    TextButton loadNewSampleButton { "Load New Sample" };
    TextButton undoButton { "Undo" };
    TextButton redoButton { "Redo" };
    Slider centreFrequency;

    TextButton loopKindNone        { "None" },
               loopKindForward     { "Forward" },
               loopKindPingpong    { "Ping Pong" };

    Label centreFrequencyLabel     { {}, "Sample Centre Freq / Hz" },
          loopKindLabel            { {}, "Looping Mode" };


    FileChooser fileChooser { "Select a file to load...", File(),
                              dataModel.getAudioFormatManager().getWildcardForAllFormats() };

    UndoManager& undoManager;
};

//==============================================================================
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
