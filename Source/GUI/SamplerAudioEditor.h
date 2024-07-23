#pragma once

#include "../DataModel.h"
#include "MainSamplerView.h"
#include "MpeSettingsComponent.h"

class SamplerAudioProcessor;
struct ProcessorState;

class SamplerAudioProcessorEditor final : public AudioProcessorEditor,
    public FileDragAndDropTarget,
    private DataModel::Listener,
    private MPESettingsDataModel::Listener
{
public:
    SamplerAudioProcessorEditor (SamplerAudioProcessor& p, ProcessorState state);

private:
    void resized () override
    {
        tabbedComponent.setBounds (getLocalBounds ());
    }

    bool keyPressed (const KeyPress& key) override
    {
        if (key == KeyPress ('z', ModifierKeys::commandModifier, 0))
        {
            undoManager.undo ();
            return true;
        }

        if (key == KeyPress ('z', ModifierKeys::commandModifier | ModifierKeys::shiftModifier, 0))
        {
            undoManager.redo ();
            return true;
        }

        return Component::keyPressed (key);
    }

    bool isInterestedInFileDrag (const StringArray& files) override
    {
        WildcardFileFilter filter (formatManager.getWildcardForAllFormats (), {}, "Known Audio Formats");
        return files.size () == 1 && filter.isFileSuitable (files[0]);
    }

    void filesDropped (const StringArray& files, int, int) override
    {
        jassert (files.size () == 1);
        undoManager.beginNewTransaction ();
        auto r = new FileAudioFormatReaderFactory (files[0]);
        dataModel.setSampleReader (std::unique_ptr<AudioFormatReaderFactory> (r),
                                   &undoManager);

    }

    void sampleReaderChanged (std::shared_ptr<AudioFormatReaderFactory> value) override;

    void centreFrequencyHzChanged (double value) override;

    void loopPointsSecondsChanged (Range<double> value) override;

    void loopModeChanged (LoopMode value) override;

    void synthVoicesChanged (int value) override;

    void voiceStealingEnabledChanged (bool value) override;

    void legacyModeEnabledChanged (bool value) override
    {
        if (value)
            setProcessorLegacyMode ();
        else
            setProcessorMPEMode ();
    }

    void mpeZoneLayoutChanged (const MPEZoneLayout&) override
    {
        setProcessorMPEMode ();
    }

    void legacyFirstChannelChanged (int) override
    {
        setProcessorLegacyMode ();
    }

    void legacyLastChannelChanged (int) override
    {
        setProcessorLegacyMode ();
    }

    void legacyPitchbendRangeChanged (int) override
    {
        setProcessorLegacyMode ();
    }

    void setProcessorLegacyMode ();

    void setProcessorMPEMode ();

    SamplerAudioProcessor& samplerAudioProcessor;
    AudioFormatManager formatManager;
    DataModel dataModel { formatManager };
    UndoManager undoManager;
    MPESettingsDataModel mpeSettings { dataModel.mpeSettings () };

    TabbedComponent tabbedComponent { TabbedButtonBar::Orientation::TabsAtTop };
    MPESettingsComponent settingsComponent { dataModel.mpeSettings (), undoManager };
    MainSamplerView mainSamplerView;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SamplerAudioProcessorEditor)
};
