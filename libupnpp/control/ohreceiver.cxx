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

#include "libupnpp/control/ohreceiver.hxx"

#include <stdlib.h>
#include <string.h>
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
#include "libupnpp/control/ohreceiver.hxx"

using namespace std;
using namespace std::placeholders;
using namespace UPnPP;

namespace UPnPClient {

const string OHReceiver::SType("urn:av-openhome-org:service:Receiver:1");

// Check serviceType string (while walking the descriptions. We don't
// include a version in comparisons, as we are satisfied with version1
bool OHReceiver::isOHRcService(const string& st)
{
    const string::size_type sz(SType.size()-2);
    return !SType.compare(0, sz, st, 0, sz);
}
bool OHReceiver::serviceTypeMatch(const std::string& tp)
{
    return isOHRcService(tp);
}

void OHReceiver::evtCallback(
    const std::unordered_map<std::string, std::string>& props)
{
    LOGDEB1("OHReceiver::evtCallback:getReporter(): " << getReporter() << endl);
    for (const auto& prop : props) {
        if (!getReporter()) {
            LOGDEB1("OHReceiver::evtCallback: " << prop.first << " -> "
                                                << prop.second << endl);
            continue;
        }

        if (!prop.first.compare("TransportState")) {
            OHPlaylist::TPState tp;
            OHPlaylist::stringToTpState(prop.second, &tp);
            getReporter()->changed(prop.first.c_str(), int(tp));
        } else if (!prop.first.compare("Metadata")) {
            getReporter()->changed(prop.first.c_str(),
                                   prop.second.c_str());
        } else if (!prop.first.compare("Uri")) {
            getReporter()->changed(prop.first.c_str(),
                                   prop.second.c_str());
        } else if (!prop.first.compare("ProtocolInfo")) {
            getReporter()->changed(prop.first.c_str(),
                                   prop.second.c_str());
        } else {
            LOGERR("OHReceiver event: unknown variable: name [" << prop.first << "] value [" << prop.second << endl);
            getReporter()->changed(prop.first.c_str(), prop.second.c_str());
        }
    }
}

void OHReceiver::registerCallback()
{
    Service::registerCallback(bind(&OHReceiver::evtCallback, this, _1));
}

int OHReceiver::play()
{
    return runTrivialAction("Play");
}
int OHReceiver::stop()
{
    return runTrivialAction("Stop");
}

int OHReceiver::setSender(const string& uri, const string& didl)
{
    SoapOutgoing args(getServiceType(), "SetSender");
    args("Uri", uri)
    ("Metadata", didl);
    SoapIncoming data;
    return runAction(args, data);
}

int OHReceiver::sender(string& uri, string& meta)
{
    SoapOutgoing args(getServiceType(), "Sender");
    SoapIncoming data;
    int ret = runAction(args, data);
    if (ret != UPNP_E_SUCCESS) {
        return ret;
    }
    if (!data.get("Uri", &uri)) {
        LOGERR("OHReceiver::Sender: missing Uri in response" << endl);
        return UPNP_E_BAD_RESPONSE;
    }
    if (!data.get("Metadata", &meta)) {
        LOGERR("OHReceiver::Sender: missing Metadata in response" << endl);
        return UPNP_E_BAD_RESPONSE;
    }
    return 0;
}

int OHReceiver::transportState(OHPlaylist::TPState *tpp)
{
    string value;
    int ret;

    if ((ret = runSimpleGet("TransportState", "Value", &value))) {
        return ret;
    }
    
    return OHPlaylist::stringToTpState(value, tpp);
}

int OHReceiver::protocolInfo(std::string *proto)
{
    SoapOutgoing args(getServiceType(), "ProtocolInfo");
    SoapIncoming data;
    int ret = runAction(args, data);
    if (ret != UPNP_E_SUCCESS) {
        return ret;
    }
    if (!data.get("Value", proto)) {
        LOGERR("OHReceiver::protocolInfo: missing Value in response" << endl);
        return UPNP_E_BAD_RESPONSE;
    }
    return 0;
}

} // End namespace UPnPClient
