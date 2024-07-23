#pragma once

#include "../DataModel.h"

class PlaybackPositionOverlay final : public Component,
    private Timer,
    private VisibleRangeDataModel::Listener
{
public:
    using Provider = std::function<std::vector<float> ()>;
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

        for (auto position : provider ())
        {
            g.drawVerticalLine (roundToInt (timeToXPosition (position)), 0.0f, (float) getHeight ());
        }
    }

    void timerCallback () override
    {
        repaint ();
    }

    void visibleRangeChanged (Range<double>) override
    {
        repaint ();
    }

    double timeToXPosition (double time) const
    {
        return (time - visibleRange.getVisibleRange ().getStart ()) * getWidth ()
            / visibleRange.getVisibleRange ().getLength ();
    }

    VisibleRangeDataModel visibleRange;
    Provider provider;
};

