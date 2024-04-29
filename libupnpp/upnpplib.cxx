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
#define  LIBUPNPP_NEED_PACKAGE_VERSION
#include "config.h"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <cstdarg>
#include <algorithm>

#ifdef __MACH__
#include <mach/clock.h>
#include <mach/mach.h>
#endif

#include "libupnpp/upnpp_p.hxx"

#include <upnp.h>
#include <upnptools.h>
#include <upnpdebug.h>
#include <netif.h>

#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "libupnpp/log.hxx"
#include "libupnpp/md5.h"
#include "libupnpp/upnpputils.hxx"
#include "libupnpp/smallut.h"

using namespace std;

#define CBCONST const

namespace UPnPP {

static LibUPnP *theLib;

struct UPnPOptions {
    unsigned int flags;
    std::string ifnames;
    std::string ipv4;
    int port;
    int substimeout{1800};
    int subsopstimeoutms{-1};
    std::string clientproduct;
    std::string clientversion;
    std::string resanitizedchars{R"raw(!$'()+,)raw"};
    int bootid{-1};
};
static UPnPOptions options;

bool LibUPnP::init(unsigned int flags, ...)
{
    va_list ap;

    if (nullptr != theLib) {
        std::cerr << "LibUPnP::init: lib already initialized\n";
        return false;
    }
    options.flags = flags;

    va_start(ap, flags);
    for (;;) {
        int option = static_cast<InitOption>(va_arg(ap, int));
        if (option == UPNP_OPTION_END) {
            break;
        }
        switch (option) {
        case UPNPPINIT_OPTION_IFNAMES:
            options.ifnames = *((std::string*)(va_arg(ap, std::string*)));
            break;
        case UPNPPINIT_OPTION_IPV4:
            options.ipv4 = *((std::string*)(va_arg(ap, std::string*)));
            break;
        case UPNPPINIT_OPTION_PORT:
            options.port = va_arg(ap, int);
            break;
        case UPNPPINIT_OPTION_SUBSCRIPTION_TIMEOUT:
            options.substimeout = va_arg(ap, int);
            break;
        case UPNPPINIT_OPTION_BOOTID:
            options.bootid = va_arg(ap, int);
            break;
        case UPNPPINIT_OPTION_SUBSOPS_TIMEOUTMS:
            options.subsopstimeoutms = va_arg(ap, int);
            break;
        case UPNPPINIT_OPTION_CLIENT_PRODUCT:
            options.clientproduct = *((std::string*)(va_arg(ap, std::string*)));
            break;
        case UPNPPINIT_OPTION_CLIENT_VERSION:
            options.clientversion = *((std::string*)(va_arg(ap, std::string*)));
            break;
        case UPNPPINIT_OPTION_RESANITIZED_CHARS:
        {
            auto val = *((std::string*)(va_arg(ap, std::string*)));
            if (!val.empty()) {
                options.resanitizedchars = val;
            }
            break;
        }
        default:
            std::cerr << "LibUPnP::init: unknown option value " << option <<"\n";
        }
    }
    if (!options.ipv4.empty() && !options.ifnames.empty()) {
        std::cerr << "LibUPnP::init: can't set both ifnames and ipv4\n";
        return false;
    }
    theLib = new LibUPnP();
    return nullptr != theLib;
}

LibUPnP *LibUPnP::getLibUPnP(
    bool serveronly, string* hwaddr, const string ifname, const string ip, unsigned short port)
{
    if (nullptr == theLib) {
        init(serveronly?UPNPPINIT_FLAG_SERVERONLY:0,
             UPNPPINIT_OPTION_IFNAMES, &ifname,
             UPNPPINIT_OPTION_IPV4, &ip,
             UPNPPINIT_OPTION_PORT, int(port),
             UPNPPINIT_OPTION_END);
        if (nullptr != theLib && nullptr != hwaddr) {
            *hwaddr = theLib->hwaddr();
        }
    }
    if (theLib && !theLib->ok()) {
        delete theLib;
        theLib = nullptr;
    }
    return theLib;
}

static int o_callback(Upnp_EventType et, CBCONST void* evp, void* cookie)
{
    LibUPnP *ulib = (LibUPnP *)cookie;
    if (ulib == 0) {
        // Because the asyncsearch calls uses a null cookie.
        cerr << "o_callback: NULL ulib!" << '\n';
        ulib = theLib;
    }
    LOGDEB1("LibUPnP::o_callback: event type: " << evTypeAsString(et) << endl);

    map<Upnp_EventType, LibUPnP::Internal::Handler>::iterator it =
        ulib->m->handlers.find(et);
    if (it != ulib->m->handlers.end()) {
        return (it->second.handler)(et, evp, it->second.cookie);
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
    return m && m->ok;
}

int LibUPnP::Internal::init_error;

int LibUPnP::getInitError()
{
    return Internal::init_error;
}

std::string LibUPnP::hwaddr()
{
    std::string addr;
    auto *ifs = NetIF::Interfaces::theInterfaces();
    if (ifs) {
        NetIF::Interfaces::Filter filt;
        filt.needs = {NetIF::Interface::Flags::HASIPV4};
        filt.rejects = {NetIF::Interface::Flags::LOOPBACK};
        auto vifs = ifs->select(filt);
        if (!vifs.empty()) {
            addr = hexprint(vifs[0].gethwaddr());
        }
    }
    if (addr.empty()) {
        LOGERR("LibUPnP: could not retrieve network hardware address\n");
    }
    return addr;
}

int LibUPnP::Internal::getSubsTimeout()
{
    return options.substimeout;
}

bool LibUPnP::Internal::reSanitizeURLs()
{
    return options.flags & UPNPPINIT_FLAG_RESANITIZE_URLS;
}

LibUPnP::LibUPnP()
{
    bool serveronly = 0 != (options.flags&UPNPPINIT_FLAG_SERVERONLY);
    LOGDEB("LibUPnP: serveronly " << serveronly << " ifnames [" << options.ifnames << "] inip [" <<
           options.ipv4 << "] port " << options.port << '\n');

    if ((m = new Internal()) == 0) {
        LOGERR("LibUPnP::LibUPnP: out of memory" << '\n');
        return;
    }

    m->ok = false;

    if (options.ifnames.empty() && !options.ipv4.empty()) {
        // If the interface name was not specified, and the IP address
        // is supplied, call the old (n)pupnp init method.
        m->init_error = UpnpInit(options.ipv4.c_str(), options.port);
    } else {
        // We use a short wait if serveronly is not set. This is not
        // correct really, what we'd want to test is something like
        // clientonly, or better, this should really be an explicit
        // option of the lib. Solves upplay init for now anyway.
        int flags = 0;
        if (0 == (options.flags & UPNPPINIT_FLAG_NOIPV6)) {
            flags |= UPNP_FLAG_IPV6;
        }        
        m->init_error = UpnpInitWithOptions(
            options.ifnames.c_str(), options.port, flags,
            UPNP_OPTION_NETWORK_WAIT, serveronly ? 60 : 1,
            UPNP_OPTION_BOOTID, options.bootid,
            UPNP_OPTION_END);
    }
    if (m->init_error != UPNP_E_SUCCESS) {
        LOGERR(errAsString("UpnpInit", m->init_error) << '\n');
        return;
    }
    setMaxContentLength(2000*1024);

#ifdef UPNP_ENABLE_IPV6
    LOGINF("LibUPnP: Using IPV4 " << UpnpGetServerIpAddress() << " port " << UpnpGetServerPort() <<
           " IPV6 " << UpnpGetServerIp6Address() << " port " << UpnpGetServerPort6() << '\n');
#else
    LOGINF("LibUPnP: Using IPV4 " << UpnpGetServerIpAddress() << " port " <<
           UpnpGetServerPort() << '\n');
#endif

    // Client initialization is simple, just do it. Defer device
    // initialization because it's more complicated.
    if (serveronly) {
        m->ok = true;
    } else {
        m->init_error = UpnpRegisterClient(o_callback, (void *)this, &m->clh);

        if (m->init_error == UPNP_E_SUCCESS) {
#if NPUPNP_VERSION >= 40100
            if (!options.clientproduct.empty()&&!options.clientversion.empty()) {
                UpnpClientSetProduct(m->clh, options.clientproduct.c_str(),
                                     options.clientversion.c_str());
            }
#endif
#if NPUPNP_VERSION >= 50003
            if (options.subsopstimeoutms > 0) {
                UpnpSubsOpsTimeoutMs(m->clh, options.subsopstimeoutms);
            }
#endif
            m->ok = true;
        } else {
            LOGERR(errAsString("UpnpRegisterClient", m->init_error) << '\n');
        }
    }
}

bool LibUPnP::setWebServerDocumentRoot(const std::string& rootpath)
{
    return UpnpSetWebServerRootDir(rootpath.c_str()) == UPNP_E_SUCCESS;
}

string LibUPnP::host()
{
    const char *cp = UpnpGetServerIpAddress();
    if (cp)
        return cp;
    return string();
}

string LibUPnP::port()
{
    unsigned short prt = UpnpGetServerPort();
    return ulltodecstr(prt);
}

int LibUPnP::Internal::setupWebServer(const string& description, UpnpDevice_Handle *dvh)
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
        LOGERR("LibUPnP::setupWebServer(): " << errAsString("UpnpRegisterRootDevice2", res) <<
               " description " << description << '\n');
    }
    return res;
}

void LibUPnP::setMaxContentLength(int bytes)
{
    UpnpSetMaxContentLength(size_t(bytes));
}

bool LibUPnP::setLogFileName(const std::string& fn, LogLevel level)
{
    setLogLevel(level);
    UpnpSetLogFileNames(fn.c_str(), "");
    int code = UpnpInitLog();
    if (code != UPNP_E_SUCCESS) {
        LOGERR(errAsString("UpnpInitLog", code) << '\n');
        return false;
    }
    return true;
}

bool LibUPnP::setLogLevel(LogLevel level)
{
    UpnpSetLogLevel(Upnp_LogLevel(level));
    return true;
}

void LibUPnP::Internal::registerHandler(Upnp_EventType et, Upnp_FunPtr handler, void *cookie)
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
        LOGINF("LibUPnP::~LibUPnP: " << errAsString("UpnpFinish", error) << '\n');
    }
    delete m;
    m = 0;
    LOGDEB1("LibUPnP: done" << '\n');
}

string LibUPnP::makeDevUUID(const std::string& name, const std::string& hw)
{
    string digest;
    MD5String(name, digest);
    // digest has 16 bytes of binary data. UUID is like:
    // f81d4fae-7dec-11d0-a765-00a0c91e6bf6
    // Where the last 12 chars are provided by the hw addr

    // Make very sure that there are no colons in the hw because this
    // trips some CPs (kazoo as usual...)
    std::string nhw;
    std::remove_copy(hw.begin(), hw.end(), std::back_inserter(nhw), ':');

    char uuid[100];
    sprintf(uuid, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%s",
            digest[0]&0xff, digest[1]&0xff, digest[2]&0xff, digest[3]&0xff,
            digest[4]&0xff, digest[5]&0xff,  digest[6]&0xff, digest[7]&0xff,
            digest[8]&0xff, digest[9]&0xff, nhw.c_str());
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

string caturl(const string& base, const string& rel)
{
    // If the url to be concatenated is actually absolute, return it
    if (rel.find("://") != string::npos)
        return rel;
    
    string out(base);
    if (out.back() == '/') {
        if (rel.front() == '/')
            out.pop_back();
    } else {
        if (rel.front() != '/')
            out.push_back('/');
    }
    out += rel;
    return out;
}

string baseurl(const string& url)
{
    string::size_type pos = url.find("://");
    if (pos == string::npos)
        return url;

    pos = url.find_first_of('/', pos + 3);
    if (pos == string::npos) {
        return url;
    } else {
        return url.substr(0, pos + 1);
    }
}

static void path_catslash(string &s) {
    if (s.empty() || s.back() != '/')
        s.push_back('/');
}
string path_getfather(const string &s)
{
    string father = s;

    // ??
    if (father.empty())
        return "./";

    if (father.back() == '/') {
        // Input ends with /. Strip it, handle special case for root
        if (father.length() == 1)
            return father;
        father.pop_back();
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
    for (char i : s) {
        switch (i) {
        case ',':
            switch(state) {
            case TOKEN:
                tokens.insert(tokens.end(), current);
                current.clear();
                continue;
            case ESCAPE:
                current.push_back(',');
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
                current.push_back('\\');
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
            current.push_back(i);
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


// Perform additional escaping on supposedly encoded URL.
//
// The UPnP standards specify that URLs should encoded according to RFC 2396
//
// Unfortunately, the set of characters that should or should not be encoded inside a path is very
// unclear and changed between the three URL RFCS: 1738, 2396, and the latest 3986.
//
// For example the "mark" characters are "unreserved" and "should not" be encoded according to
// rfc2396:  mark = "-" | "_" | "." | "!" | "~" | "*" | "'" | "(" | ")"
//
// So, among others, the single quote is "unreserved" in 1738 and 2396 (no quoting), but "reserved"
// in 3986 (should be quoted when found in data). All Python urrlib quote() versions escape it.
//
// In practise, unescaped single quotes do cause trouble with some renderers.
//
// Because they are most likely to cause confusion, this function checks/encodes the characters
// which were in the safe list in rfc 1738, but became reserved in rfc 3986. These are all part of
// the 3986 sub_delims list. 
// As an aside, wikipedia explicitely states:
//  https://en.wikipedia.org/wiki/URL_encoding: "Other characters in a URI must be percent-encoded."
// that any character that is neither reserved (and used as such) or unreserved must be
// percent-encoded, but I can't find this specified in the rfc, maybe it's somewhere...
//
// Hopefully, none of the gen_delims ones will be found unencoded because they were all reserved or
// unsafe in 1738. 
//
// None of the characters that we change are likely to be used as actual syntactic delimiters in
// UPnP URLs (hopefully).
//
// This function is used optionaly and not by default because encoding the * is causing trouble with
// some renderers (Yamaha), which then emit the URI change events with a decoded *, defeating string
// comparisons...
// 
// This code should probably be moved to into Control Points to be used on a per-renderer basis.
std::string reSanitizeURL(const std::string& in)
{
    std::string out;
    for (unsigned char c : in) {
        const char *h = "0123456789ABCDEF";
        if (c <= 0x20 || c >= 0x7f || options.resanitizedchars.find(c) != std::string::npos) {
            out.push_back('%');
            out.push_back(h[(c >> 4) & 0xf]);
            out.push_back(h[c & 0xf]);
        } else {
            out.push_back(char(c));
        }
    }
    return out;
}

// Note: this differs from smallut stringToBool. Check one day if this is necessary ?
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

bool getAdapterNames(vector<string>& names)
{
    auto *ifs = NetIF::Interfaces::theInterfaces();
    if (ifs) {
        NetIF::Interfaces::Filter filt;
        filt.needs = {NetIF::Interface::Flags::HASIPV4};
        filt.rejects = {NetIF::Interface::Flags::LOOPBACK};
        auto vifs = ifs->select(filt);
        for (const auto& adapter : vifs) {
            names.push_back(adapter.getfriendlyname());
        }
        return true;
    }
    return false;
}

}
