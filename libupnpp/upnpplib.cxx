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
#define  LIBUPNPP_NEED_PACKAGE_VERSION
#include "libupnpp/config.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#ifdef __MACH__
#include <mach/clock.h>
#include <mach/mach.h>
#endif

#include "libupnpp/upnpp_p.hxx"

#include <upnp/upnp.h>
#include <upnp/upnptools.h>
#include <upnp/upnpdebug.h>

#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "libupnpp/getsyshwaddr.h"
#include "libupnpp/log.hxx"
#include "libupnpp/md5.h"
#include "libupnpp/upnpputils.hxx"

using namespace std;

#define CBCONST const

namespace UPnPP {

static LibUPnP *theLib;

LibUPnP *LibUPnP::getLibUPnP(bool serveronly, string* hwaddr,
                             const string ifname, const string ip,
                             unsigned short port)
{
    if (theLib == 0)
        theLib = new LibUPnP(serveronly, hwaddr, ifname, ip, port);
    if (theLib && !theLib->ok()) {
        delete theLib;
        theLib = 0;
        return 0;
    }
    return theLib;
}

static int o_callback(Upnp_EventType et, CBCONST void* evp, void* cookie)
{
    LibUPnP *ulib = (LibUPnP *)cookie;
    if (ulib == 0) {
        // Because the asyncsearch calls uses a null cookie.
        cerr << "o_callback: NULL ulib!" << endl;
        ulib = theLib;
    }
    LOGDEB1("LibUPnP::o_callback: event type: " << evTypeAsString(et) << endl);

    map<Upnp_EventType, LibUPnP::Internal::Handler>::iterator it =
        ulib->m->handlers.find(et);
    if (it != ulib->m->handlers.end()) {
        (it->second.handler)(et, evp, it->second.cookie);
    }
    return UPNP_E_SUCCESS;
}

string LibUPnP::versionString()
{
    // We would like to print libupnp version too, but there is no way to
    // get this at runtime, and printing the compile time version is wrong.
    return string("libupnpp ") + LIBUPNPP_PACKAGE_VERSION;
}

UpnpClient_Handle LibUPnP::Internal::getclh()
{
    return clh;
}

bool LibUPnP::ok() const
{
    return m->ok;
}

int LibUPnP::getInitError() const
{
    return m->init_error;
}

#if defined(HAVE_UPNPSETLOGLEVEL)
static const char* ccpDevNull = "/dev/null";
#endif

LibUPnP::LibUPnP(bool serveronly, string* hwaddr,
                 const string ifname, const string inip, unsigned short port)
{
    LOGDEB("LibUPnP: serveronly " << serveronly << " &hwaddr " << hwaddr <<
           " ifname [" << ifname << "] inip [" << inip << "] port " << port
           << endl);

    if ((m = new Internal()) == 0) {
        LOGERR("LibUPnP::LibUPnP: out of memory" << endl);
        return;
    }

    m->ok = false;

    // If our caller wants to retrieve an ethernet address (typically
    // for uuid purposes), or has specified an interface we have to
    // look at the network config.
    const int ipalen(100);
    char ip_address[ipalen];
    ip_address[0] = 0;
    if (hwaddr || !ifname.empty()) {
        char mac[20];
        if (getsyshwaddr(ifname.c_str(), ip_address, ipalen, mac, 13,0,0) < 0) {
            LOGERR("LibUPnP::LibUPnP: failed retrieving addr" << endl);
            return;
        }
        if (hwaddr)
            *hwaddr = string(mac);
    }

    // If the interface name was not specified, we possibly use the
    // supplied IP address. If this is empty too, libupnp will choose
    // by itself (the first adapter).
    if (ifname.empty())
        strncpy(ip_address, inip.c_str(), ipalen-1);

    // Work around a bug in some releases of libupnp 1.8: when
    // compiled with the reuseaddr option, the lib does not try higher
    // ports if the initial one is busy. So do it ourselves if the
    // specified port is 0 (meaning default : 49152)
    int maxloop = (port == 0) ? 20 : 1;
    for (int i = 0; i < maxloop; i++) {
#ifdef UPNP_ENABLE_IPV6
        m->init_error = UpnpInit2(ifname.empty()?nullptr:ifname.c_str(), port);
#else
        m->init_error = UpnpInit(ip_address[0] ? ip_address : 0, port);
#endif
        if (m->init_error != UPNP_E_SOCKET_BIND) {
            break;
        }
        // bind() error: possibly increment and retry
        port = 49152 + i + 1;
    }

    if (m->init_error != UPNP_E_SUCCESS) {
        LOGERR(errAsString("UpnpInit", m->init_error) << endl);
        return;
    }
    setMaxContentLength(2000*1024);

    LOGDEB("LibUPnP: Using IP " << UpnpGetServerIpAddress() << " port " <<
           UpnpGetServerPort() << endl);

    // Client initialization is simple, just do it. Defer device
    // initialization because it's more complicated.
    if (serveronly) {
        m->ok = true;
    } else {
        m->init_error = UpnpRegisterClient(o_callback, (void *)this, &m->clh);

        if (m->init_error == UPNP_E_SUCCESS) {
            m->ok = true;
        } else {
            LOGERR(errAsString("UpnpRegisterClient", m->init_error) << endl);
        }
    }
}

string LibUPnP::host()
{
    const char *cp = UpnpGetServerIpAddress();
    if (cp)
        return cp;
    return string();
}

int LibUPnP::Internal::setupWebServer(const string& description,
                                      UpnpDevice_Handle *dvh)
{
    // If we send description as a string (UPNPREG_BUF_DESC), libupnp
    // wants config_baseURL to be set, and it will serve the edited
    // description (with URLBase added) from the root directory. Which
    // is not nice. So we specify it as an URL, pointing to the
    // version in the vdir: http://myip:myport/somedir/description.xml
    // This is called from device.cxx where the corresponding right
    // thing must be done of course.
    int res = UpnpRegisterRootDevice2(
#ifdef SETUP_DESCRIPTION_BY_BUFFER
        UPNPREG_BUF_DESC,
#else
        UPNPREG_URL_DESC,
#endif
        description.c_str(), description.size(), 
        0, /* config_baseURL */
        o_callback, (void *)theLib, dvh);

    if (res != UPNP_E_SUCCESS) {
        LOGERR("LibUPnP::setupWebServer(): " <<
               errAsString("UpnpRegisterRootDevice2", res) << " description " <<
               description << endl);
    }
    return res;
}

void LibUPnP::setMaxContentLength(int bytes)
{
    UpnpSetMaxContentLength(size_t(bytes));
}

bool LibUPnP::setLogFileName(const std::string& fn, LogLevel level)
{
#if defined(HAVE_UPNPSETLOGLEVEL)
    std::unique_lock<std::mutex> lock(m->mutex);
    if (level == LogLevelNone) {
        // Can't call UpnpCloseLog() ! This closes the FILEs without
        // any further precautions -> crashes
        UpnpSetLogFileNames(ccpDevNull, ccpDevNull);
        UpnpInitLog();
    } else {
        setLogLevel(level);
        // the old lib only kept a pointer !
        static string fnkeep(fn);
        UpnpSetLogFileNames(fnkeep.c_str(), fnkeep.c_str());
        // Because of the way upnpdebug.c is horribly coded, this
        // loses 2 FILEs every time it's called.
        int code = UpnpInitLog();
        if (code != UPNP_E_SUCCESS) {
            LOGERR(errAsString("UpnpInitLog", code) << endl);
            return false;
        }
    }
    return true;
#else
    return false;
#endif
}

bool LibUPnP::setLogLevel(LogLevel level)
{
#if defined(HAVE_UPNPSETLOGLEVEL)
    switch (level) {
    case LogLevelNone:
        // This does not exist directly in pupnp, so log to
        // /dev/null. SetLogFileName knows not to call us back in this
        // case...
        UpnpSetLogLevel(UPNP_CRITICAL);
        setLogFileName("", LogLevelNone);
        break;
    case LogLevelError:
        UpnpSetLogLevel(UPNP_CRITICAL);
        break;
    case LogLevelDebug:
        UpnpSetLogLevel(UPNP_ALL);
        break;
    }
    return true;
#else
    return false;
#endif
}

void LibUPnP::Internal::registerHandler(Upnp_EventType et, Upnp_FunPtr handler,
                                        void *cookie)
{
    std::unique_lock<std::mutex> lock(mutex);
    if (handler == 0) {
        handlers.erase(et);
    } else {
        Internal::Handler h(handler, cookie);
        handlers[et] = h;
    }
}

std::string LibUPnP::errAsString(const std::string& who, int code)
{
    std::ostringstream os;
    os << who << " :" << code << ": " << UpnpGetErrorMessage(code);
    return os.str();
}

LibUPnP::~LibUPnP()
{
    int error = UpnpFinish();
    if (error != UPNP_E_SUCCESS) {
        LOGINF("LibUPnP::~LibUPnP: " << errAsString("UpnpFinish", error)
               << endl);
    }
    delete m;
    m = 0;
    LOGDEB1("LibUPnP: done" << endl);
}

string LibUPnP::makeDevUUID(const std::string& name, const std::string& hw)
{
    string digest;
    MD5String(name, digest);
    // digest has 16 bytes of binary data. UUID is like:
    // f81d4fae-7dec-11d0-a765-00a0c91e6bf6
    // Where the last 12 chars are provided by the hw addr

    char uuid[100];
    sprintf(uuid, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%s",
            digest[0]&0xff, digest[1]&0xff, digest[2]&0xff, digest[3]&0xff,
            digest[4]&0xff, digest[5]&0xff,  digest[6]&0xff, digest[7]&0xff,
            digest[8]&0xff, digest[9]&0xff, hw.c_str());
    return uuid;
}


string evTypeAsString(Upnp_EventType et)
{
    switch (et) {
    case UPNP_CONTROL_ACTION_REQUEST:
        return "UPNP_CONTROL_ACTION_REQUEST";
    case UPNP_CONTROL_ACTION_COMPLETE:
        return "UPNP_CONTROL_ACTION_COMPLETE";
    case UPNP_CONTROL_GET_VAR_REQUEST:
        return "UPNP_CONTROL_GET_VAR_REQUEST";
    case UPNP_CONTROL_GET_VAR_COMPLETE:
        return "UPNP_CONTROL_GET_VAR_COMPLETE";
    case UPNP_DISCOVERY_ADVERTISEMENT_ALIVE:
        return "UPNP_DISCOVERY_ADVERTISEMENT_ALIVE";
    case UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE:
        return "UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE";
    case UPNP_DISCOVERY_SEARCH_RESULT:
        return "UPNP_DISCOVERY_SEARCH_RESULT";
    case UPNP_DISCOVERY_SEARCH_TIMEOUT:
        return "UPNP_DISCOVERY_SEARCH_TIMEOUT";
    case UPNP_EVENT_SUBSCRIPTION_REQUEST:
        return "UPNP_EVENT_SUBSCRIPTION_REQUEST";
    case UPNP_EVENT_RECEIVED:
        return "UPNP_EVENT_RECEIVED";
    case UPNP_EVENT_RENEWAL_COMPLETE:
        return "UPNP_EVENT_RENEWAL_COMPLETE";
    case UPNP_EVENT_SUBSCRIBE_COMPLETE:
        return "UPNP_EVENT_SUBSCRIBE_COMPLETE";
    case UPNP_EVENT_UNSUBSCRIBE_COMPLETE:
        return "UPNP_EVENT_UNSUBSCRIBE_COMPLETE";
    case UPNP_EVENT_AUTORENEWAL_FAILED:
        return "UPNP_EVENT_AUTORENEWAL_FAILED";
    case UPNP_EVENT_SUBSCRIPTION_EXPIRED:
        return "UPNP_EVENT_SUBSCRIPTION_EXPIRED";
    default:
        return "UPNP UNKNOWN EVENT";
    }
}

/////////////////////// Small global helpers

string caturl(const string& s1, const string& s2)
{
    string out(s1);
    if (out[out.size()-1] == '/') {
        if (s2[0] == '/')
            out.erase(out.size()-1);
    } else {
        if (s2[0] != '/')
            out.push_back('/');
    }
    out += s2;
    return out;
}

string baseurl(const string& url)
{
    string::size_type pos = url.find("://");
    if (pos == string::npos)
        return url;

    pos = url.find_first_of("/", pos + 3);
    if (pos == string::npos) {
        return url;
    } else {
        return url.substr(0, pos + 1);
    }
}

static void path_catslash(string &s) {
    if (s.empty() || s[s.length() - 1] != '/')
        s += '/';
}
string path_getfather(const string &s)
{
    string father = s;

    // ??
    if (father.empty())
        return "./";

    if (father[father.length() - 1] == '/') {
        // Input ends with /. Strip it, handle special case for root
        if (father.length() == 1)
            return father;
        father.erase(father.length()-1);
    }

    string::size_type slp = father.rfind('/');
    if (slp == string::npos)
        return "./";

    father.erase(slp);
    path_catslash(father);
    return father;
}
string path_getsimple(const string &s) {
    string simple = s;

    if (simple.empty())
        return simple;

    string::size_type slp = simple.rfind('/');
    if (slp == string::npos)
        return simple;

    simple.erase(0, slp+1);
    return simple;
}

template <class T> bool csvToStrings(const string &s, T &tokens)
{
    string current;
    tokens.clear();
    enum states {TOKEN, ESCAPE};
    states state = TOKEN;
    for (unsigned int i = 0; i < s.length(); i++) {
        switch (s[i]) {
        case ',':
            switch(state) {
            case TOKEN:
                tokens.insert(tokens.end(), current);
                current.clear();
                continue;
            case ESCAPE:
                current += ',';
                state = TOKEN;
                continue;
            }
            break;
        case '\\':
            switch(state) {
            case TOKEN:
                state=ESCAPE;
                continue;
            case ESCAPE:
                current += '\\';
                state = TOKEN;
                continue;
            }
            break;

        default:
            switch(state) {
            case ESCAPE:
                state = TOKEN;
                break;
            case TOKEN:
                break;
            }
            current += s[i];
        }
    }
    switch(state) {
    case TOKEN:
        tokens.insert(tokens.end(), current);
        break;
    case ESCAPE:
        return false;
    }
    return true;
}

//template bool csvToStrings<list<string> >(const string &, list<string> &);
template bool csvToStrings<vector<string> >(const string &, vector<string> &);
template bool csvToStrings<set<string> >(const string &, set<string> &);


bool stringToBool(const string& s, bool *value)
{
    if (s[0] == 'F' ||s[0] == 'f' ||s[0] == 'N' || s[0] == 'n' ||s[0] == '0') {
        *value = false;
    } else if (s[0] == 'T'|| s[0] == 't' ||s[0] == 'Y' ||s[0] == 'y' ||
               s[0] == '1') {
        *value = true;
    } else {
        return false;
    }
    return true;
}

static void takeadapt(void *tok, const char *name)
{
    vector<string>* ads = (vector<string>*)tok;
    ads->push_back(name);
}

bool getAdapterNames(vector<string>& names)
{
    return getsyshwaddr("!?#@", 0, 0, 0, 0, takeadapt, &names) == 0;
}

}
