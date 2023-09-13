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

#include "libupnpp/control/service.hxx"

#include <upnp.h>
#include <upnptools.h>

#include <string>
#include <utility>

#include "libupnpp/control/description.hxx"
#include "libupnpp/log.hxx"
#include "libupnpp/smallut.h"
#include "libupnpp/upnpp_p.hxx"
#include "libupnpp/upnpplib.hxx"

using namespace std::placeholders;
using namespace UPnPP;

#define CBCONST const

namespace UPnPClient {
static bool initEvents();
static int srvCB(Upnp_EventType et, CBCONST void* vevp, void*);

class Service::Internal {
public:
    /** Upper level client code event callbacks. To be called by derived class
     * for reporting events. */
    VarEventReporter *reporter{nullptr};
    std::string actionURL;
    std::string eventURL;
    std::string serviceType;
    std::string deviceId;
    std::string friendlyName;
    std::string manufacturer;
    std::string modelName;
    Upnp_SID    SID; /* Subscription Id */

    void initFromDeviceAndService(const UPnPDeviceDesc& devdesc, const UPnPServiceDesc& servdesc) {
        actionURL = caturl(devdesc.URLBase, servdesc.controlURL);
        eventURL = caturl(devdesc.URLBase, servdesc.eventSubURL);
        serviceType = servdesc.serviceType;
        deviceId = devdesc.UDN;
        friendlyName = devdesc.friendlyName;
        manufacturer = devdesc.manufacturer;
        modelName = devdesc.modelName;
    }
    /* Tell the UPnP device (through libupnp) that we want to receive
       its events. This is called by registerCallback() and sets m_SID */
    bool subscribe();
    bool unSubscribe();
};

/** Registered callbacks for all the service objects. The map is
 * indexed by SID, the subscription id which was obtained by
 * each object when subscribing to receive the events for its
 * device. The map allows the static function registered with
 * libupnp to call the appropriate object method when it receives
 * an event. */
static std::unordered_map<std::string, evtCBFunc> o_calls;
static std::mutex cblock;


Service::Service(const UPnPDeviceDesc& devdesc, const UPnPServiceDesc& servdesc)
    : m(new Internal())
{
    m->initFromDeviceAndService(devdesc, servdesc);
    // Only does anything the first time
    initEvents();
    // serviceInit() will be called from the derived class constructor if needed
}

bool Service::initFromDescription(const UPnPDeviceDesc& devdesc)
{
    if (!m) {
        LOGERR("Device::Device: Internal is null" << "\n");
        return false;
    }
    for (auto& servdesc : devdesc.services) {
        if (serviceTypeMatch(servdesc.serviceType)) {
            m->initFromDeviceAndService(devdesc, servdesc);
            // Only does anything the first time
            initEvents();
            return serviceInit(devdesc, servdesc);
        }
    }
    return false;
}

Service::Service()
    : m(new Internal())
{
}

Service::~Service()
{
    LOGDEB1("Service::~Service: " << m->eventURL << " SID " << m->SID << "\n");
    unregisterCallback();
    delete m;
    m = nullptr;
}

const std::string& Service::getFriendlyName() const
{
    return m->friendlyName;
}

const std::string& Service::getDeviceId() const
{
    return m->deviceId;
}

const std::string& Service::getServiceType() const
{
    return m->serviceType;
}

const std::string& Service::getActionURL() const
{
    return m->actionURL;
}

const std::string& Service::getModelName() const
{
    return m->modelName;
}

const std::string& Service::getManufacturer() const
{
    return m->manufacturer;
}

int Service::runAction(const SoapOutgoing& args, SoapIncoming& data, ActionOptions *opts)
{
    LibUPnP* lib = LibUPnP::getLibUPnP();
    if (lib == 0) {
        LOGINF("Service::runAction: no lib" << "\n");
        return UPNP_E_OUTOF_MEMORY;
    }
    UpnpClient_Handle hdl = lib->m->getclh();

    std::vector<std::pair<std::string, std::string>> response;
    if (opts && (opts->active_options & AOM_TIMEOUTMS)) {
        response.push_back({"timeoutms", lltodecstr(opts->timeoutms)});
    }
    int errcode;
    std::string errdesc;
    int ret =  UpnpSendAction(hdl, "", m->actionURL, m->serviceType,
                              args.m->name, args.m->data, response, &errcode, errdesc);
    if (ret != UPNP_E_SUCCESS) {
        LOGINF("Service::runAction: UpnpSendAction error " << ret << " for service: " <<
               args.m->serviceType << " action: " << args.m->name << " args: " <<
               SoapHelp::argsToStr(args.m->data.begin(), args.m->data.end()) << "\n");
        if (ret < 0) {
            LOGINF("    error message: " << UpnpGetErrorMessage(ret) << "\n");
        } else {
            LOGINF("    Response errorCode: " << errcode << " errorDescription: " << errdesc<<"\n");
        }
        return ret;
    }
    data.m->args.insert(response.begin(), response.end());
    return UPNP_E_SUCCESS;
}

int Service::runTrivialAction(const std::string& actionName, ActionOptions *opts)
{
    SoapOutgoing args(m->serviceType, actionName);
    SoapIncoming data;
    return runAction(args, data, opts);
}

template <class T> int Service::runSimpleGet(
    const std::string& actnm, const std::string& valnm, T *valuep, ActionOptions *opts)
{
    SoapOutgoing args(m->serviceType, actnm);
    SoapIncoming data;
    int ret = runAction(args, data, opts);
    if (ret != UPNP_E_SUCCESS) {
        return ret;
    }
    if (!data.get(valnm.c_str(), valuep)) {
        LOGERR("Service::runSimpleAction: " << actnm << " missing " << valnm <<" in response"<<"\n");
        return UPNP_E_BAD_RESPONSE;
    }
    return 0;
}

template <class T> int Service::runSimpleAction(
    const std::string& actnm, const std::string& valnm, T value, ActionOptions *opts)
{
    SoapOutgoing args(m->serviceType, actnm);
    args(valnm, SoapHelp::val2s(value));
    SoapIncoming data;
    return runAction(args, data, opts);
}

// The static event callback given to libupnp
static int srvCB(Upnp_EventType et, CBCONST void* vevp, void*)
{
    std::unique_lock<std::mutex> lock(cblock);

    // All event types have a SID field
    const char *sid = UpnpEvent_get_SID_cstr((UpnpEvent*)vevp);
    
    LOGDEB0("Service:srvCB: " << evTypeAsString(et) << " SID " << sid << "\n");

    auto it = o_calls.find(sid);
    if (it == o_calls.end()) {
        LOGINF("Service::srvCB: no callback found for SID " << sid << "\n");
    }

    switch (et) {
    case UPNP_EVENT_AUTORENEWAL_FAILED:
    {
        if (it != o_calls.end()) {
            it->second({});
        }
        break;
    }
    case UPNP_EVENT_RECEIVED:
    {
        UpnpEvent *evp = (UpnpEvent *)vevp;
        LOGDEB1("Service:srvCB: var change event: SID " << sid << " EventKey " <<
                UpnpEvent_get_EventKey(evp) << " changed " <<
                SoapHelp::argsToStr(evp->ChangedVariables.begin(),
                                    evp->ChangedVariables.end()) << "\n");

        if (it != o_calls.end()) {
            (it->second)(evp->ChangedVariables);
        }
        break;
    }

    default:
        LOGDEB("Service:srvCB: unprocessed evt type: [" << evTypeAsString(et) << "]"  << "\n");
        break;
    }

    return UPNP_E_SUCCESS;
}

// Only actually does something on the first call, to register our
// (static) library callback
static bool initEvents()
{
    LOGDEB1("Service::initEvents" << "\n");

    std::unique_lock<std::mutex> lock(cblock);
    
    static bool eventinit(false);
    if (eventinit)
        return true;
    eventinit = true;

    LibUPnP *lib = LibUPnP::getLibUPnP();
    if (lib == 0) {
        LOGERR("Service::initEvents: Can't get lib" << "\n");
        return false;
    }
    lib->m->registerHandler(UPNP_EVENT_AUTORENEWAL_FAILED, srvCB, 0);
    lib->m->registerHandler(UPNP_EVENT_RECEIVED, srvCB, 0);
    return true;
}

bool Service::Internal::subscribe()
{
    LOGDEB1("Service::subscribe: " << eventURL << "\n");
    LibUPnP* lib = LibUPnP::getLibUPnP();
    if (lib == 0) {
        LOGINF("Service::subscribe: no lib" << "\n");
        return false;
    }
    int timeout = lib->m->getSubsTimeout();
    int ret = UpnpSubscribe(lib->m->getclh(), eventURL.c_str(), &timeout, SID);
    if (ret != UPNP_E_SUCCESS) {
        LOGERR("Service:subscribe: " << eventURL << " failed: " << ret << " : " <<
               UpnpGetErrorMessage(ret) << "\n");
        return false;
    }
    LOGDEB("Service::subscribe:   " << eventURL << " SID " << SID << "\n");
    return true;
}

bool Service::Internal::unSubscribe()
{
    LOGDEB1("Service::unSubs: " << m->eventURL << " SID " << m->SID << "\n");
    LibUPnP* lib = LibUPnP::getLibUPnP();
    if (lib == 0) {
        LOGINF("Service::unSubscribe: no lib" << "\n");
        return false;
    }
    if (!SID.empty()) {
        int ret = UpnpUnSubscribe(lib->m->getclh(), SID);
        if (ret != UPNP_E_SUCCESS) {
            LOGERR("Service:unSubscribe: failed: " << ret << " : " <<
                   UpnpGetErrorMessage(ret) << " for SID [" << SID << "]\n");
            return false;
        }
        // Let the caller erase m->SID[] because there may be other
        // cleanup to do, based on its value
    }
    return true;
}

bool Service::registerCallback(evtCBFunc c)
{
    if (!m || !m->subscribe()) {
        LOGERR("registerCallback: subscribe failed\n");
        return false;
    }
    std::unique_lock<std::mutex> lock(cblock);
    LOGDEB1("Service::registerCallback: " << m->eventURL << " SID " << m->SID << "\n");
    o_calls[m->SID] = c;
    return true;
}

void Service::unregisterCallback()
{
    LOGDEB1("Service::unregisterCallback: " << m->eventURL << " SID " << m->SID << "\n");
    if (!m->SID.empty()) {
        m->unSubscribe();
        std::unique_lock<std::mutex> lock(cblock);
        o_calls.erase(m->SID);
        m->SID.clear();
    }
}

VarEventReporter *Service::getReporter()
{
    if (m)
        return m->reporter;
    return nullptr;
}

void Service::installReporter(VarEventReporter* reporter)
{
    if (reporter) {
        registerCallback();
    } else {
        unregisterCallback();
    }
    m->reporter = reporter;
}

bool Service::reSubscribe()
{
    LOGDEB("Service::reSubscribe()\n");
    if (m->SID.empty()) {
        LOGINF("Service::reSubscribe: no subscription (null SID)\n");
        return false;
    }
    evtCBFunc c;
    {
        std::unique_lock<std::mutex> lock(cblock);
        auto it = o_calls.find(m->SID);
        if (it == o_calls.end()) {
            LOGINF("Service::reSubscribe: no callback found for m->SID " << m->SID << "\n");
            return false;
        }
        c = it->second;
    }
    unregisterCallback();
    registerCallback(c);
    return true;
}

template int Service::runSimpleAction<int>(std::string const&, std::string const&, int, 
                                           ActionOptions *opts=nullptr);
template int Service::runSimpleAction<std::string>(std::string const&, std::string const&,
                                                   std::string, ActionOptions *opts=nullptr);
template int Service::runSimpleGet<int>(std::string const&, std::string const&, int*, 
                                        ActionOptions *opts=nullptr);
template int Service::runSimpleGet<bool>(std::string const&, std::string const&, bool*, 
                                         ActionOptions *opts=nullptr);
template int Service::runSimpleAction<bool>(std::string const&, std::string const&, bool, 
                                            ActionOptions *opts=nullptr);
template int Service::runSimpleGet<std::string>(std::string const&,std::string const&,std::string*,
                                           ActionOptions *opts=nullptr);

}
