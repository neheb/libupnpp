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
#include "libupnpp/control/ohsender.hxx"
#include "libupnpp/control/cdircontent.hxx"

#include <stdlib.h>                     // for atoi
#include <string.h>                     // for strcmp
#include <upnp/upnp.h>                  // for UPNP_E_BAD_RESPONSE, etc

#include <functional>                   // for _Bind, bind, _1
#include <ostream>                      // for endl, basic_ostream, etc
#include <string>                       // for string, basic_string, etc
#include <utility>                      // for pair
#include <vector>                       // for vector

#include "libupnpp/upnpavutils.hxx"
#include "libupnpp/control/service.hxx"  // for VarEventReporter, Service
#include "libupnpp/log.hxx"             // for LOGERR, LOGDEB1, LOGINF
#include "libupnpp/soaphelp.hxx"        // for SoapIncoming, etc
#include "libupnpp/upnpp_p.hxx"         // for stringToBool

using namespace std;
using namespace STD_PLACEHOLDERS;
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

void OHSender::evtCallback(
    const STD_UNORDERED_MAP<std::string, std::string>& props)
{
    LOGDEB1("OHSender::evtCallback:getReporter(): " << getReporter() << endl);
    for (STD_UNORDERED_MAP<std::string, std::string>::const_iterator it = 
             props.begin(); it != props.end(); it++) {
        if (!getReporter()) {
            LOGDEB1("OHSender::evtCallback: " << it->first << " -> " 
                    << it->second << endl);
            continue;
        }

        if (!it->first.compare("Audio")) {
            bool val = false;
            stringToBool(it->second, &val);
            getReporter()->changed(it->first.c_str(), val ? 1 : 0);
        } else if (!it->first.compare("Metadata") ||
                   !it->first.compare("Attributes") ||
                   !it->first.compare("PresentationUrl") ||
                   !it->first.compare("Status")) {
            getReporter()->changed(it->first.c_str(), it->second.c_str());
        } else {
            LOGERR("OHSender event: unknown variable: name [" <<
                   it->first << "] value [" << it->second << endl);
            getReporter()->changed(it->first.c_str(), it->second.c_str());
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
        LOGERR("OHSender::Sender: missing Value in response" << endl);
        return UPNP_E_BAD_RESPONSE;
    }

    // Try to parse the metadata and extract the Uri.
    UPnPDirContent dir;
    if (!dir.parse(didl)) {
        LOGERR("OHSender::Metadata: didl parse failed: " << didl << endl);
        return UPNP_E_BAD_RESPONSE;
    }
    if (dir.m_items.size() != 1) {
        LOGERR("OHSender::Metadata: " << dir.m_items.size() <<
               " in response!" << endl);
        return UPNP_E_BAD_RESPONSE;
    }
    UPnPDirObject *dirent = &dir.m_items[0];
    if (dirent->m_resources.size() < 1) {
        LOGERR("OHSender::Metadata: no resources in metadata!" << endl);
        return UPNP_E_BAD_RESPONSE;
    }
    uri = dirent->m_resources[0].m_uri;
    return 0;
}

} // End namespace UPnPClient
