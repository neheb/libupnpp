/* Copyright (C) 2006-2019 J.F.Dockes
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
#include "libupnpp/control/conman.hxx"

#include <upnp/upnp.h>

#include "libupnpp/upnpavutils.hxx"

using namespace std;
using namespace std::placeholders;
using namespace UPnPP;

namespace UPnPClient {

static const string SType{"urn:schemas-upnp-org:service:ConnectionManager:1"};

bool ConnectionManager::isConManService(const string& st)
{
    const string::size_type sz(SType.size()-2);
    return !SType.compare(0, sz, st, 0, sz);
}

bool ConnectionManager::serviceTypeMatch(const std::string& tp)
{
    return isConManService(tp);
}

int ConnectionManager::getProtocolInfo(
    std::vector<UPnPP::ProtocolinfoEntry>& sourceEntries,
    std::vector<UPnPP::ProtocolinfoEntry>& sinkEntries)
{
    sourceEntries.clear();
    sinkEntries.clear();
    std::map<std::string, std::string> data;
    int ret = runAction("GetProtocolInfo", std::vector<std::string>(), data);
    if (ret != UPNP_E_SUCCESS) {
        return ret;
    }
    auto it = data.find("Source");
    if (it == data.end()) {
        LOGERR("ConnectionManager::getProtocolInfo: no Source data\n");
        return UPNP_E_BAD_RESPONSE;
    }
    if (!parseProtocolInfo(it->second, sourceEntries)) {
        LOGERR("ConnectionManager::getProtocolInfo: Source data parse failed\n");
        return UPNP_E_BAD_RESPONSE;
    }
    it = data.find("Sink");
    if (it == data.end()) {
        LOGERR("ConnectionManager::getProtocolInfo: no Sink data\n");
        return UPNP_E_BAD_RESPONSE;
    }
    if (!parseProtocolInfo(it->second, sinkEntries)) {
        LOGERR("ConnectionManager::getProtocolInfo: Sink data parse failed\n");
        return UPNP_E_BAD_RESPONSE;
    }
    return UPNP_E_SUCCESS;
}


} // namespace
