/* Copyright (C) 2006-2016 J.F.Dockes
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

#include "libupnpp/control/ohvolume.hxx"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <upnp.h>

#include <functional>
#include <ostream>
#include <string>

#include "libupnpp/control/service.hxx"
#include "libupnpp/log.hxx"
#include "libupnpp/soaphelp.hxx"
#include "libupnpp/upnpp_p.hxx"

using namespace std;
using namespace std::placeholders;
using namespace UPnPP;

namespace UPnPClient {

const string OHVolume::SType("urn:av-openhome-org:service:Volume:1");

// Check serviceType string (while walking the descriptions. We don't
// include a version in comparisons, as we are satisfied with version1
bool OHVolume::isOHVLService(const string& st)
{
    const string::size_type sz(SType.size() - 2);
    return !SType.compare(0, sz, st, 0, sz);
}
bool OHVolume::serviceTypeMatch(const std::string& tp)
{
    return isOHVLService(tp);
}

void OHVolume::evtCallback(const std::unordered_map<std::string, std::string>& props)
{
    LOGDEB1("OHVolume::evtCallback: getReporter(): " << getReporter() << endl);
    for (const auto& [propname, propvalue] : props) {
        if (!getReporter()) {
            LOGDEB1("OHVolume::evtCallback: " << propname << " -> " << propvalue << endl);
            continue;
        }

        if (propname == "Volume") {
            int vol = devVolTo0100(atoi(propvalue.c_str()));
            getReporter()->changed(propname.c_str(), vol);
        } else if (propname == "VolumeLimit") {
            m_volmax = atoi(propvalue.c_str());
            LOGDEB1("OHVolume: event: VolumeLimit: " << m_volmax << endl);
        } else if (propname == "Mute") {
            bool val = false;
            stringToBool(propvalue, &val);
            LOGDEB1("OHVolume: event: Mute: " << val << endl);
            getReporter()->changed(propname.c_str(), val ? 1 : 0);
        } else {
            LOGDEB1("OHVolume event: untracked variable: name [" <<
                   it->first << "] value [" << it->second << endl);
            getReporter()->changed(propname.c_str(), propvalue.c_str());
        }
    }
}

void OHVolume::registerCallback()
{
    Service::registerCallback(bind(&OHVolume::evtCallback, this, _1));
}

int OHVolume::maybeInitVolmax()
{
    // We only query volumelimit once. We should get updated by events
    // later on
    if (m_volmax < 0) {
        volumeLimit(&m_volmax);
    }
    // If volumeLimit fails, we're lost really. Hope it will get
    // better.
    if (m_volmax > 0)
        return m_volmax;
    else
        return 100;
}

// Translate device volume to 0-100
int OHVolume::devVolTo0100(int dev_vol)
{
    int volmin = 0;
    int volmax = maybeInitVolmax();

    int volume;
    if (dev_vol < 0)
        dev_vol = 0;
    if (dev_vol > volmax)
        dev_vol = volmax;
    if (volmax != 100) {
        double fact = double(volmax - volmin) / 100.0;
        if (fact <= 0.0) // ??
            fact = 1.0;
        volume = int((dev_vol - volmin) / fact);
    } else {
        volume = dev_vol;
    }
    return volume;
}

// Translate 0-100 to device volume
int OHVolume::vol0100ToDev(int ivol)
{
    int volmin = 0;
    int volmax = maybeInitVolmax();
    int volstep = 1;

    int desiredVolume = ivol;
    int currentVolume;
    volume(&currentVolume);

    bool goingUp = desiredVolume > currentVolume;
    if (volmax != 100) {
        double fact = double(volmax - volmin) / 100.0;
        // Round up when going up, down when going down. Else the user
        // will be surprised by the GUI control going back if he does
        // not go a full step
        desiredVolume = volmin + (goingUp ? int(ceil(ivol * fact)) :
                                  int(floor(ivol * fact)));
    }
    // Insure integer number of steps (are there devices where step != 1?)
    int remainder = (desiredVolume - volmin) % volstep;
    if (remainder) {
        if (goingUp)
            desiredVolume += volstep - remainder;
        else
            desiredVolume -= remainder;
    }

    return desiredVolume;
}

int OHVolume::volume(int *value)
{
    int mval;
    int ret = runSimpleGet("Volume", "Value", &mval);
    if (ret == 0) {
        *value = devVolTo0100(mval);
    } else {
        *value = 20;
    }
    LOGDEB1("OHVolume::volume: " << *value << endl);
    return ret;
}

int OHVolume::setVolume(int value)
{
    int mval = vol0100ToDev(value);
    LOGDEB1("OHVolume::setVolume: input " << value << " vol " << mval << endl);
    return runSimpleAction("SetVolume", "Value", mval);
}

int OHVolume::volumeLimit(int *value)
{
    int ret = runSimpleGet("VolumeLimit", "Value", value);
    LOGDEB1("OHVolume::volumeLimit(): " << *value << endl);
    return ret;
}

int OHVolume::mute(bool *value)
{
    return runSimpleGet("Mute", "Value", value);
}

int OHVolume::setMute(bool value)
{
    return runSimpleAction("SetMute", "Value", value);
}

int OHVolume::characteristics(OHVCharacteristics* c)
{
    SoapOutgoing args(getServiceType(), "Characteristics");
    SoapIncoming data;
    int ret = runAction(args, data);
    if (ret != UPNP_E_SUCCESS) {
        return ret;
    }
    data.get("VolumeMax", &c->volumeMax);
    data.get("VolumeUnity", &c->volumeUnity);
    data.get("VolumeSteps", &c->volumeSteps);
    data.get("VolumeMilliDbPerStep", &c->volumeMilliDbPerStep);
    data.get("BalanceMax", &c->balanceMax);
    data.get("FadeMax", &c->fadeMax);
    LOGDEB1("OHVolume::characteristics: max " << c->volumeMax << " unity " <<
            c->volumeUnity << " steps " << c->volumeSteps << " mdbps " <<
            c->volumeMilliDbPerStep << " balmx " << c->balanceMax <<
            " fademx " << c->fadeMax << endl);
    return UPNP_E_SUCCESS;
}
  
} // End namespace UPnPClient
