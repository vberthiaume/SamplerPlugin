#include "MpeSettingsComponent.h"

MPELegacySettingsComponent::MPELegacySettingsComponent (const MPESettingsDataModel& model, UndoManager& um)
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

//==============================================================================

MPENewSettingsComponent::MPENewSettingsComponent (const MPESettingsDataModel& model, UndoManager& um)
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

void MPENewSettingsComponent::resized ()
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

//==============================================================================

MPESettingsComponent::MPESettingsComponent (const MPESettingsDataModel& model, UndoManager& um)
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

void MPESettingsComponent::resized ()
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
