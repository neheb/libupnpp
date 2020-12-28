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

#include "libupnpp/control/cdirectory.hxx"

#include <sys/types.h>

#include <upnp.h>
#include <upnptools.h>

#include <functional>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#include "libupnpp/control/cdircontent.hxx"
#include "libupnpp/control/description.hxx"
#include "libupnpp/control/discovery.hxx"
#include "libupnpp/log.hxx"
#include "libupnpp/smallut.h"
#include "libupnpp/soaphelp.hxx"
#include "libupnpp/upnpp_p.hxx"

using namespace std;
using namespace std::placeholders;
using namespace UPnPP;

namespace UPnPClient {

// The service type string for Content Directories:
const string ContentDirectory::SType("urn:schemas-upnp-org:service:ContentDirectory:1");

// We don't include a version in comparisons, as we are satisfied with
// version 1
bool ContentDirectory::isCDService(const string& st)
{
    const string::size_type sz(SType.size()-2);
    return !SType.compare(0, sz, st, 0, sz);
}
bool ContentDirectory::serviceTypeMatch(const std::string& tp)
{
    return isCDService(tp);
}

void ContentDirectory::evtCallback(
    const std::unordered_map<string, string>& props)
{
    for (std::unordered_map<std::string, std::string>::const_iterator it =
             props.begin(); it != props.end(); it++) {
        if (!getReporter()) {
            LOGDEB1("ContentDirectory::evtCallback: " << it->first << " -> "
                    << it->second << endl);
            continue;
        }
        if (!it->first.compare("SystemUpdateID")) {
            getReporter()->changed(it->first.c_str(), atoi(it->second.c_str()));

        } else if (!it->first.compare("ContainerUpdateIDs") ||
                   !it->first.compare("TransferIDs")) {
            getReporter()->changed(it->first.c_str(),
                                   it->second.c_str());

        } else {
            LOGERR("ContentDirectory event: unknown variable: name [" <<
                   it->first << "] value [" << it->second << endl);
            getReporter()->changed(it->first.c_str(), it->second.c_str());
        }
    }
}

/*
  manufacturer: Bubblesoft model BubbleUPnP Media Server
  manufacturer: Justin Maggard model Windows Media Connect compatible (MiniDLNA)
  manufacturer: minimserver.com model MinimServer
  manufacturer: PacketVideo model TwonkyMedia Server
  manufacturer: ? model MediaTomb
*/
static const int sreflags = SimpleRegexp::SRE_ICASE|SimpleRegexp::SRE_NOSUB;
static const SimpleRegexp bubble_rx("bubble", sreflags);
static const SimpleRegexp mediatomb_rx("mediatomb", sreflags);
static const SimpleRegexp minidlna_rx("minidlna", sreflags);
static const SimpleRegexp minim_rx("minim", sreflags);
static const SimpleRegexp twonky_rx("twonky", sreflags);

ContentDirectory::ContentDirectory(const UPnPDeviceDesc& device,
                                   const UPnPServiceDesc& service)
    : Service(device, service)
{
    LOGDEB("ContentDirectory::ContentDirectory: manufacturer: [" <<
           getManufacturer() << "] model [" << getModelName() << "]\n");
    serviceInit(device, service);
}

bool ContentDirectory::serviceInit(const UPnPDeviceDesc&,
                                   const UPnPServiceDesc&)
{
    if (bubble_rx(getModelName())) {
        m_serviceKind = CDSKIND_BUBBLE;
        LOGDEB1("ContentDirectory::ContentDirectory: BUBBLE" << endl);
    } else if (mediatomb_rx(getModelName())) {
        // Readdir by 200 entries is good for most, but MediaTomb likes
        // them really big. Actually 1000 is better but I don't dare
        m_rdreqcnt = 500;
        m_serviceKind = CDSKIND_MEDIATOMB;
        LOGDEB1("ContentDirectory::ContentDirectory: MEDIATOMB" << endl);
    } else if (minidlna_rx(getModelName())) {
        m_serviceKind = CDSKIND_MINIDLNA;
        LOGDEB1("ContentDirectory::ContentDirectory: MINIDLNA" << endl);
    } else if (minim_rx(getModelName())) {
        m_serviceKind = CDSKIND_MINIM;
        LOGDEB1("ContentDirectory::ContentDirectory: MINIM" << endl);
    } else if (twonky_rx(getModelName())) {
        m_serviceKind = CDSKIND_TWONKY;
        LOGDEB1("ContentDirectory::ContentDirectory: TWONKY" << endl);
    }
    return true;
}

static bool DSAccum(vector<CDSH>* out,
                    const UPnPDeviceDesc& device,
                    const UPnPServiceDesc& service)
{
    if (ContentDirectory::isCDService(service.serviceType)) {
        out->push_back(CDSH(new ContentDirectory(device, service)));
    }
    return true;
}

bool ContentDirectory::getServices(vector<CDSH>& vds)
{
    //LOGDEB("UPnPDeviceDirectory::getDirServices" << endl);
    UPnPDeviceDirectory::Visitor visitor = bind(DSAccum, &vds, _1, _2);
    UPnPDeviceDirectory::getTheDir()->traverse(visitor);
    return !vds.empty();
}

// Get server by friendly name.
bool ContentDirectory::getServerByName(const string& fname, CDSH& server)
{
    UPnPDeviceDesc ddesc;
    bool found = UPnPDeviceDirectory::getTheDir()->getDevByFName(fname, ddesc);
    if (!found)
        return false;

    found = false;
    for (std::vector<UPnPServiceDesc>::const_iterator it =
                ddesc.services.begin(); it != ddesc.services.end(); it++) {
        if (isCDService(it->serviceType)) {
            server = CDSH(new ContentDirectory(ddesc, *it));
            found = true;
            break;
        }
    }
    return found;
}

void ContentDirectory::registerCallback()
{
    Service::registerCallback(bind(&ContentDirectory::evtCallback, this, _1));
}

int ContentDirectory::readDirSlice(
    const string& objectId, int offset, int count,
    UPnPDirContent& dirbuf, int *didread, int *total)
{
    LOGDEB("CDService::readDirSlice: objId [" << objectId << "] offset " <<
           offset << " count " << count << endl);

    // Create request
    // Some devices require an empty SortCriteria, else bad params
    SoapOutgoing args(getServiceType(), "Browse");
    args("ObjectID", objectId)
    ("BrowseFlag", "BrowseDirectChildren")
    ("Filter", "*")
    ("SortCriteria", "")
    ("StartingIndex", SoapHelp::i2s(offset))
    ("RequestedCount", SoapHelp::i2s(count));

    SoapIncoming data;
    int ret = runAction(args, data);
    if (ret != UPNP_E_SUCCESS) {
        return ret;
    }

    string tbuf;
    if (!data.get("NumberReturned", didread) ||
            !data.get("TotalMatches", total) ||
            !data.get("Result", &tbuf)) {
        LOGERR("CDService::readDir: missing elts in response" << endl);
        return UPNP_E_BAD_RESPONSE;
    }

    if (*didread <= 0) {
        LOGINF("CDService::readDir: got -1 or 0 entries" << endl);
        return UPNP_E_BAD_RESPONSE;
    }

    LOGDEB0("ContentDirectory::readDirSlice: got count " << count <<
            " offset " << offset << " total " << *total << " Data:\n" <<
            tbuf << "\n");

    dirbuf.parse(tbuf);

    return UPNP_E_SUCCESS;
}

int ContentDirectory::readDir(const string& objectId, UPnPDirContent& dirbuf)
{
    LOGDEB("CDService::readDir: url [" << getActionURL() << "] type [" <<
           getServiceType() << "] udn [" << getDeviceId() << "] objId [" <<
           objectId << endl);

    int offset = 0;
    int total = 0;// Updated on first read.

    while (total == 0 || (offset < total)) {
        int count;
        int error = readDirSlice(objectId, offset, m_rdreqcnt, dirbuf,
                                 &count, &total);
        if (error != UPNP_E_SUCCESS)
            return error;

        offset += count;
        if (count != m_rdreqcnt)
            break;
    }

    return UPNP_E_SUCCESS;
}

int ContentDirectory::searchSlice(
    const string& objectId, const string& ss, int offset, int count,
    UPnPDirContent& dirbuf, int *didread, int *total)
{
    LOGDEB("CDService::searchSlice: objId [" << objectId << "] offset " <<
           offset << " count " << count << endl);

    // Create request
    SoapOutgoing args(getServiceType(), "Search");
    args("ContainerID", objectId)
    ("SearchCriteria", ss)
    ("Filter", "*")
    ("SortCriteria", "")
    ("StartingIndex", SoapHelp::i2s(offset))
    ("RequestedCount", SoapHelp::i2s(count));

    SoapIncoming data;
    int ret = runAction(args, data);

    if (ret != UPNP_E_SUCCESS) {
        LOGINF("CDService::search: UpnpSendAction failed: " <<
               UpnpGetErrorMessage(ret) << endl);
        return ret;
    }

    string tbuf;
    if (!data.get("NumberReturned", didread) ||
            !data.get("TotalMatches", total) ||
            !data.get("Result", &tbuf)) {
        LOGERR("CDService::search: missing elts in response" << endl);
        return UPNP_E_BAD_RESPONSE;
    }
    if (*didread <=  0) {
        LOGINF("CDService::search: got -1 or 0 entries" << endl);
        return count < 0 ? UPNP_E_BAD_RESPONSE : UPNP_E_SUCCESS;
    }

    dirbuf.parse(tbuf);

    return UPNP_E_SUCCESS;
}

int ContentDirectory::search(
    const string& objectId, const string& ss, UPnPDirContent& dirbuf)
{
    LOGDEB("CDService::search: url [" << getActionURL() << "] type [" <<
           getServiceType() << "] udn [" << getDeviceId() << "] objid [" <<
           objectId <<  "] search [" << ss << "]" << endl);

    int offset = 0;
    int total = 0;// Updated on first read.

    while (total == 0 || (offset < total)) {
        int count;
        int error = searchSlice(objectId, ss, offset, m_rdreqcnt, dirbuf,
                                &count, &total);
        if (error != UPNP_E_SUCCESS)
            return error;
        if (count != m_rdreqcnt)
            break;
        offset += count;
    }

    return UPNP_E_SUCCESS;
}

int ContentDirectory::getSearchCapabilities(set<string>& result)
{
    LOGDEB("CDService::getSearchCapabilities:" << endl);

    SoapOutgoing args(getServiceType(), "GetSearchCapabilities");
    SoapIncoming data;
    int ret = runAction(args, data);
    if (ret != UPNP_E_SUCCESS) {
        LOGINF("CDService::getSearchCapa: UpnpSendAction failed: " <<
               UpnpGetErrorMessage(ret) << endl);
        return ret;
    }
    string tbuf;
    if (!data.get("SearchCaps", &tbuf)) {
        LOGERR("CDService::getSearchCaps: missing Result in response" << endl);
        cerr << tbuf << endl;
        return UPNP_E_BAD_RESPONSE;
    }

    result.clear();
    if (!tbuf.compare("*")) {
        result.insert(result.end(), "*");
    } else if (!tbuf.empty()) {
        if (!csvToStrings(tbuf, result)) {
            return UPNP_E_BAD_RESPONSE;
        }
    }

    return UPNP_E_SUCCESS;
}

int ContentDirectory::getMetadata(const string& objectId,
                                  UPnPDirContent& dirbuf)
{
    LOGDEB("CDService::getMetadata: url [" << getActionURL() << "] type [" <<
           getServiceType() << "] udn [" << getDeviceId() << "] objId [" <<
           objectId << "]" << endl);

    SoapOutgoing args(getServiceType(), "Browse");
    SoapIncoming data;
    args("ObjectID", objectId)
    ("BrowseFlag", "BrowseMetadata")
    ("Filter", "*")
    ("SortCriteria", "")
    ("StartingIndex", "0")
    ("RequestedCount", "1");
    int ret = runAction(args, data);
    if (ret != UPNP_E_SUCCESS) {
        LOGINF("CDService::getmetadata: UpnpSendAction failed: " <<
               UpnpGetErrorMessage(ret) << endl);
        return ret;
    }
    string tbuf;
    if (!data.get("Result", &tbuf)) {
        LOGERR("CDService::getmetadata: missing Result in response" << endl);
        return UPNP_E_BAD_RESPONSE;
    }

    if (dirbuf.parse(tbuf))
        return UPNP_E_SUCCESS;
    else
        return UPNP_E_BAD_RESPONSE;
}

} // namespace UPnPClient
