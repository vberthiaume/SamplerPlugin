/*
  ==============================================================================

    Sampler.cpp
    Created: 23 Jul 2024 10:35:57am
    Author:  barth

  ==============================================================================
*/

#include "Sampler.h"

void MPESamplerVoice::noteStarted ()
{
    jassert (currentlyPlayingNote.isValid ());
    jassert (currentlyPlayingNote.keyState == MPENote::keyDown
             || currentlyPlayingNote.keyState == MPENote::keyDownAndSustained);

    level.setTargetValue (currentlyPlayingNote.noteOnVelocity.asUnsignedFloat ());
    frequency.setTargetValue (currentlyPlayingNote.getFrequencyInHertz ());

    for (auto smoothed : { &level, &frequency })
        smoothed->reset (currentSampleRate, smoothingLengthInSeconds);

    previousPressure = currentlyPlayingNote.pressure.asUnsignedFloat ();
    currentSamplePos = 0.0;
    tailOff = 0.0;
}

void MPESamplerVoice::noteStopped (bool allowTailOff)
{
    jassert (currentlyPlayingNote.keyState == MPENote::off);

    if (allowTailOff && approximatelyEqual (tailOff, 0.0))
        tailOff = 1.0;
    else
        stopNote ();
}

void MPESamplerVoice::notePressureChanged ()
{
    const auto currentPressure = static_cast<double> (currentlyPlayingNote.pressure.asUnsignedFloat ());
    const auto deltaPressure = currentPressure - previousPressure;
    level.setTargetValue (jlimit (0.0, 1.0, level.getCurrentValue () + deltaPressure));
    previousPressure = currentPressure;
}
