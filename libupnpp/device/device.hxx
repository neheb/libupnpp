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
#ifndef _DEVICE_H_X_INCLUDED_
#define _DEVICE_H_X_INCLUDED_
#include "libupnpp/config.h"

#include <pthread.h>
#include <upnp/upnp.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <map>

#include "libupnpp/soaphelp.hxx"

namespace UPnPP {
class LibUPnP;
}
namespace UPnPProvider {
class UpnpService;
}

namespace UPnPProvider {

typedef std::function<int (const UPnPP::SoapIncoming&, UPnPP::SoapOutgoing&)>
soapfun;

/** Definition of a virtual directory entry: data and MIME type */
struct VDirContent {
    VDirContent(const std::string& ct, const std::string& mt)
        : content(ct), mimetype(mt) {}
    std::string content;
    std::string mimetype;
};

/** 
 * Interface to link libupnp operations to a device implementation.
 */
class UpnpDevice {
public:
    /** Construct a device object. 
     *
     * The device is not started. This will be done by the startloop() or 
     * eventloop() call when everything is set up.
     *
     * @param deviceId uuid for device: "uuid:UUIDvalue"
     */
    UpnpDevice(const std::string& deviceId);
    ~UpnpDevice();

    /** Retrieve the network endpoint the server is listening on */
    static bool ipv4(std::string *host, unsigned short *port);

    /** Librarian utility: must be implemented for the services to retrieve
     *  their service definition XML files, 
     *
     * This is also used by the base class (with an empty name) to retrieve an 
     * XML text fragment to be added to the <device> node in the device 
     * description XML. E.G. things like <serialNumber>42</serialNumber>. 
     * *Mandatory*: deviceType and friendlyName *must* be in there.
     * 
     *  @param name the designator set in the service constructor 
     *         (e.g. AVTransport.xml). The base class uses an empty name to 
     *         retrieve the description properties (see above).
     *  @param[output] contents the output data.
     *  @return false for error.
     */
    virtual bool readLibFile(const std::string& name,
                             std::string& contents) = 0;

    /** Add file to virtual directory. Returns path in @param path */
    bool addVFile(const std::string& name, const std::string& contents,
                  const std::string& mime, std::string& path);

    /**
     * Main routine. To be called by main() on the root device when done 
     * with initialization.
     *
     * This loop mostly polls getEventData and generates an UPnP event if
     * there is anything to broadcast. The UPnP action calls happen in
     * other threads with which we synchronize, currently using a global lock.
     */
    void eventloop();

    /** 
     * Start a thread to run the event loop and return immediately. 
     *
     * This is an alternative to running eventloop() from
     * the main thread. The destructor will take care of the internal
     * thread.
     */
    void startloop();
    
    /**
     * To be called from a service action callback to wake up the
     * event loop early if something needs to be broadcast without
     * waiting for the normal delay.
     *
     * Will only do something if the previous event is not too recent.
     */
    void loopWakeup(); // To trigger an early event

    const std::string& getDeviceId() const;
    
    /**
     * To be called to get the event loop to return
     */
    void shouldExit();

    /** Check status */
    bool ok();


    /* *******************************************************************
     * Methods called by the service constructor to link the
     * service object and its methods to the Device one, establishing
     * the eventing and action communication. This is internal and
     * should not be called by the library user. Not too sure how this
     * could/should be expressed in C++... */
    
    /** Add service to our list. 
     *
     * We only ever keep one instance of a serviceId. Multiple calls 
     * will only keep the last one. This is called from the generic 
     * UpnpService constructor.
     */
    void addService(UpnpService *);
    void forgetService(const std::string& serviceId);

    /**
     * Add mapping from service+action-name to handler function.
     * This is called by the services implementations during their 
     * initialization.
     */
    void addActionMapping(const UpnpService*,
                          const std::string& actName, soapfun);

private:
    class Internal;
    Internal *m;
    class InternalStatic;
    static InternalStatic *o;
};

/**
 * Upnp service implementation class. This does not do much useful beyond
 * encapsulating the service actions and event callback. In most cases, the
 * services will need full access to the device state anyway.
 */
class UpnpService {
public:
    /**
     * The main role of the derived constructor is to register the
     * service action callbacks by calling UpnpDevice::addActionMapping(). 
     * The generic constructor registers the object with the device.
     *
     * @param noevents if set, the service will function normally except that
     *                 no calls will be made to libupnp to broadcast events.
     *                 This allows a service object to retain its possible
     *                 internal functions without being externally visible
     *                 (in conjunction with a description doc edit).
     */
    UpnpService(const std::string& stp,const std::string& sid,
                const std::string& xmlfn, UpnpDevice *dev, bool noevents=false);

    virtual ~UpnpService();

    UpnpDevice *getDevice();
    
    /**
     * Poll to retrieve evented data changed since last call (see
     * Device::eventLoop).
     *
     * To be implemented by the derived class if it does generate event data.
     * Also called by the library when a control point subscribes, to
     * retrieve eventable data.
     * Return name/value pairs for changed variables in the data arrays.
     *
     * @param all if true, treat all state variable as changed (return
     *            full state)
     * @param names names of returned state variable
     * @param values array parallel to names, holding the values.
     */
    virtual bool getEventData(bool all, std::vector<std::string>& names,
                              std::vector<std::string>& values);
    virtual const std::string& getServiceType() const;
    virtual const std::string& getServiceId() const;
    virtual const std::string& getXMLFn() const;

    /** Get value of the noevents property */
    bool noevents() const;

    /** 
     * Error number to string translation. UPnP error code values are
     * duplicated and mean different things for different services, so
     * this handles the common codes and calls serviceErrString which
     * should be overriden by the subclasses.
     */
    virtual const std::string errString(int error) const;

    virtual const std::string serviceErrString(int) const {
        return "Unknown error";
    }
    // Common (service-type-independant) error codes
    enum UPnPError {
        UPNP_INVALID_ACTION = 401,
        UPNP_INVALID_ARGS = 402,
        UPNP_INVALID_VAR = 404,
        UPNP_ACTION_FAILED = 501,

        /* 600-699 common action errors */
        UPNP_ARG_VALUE_INVALID = 600,
        UPNP_ARG_VALUE_OUT_OF_RANGE = 601,
        UPNP_OPTIONAL_ACTION_NOT_IMPLEMENTED = 602,
        UPNP_OUT_OF_MEMORY = 603,
        UPNP_HUMAN_INTERVENTION_REQUIRED = 604,
        UPNP_STRING_ARGUMENT_TOO_LONG = 605,
        UPNP_ACTION_NOT_AUTHORIZED = 606,
        UPNP_SIGNATURE_FAILING = 607,
        UPNP_SIGNATURE_MISSING = 608,
        UPNP_NOT_ENCRYPTED = 609,
        UPNP_INVALID_SEQUENCE = 610,
        UPNP_INVALID_CONTROL_URLS = 611,
        UPNP_NO_SUCH_SESSION = 612,
    };

    
private:
    class Internal;
    Internal *m;
};

} // End namespace UPnPProvider

#endif /* _DEVICE_H_X_INCLUDED_ */
