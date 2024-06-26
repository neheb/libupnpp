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

#include <cstdlib>
#include <ctime>
#include <cstdio>

#include <upnp.h>
#include <netif.h>

#include <unordered_set>
#include <map>
#include <utility>
#include <vector>
#include <chrono>
#include <thread>

#include "libupnpp/log.hxx"
#include "libupnpp/upnpp_p.hxx"
#include "libupnpp/upnpputils.hxx"
#include "libupnpp/workqueue.h"
#include "libupnpp/control/httpdownload.hxx"
#include "libupnpp/control/description.hxx"
#include "libupnpp/control/discovery.hxx"

using namespace std;
using namespace std::placeholders;
using namespace UPnPP;

#define CBCONST const

#ifndef DISCO_HTTP_TIMEOUT
#define DISCO_HTTP_TIMEOUT 5
#endif

namespace UPnPClient {

// The singleton instance pointer
static UPnPDeviceDirectory *theDevDir;
// Are we working?
static bool o_ok{false};
// If not, why?
static string o_reason;
// Search window (seconds)
#ifndef UPNP_MIN_SEARCH_TIME
// Old versions of the lib did not export this.
#define UPNP_MIN_SEARCH_TIME 2
#define UPNP_MAX_SEARCH_TIME 80
#endif
static time_t o_searchTimeout{UPNP_MIN_SEARCH_TIME};
// Last time we broadcasted a search request
static std::chrono::steady_clock::time_point o_lastSearch;
// Directory initialized at least once ?
static bool o_initialSearchDone{false};

// Start UPnP search and record start of window
static bool search();
// This is called by the thread which processes the device events
// when a new device appears. It wakes up any thread waiting for a
// device.
static bool deviceFound(const UPnPDeviceDesc&, const UPnPServiceDesc&);
static void expireDevices();

static string cluDiscoveryToStr(const UpnpDiscovery *disco)
{
    stringstream ss;
    ss << "ErrCode: " << UpnpDiscovery_get_ErrCode(disco) << '\n';
    ss << "Expires: " << UpnpDiscovery_get_Expires(disco) << '\n';
    ss << "DeviceId: " << UpnpDiscovery_get_DeviceID_cstr(disco) << '\n';
    ss << "DeviceType: " << UpnpDiscovery_get_DeviceType_cstr(disco) << '\n';
    ss << "ServiceType: " << UpnpDiscovery_get_ServiceType_cstr(disco) << '\n';
    ss << "ServiceVer: " << UpnpDiscovery_get_ServiceVer_cstr(disco) << '\n';
    ss << "Location: " << UpnpDiscovery_get_Location_cstr(disco) << '\n';
    ss << "Os: " << UpnpDiscovery_get_Os_cstr(disco) << '\n';
    ss << "Date: " << UpnpDiscovery_get_Date_cstr(disco) << '\n';
    ss << "Ext: " << UpnpDiscovery_get_Ext_cstr(disco) << '\n';

    /** The host address of the device responding to the search. */
    // struct sockaddr_storage DestAddr;
    return ss.str();
}

// Each appropriate discovery event (executing in a libupnp thread
// context) queues the following task object for processing by the
// discovery thread.
class DiscoveredTask {
public:
    DiscoveredTask(bool _alive, const UpnpDiscovery *disco)
        : alive(_alive), url(UpnpDiscovery_get_Location_cstr(disco)),
          deviceId(UpnpDiscovery_get_DeviceID_cstr(disco)),
          expires(UpnpDiscovery_get_Expires(disco))
        {}

    bool alive;
    string url;
    string description;
    string deviceId;
    int expires; // Seconds valid
};

// The workqueue on which callbacks from libupnp (cluCallBack()) queue
// discovered object descriptors for processing by our dedicated
// thread.
static WorkQueue<DiscoveredTask*> discoveredQueue("DiscoveredQueue");

// Set of currently downloading URIs (for avoiding multiple downloads)
static std::unordered_set<string> o_downloading;
static std::mutex o_downloading_mutex;

// This gets called in a libupnp thread context for all asynchronous
// events which we asked for.
// Example: ContentDirectories appearing and disappearing from the network
// We queue a task for our worker thread(s)
// We can get called by several threads.
static int cluCallBack(Upnp_EventType et, CBCONST void* evp, void*)
{
    switch (et) {
    case UPNP_DISCOVERY_SEARCH_RESULT:
    case UPNP_DISCOVERY_ADVERTISEMENT_ALIVE:
    {
        UpnpDiscovery *disco = (UpnpDiscovery *)evp;

        // Devices send multiple messages for themselves, their subdevices and
        // services. AFAIK they all point to the same description.xml document,
        // which has all the interesting data. So let's try to only process
        // one message per device: the one which probably correspond to the
        // upnp "root device" message and has empty service and device types:
        if (UpnpDiscovery_get_DeviceType_cstr(disco)[0] ||
            UpnpDiscovery_get_ServiceType_cstr(disco)[0]) {
            LOGDEB1("discovery:cllb:SearchRes/Alive: ignoring message with "
                    "device/service type\n");
            return UPNP_E_SUCCESS;
        }

        // Get rid of unused warnings (the func is only used conditionally)
        (void)&cluDiscoveryToStr;
        LOGDEB1("discovery:cllb:SearchRes/Alive: " << cluDiscoveryToStr(disco) << '\n');

        // Device signals its existence and well-being. Perform the
        // UPnP "description" phase by downloading and decoding the
        // description document.

        DiscoveredTask *tp = new DiscoveredTask(1, disco);

        {
            // Note that this does not prevent multiple successive
            // downloads of a normal url, just multiple
            // simultaneous downloads of a slow one, to avoid
            // tying up threads.
            std::unique_lock<std::mutex> lock(o_downloading_mutex);
            pair<std::unordered_set<string>::iterator,bool> res = o_downloading.insert(tp->url);
            if (!res.second) {
                LOGDEB1("discovery:cllb: already downloading " << tp->url << '\n');
                delete tp;
                return UPNP_E_SUCCESS;
            }
        }

#undef ONLY_IPV6
#ifdef ONLY_IPV6
        struct sockaddr *claddr =
            reinterpret_cast<struct sockaddr*>(UpnpDiscovery_get_DestAddr(disco));
        if (claddr->sa_family != AF_INET6) {
            delete tp;
            return UPNP_E_SUCCESS;
        }
#endif
        
        LOGDEB1("discovery:cluCallback:: downloading " << tp->url << '\n');
        if (!downloadUrlWithCurl(tp->url, tp->description,DISCO_HTTP_TIMEOUT, &disco->DestAddr)) {
            LOGERR("discovery:cllb: downloadUrlWithCurl error for: " << tp->url << '\n');
            {   std::unique_lock<std::mutex> lock(o_downloading_mutex);
                o_downloading.erase(tp->url);
            }
            delete tp;
            return UPNP_E_SUCCESS;
        }
        LOGDEB1("discovery:cllb: downloaded description document of " <<
                tp->description.size() << " bytes" << '\n');

        {   std::unique_lock<std::mutex> lock(o_downloading_mutex);
            o_downloading.erase(tp->url);
        }

        if (!discoveredQueue.put(tp)) {
            delete tp;
            LOGERR("discovery:cllb: queue.put failed\n");
        }
        break;
    }
    case UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE:
    {
        UpnpDiscovery *disco = (UpnpDiscovery *)evp;
        LOGDEB1("discovery:cllB:BYEBYE: " << cluDiscoveryToStr(disco) << '\n');
        DiscoveredTask *tp = new DiscoveredTask(0, disco);
        if (!discoveredQueue.put(tp)) {
            delete tp;
            LOGERR("discovery:cllb: queue.put failed\n");
        }
        break;
    }
    default:
        // Ignore other events for now
        LOGDEB("discovery:cluCallBack: unprocessed evt type: [" << evTypeAsString(et) << "]"<<'\n');
        break;
    }

    return UPNP_E_SUCCESS;
}

// Our client can set up functions to be called when we process a new device.
// This is used during startup, when the pool is not yet complete, to enable
// finding and listing devices as soon as they appear.
static vector<UPnPDeviceDirectory::Visitor> o_callbacks;
static vector<UPnPDeviceDirectory::Visitor> o_lostCallbacks;
static std::mutex o_callbacks_mutex;
static bool simpleTraverse(UPnPDeviceDirectory::Visitor visit);
static bool simpleVisit(const UPnPDeviceDesc&, UPnPDeviceDirectory::Visitor);

unsigned int UPnPDeviceDirectory::addCallback(UPnPDeviceDirectory::Visitor v)
{
    std::unique_lock<std::mutex> lock(o_callbacks_mutex);
    o_callbacks.push_back(v);
    // People use this method to avoid waiting for the initial
    // delay. Return all data which we already have ! Else the
    // quick-responding devices won't be found before the
    // delay ends and the user finally calls traverse().
    simpleTraverse(v);
    return (unsigned int)(o_callbacks.size() - 1);
}

void UPnPDeviceDirectory::delCallback(unsigned int idx)
{
    std::unique_lock<std::mutex> lock(o_callbacks_mutex);
    if (idx >= o_callbacks.size())
        return;
    o_callbacks.erase(o_callbacks.begin() + idx);
}

unsigned int UPnPDeviceDirectory::addLostCallback(Visitor v)
{
    std::unique_lock<std::mutex> lock(o_callbacks_mutex);
    o_lostCallbacks.push_back(v);
    return (unsigned int)(o_lostCallbacks.size() - 1);
}

void UPnPDeviceDirectory::delLostCallback(unsigned int idx)
{
    std::unique_lock<std::mutex> lock(o_callbacks_mutex);
    if (idx >= o_lostCallbacks.size())
        return;
    o_lostCallbacks.erase(o_lostCallbacks.begin() + idx);
}


// Descriptor kept in the device pool for each device found on the network.
class DeviceDescriptor {
public:
    DeviceDescriptor(const string& url, const string& description,
                     std::chrono::steady_clock::time_point last, int exp)
        : device(url, description), last_seen(last), expires(std::chrono::seconds(exp)) {}
    DeviceDescriptor() = default;
    UPnPDeviceDesc device;
    std::chrono::steady_clock::time_point last_seen;
    std::chrono::seconds expires; // seconds valid
};

// A DevicePool holds the characteristics of the devices
// currently on the network.
// The map is referenced by deviceId (==UDN)
// The class is instanciated as a static (unenforced) singleton.
// There should only be entries for root devices. The embedded devices
// are described by a list inside their root device entry.
class DevicePool {
public:
    std::mutex m_mutex;
    map<string, DeviceDescriptor> m_devices;
};
static DevicePool o_pool;

// Worker routine for the discovery queue. Get messages about devices
// appearing and disappearing, and update the directory pool
// accordingly.
static void *discoExplorer(void *)
{
    for (;;) {
        DiscoveredTask *tsk = 0;
        size_t qsz;
        if (!discoveredQueue.take(&tsk, &qsz, {1min})) {
            discoveredQueue.workerExit();
            return (void*)1;
        }

        if (!tsk) {
            LOGDEB1("discoExplorer: empty queue timeout");
            expireDevices();
            continue;
        }

        LOGDEB1("discoExplorer: got task: alive " << tsk->alive << " deviceId ["
                << tsk->deviceId << " URL [" << tsk->url << "]" << '\n');

        if (!tsk->alive) {
            // Device signals it is going off.
            std::unique_lock<std::mutex> lock(o_pool.m_mutex);
            auto it = o_pool.m_devices.find(tsk->deviceId);
            if (it != o_pool.m_devices.end()) {
                std::unique_lock<std::mutex> lock(o_callbacks_mutex);
                for (auto& cbp : o_lostCallbacks) {
                    simpleVisit(it->second.device, cbp);
                }

                o_pool.m_devices.erase(it);
                LOGDEB2("discoExplorer: delete " << tsk->deviceId.c_str() << '\n');
            }
        } else {
            // Update or insert the device
            DeviceDescriptor d(
                tsk->url, tsk->description, std::chrono::steady_clock::now(), tsk->expires);
            if (!d.device.ok) {
                LOGERR("discoExplorer: description parse failed for " << tsk->deviceId << '\n');
                LOGINF("discoExplorer: description data: [" << tsk->description << "]\n");
                delete tsk;
                continue;
            }
            LOGDEB1("discoExplorer: found id [" << tsk->deviceId  << "]"
                    << " name " << d.device.friendlyName
                    << " devtype " << d.device.deviceType << " expires " <<
                    tsk->expires << '\n');
            {
                std::unique_lock<std::mutex> lock(o_pool.m_mutex);
                LOGDEB1("discoExplorer: inserting device id "<< tsk->deviceId
                        << " description: " << '\n' << d.device.dump() << '\n');
                o_pool.m_devices[tsk->deviceId] = d;
            }

            {
                std::unique_lock<std::mutex> lock(o_callbacks_mutex);
                for (auto& cbp : o_callbacks) {
                    simpleVisit(d.device, cbp);
                }
            }
        }
        delete tsk;
    }
}

// Look at the devices and get rid of those which have not been seen for too long.
// We do this when listing the top directory or on a one minute timeout from the lower lib discovery
// worker.
static void expireDevices()
{
    LOGDEB1("discovery: expireDevices:" << '\n');
    std::unique_lock<std::mutex> lock(o_pool.m_mutex);
    auto now = std::chrono::steady_clock::now();
    bool didsomething = false;

    for (auto it = o_pool.m_devices.begin(); it != o_pool.m_devices.end();) {
        LOGDEB1("Dev in pool: type: " << it->second.device.deviceType <<
                " friendlyName " << it->second.device.friendlyName << '\n');
        if (now - it->second.last_seen > it->second.expires) {
            LOGDEB1("expireDevices: deleting " <<  it->first.c_str() << " " <<
                    it->second.device.friendlyName.c_str() << '\n');

            {
                std::unique_lock<std::mutex> lock(o_callbacks_mutex);

                for (auto& cbp : o_lostCallbacks) {
                    simpleVisit(it->second.device, cbp);
                }
            }

            it = o_pool.m_devices.erase(it);
            didsomething = true;
        } else {
            ++it;
        }
    }
    // start a search if something changed or 5 S elapsed. upnp-inspector uses a 2 S permanent loop
    // (in msearch.py, __init__()). This ought not to be necessary of course...
    if (didsomething || std::chrono::steady_clock::now() - o_lastSearch > std::chrono::seconds(5)) {
        search();
    }
}

// m_searchTimeout is the UPnP device search timeout, which should actually be called delay because
// it's the base of a random delay that the devices apply to avoid responding all at the same time.
// This means that you have to wait for the specified period before the results are complete.
UPnPDeviceDirectory::UPnPDeviceDirectory(time_t search_window)
{
    if (search_window < UPNP_MIN_SEARCH_TIME)
        search_window = UPNP_MIN_SEARCH_TIME;
    else if (search_window > UPNP_MAX_SEARCH_TIME)
        search_window = UPNP_MAX_SEARCH_TIME;

    o_searchTimeout = search_window;

    addCallback(std::bind(&deviceFound, _1, _2));

    if (!discoveredQueue.start(1, discoExplorer, 0)) {
        o_reason = "Discover work queue start failed";
        return;
    }
    std::this_thread::yield();
    LibUPnP *lib = LibUPnP::getLibUPnP();
    if (lib == 0) {
        o_reason = "Can't get lib";
        return;
    }
    lib->m->registerHandler(UPNP_DISCOVERY_SEARCH_RESULT, cluCallBack, this);
    lib->m->registerHandler(UPNP_DISCOVERY_ADVERTISEMENT_ALIVE, cluCallBack, this);
    lib->m->registerHandler(UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE, cluCallBack, this);

    o_ok = search();
}

bool UPnPDeviceDirectory::ok()
{
    return o_ok;
}

const string UPnPDeviceDirectory::getReason()
{
    return o_reason;
}

bool UPnPDeviceDirectory::uniSearch(const std::string& url)
{
    LibUPnP *lib = LibUPnP::getLibUPnP();
    if (lib == 0) {
        o_reason = "Can't get lib";
        return false;
    }

    const char *target = "upnp:rootdevice";

    int code = UpnpSearchAsyncUnicast(lib->m->getclh(), url, target, lib);
    if (code != UPNP_E_SUCCESS) {
        o_reason = LibUPnP::errAsString("UpnpSearchAsyncUnicast", code);
        LOGERR("UPnPDeviceDirectory::search: UpnpSearchAsyncUnicast failed: " << o_reason << "\n");
        return false;
    }
    return true;
}

static bool search()
{
    LOGDEB1("UPnPDeviceDirectory::search" << '\n');

    if (std::chrono::steady_clock::now() - o_lastSearch < std::chrono::seconds(o_searchTimeout)) {
        LOGDEB1("UPnPDeviceDirectory: last search too close\n");
        return true;
    }
    
    LibUPnP *lib = LibUPnP::getLibUPnP();
    if (lib == 0) {
        o_reason = "Can't get lib";
        return false;
    }

    //const char *cp = "ssdp:all";
    const char *target = "upnp:rootdevice";

    LOGDEB1("UPnPDeviceDirectory::search: calling upnpsearchasync" << "\n");
    int code1 = UpnpSearchAsync(lib->m->getclh(), (int)o_searchTimeout, target, lib);
    if (code1 != UPNP_E_SUCCESS) {
        o_reason = LibUPnP::errAsString("UpnpSearchAsync", code1);
        LOGERR("UPnPDeviceDirectory::search: UpnpSearchAsync failed: " << o_reason << "\n");
        return false;
    }
    o_lastSearch = std::chrono::steady_clock::now();
    return true;
}

UPnPDeviceDirectory *UPnPDeviceDirectory::getTheDir(time_t search_window)
{
    if (theDevDir == 0)
        theDevDir = new UPnPDeviceDirectory(search_window);
    if (theDevDir && !theDevDir->ok())
        return 0;
    return theDevDir;
}

void UPnPDeviceDirectory::terminate()
{
    LibUPnP *lib = LibUPnP::getLibUPnP();
    if (lib) {
        lib->m->registerHandler(UPNP_DISCOVERY_SEARCH_RESULT, 0, 0);
        lib->m->registerHandler(UPNP_DISCOVERY_ADVERTISEMENT_ALIVE, 0, 0);
        lib->m->registerHandler(UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE, 0, 0);
    }
    discoveredQueue.setTerminateAndWait();
}

time_t UPnPDeviceDirectory::getRemainingDelayMs()
{
    if (o_initialSearchDone) {
        // Once the initial search is done, the directory is
        // supposedly up to date at all times, there is no remainingDelay.
        return 0;
    }
    
    auto remain = std::chrono::seconds(o_searchTimeout) -
        (std::chrono::steady_clock::now() - o_lastSearch);
    // Let's give them a grace delay beyond the search window
    remain += std::chrono::milliseconds(200);
    if (remain.count() < 0)
        return 0;

    return std::chrono::duration_cast<std::chrono::milliseconds>(remain).count();
}

time_t UPnPDeviceDirectory::getRemainingDelay()
{
    time_t millis = getRemainingDelayMs();
    if (millis <= 0)
        return 0;
    return millis >= 1000 ? millis / 1000 : 1;
}

static std::mutex devWaitLock;
static std::condition_variable devWaitCond;

// Call user function on one device (for all services)
static bool simpleVisit(const UPnPDeviceDesc& dev, UPnPDeviceDirectory::Visitor visit)
{
    for (const auto& service : dev.services) {
        if (!visit(dev, service)) {
            return false;
        }
    }
    for (const auto& subdev : dev.embedded) {
        for (const auto& service : subdev.services) {
            if (!visit(subdev, service)) {
                return false;
            }
        }
    }
    return true;
}

// Walk the device list and call simpleVisit() on each.
static bool simpleTraverse(UPnPDeviceDirectory::Visitor visit)
{
    std::unique_lock<std::mutex> lock(o_pool.m_mutex);

    for (auto& it : o_pool.m_devices) {
        if (!simpleVisit(it.second.device, visit)) {
            return false;
        }
    }
    return true;
}

bool UPnPDeviceDirectory::traverse(UPnPDeviceDirectory::Visitor visit)
{
    //LOGDEB("UPnPDeviceDirectory::traverse" << '\n');
    if (!o_ok)
        return false;

    // Wait until the discovery delay is over. We need to loop because of spurious wakeups each time
    // a new device is discovered. We could use a separate cv or another way of sleeping instead. We
    // only do this once, after which we're sure that the initial discovery is done and that the
    // directory is supposedly up to date. There is no reason to wait during further searches. We
    // may wait for nothing once but it's simpler than detecting the end of the actual initial
    // discovery.
    for (;!o_initialSearchDone;) {
        std::unique_lock<std::mutex> lock(devWaitLock);
        time_t ms;
        if ((ms = getRemainingDelayMs()) > 0) {
            devWaitCond.wait_for(lock, chrono::milliseconds(ms));
        } else {
            o_initialSearchDone = true;
            break;
        }
    } 

    // Has locking, do it before our own lock
    expireDevices();
    return simpleTraverse(visit);
}

static bool deviceFound(const UPnPDeviceDesc&, const UPnPServiceDesc&)
{
    devWaitCond.notify_all();
    return true;
}

// Lookup a device in the pool. If not found and a search is active,
// use a cond_wait to wait for device events (awaken by deviceFound).
static bool getDevBySelector(bool cmp(const UPnPDeviceDesc& ddesc, const string&),
                             const string& value, UPnPDeviceDesc& ddesc)
{
    // Has locking, do it before our own lock
    expireDevices();

    for (;;) {
        std::unique_lock<std::mutex> lock(devWaitLock);
        time_t ms = UPnPDeviceDirectory::getTheDir()->getRemainingDelayMs();
        {
            std::unique_lock<std::mutex> lock(o_pool.m_mutex);
            for (auto& it : o_pool.m_devices) {
                if (!cmp(it.second.device, value)) {
                    ddesc = it.second.device;
                    return true;
                }
                for (auto& it1 : it.second.device.embedded) {
                    if (!cmp(it1, value)) {
                        ddesc = it1;
                        return true;
                    }
                }
            }
        }

        if (ms > 0) {
            devWaitCond.wait_for(lock, chrono::milliseconds(ms));
        } else {
            break;
        }
    }
    return false;
}

static bool cmpFName(const UPnPDeviceDesc& ddesc, const string& fname)
{
    return ddesc.friendlyName != fname;
}

bool UPnPDeviceDirectory::getDevByFName(const string& fname, UPnPDeviceDesc& ddesc)
{
    return getDevBySelector(cmpFName, fname, ddesc);
}

static bool cmpUDN(const UPnPDeviceDesc& ddesc, const string& value)
{
    return ddesc.UDN != value;
}

bool UPnPDeviceDirectory::getDevByUDN(const string& value, UPnPDeviceDesc& ddesc)
{
    return getDevBySelector(cmpUDN, value, ddesc);
}

bool UPnPDeviceDirectory::getDescriptionDocuments(
    const string &uidOrFriendly, string& deviceXML, unordered_map<string, string>& srvsXML)
{
    UPnPDeviceDesc ddesc;
    if (!getDevByUDN(uidOrFriendly, ddesc) && !getDevByFName(uidOrFriendly, ddesc)) {
        return false;
    }
    deviceXML = ddesc.XMLText;
    for (const auto& entry : ddesc.services) {
        srvsXML[entry.serviceId] = "";
        UPnPServiceDesc::Parsed parsed;
        entry.fetchAndParseDesc(ddesc.URLBase, parsed, &srvsXML[entry.serviceId]);
    }
    return true;
}


} // namespace UPnPClient
