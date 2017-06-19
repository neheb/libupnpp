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
#ifndef _OHVOLUME_HXX_INCLUDED_
#define _OHVOLUME_HXX_INCLUDED_

#include "libupnpp/config.h"

#include <unordered_map>
#include <memory>                       // for shared_ptr
#include <string>                       // for string
#include <vector>                       // for vector

#include "service.hxx"                  // for Service


namespace UPnPClient {

class OHVolume;
class UPnPDeviceDesc;
class UPnPServiceDesc;

typedef std::shared_ptr<OHVolume> OHVLH;

struct OHVCharacteristics {
    /// VolumeMax defines the absolute maximum Volume setting.
    int volumeMax;;
    /// VolumeUnity defines the value of Volume that will result in
    /// unity system gain (i.e. output amplitude = input amplitude).
    int volumeUnity;
    /// VolumeSteps defines the number of step increments required to
    /// increase the Volume from zero to VolumeMax.
    int volumeSteps;
    /// VolumeMilliDbPerStep defines the size of each volume step in
    /// binary milli decibels (mibi dB). [1024mibi dB = 1dB]
    int volumeMilliDbPerStep;
    /// BalanceMax defines the maximum Balance setting. The minimum
    /// Balance setting is (-BalanceMax).
    int balanceMax;
    /// FadeMax defines the maximum Fade setting. The minimum Fade
    /// setting is (-FadeMax).
    int fadeMax;
};

/**
 * OHVolume Service client class.
 *
 */
class OHVolume : public Service {
public:

    OHVolume(const UPnPDeviceDesc& device, const UPnPServiceDesc& service)
        : Service(device, service) {
    }
    virtual ~OHVolume() {}

    OHVolume() {}

    /** Test service type from discovery message */
    static bool isOHVLService(const std::string& st);
    virtual bool serviceTypeMatch(const std::string& tp);

    int characteristics(OHVCharacteristics* c);
    int volume(int *value);
    int setVolume(int value);
    int volumeLimit(int *value);
    int mute(bool *value);
    int setMute(bool value);

protected:
    /* My service type string */
    static const std::string SType;

private:
    void evtCallback(const std::unordered_map<std::string, std::string>&);
    void registerCallback();
    int devVolTo0100(int);
    int vol0100ToDev(int vol);
    int maybeInitVolmax();

    int m_volmax{-1};
};

} // namespace UPnPClient

#endif /* _OHVOLUME_HXX_INCLUDED_ */
