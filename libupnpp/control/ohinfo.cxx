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
#include "libupnpp/config.h"
#include "libupnpp/control/ohinfo.hxx"

#include <string.h>                     // for strcmp
#include <upnp.h>                  // for UPNP_E_BAD_RESPONSE, etc
#include <ostream>                      // for endl
#include <string>                       // for string
#include <vector>                       // for vector

#include "libupnpp/control/ohradio.hxx"
#include "libupnpp/log.hxx"             // for LOGERR
#include "libupnpp/soaphelp.hxx"        // for SoapIncoming, etc

using namespace std;
using namespace std::placeholders;
using namespace UPnPP;

namespace UPnPClient {
const string OHInfo::SType("urn:av-openhome-org:service:Info:1");

// Check serviceType string (while walking the descriptions. We don't
// include a version in comparisons, as we are satisfied with version1
bool OHInfo::isOHInfoService(const string& st)
{
    const string::size_type sz(SType.size()-2);
    return !SType.compare(0, sz, st, 0, sz);
}
bool OHInfo::serviceTypeMatch(const std::string& tp)
{
    return isOHInfoService(tp);
}

void OHInfo::evtCallback(
    const std::unordered_map<std::string, std::string>& props)
{
    LOGDEB1("OHInfo::evtCallback: getReporter(): " << getReporter() << endl);
    for (std::unordered_map<std::string, std::string>::const_iterator it =
                props.begin(); it != props.end(); it++) {
        if (!getReporter()) {
            LOGDEB1("OHInfo::evtCallback: " << it->first << " -> "
                    << it->second << endl);
            continue;
        }

        if (!it->first.compare("Metatext")) {
            /* Metadata is a didl-lite string */
            UPnPDirObject dirent;
            if (OHRadio::decodeMetadata("OHInfo:evt",
                                        it->second, &dirent) == 0) {
                getReporter()->changed(it->first.c_str(), dirent);
            } else {
                LOGDEB("OHInfo:evtCallback: bad metadata in event\n");
            }
        } else {
            LOGDEB1("OHInfo event: unknown variable: name [" <<
                    it->first << "] value [" << it->second << endl);
            getReporter()->changed(it->first.c_str(), it->second.c_str());
        }
    }
}

void OHInfo::registerCallback()
{
    Service::registerCallback(bind(&OHInfo::evtCallback, this, _1));
}

int OHInfo::metatext(UPnPDirObject *dirent)
{
    SoapOutgoing args(getServiceType(), "Metatext");
    SoapIncoming data;
    int ret = runAction(args, data);
    if (ret != UPNP_E_SUCCESS) {
        LOGERR("OHInfo::metatext: runAction failed\n");
        return ret;
    }
    string didl;
    if (!data.get("Value", &didl)) {
        LOGERR("OHInfo::metatext: missing Value in response" << endl);
        return UPNP_E_BAD_RESPONSE;
    }
    if (didl.empty()) {
        *dirent = UPnPDirObject();
        return UPNP_E_SUCCESS;
    }
    return OHRadio::decodeMetadata("OHInfo::metatext", didl, dirent);
}

int OHInfo::track(std::string *uri, UPnPDirObject *dirent)
{
    SoapOutgoing args(getServiceType(), "Counters");
    SoapIncoming data;
    int ret = runAction(args, data);
    if (ret != UPNP_E_SUCCESS) {
        LOGERR("OHInfo::counters: runAction failed\n");
        return ret;
    }
    if (uri) {
        if (!data.get("Uri", uri)) {
            LOGERR("OHInfo::track: missing Uri in response" << endl);
            return UPNP_E_BAD_RESPONSE;
        }
    }
    if (dirent) {
        string didl;
        if (!data.get("Metadata", &didl)) {
            LOGERR("OHInfo::track: missing Metadata in response" << endl);
            return UPNP_E_BAD_RESPONSE;
        }
        return OHRadio::decodeMetadata("OHInfo::metatext", didl, dirent);
    }
    return UPNP_E_SUCCESS;
}

int OHInfo::counters(int *trackcount, int *detailscount, int *metatextcount)
{
    SoapOutgoing args(getServiceType(), "Counters");
    SoapIncoming data;
    int ret = runAction(args, data);
    if (ret != UPNP_E_SUCCESS) {
        LOGERR("OHInfo::counters: runAction failed\n");
        return ret;
    }
    if (trackcount) {
        const char *nm = "TrackCount";
        if (!data.get(nm, trackcount)) {
            LOGERR("OHInfo::counters: missing " << nm << " in response" << endl);
            return UPNP_E_BAD_RESPONSE;
        }
    }
    if (detailscount) {
        const char *nm = "DetailsCount";
        if (!data.get(nm, detailscount)) {
            LOGERR("OHInfo::counters: missing " << nm << " in response" << endl);
            return UPNP_E_BAD_RESPONSE;
        }
    }
    if (metatextcount) {
        const char *nm = "MetatextCount";
        if (!data.get(nm, metatextcount)) {
            LOGERR("OHInfo::counters: missing " << nm << " in response" << endl);
            return UPNP_E_BAD_RESPONSE;
        }
    }
    return UPNP_E_SUCCESS;
}


int OHInfo::details(int *duration, int *bitrate, int *bitdepth, int *samplerate, bool *lossless,
                    std::string *codecname)
{
    SoapOutgoing args(getServiceType(), "Details");
    SoapIncoming data;
    int ret = runAction(args, data);
    if (ret != UPNP_E_SUCCESS) {
        LOGERR("OHInfo::details: runAction failed\n");
        return ret;
    }
    if (duration) {
        const char *nm = "Duration";
        if (!data.get(nm, duration)) {
            LOGERR("OHInfo::counters: missing " << nm << " in response" << endl);
            return UPNP_E_BAD_RESPONSE;
        }
    }
    if (bitrate) {
        const char *nm = "BitRate";
        if (!data.get(nm, bitrate)) {
            LOGERR("OHInfo::counters: missing " << nm << " in response" << endl);
            return UPNP_E_BAD_RESPONSE;
        }
    }
    if (bitdepth) {
        const char *nm = "BitDepth";
        if (!data.get(nm, bitdepth)) {
            LOGERR("OHInfo::counters: missing " << nm << " in response" << endl);
            return UPNP_E_BAD_RESPONSE;
        }
    }
    if (samplerate) {
        const char *nm = "SampleRate";
        if (!data.get(nm, samplerate)) {
            LOGERR("OHInfo::counters: missing " << nm << " in response" << endl);
            return UPNP_E_BAD_RESPONSE;
        }
    }
    if (lossless) {
        const char *nm = "Lossless";
        if (!data.get(nm, lossless)) {
            LOGERR("OHInfo::counters: missing " << nm << " in response" << endl);
            return UPNP_E_BAD_RESPONSE;
        }
    }
    if (codecname) {
        const char *nm = "Codecname";
        if (!data.get(nm, codecname)) {
            LOGERR("OHInfo::counters: missing " << nm << " in response" << endl);
            return UPNP_E_BAD_RESPONSE;
        }
    }
    return UPNP_E_SUCCESS;
}

} // End namespace UPnPClient
