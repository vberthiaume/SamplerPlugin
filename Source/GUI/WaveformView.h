#pragma once

#include "../DataModel.h"

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
        thumbnail (4, dataModel.getAudioFormatManager (), thumbnailCache)
    {
        dataModel.addListener (*this);
        visibleRange.addListener (*this);
        thumbnail.addChangeListener (this);
    }

private:
    void paint (Graphics& g) override
    {
        // Draw the waveforms
        g.fillAll (Colours::black);
        auto numChannels = thumbnail.getNumChannels ();

        if (numChannels == 0)
        {
            g.setColour (Colours::white);
            g.drawFittedText ("No File Loaded", getLocalBounds (), Justification::centred, 1);
            return;
        }

        auto bounds = getLocalBounds ();
        auto channelHeight = bounds.getHeight () / numChannels;

        for (auto i = 0; i != numChannels; ++i)
        {
            drawChannel (g, i, bounds.removeFromTop (channelHeight));
        }
    }

    void changeListenerCallback (ChangeBroadcaster* source) override
    {
        if (source == &thumbnail)
            repaint ();
    }

    void sampleReaderChanged (std::shared_ptr<AudioFormatReaderFactory> value) override
    {
        if (value != nullptr)
        {
            if (auto reader = value->make (dataModel.getAudioFormatManager ()))
            {
                thumbnail.setReader (reader.release (), currentHashCode);
                currentHashCode += 1;

                return;
            }
        }

        thumbnail.clear ();
    }

    void visibleRangeChanged (Range<double>) override
    {
        repaint ();
    }

    void drawChannel (Graphics& g, int channel, Rectangle<int> bounds)
    {
        g.setGradientFill (ColourGradient (Colours::lightblue,
                                           bounds.getTopLeft ().toFloat (),
                                           Colours::darkgrey,
                                           bounds.getBottomLeft ().toFloat (),
                                           false));
        thumbnail.drawChannel (g,
                               bounds,
                               visibleRange.getVisibleRange ().getStart (),
                               visibleRange.getVisibleRange ().getEnd (),
                               channel,
                               1.0f);
    }

    DataModel dataModel;
    VisibleRangeDataModel visibleRange;
    AudioThumbnailCache thumbnailCache;
    AudioThumbnail thumbnail;
    int64 currentHashCode = 0;
};

