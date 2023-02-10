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
#include "libupnpp/config.h"

#include "libupnpp/control/mediaserver.hxx"

#include <unordered_map>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "libupnpp/control/cdirectory.hxx"
#include "libupnpp/control/description.hxx"
#include "libupnpp/control/discovery.hxx"
#include "libupnpp/log.hxx"

using namespace std;
using namespace std::placeholders;
using namespace UPnPP;

namespace UPnPClient {

const string
MediaServer::DType("urn:schemas-upnp-org:device:MediaServer:1");

// We don't include a version in comparisons, as we are satisfied with version 1
bool MediaServer::isMSDevice(const string& st)
{
    const string::size_type sz(DType.size()-2);
    return !DType.compare(0, sz, st, 0, sz);
}

static bool MDAccum(std::unordered_map<string, UPnPDeviceDesc>* out,
                    const string& friendlyName,
                    const UPnPDeviceDesc& device,
                    const UPnPServiceDesc& service)
{
    //LOGDEB("MDAccum: friendlyname: " << friendlyName <<
    //    " dev friendlyName " << device.friendlyName << endl);
    if (ContentDirectory::isCDService(service.serviceType) &&
            (friendlyName.empty() ? true : !friendlyName.compare(device.friendlyName))) {
        //LOGDEB("MDAccum setting " << device.UDN << endl);
        (*out)[device.UDN] = device;
    }
    return true;
}

bool MediaServer::getDeviceDescs(vector<UPnPDeviceDesc>& devices, const string& friendlyName)
{
    std::unordered_map<string, UPnPDeviceDesc> mydevs;
    UPnPDeviceDirectory::Visitor visitor = bind(MDAccum, &mydevs, friendlyName, _1, _2);
    UPnPDeviceDirectory::getTheDir()->traverse(visitor);
    for (const auto& dev : mydevs) {
        devices.push_back(dev.second);
    }
    return !devices.empty();
}

MediaServer::MediaServer(const UPnPDeviceDesc& desc)
    : Device(desc)
{
    bool found = false;
    for (const auto& service : desc.services) {
        if (ContentDirectory::isCDService(service.serviceType)) {
            m_cds = std::make_shared<ContentDirectory>(desc, service);
            found = true;
            break;
        }
    }
    if (!found) {
        LOGERR("MediaServer::MediaServer: ContentDirectory service not found in device\n");
    }
}

}
