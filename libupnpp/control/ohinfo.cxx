/* Copyright (C) 2014 J.F.Dockes
 *       This program is free software; you can redistribute it and/or modify
 *       it under the terms of the GNU General Public License as published by
 *       the Free Software Foundation; either version 2 of the License, or
 *       (at your option) any later version.
 *
 *       This program is distributed in the hope that it will be useful,
 *       but WITHOUT ANY WARRANTY; without even the implied warranty of
 *       MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *       GNU General Public License for more details.
 *
 *       You should have received a copy of the GNU General Public License
 *       along with this program; if not, write to the
 *       Free Software Foundation, Inc.,
 *       59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "libupnpp/config.h"

#include <string.h>                     // for strcmp
#include <upnp/upnp.h>                  // for UPNP_E_BAD_RESPONSE, etc
#include <ostream>                      // for endl
#include <string>                       // for string
#include <vector>                       // for vector

#include "libupnpp/control/ohinfo.hxx"
#include "libupnpp/control/ohradio.hxx"
#include "libupnpp/log.hxx"             // for LOGERR
#include "libupnpp/soaphelp.hxx"        // for SoapIncoming, etc
#include "libupnpp/upnpp_p.hxx"         // for stringToBool, trimstring

using namespace std;
using namespace STD_PLACEHOLDERS;
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

void OHInfo::evtCallback(
    const STD_UNORDERED_MAP<std::string, std::string>& props)
{
    LOGDEB1("OHInfo::evtCallback: getReporter(): " << getReporter() << endl);
    for (STD_UNORDERED_MAP<std::string, std::string>::const_iterator it =
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
        LOGERR("OHInfo::Read: missing Value in response" << endl);
        return UPNP_E_BAD_RESPONSE;
    }
    return OHRadio::decodeMetadata("OHInfo::metatext", didl, dirent);
}

} // End namespace UPnPClient
