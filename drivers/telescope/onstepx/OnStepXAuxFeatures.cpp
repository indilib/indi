/*
    OnStepX INDI Driver — Auxiliary feature helper
    Copyright (C) 2026 Ed Lee

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.
*/

#include "OnStepXAuxFeatures.h"
#include "OnStepXComm.h"

#include <defaultdevice.h>
#include <indilogger.h>

#include <cctype>
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <cstring>

#define AUX_TAB_OUTPUTS      "Power"   // with the PowerInterface DC outputs
#define AUX_TAB_DEW          "Dew Heaters"
#define AUX_TAB_IVO          "Intervalometer"

// ---------------------------------------------------------------------------
// discoverAndDefine — probe each active slot and create INDI properties
// ---------------------------------------------------------------------------
void OnStepXAuxFeatures::discoverAndDefine(uint8_t featureMask)
{
    probe(featureMask);
    defineAll();
}

void OnStepXAuxFeatures::probe(uint8_t featureMask)
{
    // Reset every slot first: probeSlot() compares names with the other
    // active slots, which must not be left over from a previous connection.
    for (auto &slot : m_slots)
        slot = Slot{};

    for (int i = 0; i < 8; i++)
    {
        Slot &slot = m_slots[i];
        if (!(featureMask & (1u << i)))
            continue;

        slot.index = i + 1;
        if (!probeSlot(i + 1, slot))
        {
            LOGF_WARN("Could not probe aux slot %d, skipping", i + 1);
            continue;
        }

        if (slot.hidden)
        {
            // Not for client control, and :GXX# would answer a bare '0' on every poll.
            LOGF_DEBUG("Aux slot %d: '%s' is a hidden switch, skipped", i + 1, slot.label);
            slot = Slot{};
            continue;
        }

        slot.active = true;
        LOGF_DEBUG("Aux slot %d: '%s' type=%d", i + 1, slot.label, (int)slot.type);
    }
}

void OnStepXAuxFeatures::defineAll()
{
    for (auto &slot : m_slots)
        if (slot.active)
            defineSlot(slot);
}

std::vector<int> OnStepXAuxFeatures::switchSlots() const
{
    std::vector<int> out;
    for (const auto &slot : m_slots)
        if (slot.active && slot.type == FeatureType::SWITCH)
            out.push_back(slot.index);
    return out;
}

const char *OnStepXAuxFeatures::slotLabel(int slotIndex) const
{
    return (slotIndex >= 1 && slotIndex <= 8) ? m_slots[slotIndex - 1].label : "";
}

bool OnStepXAuxFeatures::writeSwitch(int slotIndex, bool on)
{
    if (slotIndex < 1 || slotIndex > 8 || m_slots[slotIndex - 1].type != FeatureType::SWITCH)
        return false;
    if (!sendWrite(slotIndex, 'V', on ? 1 : 0))
        return false;
    m_slots[slotIndex - 1].value = on ? 1 : 0;
    return true;
}

int OnStepXAuxFeatures::switchValue(int slotIndex) const
{
    return (slotIndex >= 1 && slotIndex <= 8) ? m_slots[slotIndex - 1].value : -1;
}

// ---------------------------------------------------------------------------
// deleteAll — remove all slot properties on disconnect
// ---------------------------------------------------------------------------
void OnStepXAuxFeatures::deleteAll()
{
    for (auto &slot : m_slots)
    {
        if (slot.active)
            deleteSlot(slot);
    }
}

// ---------------------------------------------------------------------------
// handleSwitch
// ---------------------------------------------------------------------------
bool OnStepXAuxFeatures::handleSwitch(const char *name, ISState *states, char *names[], int n)
{
    for (auto &slot : m_slots)
    {
        if (!slot.active)
            continue;

        if (slot.type == FeatureType::SWITCH && !m_externalSwitches && slot.switchSP.isNameMatch(name))
        {
            slot.switchSP.update(states, names, n);
            int val = (slot.switchSP[1].getState() == ISS_ON) ? 1 : 0;
            if (sendWrite(slot.index, 'V', val))
                slot.switchSP.setState(IPS_OK);
            else
                slot.switchSP.setState(IPS_ALERT);
            slot.switchSP.apply();
            return true;
        }

        if (slot.type == FeatureType::DEW_HEATER && slot.dewEnSP.isNameMatch(name))
        {
            slot.dewEnSP.update(states, names, n);
            int val = (slot.dewEnSP[1].getState() == ISS_ON) ? 1 : 0;
            if (sendWrite(slot.index, 'V', val))
                slot.dewEnSP.setState(IPS_OK);
            else
                slot.dewEnSP.setState(IPS_ALERT);
            slot.dewEnSP.apply();
            return true;
        }

        if (slot.type == FeatureType::INTERVALOMETER && slot.ivoEnSP.isNameMatch(name))
        {
            slot.ivoEnSP.update(states, names, n);
            int val = (slot.ivoEnSP[1].getState() == ISS_ON) ? 1 : 0;
            if (sendWrite(slot.index, 'V', val))
                slot.ivoEnSP.setState(IPS_OK);
            else
                slot.ivoEnSP.setState(IPS_ALERT);
            slot.ivoEnSP.apply();
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// handleNumber
// ---------------------------------------------------------------------------
bool OnStepXAuxFeatures::handleNumber(const char *name, double *values, char *names[], int n)
{
    for (auto &slot : m_slots)
    {
        if (!slot.active)
            continue;

        if ((slot.type == FeatureType::ANALOG || slot.type == FeatureType::UNKNOWN) &&
                slot.analogNP.isNameMatch(name))
        {
            slot.analogNP.update(values, names, n);
            int val = static_cast<int>(slot.analogNP[0].getValue());
            if (sendWrite(slot.index, 'V', val))
                slot.analogNP.setState(IPS_OK);
            else
                slot.analogNP.setState(IPS_ALERT);
            slot.analogNP.apply();
            return true;
        }

        if (slot.type == FeatureType::DEW_HEATER && slot.dewNP.isNameMatch(name))
        {
            slot.dewNP.update(values, names, n);
            const bool ok = sendWrite(slot.index, 'Z', slot.dewNP[0].getValue()) &&
                            sendWrite(slot.index, 'S', slot.dewNP[1].getValue());
            slot.dewNP.setState(ok ? IPS_OK : IPS_ALERT);
            slot.dewNP.apply();
            return true;
        }

        if (slot.type == FeatureType::INTERVALOMETER && slot.ivoNP.isNameMatch(name))
        {
            slot.ivoNP.update(values, names, n);
            const bool ok = sendWrite(slot.index, 'E', slot.ivoNP[0].getValue()) &&
                            sendWrite(slot.index, 'D', slot.ivoNP[1].getValue()) &&
                            sendWrite(slot.index, 'C', slot.ivoNP[2].getValue());
            slot.ivoNP.setState(ok ? IPS_OK : IPS_ALERT);
            slot.ivoNP.apply();
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// pollStatus — refresh current values from :GXX[n]#
// ---------------------------------------------------------------------------
void OnStepXAuxFeatures::pollStatus()
{
    char reply[OnStepXComm::REPLY_BUF_SIZE];

    for (auto &slot : m_slots)
    {
        if (!slot.active)
            continue;

        char cmd[OnStepXComm::CMD_MAX_LEN];
        snprintf(cmd, sizeof(cmd), ":GXX%d#", slot.index);
        if (!m_comm->sendText(cmd, reply))
            continue;

        // Drop any power-monitor suffix (";volts,amps,flags").
        if (char *semi = strchr(reply, ';'))
        {
            *semi = '\0';
        }

        switch (slot.type)
        {
            case FeatureType::SWITCH:
            {
                char *end;
                const long val = std::strtol(reply, &end, 10);
                if (end == reply)
                    break;
                slot.value = val != 0 ? 1 : 0;
                if (m_externalSwitches)
                    break;
                slot.switchSP[0].setState(val == 0 ? ISS_ON : ISS_OFF);
                slot.switchSP[1].setState(val != 0 ? ISS_ON : ISS_OFF);
                slot.switchSP.setState(IPS_OK);
                slot.switchSP.apply();
                break;
            }

            case FeatureType::ANALOG:
            case FeatureType::UNKNOWN:
            {
                char *end;
                const long val = std::strtol(reply, &end, 10);
                if (end == reply)
                    break;
                slot.analogNP[0].setValue(static_cast<double>(val));
                slot.analogNP.setState(IPS_OK);
                slot.analogNP.apply();
                break;
            }

            case FeatureType::DEW_HEATER:
            {
                // "enabled,zero,span,deltaT"
                int en = 0;
                double zero = 0, span = 0;
                char dt[16] = "";
                if (std::sscanf(reply, "%d,%lf,%lf,%15[^,]", &en, &zero, &span, dt) < 3)
                    break;
                slot.dewEnSP[0].setState(en ? ISS_OFF : ISS_ON);
                slot.dewEnSP[1].setState(en ? ISS_ON : ISS_OFF);
                slot.dewEnSP.setState(IPS_OK);
                slot.dewEnSP.apply();
                slot.dewNP[0].setValue(zero);
                slot.dewNP[1].setValue(span);
                slot.dewNP.setState(IPS_OK);
                slot.dewNP.apply();
                char *end;
                const double deltaT = std::strtod(dt, &end);
                if (end != dt && std::isfinite(deltaT))
                {
                    slot.dewStatusNP[0].setValue(deltaT);
                    slot.dewStatusNP.setState(IPS_OK);
                }
                else
                    slot.dewStatusNP.setState(IPS_IDLE);   // no temperature sensor
                slot.dewStatusNP.apply();
                break;
            }

            case FeatureType::INTERVALOMETER:
            {
                // "currentCount,exposure,delay,count"
                int current = 0, count = 0;
                double exposure = 0, delay = 0;
                if (std::sscanf(reply, "%d,%lf,%lf,%d", &current, &exposure, &delay, &count) != 4)
                    break;
                slot.ivoNP[0].setValue(exposure);
                slot.ivoNP[1].setValue(delay);
                slot.ivoNP[2].setValue(count);
                slot.ivoNP.setState(IPS_OK);
                slot.ivoNP.apply();
                slot.ivoStatusNP[0].setValue(current);
                slot.ivoStatusNP.setState(IPS_OK);
                slot.ivoStatusNP.apply();
                break;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// saveConfig
// ---------------------------------------------------------------------------
void OnStepXAuxFeatures::saveConfig(FILE *fp)
{
    // Nothing is saved: the firmware keeps each feature's value (switch,
    // analog, dew heater and intervalometer on/off; FEATUREn_VALUE_MEMORY),
    // the dew heater zero/span and the intervalometer settings in NV, and a
    // config load (Ekos sends one on connect) would overwrite changes made
    // from the hand controller or web UI.
    (void)fp;
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

bool OnStepXAuxFeatures::probeSlot(int idx, Slot &slot)
{
    char cmd[OnStepXComm::CMD_MAX_LEN], reply[OnStepXComm::REPLY_BUF_SIZE];
    snprintf(cmd, sizeof(cmd), ":GXY%d#", idx);
    if (!m_comm->sendText(cmd, reply))
        return false;

    // Expected reply format: "name,T" where T is a type digit
    // e.g. "DEW_A,2"
    char *comma = strrchr(reply, ',');
    char typeChar = '1';   // default: SWITCH

    if (comma && comma[1] != '\0')
    {
        typeChar = comma[1];
        *comma = '\0';  // terminate name at comma
    }

    snprintf(slot.label, sizeof(slot.label), "%s", reply);
    makePropBase(reply, idx, slot.propBase, sizeof(slot.propBase));

    // Two features may share a firmware name; keep INDI property names unique.
    for (const auto &other : m_slots)
    {
        if (&other != &slot && other.active && strcmp(other.propBase, slot.propBase) == 0)
        {
            char unique[sizeof(slot.propBase)];
            snprintf(unique, sizeof(unique), "%s_%d", slot.propBase, idx);
            snprintf(slot.propBase, sizeof(slot.propBase), "%s", unique);
            break;
        }
    }
    slot.type = parseType(typeChar);
    slot.hidden = (typeChar == '6');   // HIDDEN_SWITCH (firmware Constants.h)

    return true;
}

void OnStepXAuxFeatures::defineSlot(Slot &slot)
{
    const char *dev = m_dev ? m_dev->getDeviceName() : "";
    char nameBuf[64];

    switch (slot.type)
    {
        case FeatureType::SWITCH:
        {
            if (m_externalSwitches)
                break;   // a PowerInterface DC output of the owner
            snprintf(nameBuf, sizeof(nameBuf), "%s_SW", slot.propBase);
            slot.switchSP[0].fill("OFF", "Off", ISS_ON);
            slot.switchSP[1].fill("ON",  "On",  ISS_OFF);
            slot.switchSP.fill(dev, nameBuf, slot.label,
                               AUX_TAB_OUTPUTS, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
            m_dev->defineProperty(slot.switchSP);
            break;
        }

        case FeatureType::ANALOG:
        {
            snprintf(nameBuf, sizeof(nameBuf), "%s_AN", slot.propBase);
            slot.analogNP[0].fill("VALUE", "Value (0-255)", "%.0f", 0, 255, 1, 0);
            slot.analogNP.fill(dev, nameBuf, slot.label,
                               AUX_TAB_OUTPUTS, IP_RW, 60, IPS_IDLE);
            m_dev->defineProperty(slot.analogNP);
            break;
        }

        case FeatureType::DEW_HEATER:
        {
            // Enable switch
            snprintf(nameBuf, sizeof(nameBuf), "%s_EN", slot.propBase);
            char enableLabel[64];
            snprintf(enableLabel, sizeof(enableLabel), "%s Enable", slot.label);
            slot.dewEnSP[0].fill("DISABLE", "Disable", ISS_ON);
            slot.dewEnSP[1].fill("ENABLE",  "Enable",  ISS_OFF);
            slot.dewEnSP.fill(dev, nameBuf, enableLabel,
                              AUX_TAB_DEW, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
            m_dev->defineProperty(slot.dewEnSP);

            // Control band: full power at "zero" deg C above dew point,
            // off at zero + span (firmware DewHeater).
            snprintf(nameBuf, sizeof(nameBuf), "%s_NP", slot.propBase);
            slot.dewNP[0].fill("ZERO", "Zero Point (C)", "%.1f", -5, 20, 0.5, 0);
            slot.dewNP[1].fill("SPAN", "Span (C)",       "%.1f", -5, 20, 0.5, 5);
            slot.dewNP.fill(dev, nameBuf, slot.label,
                            AUX_TAB_DEW, IP_RW, 60, IPS_IDLE);
            m_dev->defineProperty(slot.dewNP);

            snprintf(nameBuf, sizeof(nameBuf), "%s_DT", slot.propBase);
            char dtLabel[64];
            snprintf(dtLabel, sizeof(dtLabel), "%s Status", slot.label);
            slot.dewStatusNP[0].fill("DELTA_T", "Above Dew Point (C)", "%.1f", -100, 100, 0, 0);
            slot.dewStatusNP.fill(dev, nameBuf, dtLabel,
                                  AUX_TAB_DEW, IP_RO, 60, IPS_IDLE);
            m_dev->defineProperty(slot.dewStatusNP);
            break;
        }

        case FeatureType::INTERVALOMETER:
        {
            // Start/stop switch
            snprintf(nameBuf, sizeof(nameBuf), "%s_EN", slot.propBase);
            char enableLabel[64];
            snprintf(enableLabel, sizeof(enableLabel), "%s Control", slot.label);
            slot.ivoEnSP[0].fill("STOP",  "Stop",  ISS_ON);
            slot.ivoEnSP[1].fill("START", "Start", ISS_OFF);
            slot.ivoEnSP.fill(dev, nameBuf, enableLabel,
                              AUX_TAB_IVO, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
            m_dev->defineProperty(slot.ivoEnSP);

            // Parameters (firmware Intervalometer)
            snprintf(nameBuf, sizeof(nameBuf), "%s_NP", slot.propBase);
            slot.ivoNP[0].fill("EXPOSURE", "Exposure (s)",  "%.3f", 0, 3600, 1, 1);
            slot.ivoNP[1].fill("DELAY",    "Delay (s)",     "%.2f", 1, 3600, 1, 1);
            slot.ivoNP[2].fill("COUNT",    "Count",         "%.0f", 0, 255,  1, 1);
            slot.ivoNP.fill(dev, nameBuf, slot.label,
                            AUX_TAB_IVO, IP_RW, 60, IPS_IDLE);
            m_dev->defineProperty(slot.ivoNP);

            snprintf(nameBuf, sizeof(nameBuf), "%s_PROGRESS", slot.propBase);
            char progLabel[64];
            snprintf(progLabel, sizeof(progLabel), "%s Status", slot.label);
            slot.ivoStatusNP[0].fill("FRAMES", "Frames Taken", "%.0f", 0, 255, 0, 0);
            slot.ivoStatusNP.fill(dev, nameBuf, progLabel,
                                  AUX_TAB_IVO, IP_RO, 60, IPS_IDLE);
            m_dev->defineProperty(slot.ivoStatusNP);
            break;
        }

        default:
        {
            // Unknown type — treat as a plain 0-255 analog output
            snprintf(nameBuf, sizeof(nameBuf), "%s_AN", slot.propBase);
            slot.analogNP[0].fill("VALUE", "Value (0-255)", "%.0f", 0, 255, 1, 0);
            slot.analogNP.fill(dev, nameBuf, slot.label,
                               AUX_TAB_OUTPUTS, IP_RW, 60, IPS_IDLE);
            m_dev->defineProperty(slot.analogNP);
            break;
        }
    }
}

void OnStepXAuxFeatures::deleteSlot(Slot &slot)
{
    if (!m_dev)
        return;
    switch (slot.type)
    {
        case FeatureType::SWITCH:
            if (!m_externalSwitches)
                m_dev->deleteProperty(slot.switchSP);
            break;
        case FeatureType::ANALOG:
        case FeatureType::UNKNOWN:
            m_dev->deleteProperty(slot.analogNP);
            break;
        case FeatureType::DEW_HEATER:
            m_dev->deleteProperty(slot.dewEnSP);
            m_dev->deleteProperty(slot.dewNP);
            m_dev->deleteProperty(slot.dewStatusNP);
            break;
        case FeatureType::INTERVALOMETER:
            m_dev->deleteProperty(slot.ivoEnSP);
            m_dev->deleteProperty(slot.ivoNP);
            m_dev->deleteProperty(slot.ivoStatusNP);
            break;
    }
    slot.active = false;
}

bool OnStepXAuxFeatures::sendWrite(int slotIdx, char field, double value)
{
    char cmd[OnStepXComm::CMD_MAX_LEN];
    snprintf(cmd, sizeof(cmd), ":SXX%d,%c%.3f#", slotIdx, field, value);
    return m_comm->sendBool(cmd);
}

// Produce a sanitized INDI property name base from the firmware label.
// e.g. "Dew A" -> "OSX_DEW_A" (slot index appended as fallback if label empty)
void OnStepXAuxFeatures::makePropBase(const char *label, int slotIdx, char *out, int outLen)
{
    char tmp[64];
    // Copy, replace non-alphanumeric with '_', uppercase
    int j = 0;
    for (int i = 0; label[i] && j < (int)sizeof(tmp) - 1; i++)
    {
        char c = label[i];
        if (std::isalnum((unsigned char)c))
            tmp[j++] = static_cast<char>(std::toupper((unsigned char)c));
        else if (j > 0 && tmp[j - 1] != '_')
            tmp[j++] = '_';
    }
    // Trim trailing underscores
    while (j > 0 && tmp[j - 1] == '_')
        j--;
    tmp[j] = '\0';

    if (j == 0)
        snprintf(out, outLen, "OSX_AUX%d", slotIdx);
    else
        snprintf(out, outLen, "OSX_%s", tmp);
}

OnStepXAuxFeatures::FeatureType OnStepXAuxFeatures::parseType(char t)
{
    switch (t)
    {
        case '1':
        case 'S':
        case 's':
            return FeatureType::SWITCH;
        case '2':
        case 'A':
        case 'a':
            return FeatureType::ANALOG;
        case '3':
        case 'D':
        case 'd':
            return FeatureType::DEW_HEATER;
        case '4':
        case 'I':
        case 'i':
            return FeatureType::INTERVALOMETER;
        default:
            return FeatureType::UNKNOWN;
    }
}
