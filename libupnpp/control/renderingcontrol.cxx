/* Copyright (C) 2006-2023 J.F.Dockes
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 *   02110-1301 USA
 */
#include "config.h"

#include "libupnpp/control/renderingcontrol.hxx"

#include <cstdlib>
#include <upnp.h>
#include <cmath>

#include <functional>
#include <ostream>
#include <string>
#include <utility>

#include "libupnpp/control/description.hxx"
#include "libupnpp/control/avlastchg.hxx"
#include "libupnpp/control/service.hxx"
#include "libupnpp/log.hxx"
#include "libupnpp/soaphelp.hxx"
#include "libupnpp/upnpp_p.hxx"
#include "libupnpp/smallut.h"

using namespace std;
using namespace std::placeholders;
using namespace UPnPP;

namespace UPnPClient {

const string RenderingControl::SType("urn:schemas-upnp-org:service:RenderingControl:1");

// Check serviceType string (while walking the descriptions. We don't
// include a version in comparisons, as we are satisfied with version1
bool RenderingControl::isRDCService(const string& st)
{
    const string::size_type sz(SType.size()-2);
    return !SType.compare(0, sz, st, 0, sz);
}

bool RenderingControl::serviceTypeMatch(const std::string& tp)
{
    return isRDCService(tp);
}

RenderingControl::RenderingControl(const UPnPDeviceDesc& device, const UPnPServiceDesc& service)
    : Service(device, service)
{
    serviceInit(device, service);
}

bool RenderingControl::serviceInit(const UPnPDeviceDesc& device, const UPnPServiceDesc& service)
{
    UPnPServiceDesc::Parsed sdesc;
    if (service.fetchAndParseDesc(device.URLBase, sdesc)) {
        const auto it = sdesc.stateTable.find("Volume");
        if (it != sdesc.stateTable.end() && it->second.hasValueRange) {
            setVolParams(it->second.minimum, it->second.maximum, it->second.step);
        }
    }
    return true;
}

// Translate device volume to 0-100
int RenderingControl::devVolTo0100(int dev_vol)
{
    int volume;
    if (dev_vol < m_volmin)
        dev_vol = m_volmin;
    if (dev_vol > m_volmax)
        dev_vol = m_volmax;
    if (m_volmin != 0 || m_volmax != 100) {
        double fact = double(m_volmax - m_volmin) / 100.0;
        if (fact <= 0.0) // ??
            fact = 1.0;
        volume = int((dev_vol - m_volmin) / fact);
    } else {
        volume = dev_vol;
    }
    return volume;
}

const static std::string volumevarname{"Volume"};
const static std::string mutevarname{"Mute"};

void RenderingControl::evtCallback(const std::unordered_map<std::string, std::string>& vars)
{
    auto reporter = getReporter();
    LOGINF("RenderingControl::evtCallback: getReporter() " << reporter << "\n");
    if (nullptr == reporter)
        return;

    for (const auto& var : vars) {
        if (var.first.compare("LastChange")) {
            LOGINF("RenderingControl:event: not LastChange? "<< var.first<<","<<var.second<<"\n");
            continue;
        }
        std::unordered_map<std::string, std::string> props;
        if (!decodeAVLastChange(var.second, props)) {
            LOGERR("RenderingControl::evtCallback: bad LastChange value: " << var.second << "\n");
            return;
        }
        for (const auto& prop: props) {
            LOGINF("    " << prop.first << " -> " << prop.second << "\n");
            if (beginswith(prop.first, volumevarname)) {
                int vol = devVolTo0100(atoi(prop.second.c_str()));
                reporter->changed(prop.first.c_str(), vol);
            } else if (beginswith(prop.first, mutevarname)) {
                bool mute;
                if (stringToBool(prop.second, &mute))
                    reporter->changed(prop.first.c_str(), mute);
            }
        }
    }
}

void RenderingControl::registerCallback()
{
    Service::registerCallback(bind(&RenderingControl::evtCallback, this, _1));
}

void RenderingControl::setVolParams(int min, int max, int step)
{
    LOGDEB0("RenderingControl::setVolParams: min " << min << " max " << max <<
            " step " << step << "\n");
    m_volmin = min >= 0 ? min : 0;
    m_volmax = max > 0 ? max : 100;
    m_volstep = step > 0 ? step : 1;
}

// Translate 0-100 scale to device scale, then set device volume
int RenderingControl::setVolume(int ivol, const string& channel)
{
    // Input is always 0-100. Translate to actual range
    if (ivol < 0)
        ivol = 0;
    if (ivol > 100)
        ivol = 100;
    // Volumes 0-100
    int desiredVolume = ivol;
    int currentVolume = getVolume("Master");
    if (desiredVolume == currentVolume) {
        return UPNP_E_SUCCESS;
    }

    bool goingUp = desiredVolume > currentVolume;
    if (m_volmin != 0 || m_volmax != 100) {
        double fact = double(m_volmax - m_volmin) / 100.0;
        // Round up when going up, down when going down. Else the user will be surprised by the GUI
        // control going back if he does not go a full step
        desiredVolume = m_volmin + (goingUp ? int(ceil(ivol * fact)) : int(floor(ivol * fact)));
    }
    // Insure integer number of steps (are there devices where step != 1?)
    int remainder = (desiredVolume - m_volmin) % m_volstep;
    if (remainder) {
        if (goingUp)
            desiredVolume += m_volstep - remainder;
        else
            desiredVolume -= remainder;
    }

    LOGDEB0("RenderingControl::setVolume: ivol " << ivol <<
            " m_volmin " << m_volmin << " m_volmax " << m_volmax <<
            " m_volstep " << m_volstep << " computed desiredVolume " <<  desiredVolume << "\n");

    SoapOutgoing args(getServiceType(), "SetVolume");
    args("InstanceID", "0")("Channel", channel) ("DesiredVolume", SoapHelp::i2s(desiredVolume));
    SoapIncoming data;
    return runAction(args, data);
}

int RenderingControl::getVolume(const string& channel)
{
    SoapOutgoing args(getServiceType(), "GetVolume");
    args("InstanceID", "0")("Channel", channel);
    SoapIncoming data;
    int ret = runAction(args, data);
    if (ret != UPNP_E_SUCCESS) {
        return ret;
    }
    int dev_volume;
    if (!data.get("CurrentVolume", &dev_volume)) {
        LOGERR("RenderingControl:getVolume: missing CurrentVolume in response" << "\n");
        return UPNP_E_BAD_RESPONSE;
    }
    LOGDEB0("RenderingControl::getVolume: got " << dev_volume << "\n");
    // Output is always 0-100. Translate from device range
    return devVolTo0100(dev_volume);
}

int RenderingControl::setMute(bool mute, const string& channel)
{
    SoapOutgoing args(getServiceType(), "SetMute");
    args("InstanceID", "0")("Channel", channel) ("DesiredMute", SoapHelp::i2s(mute?1:0));
    SoapIncoming data;
    return runAction(args, data);
}

bool RenderingControl::getMute(const string& channel)
{
    SoapOutgoing args(getServiceType(), "GetMute");
    args("InstanceID", "0")("Channel", channel);
    SoapIncoming data;
    int ret = runAction(args, data);
    if (ret != UPNP_E_SUCCESS) {
        return false;
    }
    bool mute;
    if (!data.get("CurrentMute", &mute)) {
        LOGERR("RenderingControl:getMute: missing CurrentMute in response" << "\n");
        return false;
    }
    return mute;
}

} // End namespace UPnPClient
