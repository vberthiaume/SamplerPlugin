#include "MainSamplerView.h"

MainSamplerView::MainSamplerView (const DataModel& model, PlaybackPositionOverlay::Provider provider, UndoManager& um)
    : dataModel (model),
    undoManager (um)
{
    dataModel.addListener (*this);

    addAndMakeVisible (loadNewSampleButton);
    addAndMakeVisible (undoButton);
    addAndMakeVisible (redoButton);

    auto setReader = [this](const FileChooser& fc)
        {
            const auto result = fc.getResult ();

            if (result != File ())
            {
                undoManager.beginNewTransaction ();
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
            undoManager.beginNewTransaction ();
            dataModel.setCentreFrequencyHz (centreFrequency.getValue (),
                                            centreFrequency.isMouseButtonDown () ? nullptr : &undoManager);
        };

    centreFrequency.setRange (20, 20000, 1);
    centreFrequency.setSliderStyle (Slider::SliderStyle::IncDecButtons);
    centreFrequency.setIncDecButtonsMode (Slider::IncDecButtonMode::incDecButtonsDraggable_Vertical);

    undoButton.onClick = [this] { undoManager.undo (); };
    redoButton.onClick = [this] { undoManager.redo (); };

    addAndMakeVisible (centreFrequencyLabel);

    changeListenerCallback (&undoManager);
    undoManager.addChangeListener (this);
}

void MainSamplerView::changeListenerCallback (ChangeBroadcaster* source)
{
    if (source == &undoManager)
    {
        undoButton.setEnabled (undoManager.canUndo ());
        redoButton.setEnabled (undoManager.canRedo ());
    }
}

void MainSamplerView::resized ()
{
    auto bounds = getLocalBounds ();

    auto topBar = bounds.removeFromTop (50);
    auto padding = 4;
    loadNewSampleButton.setBounds (topBar.removeFromRight (100).reduced (padding));
    redoButton.setBounds (topBar.removeFromRight (100).reduced (padding));
    undoButton.setBounds (topBar.removeFromRight (100).reduced (padding));
    centreFrequencyLabel.setBounds (topBar.removeFromLeft (100).reduced (padding));
    centreFrequency.setBounds (topBar.removeFromLeft (100).reduced (padding));
}
