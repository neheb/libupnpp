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
#ifndef _LIBUPNP_H_X_INCLUDED_
#define _LIBUPNP_H_X_INCLUDED_

#include <map>
#include <string>

#include "upnppexports.hxx"

/** Version components. */
#define LIBUPNPP_VERSION_MAJOR 0
#define LIBUPNPP_VERSION_MINOR 26
#define LIBUPNPP_VERSION_REVISION 4
/// Got this from Xapian...
#define LIBUPNPP_AT_LEAST(A,B,C)                                        \
    (LIBUPNPP_VERSION_MAJOR > (A) ||                                    \
     (LIBUPNPP_VERSION_MAJOR == (A) &&                                  \
      (LIBUPNPP_VERSION_MINOR > (B) ||                                  \
       (LIBUPNPP_VERSION_MINOR == (B) && LIBUPNPP_VERSION_REVISION >= (C)))))

// Let clients avoid including upnp.h if they don't need it for other reasons
#ifndef UPNP_E_SUCCESS
#define UPNP_E_SUCCESS  0
#endif

// Feature test defines to help client programs, esp. for features
// added without breaking api and avi

// upnp/av connection manager control api 
#define LIBUPNPP_HAS_UPNPAVCONMAN 1

namespace UPnPP {

/** Our link to libupnp. Initialize and keep the handle around.
 * 
 * Not all applications will need to use this. The initialization call will be
 * performed internally as needed, you only need to call it if you need to set 
 * specific parameters.
 */
class UPNPP_API LibUPnP {
public:
    ~LibUPnP();

    LibUPnP(const LibUPnP&) = delete;
    LibUPnP& operator=(const LibUPnP&) = delete;

    /** Configuration flags for the initialisation call */
    enum InitFlags {
        UPNPPINIT_FLAG_NONE = 0,
        /** Disable IPV6 support */
        UPNPPINIT_FLAG_NOIPV6 = 0x1,
        /** Do not initialize the client side (we are a device) */
        UPNPPINIT_FLAG_SERVERONLY = 0x2,
        /** Perform extra escaping on URLs read from a content directory. This will encode some
            characters which have a tendancy to cause problems even if they do not *have* to be
            escaped according to the standard. Unfortunately, this also causes issues with some
            UPnP/AV renderers (Yamaha) which emit change event containing the (maybe partially)
            *decoded* URLs, which breaks string comparisons in the CP. This could be worked around
            in Upplay, but this also triggers issues inside other CPs, e.g. when Bubble server is
            proxying an UPnP/AV renderer. */
        UPNPPINIT_FLAG_RESANITIZE_URLS = 0x4,
    };

    /** Options for the initialisation call. Each option argument may be 
     * followed by specific parameters. */
    enum InitOption {
        /** Terminate the VARARGs list. */
        UPNPPINIT_OPTION_END = 0,
        /** Names of the interfaces to use. A const std::string* follows. 
         * This is a space-separated list. If not set, we will use the first interface. 
         * If set to '*', we will use all possible interfaces. Use an empty string to keep the
         * default setting. */
        UPNPPINIT_OPTION_IFNAMES,
        /** Use single IPV4 address. A const std::string* address in dot notation follows. 
         * This is incompatible with OPTION_IFNAMES. 
         * Use an empty string to keep the default value. */
        UPNPPINIT_OPTION_IPV4,
        /** IP Port to use. An int parameter follows. The lower lib default is 49152. */
        UPNPPINIT_OPTION_PORT,
        /** Control: subscription timeout in seconds. An int parameter follows. Default: 1800. */
        UPNPPINIT_OPTION_SUBSCRIPTION_TIMEOUT,
        /** Control: product name to set in user-agent strings.
         * A const std::string* follows. Use an empty string to keep the default. */
        UPNPPINIT_OPTION_CLIENT_PRODUCT,
        /** Control: product version to set in user-agent strings.
         * A const std::string* follows. Use an empty string to keep the default. */
        UPNPPINIT_OPTION_CLIENT_VERSION,
        /** Control: HTTP timeout for subscription/unsubscriptions connections to the device. 
         * The libupnp default of 30000 ms is very high. An int parameter follows. 
         * Use -1 to keep the default. */
        UPNPPINIT_OPTION_SUBSOPS_TIMEOUTMS,
        /** Control: characters that we should further url-escape when found in URLs out of a
            content directory. The default is <!$'()+,> (delimited by excluded <>) 
            A const std::string* follows. Use an empty string to keep the default. */
        UPNPPINIT_OPTION_RESANITIZED_CHARS,
        /** Device: UPnP 1.1 BOOTINIT.UPNP.ORG value. int value.
         * Use -1 to keep the constant default of 1 */
        UPNPPINIT_OPTION_BOOTID,
    };

    /** Initialize the library, with more complete control than a direct getLibUPnP() call.
     *
     * On success you will then call getLibUPnP() to access the object instance.
     * @param flags A bitfield of @ref InitFlags values.
     * @param ... A list of @ref InitOption and values, ended by UPNPPINIT_OPTION_END.
     * @return false for failure, true for success. 
     */
    static bool init(unsigned int flags, ...);

    /** Retrieve the singleton LibUPnP object.
     *
     * Using this call with arguments is deprecated. Call init() instead, (creates the lib) then 
     * getLibUPnP() without arguments.
     *
     * This initializes libupnp, possibly setting an address and port, possibly
     * registering a client if serveronly is false.
     *
     * @param serveronly no client init
     * @param[output] hwaddr returns the hardware address for the
     *   specified network interface, or the first one is ifname is
     *   empty. If the IP address is specified instead of the interface
     *   name, the hardware address returned is not necessarily the one
     *   matching the IP.
     * @param ifname if not empty, network interface to use. Passed to 
     *   libnpupnp. Null or empty to use the first interface, 
     *   "*" for all interfaces,
     *   or space-separated list of interface names.
     * @param ip if not empty, IP address to use. Only used if ifname is empty.
     * @param port port parameter to UpnpInit() (0 for default).
     * @return 0 for failure.
     */
    static LibUPnP* getLibUPnP(bool serveronly = false, std::string* hwaddr = 0,
                               const std::string ifname = std::string(),
                               const std::string ip = std::string(),
                               unsigned short port = 0);

    /** Return the IP v4 address as dotted notation */
    std::string host();
    /** Return the listening port */
    std::string port();
    /** Return one ethernet address. */
    std::string hwaddr();

    /** Set the root/base directory for the HTTP server.
     * This is for serving files directly from the file system. If this is not called, no local files
     * will be directly served, all data will have to go through the VDir interface.
     * @param rootpath the absolute path to a file system directory. The paths for all requests 
     * which do not match a virtual directory will be interpreted as subdirectories of rootpath.
     */
    bool setWebServerDocumentRoot(const std::string& rootpath);
    
    /** Set max library buffer size for reading content from servers. 
     * 
     * Just calls libupnp UpnpSetMaxContentLength(). This defines the maximum amount of data 
     * that Soap calls will accept from the remote. The libupnp default is very low (16KB), 
     * but libupnpp sets the value to 2MB during initialization. You can reset the value to your 
     * own choice by using this method.
     */
    void setMaxContentLength(int bytes);

    /** Check state after initialization */
    bool ok() const;

    /** Returns something like "libupnpp 0.14.0 libupnp x.y.z" */
    static std::string versionString();

    /** libnpupnp (pupnp) logging: this is distinct from libupnpp logging */
    /** libnpupnp log levels */
    enum LogLevel {LogLevelNone, LogLevelError, LogLevelInfo, LogLevelDebug, LogLevelAll};

    /** Set libnpupnp log file name and activate/deactivate logging.
     *
     * Do not use the same file as the one used for libupnpp logging.
     *
     * @param fn file name to use. Use an empty string log to stderr. 
     * @param level verbosity. Set to LogLevelNone to turn off.
     */
    static bool setLogFileName(const std::string& fn, LogLevel level = LogLevelError);

    /** Set libnpupnp log level. 
     *
     * @return true if logging is enabled, else false.
     */
    static bool setLogLevel(LogLevel level);

    /** Retrieve init error if state not ok */
    static int getInitError();

    /** Build a unique stable UUID. 
     *
     * This uses a hash of the input name (e.g.: friendlyName), and the Ethernet address.
     * @param name device "friendly name"
     * @param hw device ethernet address (as 12 ascii hexadecimal bytes)
     */
    static std::string makeDevUUID(const std::string& name, const std::string& hw);

    /** Translate libupnp integer error code (UPNP_E_XXX) to string */
    static std::string errAsString(const std::string& who, int code);

    class UPNPP_LOCAL Internal;
    Internal *m;

private:
    LibUPnP();
};

} // namespace UPnPP

#endif /* _LIBUPNP.H_X_INCLUDED_ */
