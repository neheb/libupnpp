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

#include <upnp.h>

#include <time.h>

#include <string>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <utility>

#include "config.h"
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
    std::string name;
    std::unordered_map<std::string, std::string> args;
};

class SoapOutgoing::Internal {
public:
    Internal() = default;
    Internal(const std::string& st, const std::string& nm)
        : serviceType(st), name(nm) {}
    std::string serviceType;
    std::string name;
    std::vector<std::pair<std::string, std::string> > data;
};

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

/** Sanitize URL which is supposedly already encoded but maybe not fully */
std::string reSanitizeURL(const std::string& in);

/** Translate libupnp event type to string */
extern std::string evTypeAsString(Upnp_EventType);

class LibUPnP::Internal {
public:

    int getSubsTimeout();
    bool reSanitizeURLs();
    
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
    static int init_error;
    UpnpClient_Handle clh;
    std::mutex mutex;
    std::map<Upnp_EventType, Handler> handlers;
};

} // namespace

#endif /* _UPNPP_H_X_INCLUDED_ */
