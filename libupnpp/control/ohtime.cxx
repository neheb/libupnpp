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

#include "libupnpp/control/ohtime.hxx"

#include <cstdlib>
#include <cstring>
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

const string OHTime::SType("urn:av-openhome-org:service:Time:1");

// Check serviceType string (while walking the descriptions. We don't
// include a version in comparisons, as we are satisfied with version1
bool OHTime::isOHTMService(const string& st)
{
    const string::size_type sz(SType.size()-2);
    return !SType.compare(0, sz, st, 0, sz);
}
bool OHTime::serviceTypeMatch(const std::string& tp)
{
    return isOHTMService(tp);
}

void OHTime::evtCallback(
    const std::unordered_map<std::string, std::string>& props)
{
    VarEventReporter *reporter = getReporter();
    LOGDEB1("OHTime::evtCallback: reporter: " << reporter << endl);
    for (const auto& ent : props) {
        if (!reporter) {
            LOGDEB1("OHTime::evtCallback: " << ent.first << " -> "
                    << ent.second << endl);
            continue;
        }

        if (ent.first == "TrackCount" ||
                ent.first == "Duration" ||
                ent.first == "Seconds") {

            reporter->changed(ent.first.c_str(), atoi(ent.second.c_str()));

        } else {
            LOGERR("OHTime event: unknown variable: name [" << ent.first << "] value [" << ent.second << '\n');
            reporter->changed(ent.first.c_str(), ent.second.c_str());
        }
    }
}

void OHTime::registerCallback()
{
    Service::registerCallback(bind(&OHTime::evtCallback, this, _1));
}

int OHTime::time(Time& out)
{
    SoapOutgoing args(getServiceType(), "Time");
    SoapIncoming data;
    int ret = runAction(args, data);
    if (ret != UPNP_E_SUCCESS) {
        return ret;
    }
    if (!data.get("TrackCount", &out.trackCount)) {
        LOGERR("OHPlaylist::insert: missing 'TrackCount' in response" << '\n');
        return UPNP_E_BAD_RESPONSE;
    }
    if (!data.get("Duration", &out.duration)) {
        LOGERR("OHPlaylist::insert: missing 'Duration' in response" << '\n');
        return UPNP_E_BAD_RESPONSE;
    }
    if (!data.get("Seconds", &out.seconds)) {
        LOGERR("OHPlaylist::insert: missing 'Seconds' in response" << '\n');
        return UPNP_E_BAD_RESPONSE;
    }
    return UPNP_E_SUCCESS;
}

} // End namespace UPnPClient
