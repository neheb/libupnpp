/* Copyright (C) 2006-2024 J.F.Dockes
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

#include "libupnpp/control/ohsender.hxx"
#include "libupnpp/control/cdircontent.hxx"

#include <cstdlib>
#include <cstring>
#include <upnp.h>

#include <functional>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "libupnpp/upnpavutils.hxx"
#include "libupnpp/control/service.hxx"
#include "libupnpp/log.hxx"
#include "libupnpp/soaphelp.hxx"
#include "libupnpp/upnpp_p.hxx"

using namespace std;
using namespace std::placeholders;
using namespace UPnPP;

namespace UPnPClient {

const string OHSender::SType("urn:av-openhome-org:service:Sender:1");

// Check serviceType string (while walking the descriptions. We don't
// include a version in comparisons, as we are satisfied with version1
bool OHSender::isOHSenderService(const string& st)
{
    const string::size_type sz(SType.size()-2);
    return !SType.compare(0, sz, st, 0, sz);
}
bool OHSender::serviceTypeMatch(const std::string& tp)
{
    return isOHSenderService(tp);
}

void OHSender::evtCallback(const std::unordered_map<std::string, std::string>& props)
{
    LOGDEB1("OHSender::evtCallback:getReporter(): " << getReporter() << '\n');
    for (const auto& [propname, propvalue] : props) {
        if (!getReporter()) {
            LOGDEB1("OHSender::evtCallback: " << propname << " -> " << propvalue << '\n');
            continue;
        }

        if (propname == "Audio") {
            bool val = false;
            stringToBool(propvalue, &val);
            getReporter()->changed(propname.c_str(), val ? 1 : 0);
        } else if (propname == "Metadata" || propname == "Attributes" ||
                   propname == "PresentationUrl" || propname == "Status") {
            getReporter()->changed(propname.c_str(), propvalue.c_str());
        } else {
            LOGERR("OHSender event: unknown variable: name [" <<
                   propname << "] value [" << propvalue << '\n');
            getReporter()->changed(propname.c_str(), propvalue.c_str());
        }
    }
}

void OHSender::registerCallback()
{
    Service::registerCallback(bind(&OHSender::evtCallback, this, _1));
}

int OHSender::metadata(string& uri, string& didl)
{
    SoapOutgoing args(getServiceType(), "Metadata");
    SoapIncoming data;
    int ret = runAction(args, data);
    if (ret != UPNP_E_SUCCESS) {
        return ret;
    }
    if (!data.get("Value", &didl)) {
        LOGERR("OHSender::Sender: missing Value in response" << '\n');
        return UPNP_E_BAD_RESPONSE;
    }

    // Try to parse the metadata and extract the Uri.
    UPnPDirContent dir;
    if (!dir.parse(didl)) {
        LOGERR("OHSender::Metadata: didl parse failed: " << didl << '\n');
        return UPNP_E_BAD_RESPONSE;
    }
    if (dir.m_items.size() != 1) {
        LOGERR("OHSender::Metadata: " << dir.m_items.size() << " in response!" << '\n');
        return UPNP_E_BAD_RESPONSE;
    }
    UPnPDirObject* dirent = dir.m_items.data();
    if (dirent->m_resources.size() < 1) {
        LOGERR("OHSender::Metadata: no resources in metadata!" << '\n');
        return UPNP_E_BAD_RESPONSE;
    }
    uri = dirent->m_resources[0].m_uri;
    return 0;
}

} // End namespace UPnPClient
