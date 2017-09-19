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

#include "libupnpp/control/typedservice.hxx"

#include <string>
#include <utility>
#include <functional>
#include <condition_variable>
#include <chrono>

#include "libupnpp/control/discovery.hxx"
#include "libupnpp/control/description.hxx"
#include "libupnpp/log.hxx"
#include "libupnpp/smallut.h"
#include "libupnpp/soaphelp.hxx"


using namespace std;
using namespace std::placeholders;
using namespace UPnPP;

namespace UPnPClient {
class TypedService::Internal {
public:
    string servicetype;
    int version;
    UPnPServiceDesc::Parsed proto;
};

TypedService::TypedService(const string& tp)
{
    m = new Internal;
    string::size_type colon = tp.find_last_of(":");
    m->servicetype = tp.substr(0, colon);
    if (colon != string::npos && colon != tp.size() -1) {
        m->version = atoi(tp.substr(colon+1).c_str());
    } else {
        m->version = 0;
    }
    LOGDEB2("TypedService::TypedService: tp " << m->servicetype <<
            " version " << m->version << endl);
};

TypedService::~TypedService()
{
    delete m;
}

bool TypedService::serviceTypeMatch(const string& _tp)
{
    LOGDEB2("TypedService::serviceTypeMatch: [" << _tp << "]\n");
    string::size_type colon = _tp.find_last_of(":");
    string tp = _tp.substr(0, colon);
    int version = 0;
    if (colon != string::npos && colon != _tp.size() -1) {
        version = atoi(_tp.substr(colon+1).c_str());
    }
    return !m->servicetype.compare(tp) && m->version >= version;
}


bool TypedService::serviceInit(const UPnPDeviceDesc& device,
                               const UPnPServiceDesc& service)
{
    return service.fetchAndParseDesc(device.URLBase, m->proto);
}

int TypedService::runAction(const string& actnm, vector<string> args,
                            map<string, string>& data)
{
    auto it = m->proto.actionList.find(actnm);
    if (it == m->proto.actionList.end()) {
        LOGERR("TypedService::runAction: action [" << actnm << "] not found\n");
        return UPNP_E_INVALID_ACTION;
    }
    const auto& action = it->second;
    
    unsigned int outargcnt = 0, retargcnt = 0;
    for (const auto& arg : action.argList) {
        if (arg.todevice) {
            outargcnt++;
        } else {
            retargcnt++;
        }
    }
    if (outargcnt != args.size()) {
        LOGERR("TypedService::runAction: expected " << outargcnt <<
               " outgoing arguments, got " << args.size() << endl);
        return UPNP_SOAP_E_INVALID_ARGS;
    }
    SoapOutgoing soapargs(getServiceType(), actnm);
    for (unsigned int i = 0; i < args.size(); i++) {
        soapargs.addarg(action.argList[i].name, args[i]);
    }
    SoapIncoming sdata;
    int ret = Service::runAction(soapargs, sdata);
    if (ret != 0) {
        return ret;
    }
    unordered_map<string,string> retdata;
    sdata.getMap(retdata);
    data = map<string,string>(retdata.begin(), retdata.end());
    return UPNP_E_SUCCESS;
}

void TypedService::evtCallback(
    const std::unordered_map<std::string, std::string>& props)
{
    VarEventReporter *reporter = getReporter();
    LOGDEB1("TypedService::evtCallback: reporter: " << reporter << endl);
    for (const auto& ent : props) {
        if (!reporter) {
            LOGDEB1("TypedService::evtCallback: " << ent.first << " -> "
                    << ent.second << endl);
        } else {
            reporter->changed(ent.first.c_str(), ent.second.c_str());
        }
    }
}

void TypedService::registerCallback()
{
    Service::registerCallback(bind(&TypedService::evtCallback, this, _1));
}

// Callback for the discovery service to inform us about the devices
// and services it knows or finds.
class DirCB {
public:
    DirCB(const string& dv, const string& tp, bool fz,
          std::condition_variable& cv)
        : dvname(dv),
          ldvname(stringtolower(dv)),
          stype((fz? stringtolower(tp) : tp)),
          fuzzy(fz), discocv(cv) {
    }
    string dvname;
    string ldvname;
    string stype;
    bool fuzzy;
    std::condition_variable& discocv;
    UPnPDeviceDesc founddev;
    UPnPServiceDesc foundserv;
    
    bool visit(const UPnPDeviceDesc& dev, const UPnPServiceDesc& serv) {
        LOGDEB2("findTypedService:visit: got " << dev.friendlyName << " " <<
               dev.UDN << " " << serv.serviceType << endl);
        bool matched = !dev.UDN.compare(dvname) ||
            !stringlowercmp(ldvname, dev.friendlyName);
        if (matched) {
            if (fuzzy) {
                string ltp = stringtolower(serv.serviceType);
                matched = matched &&  (ltp.find(stype) != string::npos);
            } else {
                matched =  matched && !stype.compare(serv.serviceType);
            }
        }
        if (matched) {
            founddev = dev;
            foundserv = serv;
            discocv.notify_all();
        }
        // We return false if found, to stop the traversal. Success is
        // indicated by our data.
        return !matched;
    }
};

TypedService *findTypedService(const std::string& devname,
                               const std::string& servicetype,
                               bool fuzzy)
{
    UPnPDeviceDirectory *superdir = UPnPDeviceDirectory::getTheDir();
    if (superdir == 0) {
        LOGERR("Discovery init failed\n");
        return 0;
    }
    std::mutex discolock;
    std::condition_variable discocv;

    DirCB cb(devname, servicetype, fuzzy, discocv);
    UPnPDeviceDirectory::Visitor vis = bind(&DirCB::visit, &cb, _1, _2);

    {
        std::unique_lock<std::mutex> mylock(discolock);
        // Calls to vis() may occur *during* the addCallback()
        // call. We need to check if the device was found before going
        // into the wait loop.
        int callbackidx = superdir->addCallback(vis);
        if (cb.founddev.UDN.empty()) {
            int ms;
            while ((ms = superdir->getRemainingDelayMs()) > 100) {
                discocv.wait_for(mylock, std::chrono::milliseconds(ms));
                if (!cb.founddev.UDN.empty()) {
                    break;
                }
            }
        }
        superdir->delCallback(callbackidx);
    }

    if (cb.founddev.UDN.empty()) {
        LOGDEB("findTypedService: no luck with CB, traversing\n");
        // Not found during the timeout. Let's traverse as a last
        // ditch effort
        superdir->traverse(vis);
    }

    if (!cb.founddev.UDN.empty()) {
        TypedService *service = new TypedService(cb.foundserv.serviceType);
        service->initFromDescription(cb.founddev);
        // string sdesc = cb.founddev.dump();
        return service;
    }
    LOGDEB("Service not found: " << devname << "/" << servicetype <<
           " fuzzy " << fuzzy << endl);
    return 0;
}


}
