#include "SamplerAudioEditor.h"
#include "../SamplerAudioProcessor.h"

SamplerAudioProcessorEditor::SamplerAudioProcessorEditor (SamplerAudioProcessor& p, ProcessorState state)
    : AudioProcessorEditor (&p),
    samplerAudioProcessor (p),
    mainSamplerView (dataModel, undoManager)
{
    dataModel.addListener (*this);
    mpeSettings.addListener (*this);

    formatManager.registerBasicFormats ();

    addAndMakeVisible (tabbedComponent);

    auto lookFeel = dynamic_cast<LookAndFeel_V4*> (&getLookAndFeel ());
    auto bg = lookFeel->getCurrentColourScheme ().getUIColour (LookAndFeel_V4::ColourScheme::UIColour::widgetBackground);

    tabbedComponent.addTab ("Sample Editor", bg, &mainSamplerView, false);
    tabbedComponent.addTab ("MPE Settings", bg, &settingsComponent, false);

    mpeSettings.setSynthVoices (state.synthVoices, nullptr);
    mpeSettings.setLegacyModeEnabled (state.legacyModeEnabled, nullptr);
    mpeSettings.setLegacyFirstChannel (state.legacyChannels.getStart (), nullptr);
    mpeSettings.setLegacyLastChannel (state.legacyChannels.getEnd (), nullptr);
    mpeSettings.setLegacyPitchbendRange (state.legacyPitchbendRange, nullptr);
    mpeSettings.setVoiceStealingEnabled (state.voiceStealingEnabled, nullptr);
    mpeSettings.setMPEZoneLayout (state.mpeZoneLayout, nullptr);

    dataModel.setSampleReader (std::move (state.readerFactory), nullptr);

    dataModel.setCentreFrequencyHz (state.centreFrequencyHz, nullptr);

    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    setResizable (true, true);
    setResizeLimits (640, 480, 2560, 1440);
    setSize (640, 480);
}

void SamplerAudioProcessorEditor::sampleReaderChanged (std::shared_ptr<AudioFormatReaderFactory> value)
{
    samplerAudioProcessor.setSample (value == nullptr ? nullptr : value->clone (),
                                     dataModel.getAudioFormatManager ());
}

void SamplerAudioProcessorEditor::centreFrequencyHzChanged (double value)
{
    samplerAudioProcessor.setCentreFrequency (value);
}

void SamplerAudioProcessorEditor::synthVoicesChanged (int value)
{
    samplerAudioProcessor.setNumberOfVoices (value);
}

void SamplerAudioProcessorEditor::voiceStealingEnabledChanged (bool value)
{
    samplerAudioProcessor.setVoiceStealingEnabled (value);
}

void SamplerAudioProcessorEditor::setProcessorLegacyMode ()
{
    samplerAudioProcessor.setLegacyModeEnabled (mpeSettings.getLegacyPitchbendRange (),
                                                Range<int> (mpeSettings.getLegacyFirstChannel (),
                                                            mpeSettings.getLegacyLastChannel ()));
}

void SamplerAudioProcessorEditor::setProcessorMPEMode ()
{
    samplerAudioProcessor.setMPEZoneLayout (mpeSettings.getMPEZoneLayout ());
}
