#pragma once

#include "PlaybackPositionOverlay.h"
#include "WaveformView.h"
#include "LoopPointsOverlay.h"
#include "Ruler.h"


class WaveformEditor final : public Component,
    private DataModel::Listener
{
public:
    WaveformEditor (const DataModel& model,
                    PlaybackPositionOverlay::Provider provider,
                    UndoManager& /*undoManager*/)
        : dataModel (model),
        waveformView (model, visibleRange),
        playbackOverlay (visibleRange, std::move (provider)),
        ruler (visibleRange)
    {
        dataModel.addListener (*this);

        addAndMakeVisible (waveformView);
        addAndMakeVisible (playbackOverlay);

        waveformView.toBack ();

        addAndMakeVisible (ruler);
    }

private:
    void resized () override
    {
        auto bounds = getLocalBounds ();
        ruler.setBounds (bounds.removeFromTop (25));
        waveformView.setBounds (bounds);
        playbackOverlay.setBounds (bounds);
    }

    void sampleReaderChanged (std::shared_ptr<AudioFormatReaderFactory>) override
    {
        auto lengthInSeconds = dataModel.getSampleLengthSeconds ();
        visibleRange.setTotalRange (Range<double> (0, lengthInSeconds), nullptr);
        visibleRange.setVisibleRange (Range<double> (0, lengthInSeconds), nullptr);
    }

    DataModel dataModel;
    VisibleRangeDataModel visibleRange;
    WaveformView waveformView;
    PlaybackPositionOverlay playbackOverlay;
    Ruler ruler;
};

