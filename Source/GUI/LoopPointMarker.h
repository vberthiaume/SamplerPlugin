#pragma once

#include "juceHeader.h"
using namespace juce;

class LoopPointMarker final : public Component
{
public:
    using MouseCallback = std::function<void (LoopPointMarker&, const MouseEvent&)>;

    LoopPointMarker (String marker,
                     MouseCallback onMouseDownIn,
                     MouseCallback onMouseDragIn,
                     MouseCallback onMouseUpIn)
        : text (std::move (marker)),
        onMouseDown (std::move (onMouseDownIn)),
        onMouseDrag (std::move (onMouseDragIn)),
        onMouseUp (std::move (onMouseUpIn))
    {
        setMouseCursor (MouseCursor::LeftRightResizeCursor);
    }

private:
    void resized () override
    {
        auto height = 20;
        auto triHeight = 6;

        auto bounds = getLocalBounds ();
        Path newPath;
        newPath.addRectangle (bounds.removeFromBottom (height));

        newPath.startNewSubPath (bounds.getBottomLeft ().toFloat ());
        newPath.lineTo (bounds.getBottomRight ().toFloat ());
        Point<float> apex (static_cast<float> (bounds.getX () + (bounds.getWidth () / 2)),
                           static_cast<float> (bounds.getBottom () - triHeight));
        newPath.lineTo (apex);
        newPath.closeSubPath ();

        newPath.addLineSegment (Line<float> (apex, Point<float> (apex.getX (), 0)), 1);

        path = newPath;
    }

    void paint (Graphics& g) override
    {
        g.setColour (Colours::deepskyblue);
        g.fillPath (path);

        auto height = 20;
        g.setColour (Colours::white);
        g.drawText (text, getLocalBounds ().removeFromBottom (height), Justification::centred);
    }

    bool hitTest (int x, int y) override
    {
        return path.contains ((float) x, (float) y);
    }

    void mouseDown (const MouseEvent& e) override
    {
        onMouseDown (*this, e);
    }

    void mouseDrag (const MouseEvent& e) override
    {
        onMouseDrag (*this, e);
    }

    void mouseUp (const MouseEvent& e) override
    {
        onMouseUp (*this, e);
    }

    String text;
    Path path;
    MouseCallback onMouseDown;
    MouseCallback onMouseDrag;
    MouseCallback onMouseUp;
};

