#pragma once

#include "WaveformEditor.h"

class MainSamplerView final : public Component,
                              private DataModel::Listener,
                              private ChangeListener
{
public:
    MainSamplerView (const DataModel& model,
                     PlaybackPositionOverlay::Provider provider,
                     UndoManager& um);

    ~MainSamplerView () override { undoManager.removeChangeListener (this); }

private:
    void changeListenerCallback (ChangeBroadcaster* source) override;

    void resized () override;

    void centreFrequencyHzChanged (double value) override
    {
        centreFrequency.setValue (value, dontSendNotification);
    }

    DataModel dataModel;

    TextButton loadNewSampleButton { "Load New Sample" };
    TextButton undoButton { "Undo" };
    TextButton redoButton { "Redo" };
    Slider centreFrequency;

    Label centreFrequencyLabel { {}, "Sample Centre Freq / Hz" };

    FileChooser fileChooser { "Select a file to load...", File (),
                              dataModel.getAudioFormatManager ().getWildcardForAllFormats () };

    UndoManager& undoManager;
};

