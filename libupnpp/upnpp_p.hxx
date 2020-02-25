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
#ifndef _UPNPP_H_X_INCLUDED_
#define _UPNPP_H_X_INCLUDED_

/* Private shared defs for the library. Clients need not and should
   not include this */
#include <sys/types.h>

#include <upnp/upnp.h>

typedef struct _IXML_Document IXML_Document;

#define PUPNP_AT_LEAST(A,B,C)                                           \
    (UPNP_VERSION_MAJOR > (A) ||                                        \
     (UPNP_VERSION_MAJOR == (A) &&                                      \
      (UPNP_VERSION_MINOR > (B) ||                                      \
       (UPNP_VERSION_MINOR == (B) && UPNP_VERSION_PATCH >= (C)))))

#if PUPNP_AT_LEAST(1,8,0) && !PUPNP_AT_LEAST(1,8,4)
#error PUPNP versions between 1.8.0 and 1.8.3 (included) do not work
#endif

#include <pthread.h>
#include <time.h>

#include <string>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <utility>

#include "libupnpp/config.h"
#include "libupnpp/upnpplib.hxx"
#include "libupnpp/soaphelp.hxx"

// define this for testing setting the description document as memory
// buffer Note: this does not work well for embedded devices (npupnp
// issue?), did not try to debug. Also does not work for multiple
// roots (would need a way to specify a different doc name).
#undef SETUP_DESCRIPTION_BY_BUFFER

namespace UPnPP {

class SoapIncoming::Internal {
public:
    /* Construct by decoding the XML passed from libupnp. Call ok() to check
     * if this went well.
     *
     * @param name We could get this from the XML doc, but get caller
     *    gets it from libupnp, and passing it is simpler than retrieving
     *    from the input top node where it has a namespace qualifier.
     * @param actReq the XML document containing the SOAP data.
     */
    bool decode(const char *name, IXML_Document *actReq);
    std::string name;
    std::unordered_map<std::string, std::string> args;
};

class SoapOutgoing::Internal {
public:
    /* Build the SOAP call or response data XML document from the
       vector of named values */
    IXML_Document *buildSoapBody(bool isResp = true) const;

    std::string serviceType;
    std::string name;
    std::vector<std::pair<std::string, std::string> > data;
};

/* Decode UPnP Event data. 
 *
 * In soaphelp.cxx: This is not soap, but it's quite close to
 * the other code in there.
 * The variable values are contained in a propertyset XML document:
 *     <?xml version="1.0"?>
 *     <e:propertyset xmlns:e="urn:schemas-upnp-org:event-1-0">
 *       <e:property>
 *         <variableName>new value</variableName>
 *       </e:property>
 *       <!-- Other variable names and values (if any) go here. -->
 *     </e:propertyset>
 */
extern bool decodePropertySet(IXML_Document *doc,
                              std::unordered_map<std::string, std::string>& out);


// Concatenate paths. Caller should make sure it makes sense.
extern std::string caturl(const std::string& s1, const std::string& s2);
// Return the scheme://host:port[/] part of input, or input if it is weird
extern std::string baseurl(const std::string& url);
extern std::string path_getfather(const std::string &s);
extern std::string path_getsimple(const std::string &s);
template <class T> bool csvToStrings(const std::string& s, T &tokens);

// @return false if s does not look like a bool at all (does not begin
// with [FfNnYyTt01]
extern bool stringToBool(const std::string& s, bool *v);

extern void timespec_now(struct timespec *ts);

/** Translate libupnp event type to string */
extern std::string evTypeAsString(Upnp_EventType);

class LibUPnP::Internal {
public:

    /** Specify function to be called on given UPnP
     *  event. The call will happen in the libupnp thread context.
     */
    void registerHandler(Upnp_EventType et, Upnp_FunPtr handler, void *cookie);

    // A Handler object records the data from registerHandler.
    class Handler {
    public:
        Handler()
            : handler(0), cookie(0) {}
        Handler(Upnp_FunPtr h, void *c)
            : handler(h), cookie(c) {}
        Upnp_FunPtr handler;
        void *cookie;
    };

    int setupWebServer(const std::string& description, UpnpDevice_Handle *dvh);

    UpnpClient_Handle getclh();
    
    bool ok;
    int  init_error;
    UpnpClient_Handle clh;
    std::mutex mutex;
    std::map<Upnp_EventType, Handler> handlers;
};

} // namespace

#endif /* _UPNPP_H_X_INCLUDED_ */
