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

#include "device.hxx"

#include <cerrno>
#include <ctime>
#include <cstring>

#include <iostream>
#include <sstream>
#include <utility>
#include <algorithm>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <chrono>

#include <upnp.h>

#include "libupnpp/log.hxx"
#include "libupnpp/smallut.h"
#include "libupnpp/upnpplib.hxx"
#include "libupnpp/upnpputils.hxx"
#include "libupnpp/upnpp_p.hxx"
#include "vdir.hxx"

#ifndef NPUPNP_AT_LEAST
#define NPUPNP_AT_LEAST(MAJ,MIN,REV)                                  \
    VERSION_AT_LEAST(NPUPNP_VERSION_MAJOR,NPUPNP_VERSION_MINOR,       \
                     NPUPNP_VERSION_PATCH, (MAJ),(MIN),(REV))
#endif

using namespace std;
using namespace UPnPP;

#define CBCONST const

namespace UPnPProvider {

class UpnpDevice::Internal {
public:
    Internal(UpnpDevice *dev)
        : me(dev) {}
    
    /* Generate event: called by the device event loop, which polls
     * the services. */
    void notifyEvent(const std::string& serviceId,
                     const std::vector<std::string>& names,
                     const std::vector<std::string>& values);
    bool start();

    std::unordered_map<std::string, UpnpService*>::const_iterator
    findService(const std::string& serviceid);

    /* Per-device callback */
    int callBack(Upnp_EventType et, const void* evp);

    UpnpDevice *me{nullptr};
    UpnpDevice *rootdev{nullptr};
    UPnPP::LibUPnP *lib{nullptr};
    string deviceId;

    // For SERVER headers:
    string product;
    string version;
    
    // In case startloop has been called: the event loop thread.
    std::thread loopthread;

    // Subdirectory of the virtual file tree where we store the XML files
    string devsubd;

    struct DevXML {
        // Device properties XML fragment: friendlyName, deviceType
        // must be in there UDN must not because we add it. Others can
        // be added: modelName, serialNumber, manufacturer etc.
        string propsxml;
        // Description xml service list. Incrementally built as the
        // services are added
        string servicexml;
    };
    // This object description XML
    DevXML myxml;
    
    // In case there are embedded devices, here is where they store their XML
    // fragments. The key is the embedded device UDN/aka deviceId
    map<string, DevXML> embedxml;
    
    // We keep the services in a map for easy access from id and in a
    // vector for ordered walking while fetching status. Order is
    // determine by addService() call sequence.
    std::unordered_map<std::string, UpnpService*> servicemap;
    std::vector<std::string> serviceids;
    std::unordered_map<std::string, soapfun> calls;
    bool needExit{false};
    /* My libupnp device handle */
    UpnpDevice_Handle dvh{0};
    /* Lock for device operations. Held during a service callback
       Must not be held when using dvh to call into libupnp */
    std::mutex devlock;
    std::condition_variable evloopcond;
    std::mutex evlooplock;
};

class UpnpDevice::InternalStatic {
public:
    /* Static callback for libupnp. This looks up the appropriate
     * device using the device ID (UDN), then calls its callback
     * method */
    static int sCallBack(Upnp_EventType et, CBCONST void* evp, void*);

    /** Static array of devices for dispatching */
    static std::unordered_map<std::string, UpnpDevice *> devices;
    static std::mutex devices_lock;
};

std::unordered_map<std::string, UpnpDevice *>
UpnpDevice::InternalStatic::devices;
std::mutex UpnpDevice::InternalStatic::devices_lock;
UpnpDevice::InternalStatic *UpnpDevice::o;

static bool vectorstoargslists(const vector<string>& names,
                               const vector<string>& values,
                               vector<string>& qvalues,
                               vector<const char *>& cnames,
                               vector<const char *>& cvalues)
{
    if (names.size() != values.size()) {
        LOGERR("vectorstoargslists: bad sizes" << endl);
        return false;
    }

    cnames.reserve(names.size());
    qvalues.clear();
    qvalues.reserve(values.size());
    cvalues.reserve(values.size());
    for (unsigned int i = 0; i < values.size(); i++) {
        cnames.push_back(names[i].c_str());
        qvalues.push_back(SoapHelp::xmlQuote(values[i]));
        cvalues.push_back(qvalues[i].c_str());
        //LOGDEB("Edata: " << cnames[i] << " -> " << cvalues[i] << endl);
    }
    return true;
}

static const int expiretime = 3600;

// Stuff we transform in the uuids when we use them in urls
static string replchars{"\"#%;<>?[\\]^`{|}:/ "};

UpnpDevice::UpnpDevice(const string& deviceId)
{
    if (o == 0 && (o = new InternalStatic()) == 0) {
        LOGERR("UpnpDevice::UpnpDevice: out of memory" << endl);
        return;
    }
    if ((m = new Internal(this)) == 0) {
        LOGERR("UpnpDevice::UpnpDevice: out of memory" << endl);
        return;
    }
    m->deviceId = deviceId;
    m->devsubd = string("/") + neutchars(deviceId, replchars, '-') +
        string("/");
    //LOGDEB("UpnpDevice::UpnpDevice(" << deviceId << ")" << endl);

    m->lib = LibUPnP::getLibUPnP(true);
    if (!m->lib) {
        LOGFAT(" Can't get LibUPnP" << endl);
        return;
    }
    if (!m->lib->ok()) {
        LOGFAT("Lib init failed: " <<
               m->lib->errAsString("main", m->lib->getInitError()) << endl);
        m->lib = 0;
        return;
    }

    {
        std::unique_lock<std::mutex> lock(o->devices_lock);
        if (o->devices.empty()) {
            // First call: init callbacks
            m->lib->m->registerHandler(UPNP_CONTROL_ACTION_REQUEST,
                                       o->sCallBack, this);
            m->lib->m->registerHandler(UPNP_CONTROL_GET_VAR_REQUEST,
                                       o->sCallBack, this);
            m->lib->m->registerHandler(UPNP_EVENT_SUBSCRIPTION_REQUEST,
                                       o->sCallBack, this);
        }
        o->devices[m->deviceId] = this;
    }
}

UpnpDevice::UpnpDevice(UpnpDevice* rootdev, const string& deviceId)
    : UpnpDevice(deviceId)
{
    if (nullptr != rootdev) {
        m->rootdev = rootdev;
        rootdev->m->embedxml[deviceId] = UpnpDevice::Internal::DevXML();
    }
}

UpnpDevice::~UpnpDevice()
{
    m->needExit = true;
    m->evloopcond.notify_all();

    if (nullptr == m->rootdev) {
        UpnpUnRegisterRootDevice(m->dvh);
    }

    if (m->loopthread.joinable())
        m->loopthread.join();

    std::unique_lock<std::mutex> lock(o->devices_lock);
    auto it = o->devices.find(m->deviceId);
    if (it != o->devices.end())
        o->devices.erase(it);
    delete m;
}

void UpnpDevice::setProductVersion(const char *product, const char *version)
{
    if (product && version) {
        m->product = product;
        m->version = version;
    }
}

const string& UpnpDevice::getDeviceId() const
{
    return m->deviceId;
}

bool UpnpDevice::ipv4(string *host, unsigned short *port)
{
    const char *hst = UpnpGetServerIpAddress();
    if (hst == 0) {
        return false;
    }
    if (port) {
        *port = UpnpGetServerPort();
    }
    if (host) {
        *host = string(hst);
    }
    return true;
}

static string ipv4tostrurl(const string& host, unsigned short port)
{
    return string("http://") + host + ":" + SoapHelp::i2s(port);
}

// Calls registerRootDevice() to register us with the lib and start up
// the web server for sending out description files.
bool UpnpDevice::Internal::start()
{
    // Nothing to do if I am an embedded device: advertisement is
    // handled by my root device
    if (nullptr != rootdev) {
        return true;
    }
    
    // Finish building the device description XML file, and add it to
    // the virtual directory.
    string descxml(
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
        "<root xmlns=\"urn:schemas-upnp-org:device-1-0\">\n"
        "  <specVersion>\n"
        "    <major>1</major>\n"
        "    <minor>1</minor>\n"
        "  </specVersion>\n"
        "  <device>\n"
        );
    descxml += myxml.propsxml;
    descxml += "    <UDN>" + deviceId + "</UDN>\n";
    descxml += "    <serviceList>\n";
    descxml += myxml.servicexml;
    descxml += "    </serviceList>\n";
    if (!embedxml.empty()) {
        descxml += "    <deviceList>\n";
        for  (const auto& entry : embedxml) {
            descxml += "      <device>\n";
            descxml += entry.second.propsxml;
            descxml += "        <UDN>" + entry.first + "</UDN>\n";
            descxml += "        <serviceList>\n";
            descxml += entry.second.servicexml;
            descxml += "        </serviceList>\n";
            descxml += "      </device>\n";
        }
        descxml += "</deviceList>\n";
    }
        
    descxml += "  </device>\n</root>\n";

    VirtualDir* theVD = VirtualDir::getVirtualDir();
    if (nullptr != theVD) {
#ifndef SETUP_DESCRIPTION_BY_BUFFER
        theVD->addFile(devsubd, "description.xml", descxml, "text/xml");
#endif
    } else {
        LOGERR("UpnpDevice: can't start: no VirtualDir??\n");
        return false;
    }

    // Tell the pupnp lib to serve the description.
    int ret;
#ifdef SETUP_DESCRIPTION_BY_BUFFER
    if ((ret = lib->m->setupWebServer(descxml, &dvh)) != 0) {
        LOGERR("UpnpDevice: libupnp can't start service. Err " << ret << endl);
        return false;
    }
#else
    string host(UpnpGetServerIpAddress());
    unsigned short port = UpnpGetServerPort();
    string url = ipv4tostrurl(host, port) + devsubd + "description.xml";
    if ((ret = lib->m->setupWebServer(url, &dvh)) != 0) {
        LOGERR("UpnpDevice: libupnp can't start service. Err " << ret << endl);
        return false;
    }
#endif
#if NPUPNP_AT_LEAST(4,1,0)
    if (!product.empty()) {
        UpnpDeviceSetProduct(dvh, product.c_str(), version.c_str());
    }
#endif
    if ((ret = UpnpSendAdvertisement(dvh, expiretime)) != 0) {
        LOGERR("UpnpDevice::Internal::start(): sendAvertisement failed: " <<
               lib->errAsString("UpnpDevice: UpnpSendAdvertisement", ret) <<
               endl);
        return false;
    }
    return true;
}


bool UpnpDevice::addVFile(const string& name, const string& contents,
                          const std::string& mime, string& path)
{
    VirtualDir* theVD = VirtualDir::getVirtualDir();
    if (theVD) {
        if (!theVD->addFile(m->devsubd, name, contents, mime)) {
            return false;
        }
        path = m->devsubd + name;
        return true;
    } else {
        return false;
    }
}

    
// Main libupnp callback: use the device id and call the right device
int UpnpDevice::InternalStatic::sCallBack(
    Upnp_EventType et, CBCONST void* evp, void*)
{
    //LOGDEB("UpnpDevice::sCallBack" << endl);

    string deviceid;
    switch (et) {
    case UPNP_CONTROL_ACTION_REQUEST:
        deviceid = UpnpActionRequest_get_DevUDN_cstr((UpnpActionRequest *)evp);
        break;

    case UPNP_CONTROL_GET_VAR_REQUEST:
        return UPNP_E_INVALID_PARAM;
        break;

    case UPNP_EVENT_SUBSCRIPTION_REQUEST:
        deviceid =
            UpnpSubscriptionRequest_get_UDN_cstr((UpnpSubscriptionRequest*)evp);
        break;

    default:
        LOGERR("UpnpDevice::sCallBack: unknown event " << et << endl);
        return UPNP_E_INVALID_PARAM;
    }
    // LOGDEB("UpnpDevice::sCallBack: deviceid[" << deviceid << "]" << endl);

    std::unordered_map<std::string, UpnpDevice *>::iterator it;
    {
        std::unique_lock<std::mutex> lock(o->devices_lock);

        it = o->devices.find(deviceid);

        if (it == o->devices.end()) {
            LOGERR("UpnpDevice::sCallBack: Device not found: [" <<
                   deviceid << "]" << endl);
            return UPNP_E_INVALID_PARAM;
        }
    }

    //LOGDEB("UpnpDevice::sCallBack: device found: ["<<it->second<<"]"<<endl);
    return (it->second)->m->callBack(et, evp);
}

bool UpnpDevice::ok()
{
    return o && m && m->lib != 0;
}

std::unordered_map<string, UpnpService*>::const_iterator
UpnpDevice::Internal::findService(const string& serviceid)
{
    std::unique_lock<std::mutex> lock(devlock);
    auto servit = servicemap.find(serviceid);
    if (servit == servicemap.end()) {
        LOGERR("UpnpDevice: Bad serviceID: [" << serviceid << "]\n");
    }
    return servit;
}

int UpnpDevice::Internal::callBack(Upnp_EventType et, const void* evp)
{
    switch (et) {
    case UPNP_CONTROL_ACTION_REQUEST:
    {
        UpnpActionRequest *act = (UpnpActionRequest *)evp;

        LOGDEB("UPNP_CONTROL_ACTION_REQUEST: " <<
               UpnpActionRequest_get_ActionName_cstr(act) << " args: " <<
               SoapHelp::argsToStr(act->args.begin(), act->args.end()) << endl);

        auto servit = findService(UpnpActionRequest_get_ServiceID_cstr(act));
        if (servit == servicemap.end()) {
            return UPNP_E_INVALID_PARAM;
        }

        string actname{UpnpActionRequest_get_ActionName_cstr(act)};
        SoapOutgoing dt(servit->second->getServiceType(), actname);
        {
            std::unique_lock<std::mutex> lock(devlock);

            auto callit = calls.find(
                actname + UpnpActionRequest_get_ServiceID_cstr(act));
            if (callit == calls.end()) {
                LOGINF("UpnpDevice: No such action: " << actname << endl);
                return UPNP_E_INVALID_PARAM;
            }

            SoapIncoming sc;
            sc.m->args.insert(act->args.begin(), act->args.end());

            // Call the action routine
            int ret = callit->second(sc, dt);
            if (ret != UPNP_E_SUCCESS) {
                if (ret > 0) {
                    act->ErrCode = ret;
                    strncpy(
                        act->ErrStr,
                        servit->second->errString(ret).c_str(), LINE_SIZE-1);
                    act->ErrStr[LINE_SIZE-1] = 0;
                }
                LOGERR("UpnpDevice: Action failed: " << sc.getName() <<
                       " code " << ret << endl);
                return ret;
            }
        }

        // Set result data
        UpnpActionRequest_set_ErrCode(act, UPNP_E_SUCCESS);
        act->resdata = dt.m->data;
        LOGDEB("Response data: " << SoapHelp::argsToStr(
                   act->resdata.begin(), act->resdata.end()) << endl);
        return UPNP_E_SUCCESS;
    }
    break;

    case UPNP_CONTROL_GET_VAR_REQUEST:
        // Note that the "Control: query for variable" action is
        // deprecated (upnp arch v1), and we should never get these.
    {
        LOGDEB("UPNP_CONTROL_GET_VAR__REQUEST??\n");
    }
    break;

    case UPNP_EVENT_SUBSCRIPTION_REQUEST:
    {
        UpnpSubscriptionRequest *act = (UpnpSubscriptionRequest*)evp;
        LOGDEB("UPNP_EVENT_SUBSCRIPTION_REQUEST: " <<
               UpnpSubscriptionRequest_get_ServiceId_cstr(act) << endl);

        auto servit=findService(UpnpSubscriptionRequest_get_ServiceId_cstr(act));
        if (servit == servicemap.end()) {
            return UPNP_E_INVALID_PARAM;
        }

        vector<string> names, values, qvalues;
        {
            std::unique_lock<std::mutex> lock(devlock);
            if (!servit->second->getEventData(true, names, values)) {
                break;
            }
        }

        vector<const char *> cnames, cvalues;
        vectorstoargslists(names, values, qvalues, cnames, cvalues);
        int ret = UpnpAcceptSubscription(
            dvh, UpnpSubscriptionRequest_get_UDN_cstr(act),
            UpnpSubscriptionRequest_get_ServiceId_cstr(act),
            cnames.size()?&cnames[0]:nullptr,
            cnames.size()?&cvalues[0]:nullptr, int(cnames.size()),
            UpnpSubscriptionRequest_get_SID_cstr(act));
        if (ret != UPNP_E_SUCCESS) {
            LOGERR(lib->errAsString("UpnpDevice::callBack: "
                                    "UpnpAcceptSubscription", ret) << endl);
        }

        return ret;
    }
    break;

    default:
        LOGINF("UpnpDevice::callBack: unknown libupnp event type: " <<
               evTypeAsString(et).c_str() << endl);
        return UPNP_E_INVALID_PARAM;
    }
    return UPNP_E_INVALID_PARAM;
}

bool UpnpDevice::addService(UpnpService *serv)
{
    const string& serviceId = serv->getServiceId();
    LOGDEB("UpnpDevice::addService: [" << serviceId << "]\n");
    std::unique_lock<std::mutex> lock(m->devlock);

    string* propsxml;
    string* servicexml;
    if (nullptr == m->rootdev) {
        propsxml = &m->myxml.propsxml;
        servicexml = &m->myxml.servicexml;
    } else {
        auto it = m->rootdev->m->embedxml.find(m->deviceId);
        if (it == m->rootdev->m->embedxml.end()) {
            LOGERR("UpnpDevice::addservice: my Id " << m->deviceId <<
                   " not found in root dev " << m->rootdev->m->deviceId << endl);
            return false;
        }
        propsxml = &it->second.propsxml;
        servicexml = &it->second.servicexml;
    }

    if (propsxml->empty()) {
        // Retrieve the XML fragment with friendlyname etc.
        if (!readLibFile("", *propsxml)) {
            LOGERR("UpnpDevice::start: Could not read description XML props\n");
            return false;
        }
    }
    
    m->servicemap[serviceId] = serv;
    vector<string>::iterator it =
        std::find(m->serviceids.begin(), m->serviceids.end(), serviceId);
    if (it != m->serviceids.end())
        m->serviceids.erase(it);
    m->serviceids.push_back(serviceId);

    string servnick = neutchars(serv->getServiceType(), replchars, '-');

    // Add the service description to the virtual directory
    string scpdxml;
    string xmlfn = serv->getXMLFn();
    VirtualDir* theVD = VirtualDir::getVirtualDir();
    if (!readLibFile(xmlfn, scpdxml)) {
        LOGERR("UpnpDevice::addService: could not retrieve service definition"
               "file: nm: [" << xmlfn << "]\n");
    } else {
        theVD->addFile(m->devsubd, servnick + ".xml",
                       scpdxml, "application/xml");
    }
    *servicexml +=
        string("<service>\n") +
        " <serviceType>" + serv->getServiceType() + "</serviceType>\n" +
        " <serviceId>"   + serv->getServiceId() + "</serviceId>\n" +
        " <SCPDURL>"     + m->devsubd + servnick + ".xml</SCPDURL>\n" +
        " <controlURL>"  + m->devsubd + "ctl-" + servnick + "</controlURL>\n" +
        " <eventSubURL>" + m->devsubd + "evt-" + servnick + "</eventSubURL>\n" +
        "</service>\n";
    return true;
}

void UpnpDevice::forgetService(const std::string& serviceId)
{
    LOGDEB("UpnpDevice::forgetService: " << serviceId << endl);
    std::unique_lock<std::mutex> lock(m->devlock);

    std::unordered_map<string, UpnpService*>::iterator servit =
        m->servicemap.find(serviceId);
    if (servit != m->servicemap.end()) {
        m->servicemap.erase(servit);
    }
    vector<string>::iterator it =
        std::find(m->serviceids.begin(), m->serviceids.end(), serviceId);
    if (it != m->serviceids.end())
        m->serviceids.erase(it);
}

void UpnpDevice::addActionMapping(const UpnpService* serv,
                                  const std::string& actName,
                                  soapfun fun)
{
    std::unique_lock<std::mutex> lock(m->devlock);
    // LOGDEB("UpnpDevice::addActionMapping:" << actName << endl);
    m->calls[actName + serv->getServiceId()] = fun;
}

void UpnpDevice::Internal::notifyEvent(const string& serviceId,
                                       const vector<string>& names,
                                       const vector<string>& values)
{
    LOGDEB1("UpnpDevice::notifyEvent: deviceId " << deviceId << " serviceId " <<
            serviceId << " chg[0] " << (names.empty() ? "Empty names??" :
                                        names[0] + "->" + values[0]) << "\n");
    if (names.empty())
        return;

    stringstream ss;
    for (unsigned int i = 0; i < names.size() && i < values.size(); i++) {
        ss << names[i] << "=" << values[i] << " ";
    }
    vector<const char *> cnames, cvalues;
    vector<string> qvalues;
    vectorstoargslists(names, values, qvalues, cnames, cvalues);

    int ret = UpnpNotify(dvh, deviceId.c_str(),
                         serviceId.c_str(), &cnames[0], &cvalues[0],
                         int(cnames.size()));
    if (ret != UPNP_E_SUCCESS) {
        LOGERR("UpnpDevice::notifyEvent: " <<
               lib->errAsString("UpnpNotify", ret)<<" for "<< serviceId << endl);
    }
}

void UpnpDevice::startloop()
{
    m->loopthread = std::thread(&UpnpDevice::eventloop, this);
}

// Loop on services, and poll each for changed data. Generate event
// only if changed data exists.
// We used to generate an artificial event with all the current state
// every 10 S or so. This does not seem to be necessary, and we do not
// do it any more.
// This is normally run by the main thread.
void UpnpDevice::eventloop()
{
    if (!m->start()) {
        LOGERR("Device would not start" << endl);
        return;
    }
    
    int count = 0;
    // Polling the services every 1 S
    const std::chrono::milliseconds loopwait_ms(1000);
    // Full state every 10 S. This should not be necessary, but it
    // ensures that CPs get updated about our state even if they miss
    // some events. For example, the Songcast windows sender does not
    // see the TransportState transition to "Playing" if it is not
    // repeated few seconds later, with bad consequences on further
    // operations
    const int nloopstofull = 10;
    static std::chrono::steady_clock::time_point wkuptime, earlytime;
    bool didearly = false;

    for (;;) {
        wkuptime = std::chrono::steady_clock::now() + loopwait_ms;

        std::unique_lock<std::mutex> lock(m->evlooplock);
        std::cv_status err = m->evloopcond.wait_for(lock, loopwait_ms);
        if (m->needExit) {
            break;
        } else if (err == std::cv_status::no_timeout) {
            // Early wakeup. Only does something if it did not already
            // happen recently, else go back to sleep (at top of loop)
            if (didearly) {
                auto sincearly = wkuptime - earlytime;
                if (sincearly < loopwait_ms) {
                    // Do nothing. didearly stays true
                    // LOGDEB("eventloop: early, previous too close "<<endl);
                    continue;
                } else {
                    // had an early wakeup previously, but it was a
                    // long time ago. Update state and wakeup
                    // LOGDEB("eventloop: early, previous is old "<<endl);
                    earlytime = wkuptime;
                }
            } else {
                // Early wakeup, previous one was normal. Remember.
                // LOGDEB("eventloop: early, no previous" << endl);
                didearly = true;
                earlytime = wkuptime;
            }
        } else if (err == std::cv_status::timeout) {
            // Normal wakeup
            // LOGDEB("eventloop: normal wakeup" << endl);
            didearly = false;
        } else {
            LOGINF("UpnpDevice:eventloop: wait returned unexpected value" <<
                   int(err) << endl);
            break;
        }

        count++;
        bool all = count && ((count % nloopstofull) == 0);
        //LOGDEB("UpnpDevice::eventloop count "<<count<<" all "<<all<<endl);

        // We can't lock devlock around the loop because we don't want
        // to hold id when calling notifyEvent() (which calls
        // libupnp). This means that we should have a separate lock
        // for the services arrays. This would only be useful during
        // startup, while we add services, but the event loop is the
        // last call the main program will make after adding the
        // services, so locking does not seem necessary
        for (auto& it : m->serviceids) {
            vector<string> names, values;
            UpnpService* serv = m->servicemap[it];
            {

                // We don't generate periodic full event data any more
                // by default. Code kept around for easier switch back
                // on in case it's needed again
#ifndef LIBUPNPP_PERIODIC_FULL_DEVICE_EVENTS
                all = false;
#endif

                std::unique_lock<std::mutex> lock1(m->devlock);
                if (!serv->getEventData(all, names, values) || names.empty()) {
                    continue;
                }
            }
            if (!serv->noevents())
                m->notifyEvent(it, names, values);
        }
    }
}

bool UpnpDevice::start()
{
    if (!m->start()) {
        LOGERR("Device would not start" << endl);
        return false;
    }
    return true;
}

void UpnpDevice::notifyEvent(
    const UpnpService *service, const std::vector<std::string>& names,
    const std::vector<std::string>& values)
{
    const std::string& id = service->getServiceId();
    m->notifyEvent(id, names, values);
}

// Can't take the loop lock here. We're called from the service and
// hold the device lock. The locks would be taken in opposite order,
// causing a potential deadlock:
//  - device action takes device lock
//  - loop wakes up, takes loop lock
//  - blocks on device lock before calling getevent
//  - device calls loopwakeup which blocks on loop lock
// -> deadlock
void UpnpDevice::loopWakeup()
{
    m->evloopcond.notify_all();
}

// It does not seem right to unregister the device before the event loop returns. I think that this
// sometimes causes INVALID_HANDLE errors from upnpNotify() calls if the timing is bad. So we should
// probably first set needExit, then join the event loop, then close the device. Otoh, this does not
// seem to cause significant issues, so leave it alone for now.
void UpnpDevice::shouldExit()
{
    if (nullptr == m->rootdev) {
        UpnpUnRegisterRootDevice(m->dvh);
    }
    m->needExit = true;
    m->evloopcond.notify_all();
}

} // End namespace UPnPProvider
