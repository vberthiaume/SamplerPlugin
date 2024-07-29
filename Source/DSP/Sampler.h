#pragma once

#include "juceHeader.h"
using namespace juce;

//==============================================================================
// Represents the constant parts of an audio sample: its name, sample rate,
// length, and the audio sample data itself.
// Samples might be pretty big, so we'll keep shared_ptrs to them most of the
// time, to reduce duplication and copying.
class Sample final
{
public:
    Sample (AudioFormatReader& source, double maxSampleLengthSecs) :
        sourceSampleRate (source.sampleRate),
        length (jmin (int (source.lengthInSamples), int (maxSampleLengthSecs* sourceSampleRate))),
        data (jmin (2, int (source.numChannels)), length + 4)
    {
        if (length == 0)
            throw std::runtime_error ("Unable to load sample");

        source.read (&data, 0, length + 4, 0, true, true);
    }

    double getSampleRate () const { return sourceSampleRate; }
    int getLength () const { return length; }
    const AudioBuffer<float>& getBuffer () const { return data; }

private:
    double sourceSampleRate;
    int length;
    AudioBuffer<float> data;
};

//==============================================================================
// A class which contains all the information related to sample-playback, such
// as sample data, loop points, and loop kind.
// It is expected that multiple sampler voices will maintain pointers to a
// single instance of this class, to avoid redundant duplication of sample
// data in memory.
class MPESamplerSound final
{
public:
    void setSample (std::unique_ptr<Sample> value)
    {
        sample = std::move (value);
    }

    Sample* getSample () const
    {
        return sample.get ();
    }

    void setCentreFrequencyInHz (double centre)
    {
        centreFrequencyInHz = centre;
    }

    double getCentreFrequencyInHz () const
    {
        return centreFrequencyInHz;
    }

private:
    std::unique_ptr<Sample> sample;
    double centreFrequencyInHz { 440.0 };
};

//==============================================================================
class MPESamplerVoice final : public MPESynthesiserVoice
{
public:
    explicit MPESamplerVoice (std::shared_ptr<const MPESamplerSound> sound)
        : samplerSound (std::move (sound))
    {
        jassert (samplerSound != nullptr);
    }

    void noteStarted () override;

    void noteStopped (bool allowTailOff) override;

    void notePressureChanged () override;

    void notePitchbendChanged () override
    {
        frequency.setTargetValue (currentlyPlayingNote.getFrequencyInHertz ());
    }

    void noteTimbreChanged ()   override {}
    void noteKeyStateChanged () override {}

    void renderNextBlock (AudioBuffer<float>& outputBuffer,
                          int startSample,
                          int numSamples) override
    {
        render (outputBuffer, startSample, numSamples);
    }

    void renderNextBlock (AudioBuffer<double>& outputBuffer,
                          int startSample,
                          int numSamples) override
    {
        render (outputBuffer, startSample, numSamples);
    }

    double getCurrentSamplePosition () const
    {
        return currentSamplePos;
    }

private:
    template <typename Element>
    void render (AudioBuffer<Element>& outputBuffer, int startSample, int numSamples);

    template <typename Element>
    bool renderNextSample (const float* inL,
                           const float* inR,
                           Element* outL,
                           Element* outR,
                           size_t writePos);

    bool isTailingOff () const { return ! approximatelyEqual (tailOff, 0.0); }

    void stopNote ()
    {
        clearCurrentNote ();
        currentSamplePos = 0.0;
    }

    double getNextState (double freq) const
    {
        auto nextPitchRatio = freq / samplerSound->getCentreFrequencyInHz ();

        auto nextSamplePos = currentSamplePos;
        nextSamplePos += nextPitchRatio;
        return nextSamplePos;
    }

    std::shared_ptr<const MPESamplerSound> samplerSound;
    SmoothedValue<double> level { 0 };
    SmoothedValue<double> frequency { 0 };
    double previousPressure { 0 };
    double currentSamplePos { 0 };
    double tailOff { 0 };
    double smoothingLengthInSeconds { 0.01 };
};

//=================================================================================

template<typename Element>
void MPESamplerVoice::render (AudioBuffer<Element>& outputBuffer, int startSample, int numSamples)
{
    jassert (samplerSound->getSample () != nullptr);

    auto& data = samplerSound->getSample ()->getBuffer ();

    auto inL = data.getReadPointer (0);
    auto inR = data.getNumChannels () > 1 ? data.getReadPointer (1) : nullptr;

    auto outL = outputBuffer.getWritePointer (0, startSample);

    if (outL == nullptr)
        return;

    auto outR = outputBuffer.getNumChannels () > 1 ? outputBuffer.getWritePointer (1, startSample)
        : nullptr;

    size_t writePos = 0;

    while (--numSamples >= 0 && renderNextSample (inL, inR, outL, outR, writePos))
        writePos += 1;
}

template<typename Element>
bool MPESamplerVoice::renderNextSample (const float* inL, const float* inR, Element* outL, Element* outR, size_t writePos)
{
    auto currentLevel = level.getNextValue ();
    auto currentFrequency = frequency.getNextValue ();

    if (isTailingOff ())
    {
        currentLevel *= tailOff;
        tailOff *= 0.9999;

        if (tailOff < 0.005)
        {
            stopNote ();
            return false;
        }
    }

    auto pos = (int) currentSamplePos;
    auto nextPos = pos + 1;
    auto alpha = (Element) (currentSamplePos - pos);
    auto invAlpha = 1.0f - alpha;

    // just using a very simple linear interpolation here..
    auto l = static_cast<Element> (currentLevel * (inL[pos] * invAlpha + inL[nextPos] * alpha));
    auto r = static_cast<Element> ((inR != nullptr) ? currentLevel * (inR[pos] * invAlpha + inR[nextPos] * alpha)
                                   : l);

    if (outR != nullptr)
    {
        outL[writePos] += l;
        outR[writePos] += r;
    }
    else
    {
        outL[writePos] += (l + r) * 0.5f;
    }

    currentSamplePos = getNextState (currentFrequency);

    if (currentSamplePos > samplerSound->getSample ()->getLength ())
    {
        stopNote ();
        return false;
    }

    return true;
}
