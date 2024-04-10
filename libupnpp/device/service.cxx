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

#include "service.hxx"
#include "device.hxx"

namespace UPnPProvider {

class UpnpService::Internal {
public:
    Internal(bool noevs)
        : noevents(noevs)
    {
    }
    std::string serviceType;
    std::string serviceId;
    std::string xmlfn;
    bool noevents;
    UpnpDevice* dev{0};
};

UpnpService::UpnpService(
    const std::string& stp, const std::string& sid, const std::string& xmlfn,
    UpnpDevice *dev, bool noevs)
    : m(new Internal(noevs))
{
    m->dev = dev;
    m->serviceType = stp;
    m->serviceId = sid;
    m->xmlfn = xmlfn;
    dev->addService(this);
}

UpnpDevice *UpnpService::getDevice()
{
    return m ? m->dev : nullptr;
}

UpnpService::~UpnpService()
{
    if (m && m->dev)
        m->dev->forgetService(m->serviceId);
    delete m;
    m = nullptr;
}

bool UpnpService::noevents() const
{
    return m && m->noevents;
}

bool UpnpService::getEventData(bool, std::vector<std::string>&,
                               std::vector<std::string>&)
{
    return true;
}

const std::string& UpnpService::getServiceType() const
{
    return m->serviceType;
}

const std::string& UpnpService::getServiceId() const
{
    return m->serviceId;
}

const std::string& UpnpService::getXMLFn() const
{
    return m->xmlfn;
}

const std::string UpnpService::errString(int error) const
{
    switch (error) {
    case UPNP_INVALID_ACTION: return "Invalid Action";
    case UPNP_INVALID_ARGS: return "Invalid Arguments";
    case UPNP_INVALID_VAR: return "Invalid Variable";
    case UPNP_ACTION_CONFLICT: return "Action Conflict";
    case UPNP_ACTION_FAILED: return "Action Failed";
    case UPNP_ARG_VALUE_INVALID: return "Arg Value Invalid";
    case UPNP_ARG_VALUE_OUT_OF_RANGE: return "Arg Value Out Of Range";
    case UPNP_OPTIONAL_ACTION_NOT_IMPLEMENTED:
        return "Optional Action Not Implemented";
    case UPNP_OUT_OF_MEMORY: return "Out Of MEMORY";
    case UPNP_HUMAN_INTERVENTION_REQUIRED: return "Human Intervention Required";
    case UPNP_STRING_ARGUMENT_TOO_LONG: return "String Argument Too Long";
    case UPNP_ACTION_NOT_AUTHORIZED: return "Action Not Authorized";
    case UPNP_SIGNATURE_FAILING: return "Signature Failing";
    case UPNP_SIGNATURE_MISSING: return "Signature Missing";
    case UPNP_NOT_ENCRYPTED: return "Not Encrypted";
    case UPNP_INVALID_SEQUENCE: return "Invalid Sequence";
    case UPNP_INVALID_CONTROL_URLS: return "Invalid Control URLS";
    case UPNP_NO_SUCH_SESSION: return "No Such Session";
    default:
        break;
    }
    return serviceErrString(error);
}

} // End namespace UPnPProvider
