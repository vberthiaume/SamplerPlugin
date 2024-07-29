#pragma once

#include "DSP/AudioFormatReaderFactory.h"

class MPESettingsDataModel final : private ValueTree::Listener
{
public:
    class Listener
    {
    public:
        virtual ~Listener () noexcept = default;
        virtual void synthVoicesChanged (int) {}
        virtual void voiceStealingEnabledChanged (bool) {}
        virtual void legacyModeEnabledChanged (bool) {}
        virtual void mpeZoneLayoutChanged (const MPEZoneLayout&) {}
        virtual void legacyFirstChannelChanged (int) {}
        virtual void legacyLastChannelChanged (int) {}
        virtual void legacyPitchbendRangeChanged (int) {}
    };

    MPESettingsDataModel ()
        : MPESettingsDataModel (ValueTree (IDs::MPE_SETTINGS))
    {
    }

    explicit MPESettingsDataModel (const ValueTree& vt)
        : valueTree (vt),
        synthVoices (valueTree, IDs::synthVoices, nullptr, 15),
        voiceStealingEnabled (valueTree, IDs::voiceStealingEnabled, nullptr, false),
        legacyModeEnabled (valueTree, IDs::legacyModeEnabled, nullptr, true),
        mpeZoneLayout (valueTree, IDs::mpeZoneLayout, nullptr, {}),
        legacyFirstChannel (valueTree, IDs::legacyFirstChannel, nullptr, 1),
        legacyLastChannel (valueTree, IDs::legacyLastChannel, nullptr, 15),
        legacyPitchbendRange (valueTree, IDs::legacyPitchbendRange, nullptr, 48)
    {
        jassert (valueTree.hasType (IDs::MPE_SETTINGS));
        valueTree.addListener (this);
    }

    MPESettingsDataModel (const MPESettingsDataModel& other)
        : MPESettingsDataModel (other.valueTree)
    {
    }

    MPESettingsDataModel& operator= (const MPESettingsDataModel& other)
    {
        auto copy (other);
        swap (copy);
        return *this;
    }

    int getSynthVoices () const
    {
        return synthVoices;
    }

    void setSynthVoices (int value, UndoManager* undoManager)
    {
        synthVoices.setValue (Range<int> (1, 20).clipValue (value), undoManager);
    }

    bool getVoiceStealingEnabled () const
    {
        return voiceStealingEnabled;
    }

    void setVoiceStealingEnabled (bool value, UndoManager* undoManager)
    {
        voiceStealingEnabled.setValue (value, undoManager);
    }

    bool getLegacyModeEnabled () const
    {
        return legacyModeEnabled;
    }

    void setLegacyModeEnabled (bool value, UndoManager* undoManager)
    {
        legacyModeEnabled.setValue (value, undoManager);
    }

    MPEZoneLayout getMPEZoneLayout () const
    {
        return mpeZoneLayout;
    }

    void setMPEZoneLayout (MPEZoneLayout value, UndoManager* undoManager)
    {
        mpeZoneLayout.setValue (value, undoManager);
    }

    int getLegacyFirstChannel () const
    {
        return legacyFirstChannel;
    }

    void setLegacyFirstChannel (int value, UndoManager* undoManager)
    {
        legacyFirstChannel.setValue (Range<int> (1, legacyLastChannel).clipValue (value), undoManager);
    }

    int getLegacyLastChannel () const
    {
        return legacyLastChannel;
    }

    void setLegacyLastChannel (int value, UndoManager* undoManager)
    {
        legacyLastChannel.setValue (Range<int> (legacyFirstChannel, 15).clipValue (value), undoManager);
    }

    int getLegacyPitchbendRange () const
    {
        return legacyPitchbendRange;
    }

    void setLegacyPitchbendRange (int value, UndoManager* undoManager)
    {
        legacyPitchbendRange.setValue (Range<int> (0, 95).clipValue (value), undoManager);
    }

    void addListener (Listener& listener)
    {
        listenerList.add (&listener);
    }

    void removeListener (Listener& listener)
    {
        listenerList.remove (&listener);
    }

    void swap (MPESettingsDataModel& other) noexcept
    {
        using std::swap;
        swap (other.valueTree, valueTree);
    }

private:
    void valueTreePropertyChanged (ValueTree&, const Identifier& property) override
    {
        if (property == IDs::synthVoices)
        {
            synthVoices.forceUpdateOfCachedValue ();
            listenerList.call ([this](Listener& l) { l.synthVoicesChanged (synthVoices); });
        }
        else if (property == IDs::voiceStealingEnabled)
        {
            voiceStealingEnabled.forceUpdateOfCachedValue ();
            listenerList.call ([this](Listener& l) { l.voiceStealingEnabledChanged (voiceStealingEnabled); });
        }
        else if (property == IDs::legacyModeEnabled)
        {
            legacyModeEnabled.forceUpdateOfCachedValue ();
            listenerList.call ([this](Listener& l) { l.legacyModeEnabledChanged (legacyModeEnabled); });
        }
        else if (property == IDs::mpeZoneLayout)
        {
            mpeZoneLayout.forceUpdateOfCachedValue ();
            listenerList.call ([this](Listener& l) { l.mpeZoneLayoutChanged (mpeZoneLayout); });
        }
        else if (property == IDs::legacyFirstChannel)
        {
            legacyFirstChannel.forceUpdateOfCachedValue ();
            listenerList.call ([this](Listener& l) { l.legacyFirstChannelChanged (legacyFirstChannel); });
        }
        else if (property == IDs::legacyLastChannel)
        {
            legacyLastChannel.forceUpdateOfCachedValue ();
            listenerList.call ([this](Listener& l) { l.legacyLastChannelChanged (legacyLastChannel); });
        }
        else if (property == IDs::legacyPitchbendRange)
        {
            legacyPitchbendRange.forceUpdateOfCachedValue ();
            listenerList.call ([this](Listener& l) { l.legacyPitchbendRangeChanged (legacyPitchbendRange); });
        }
    }

    void valueTreeChildAdded (ValueTree&, ValueTree&)      override { jassertfalse; }
    void valueTreeChildRemoved (ValueTree&, ValueTree&, int) override { jassertfalse; }
    void valueTreeChildOrderChanged (ValueTree&, int, int)        override { jassertfalse; }
    void valueTreeParentChanged (ValueTree&)                  override { jassertfalse; }

    ValueTree valueTree;

    CachedValue<int> synthVoices;
    CachedValue<bool> voiceStealingEnabled;
    CachedValue<bool> legacyModeEnabled;
    CachedValue<MPEZoneLayout> mpeZoneLayout;
    CachedValue<int> legacyFirstChannel;
    CachedValue<int> legacyLastChannel;
    CachedValue<int> legacyPitchbendRange;

    ListenerList<Listener> listenerList;
};

//==============================================================================

class DataModel final : private ValueTree::Listener
{
public:
    class Listener
    {
    public:
        virtual ~Listener () noexcept = default;
        virtual void sampleReaderChanged (std::shared_ptr<AudioFormatReaderFactory>) {}
        virtual void centreFrequencyHzChanged (double) {}
    };

    explicit DataModel (AudioFormatManager& audioFormatManagerIn)
        : DataModel (audioFormatManagerIn, ValueTree (IDs::DATA_MODEL))
    {
    }

    DataModel (AudioFormatManager& audioFormatManagerIn, const ValueTree& vt)
        : audioFormatManager (&audioFormatManagerIn),
        valueTree (vt),
        sampleReader (valueTree, IDs::sampleReader, nullptr),
        centreFrequencyHz (valueTree, IDs::centreFrequencyHz, nullptr)
    {
        jassert (valueTree.hasType (IDs::DATA_MODEL));
        valueTree.addListener (this);
    }

    DataModel (const DataModel& other)
        : DataModel (*other.audioFormatManager, other.valueTree)
    {
    }

    DataModel& operator= (const DataModel& other)
    {
        auto copy (other);
        swap (copy);
        return *this;
    }

    std::unique_ptr<AudioFormatReader> getSampleReader () const
    {
        return sampleReader != nullptr ? sampleReader.get ()->make (*audioFormatManager) : nullptr;
    }

    void setSampleReader (std::unique_ptr<AudioFormatReaderFactory> readerFactory,
                          UndoManager* undoManager)
    {
        sampleReader.setValue (std::move (readerFactory), undoManager);
    }

    double getSampleLengthSeconds () const
    {
        if (auto r = getSampleReader ())
            return (double) r->lengthInSamples / r->sampleRate;

        return 1.0;
    }

    double getCentreFrequencyHz () const
    {
        return centreFrequencyHz;
    }

    void setCentreFrequencyHz (double value, UndoManager* undoManager)
    {
        centreFrequencyHz.setValue (Range<double> (20, 20000).clipValue (value),
                                    undoManager);
    }

    MPESettingsDataModel mpeSettings ()
    {
        return MPESettingsDataModel (valueTree.getOrCreateChildWithName (IDs::MPE_SETTINGS, nullptr));
    }

    void addListener (Listener& listener)
    {
        listenerList.add (&listener);
    }

    void removeListener (Listener& listener)
    {
        listenerList.remove (&listener);
    }

    void swap (DataModel& other) noexcept
    {
        using std::swap;
        swap (other.valueTree, valueTree);
    }

    AudioFormatManager& getAudioFormatManager () const
    {
        return *audioFormatManager;
    }

private:
    void valueTreePropertyChanged (ValueTree&, const Identifier& property) override
    {
        if (property == IDs::sampleReader)
        {
            sampleReader.forceUpdateOfCachedValue ();
            listenerList.call ([this](Listener& l) { l.sampleReaderChanged (sampleReader); });
        }
        else if (property == IDs::centreFrequencyHz)
        {
            centreFrequencyHz.forceUpdateOfCachedValue ();
            listenerList.call ([this](Listener& l) { l.centreFrequencyHzChanged (centreFrequencyHz); });
        }
    }

    void valueTreeChildAdded (ValueTree&, ValueTree&)      override {}
    void valueTreeChildRemoved (ValueTree&, ValueTree&, int) override { jassertfalse; }
    void valueTreeChildOrderChanged (ValueTree&, int, int)        override { jassertfalse; }
    void valueTreeParentChanged (ValueTree&)                  override { jassertfalse; }

    AudioFormatManager* audioFormatManager;

    ValueTree valueTree;

    CachedValue<std::shared_ptr<AudioFormatReaderFactory>> sampleReader;
    CachedValue<double> centreFrequencyHz;

    ListenerList<Listener> listenerList;
};
