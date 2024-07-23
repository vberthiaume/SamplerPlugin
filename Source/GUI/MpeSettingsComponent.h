#pragma once

#include "../DataModel.h"

namespace
{
void initialiseComboBoxWithConsecutiveIntegers (Component& owner,
                                                ComboBox& comboBox,
                                                Label& label,
                                                int firstValue,
                                                int numValues,
                                                int valueToSelect)
{
    for (auto i = 0; i < numValues; ++i)
        comboBox.addItem (String (i + firstValue), i + 1);

    comboBox.setSelectedId (valueToSelect - firstValue + 1);

    label.attachToComponent (&comboBox, true);
    owner.addAndMakeVisible (comboBox);
}

constexpr int controlHeight = 24;
constexpr int controlSeparation = 6;

} // namespace

//==============================================================================
class MPELegacySettingsComponent final : public Component,
    private MPESettingsDataModel::Listener
{
public:
    explicit MPELegacySettingsComponent (const MPESettingsDataModel& model,
                                         UndoManager& um)
        : dataModel (model),
        undoManager (&um)
    {
        dataModel.addListener (*this);

        initialiseComboBoxWithConsecutiveIntegers (*this, legacyStartChannel, legacyStartChannelLabel, 1, 16, 1);
        initialiseComboBoxWithConsecutiveIntegers (*this, legacyEndChannel, legacyEndChannelLabel, 1, 16, 16);
        initialiseComboBoxWithConsecutiveIntegers (*this, legacyPitchbendRange, legacyPitchbendRangeLabel, 0, 96, 2);

        legacyStartChannel.onChange = [this]
            {
                if (isLegacyModeValid ())
                {
                    undoManager->beginNewTransaction ();
                    dataModel.setLegacyFirstChannel (getFirstChannel (), undoManager);
                }
            };

        legacyEndChannel.onChange = [this]
            {
                if (isLegacyModeValid ())
                {
                    undoManager->beginNewTransaction ();
                    dataModel.setLegacyLastChannel (getLastChannel (), undoManager);
                }
            };

        legacyPitchbendRange.onChange = [this]
            {
                if (isLegacyModeValid ())
                {
                    undoManager->beginNewTransaction ();
                    dataModel.setLegacyPitchbendRange (legacyPitchbendRange.getText ().getIntValue (), undoManager);
                }
            };
    }

    int getMinHeight () const
    {
        return (controlHeight * 3) + (controlSeparation * 2);
    }

private:
    void resized () override
    {
        Rectangle<int> r (proportionOfWidth (0.65f), 0, proportionOfWidth (0.25f), getHeight ());

        for (auto& comboBox : { &legacyStartChannel, &legacyEndChannel, &legacyPitchbendRange })
        {
            comboBox->setBounds (r.removeFromTop (controlHeight));
            r.removeFromTop (controlSeparation);
        }
    }

    bool isLegacyModeValid ()
    {
        if (! areLegacyModeParametersValid ())
        {
            handleInvalidLegacyModeParameters ();
            return false;
        }

        return true;
    }

    void legacyFirstChannelChanged (int value) override
    {
        legacyStartChannel.setSelectedId (value, dontSendNotification);
    }

    void legacyLastChannelChanged (int value) override
    {
        legacyEndChannel.setSelectedId (value, dontSendNotification);
    }

    void legacyPitchbendRangeChanged (int value) override
    {
        legacyPitchbendRange.setSelectedId (value + 1, dontSendNotification);
    }

    int getFirstChannel () const
    {
        return legacyStartChannel.getText ().getIntValue ();
    }

    int getLastChannel () const
    {
        return legacyEndChannel.getText ().getIntValue ();
    }

    bool areLegacyModeParametersValid () const
    {
        return getFirstChannel () <= getLastChannel ();
    }

    void handleInvalidLegacyModeParameters ()
    {
        auto options = MessageBoxOptions::makeOptionsOk (AlertWindow::WarningIcon,
                                                         "Invalid legacy mode channel layout",
                                                         "Cannot set legacy mode start/end channel:\n"
                                                         "The end channel must not be less than the start channel!",
                                                         "Got it");
        messageBox = AlertWindow::showScopedAsync (options, nullptr);
    }

    MPESettingsDataModel dataModel;

    ComboBox legacyStartChannel, legacyEndChannel, legacyPitchbendRange;

    Label legacyStartChannelLabel { {}, "First channel" },
        legacyEndChannelLabel { {}, "Last channel" },
        legacyPitchbendRangeLabel { {}, "Pitchbend range (semitones)" };

    UndoManager* undoManager;
    ScopedMessageBox messageBox;
};

//==============================================================================
class MPENewSettingsComponent final : public Component,
    private MPESettingsDataModel::Listener
{
public:
    MPENewSettingsComponent (const MPESettingsDataModel& model,
                             UndoManager& um)
        : dataModel (model),
        undoManager (&um)
    {
        dataModel.addListener (*this);

        addAndMakeVisible (isLowerZoneButton);
        isLowerZoneButton.setToggleState (true, NotificationType::dontSendNotification);

        initialiseComboBoxWithConsecutiveIntegers (*this, memberChannels, memberChannelsLabel, 0, 16, 15);
        initialiseComboBoxWithConsecutiveIntegers (*this, masterPitchbendRange, masterPitchbendRangeLabel, 0, 96, 2);
        initialiseComboBoxWithConsecutiveIntegers (*this, notePitchbendRange, notePitchbendRangeLabel, 0, 96, 48);

        for (auto& button : { &setZoneButton, &clearAllZonesButton })
            addAndMakeVisible (button);

        setZoneButton.onClick = [this]
            {
                auto isLowerZone = isLowerZoneButton.getToggleState ();
                auto numMemberChannels = memberChannels.getText ().getIntValue ();
                auto perNotePb = notePitchbendRange.getText ().getIntValue ();
                auto masterPb = masterPitchbendRange.getText ().getIntValue ();

                if (isLowerZone)
                    zoneLayout.setLowerZone (numMemberChannels, perNotePb, masterPb);
                else
                    zoneLayout.setUpperZone (numMemberChannels, perNotePb, masterPb);

                undoManager->beginNewTransaction ();
                dataModel.setMPEZoneLayout (zoneLayout, undoManager);
            };

        clearAllZonesButton.onClick = [this]
            {
                zoneLayout.clearAllZones ();
                undoManager->beginNewTransaction ();
                dataModel.setMPEZoneLayout (zoneLayout, undoManager);
            };
    }

    int getMinHeight () const
    {
        return (controlHeight * 6) + (controlSeparation * 6);
    }

private:
    void resized () override
    {
        Rectangle<int> r (proportionOfWidth (0.65f), 0, proportionOfWidth (0.25f), getHeight ());

        isLowerZoneButton.setBounds (r.removeFromTop (controlHeight));
        r.removeFromTop (controlSeparation);

        for (auto& comboBox : { &memberChannels, &masterPitchbendRange, &notePitchbendRange })
        {
            comboBox->setBounds (r.removeFromTop (controlHeight));
            r.removeFromTop (controlSeparation);
        }

        r.removeFromTop (controlSeparation);

        auto buttonLeft = proportionOfWidth (0.5f);

        setZoneButton.setBounds (r.removeFromTop (controlHeight).withLeft (buttonLeft));
        r.removeFromTop (controlSeparation);
        clearAllZonesButton.setBounds (r.removeFromTop (controlHeight).withLeft (buttonLeft));
    }

    void mpeZoneLayoutChanged (const MPEZoneLayout& value) override
    {
        zoneLayout = value;
    }

    MPESettingsDataModel dataModel;
    MPEZoneLayout zoneLayout;

    ComboBox memberChannels, masterPitchbendRange, notePitchbendRange;

    ToggleButton isLowerZoneButton { "Lower zone" };

    Label memberChannelsLabel { {}, "Nr. of member channels" },
        masterPitchbendRangeLabel { {}, "Master pitchbend range (semitones)" },
        notePitchbendRangeLabel { {}, "Note pitchbend range (semitones)" };

    TextButton setZoneButton { "Set zone" },
        clearAllZonesButton { "Clear all zones" };

    UndoManager* undoManager;
};

//==============================================================================
class MPESettingsComponent final : public Component,
    private MPESettingsDataModel::Listener
{
public:
    MPESettingsComponent (const MPESettingsDataModel& model,
                          UndoManager& um)
        : dataModel (model),
        legacySettings (dataModel, um),
        newSettings (dataModel, um),
        undoManager (&um)
    {
        dataModel.addListener (*this);

        addAndMakeVisible (newSettings);
        addChildComponent (legacySettings);

        initialiseComboBoxWithConsecutiveIntegers (*this, numberOfVoices, numberOfVoicesLabel, 1, 20, 15);
        numberOfVoices.onChange = [this]
            {
                undoManager->beginNewTransaction ();
                dataModel.setSynthVoices (numberOfVoices.getText ().getIntValue (), undoManager);
            };

        for (auto& button : { &legacyModeEnabledToggle, &voiceStealingEnabledToggle })
        {
            addAndMakeVisible (button);
        }

        legacyModeEnabledToggle.onClick = [this]
            {
                undoManager->beginNewTransaction ();
                dataModel.setLegacyModeEnabled (legacyModeEnabledToggle.getToggleState (), undoManager);
            };

        voiceStealingEnabledToggle.onClick = [this]
            {
                undoManager->beginNewTransaction ();
                dataModel.setVoiceStealingEnabled (voiceStealingEnabledToggle.getToggleState (), undoManager);
            };
    }

private:
    void resized () override
    {
        auto topHeight = jmax (legacySettings.getMinHeight (), newSettings.getMinHeight ());
        auto r = getLocalBounds ();
        r.removeFromTop (15);
        auto top = r.removeFromTop (topHeight);
        legacySettings.setBounds (top);
        newSettings.setBounds (top);

        r.removeFromLeft (proportionOfWidth (0.65f));
        r = r.removeFromLeft (proportionOfWidth (0.25f));

        auto toggleLeft = proportionOfWidth (0.25f);

        legacyModeEnabledToggle.setBounds (r.removeFromTop (controlHeight).withLeft (toggleLeft));
        r.removeFromTop (controlSeparation);
        voiceStealingEnabledToggle.setBounds (r.removeFromTop (controlHeight).withLeft (toggleLeft));
        r.removeFromTop (controlSeparation);
        numberOfVoices.setBounds (r.removeFromTop (controlHeight));
    }

    void legacyModeEnabledChanged (bool value) override
    {
        legacySettings.setVisible (value);
        newSettings.setVisible (! value);
        legacyModeEnabledToggle.setToggleState (value, dontSendNotification);
    }

    void voiceStealingEnabledChanged (bool value) override
    {
        voiceStealingEnabledToggle.setToggleState (value, dontSendNotification);
    }

    void synthVoicesChanged (int value) override
    {
        numberOfVoices.setSelectedId (value, dontSendNotification);
    }

    MPESettingsDataModel dataModel;
    MPELegacySettingsComponent legacySettings;
    MPENewSettingsComponent newSettings;

    ToggleButton legacyModeEnabledToggle { "Enable Legacy Mode" },
        voiceStealingEnabledToggle { "Enable synth voice stealing" };

    ComboBox numberOfVoices;
    Label numberOfVoicesLabel { {}, "Number of synth voices" };

    UndoManager* undoManager;
};

