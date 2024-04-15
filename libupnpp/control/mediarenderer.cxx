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

#include "libupnpp/control/mediarenderer.hxx"

#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "libupnpp/control/description.hxx"
#include "libupnpp/control/discovery.hxx"
#include "libupnpp/control/renderingcontrol.hxx"
#include "libupnpp/log.hxx"

using namespace std;
using namespace std::placeholders;
using namespace UPnPP;

namespace UPnPClient {

const string
MediaRenderer::DType("urn:schemas-upnp-org:device:MediaRenderer:1");

class MediaRenderer::Internal {
public:
    std::weak_ptr<RenderingControl> rdc;
    std::weak_ptr<AVTransport> avt;
    std::weak_ptr<ConnectionManager> cnm;
    std::weak_ptr<OHProduct> ohpr;
    std::weak_ptr<OHPlaylist> ohpl;
    std::weak_ptr<OHTime> ohtm;
    std::weak_ptr<OHVolume> ohvl;
    std::weak_ptr<OHReceiver> ohrc;
    std::weak_ptr<OHRadio> ohrd;
    std::weak_ptr<OHInfo> ohif;
    std::weak_ptr<OHSender> ohsn;
};

// We don't include a version in comparisons, as we are satisfied with
// version 1
bool MediaRenderer::isMRDevice(const string& st)
{
    const string::size_type sz(DType.size()-2);
    return !DType.compare(0, sz, st, 0, sz);
}

// Look at all service descriptions and store parent devices for
// either UPnP RenderingControl or OpenHome Product. Some entries will
// be set multiple times, which does not matter
static bool MDAccum(std::unordered_map<string, UPnPDeviceDesc>* out,
                    const string& friendlyName,
                    const UPnPDeviceDesc& device,
                    const UPnPServiceDesc& service)
{
    //LOGDEB("MDAccum: friendlyname: " << friendlyName <<
    //    " dev friendlyName " << device.friendlyName << endl);
    if (
        (RenderingControl::isRDCService(service.serviceType) ||
         OHProduct::isOHPrService(service.serviceType))
        &&
        (friendlyName.empty() || friendlyName == device.friendlyName)) {
        //LOGDEB("MDAccum setting " << device.UDN << endl);
        (*out)[device.UDN] = device;
    }
    return true;
}

bool MediaRenderer::getDeviceDescs(vector<UPnPDeviceDesc>& devices,
                                   const string& friendlyName)
{
    std::unordered_map<string, UPnPDeviceDesc> mydevs;

    UPnPDeviceDirectory::Visitor visitor = bind(
        MDAccum, &mydevs, friendlyName, _1, _2);
    UPnPDeviceDirectory::getTheDir()->traverse(visitor);
    for (const auto& dev : mydevs) {
        devices.push_back(dev.second);
    }
    return !devices.empty();
}

MediaRenderer::MediaRenderer(const UPnPDeviceDesc& desc)
    : Device(desc)
{
    if ((m = new Internal()) == 0) {
        LOGERR("MediaRenderer::MediaRenderer: out of memory" << '\n');
        return;
    }
}

MediaRenderer::~MediaRenderer()
{
    delete m;
}

bool MediaRenderer::hasOpenHome()
{
    return static_cast<bool>(ohpr());
}


#define RESUBS_ONE(S) \
    do {                                            \
        auto ptr = (S).lock();                      \
        if (ptr) {                                  \
            ok = ptr->reSubscribe();                \
            if (!ok) {                              \
                return false;                       \
            }                                       \
        }                                           \
    } while (false);

bool MediaRenderer::reSubscribeAll()
{
    bool ok;
    RESUBS_ONE(m->rdc);
    RESUBS_ONE(m->avt);
    RESUBS_ONE(m->cnm);
    RESUBS_ONE(m->ohpr);
    RESUBS_ONE(m->ohpl);
    RESUBS_ONE(m->ohtm);
    RESUBS_ONE(m->ohvl);
    RESUBS_ONE(m->ohrc);
    RESUBS_ONE(m->ohrd);
    RESUBS_ONE(m->ohif);
    RESUBS_ONE(m->ohsn);
    return true;
}

RDCH MediaRenderer::rdc()
{
    if (desc() == 0)
        return RDCH();

    RDCH rdcl = m->rdc.lock();
    if (rdcl)
        return rdcl;
    for (const auto& service : desc()->services) {
        if (RenderingControl::isRDCService(service.serviceType)) {
            rdcl = std::make_shared<RenderingControl>(*desc(), service);
            break;
        }
    }
    if (!rdcl)
        LOGDEB("MediaRenderer: RenderingControl service not found" << '\n');
    m->rdc = rdcl;
    return rdcl;
}

AVTH MediaRenderer::avt()
{
    AVTH avtl = m->avt.lock();
    if (avtl)
        return avtl;
    for (const auto& service : desc()->services) {
        if (AVTransport::isAVTService(service.serviceType)) {
            avtl = std::make_shared<AVTransport>(*desc(), service);
            break;
        }
    }
    if (!avtl)
        LOGDEB("MediaRenderer: AVTransport service not found" << '\n');
    m->avt = avtl;
    return avtl;
}

CNMH MediaRenderer::conman()
{
    CNMH cnml = m->cnm.lock();
    if (cnml)
        return cnml;
    for (const auto& servdesc : desc()->services) {
        if (ConnectionManager::isConManService(servdesc.serviceType)) {
            cnml = std::make_shared<ConnectionManager>(servdesc.serviceType);
            cnml->initFromDescription(*desc());
            break;
        }
    }
    if (!cnml)
        LOGDEB("MediaRenderer: ConnectionManager service not found" << '\n');
    m->cnm = cnml;
    return cnml;
}

OHPRH MediaRenderer::ohpr()
{
    OHPRH ohprl = m->ohpr.lock();
    if (ohprl)
        return ohprl;
    for (const auto& service : desc()->services) {
        if (OHProduct::isOHPrService(service.serviceType)) {
            ohprl = std::make_shared<OHProduct>(*desc(), service);
            break;
        }
    }
    if (!ohprl)
        LOGDEB("MediaRenderer: OHProduct service not found" << '\n');
    m->ohpr = ohprl;
    return ohprl;
}

OHPLH MediaRenderer::ohpl()
{
    OHPLH ohpll = m->ohpl.lock();
    if (ohpll)
        return ohpll;
    for (const auto& service : desc()->services) {
        if (OHPlaylist::isOHPlService(service.serviceType)) {
            ohpll = std::make_shared<OHPlaylist>(*desc(), service);
            break;
        }
    }
    if (!ohpll)
        LOGDEB("MediaRenderer: OHPlaylist service not found" << '\n');
    m->ohpl = ohpll;
    return ohpll;
}

OHRCH MediaRenderer::ohrc()
{
    OHRCH ohrcl = m->ohrc.lock();
    if (ohrcl)
        return ohrcl;
    for (const auto& service : desc()->services) {
        if (OHReceiver::isOHRcService(service.serviceType)) {
            ohrcl = std::make_shared<OHReceiver>(*desc(), service);
            break;
        }
    }
    if (!ohrcl)
        LOGDEB("MediaRenderer: OHReceiver service not found" << '\n');
    m->ohrc = ohrcl;
    return ohrcl;
}

OHRDH MediaRenderer::ohrd()
{
    OHRDH handle = m->ohrd.lock();
    if (handle)
        return handle;
    for (const auto& service : desc()->services) {
        if (OHRadio::isOHRdService(service.serviceType)) {
            handle = std::make_shared<OHRadio>(*desc(), service);
            break;
        }
    }
    if (!handle)
        LOGDEB("MediaRenderer: OHRadio service not found" << '\n');
    m->ohrd = handle;
    return handle;
}

OHIFH MediaRenderer::ohif()
{
    OHIFH handle = m->ohif.lock();
    if (handle)
        return handle;
    for (const auto& service : desc()->services) {
        if (OHInfo::isOHInfoService(service.serviceType)) {
            handle = std::make_shared<OHInfo>(*desc(), service);
            break;
        }
    }
    if (!handle)
        LOGDEB("MediaRenderer: OHInfo service not found" << '\n');
    m->ohif = handle;
    return handle;
}

OHSNH MediaRenderer::ohsn()
{
    OHSNH handle = m->ohsn.lock();
    if (handle)
        return handle;
    for (const auto& service : desc()->services) {
        if (OHSender::isOHSenderService(service.serviceType)) {
            handle = std::make_shared<OHSender>(*desc(), service);
            break;
        }
    }
    if (!handle)
        LOGDEB("MediaRenderer: OHSender service not found" << '\n');
    m->ohsn = handle;
    return handle;
}

OHTMH MediaRenderer::ohtm()
{
    OHTMH ohtml = m->ohtm.lock();
    if (ohtml)
        return ohtml;
    for (const auto& service : desc()->services) {
        if (OHTime::isOHTMService(service.serviceType)) {
            ohtml = std::make_shared<OHTime>(*desc(), service);
            break;
        }
    }
    if (!ohtml)
        LOGDEB("MediaRenderer: OHTime service not found" << '\n');
    m->ohtm = ohtml;
    return ohtml;
}

OHVLH MediaRenderer::ohvl()
{
    OHVLH ohvll = m->ohvl.lock();
    if (ohvll)
        return ohvll;
    for (const auto& service : desc()->services) {
        if (OHVolume::isOHVLService(service.serviceType)) {
            ohvll = std::make_shared<OHVolume>(*desc(), service);
            break;
        }
    }
    if (!ohvll)
        LOGDEB("MediaRenderer: OHVolume service not found" << '\n');
    m->ohvl = ohvll;
    return ohvll;
}

}
