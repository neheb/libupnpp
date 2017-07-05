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
#ifndef _TYPEDSERVICE_H_X_INCLUDED_
#define _TYPEDSERVICE_H_X_INCLUDED_
#include "libupnpp/config.h"

#include <string>
#include <vector>
#include <map>

#include "libupnpp/control/service.hxx"

namespace UPnPClient {

class TypedService : public Service {
public:
    TypedService(const std::string& tp);
    virtual ~TypedService();
    virtual bool serviceTypeMatch(const std::string& tp);

    /**
       @param[outgoing] retdata a map was used (instead of
       unordered..) for swig 2.0 compatibility.
    */
    virtual int runAction(const std::string& name,
                          std::vector<std::string> args,
                          std::map<std::string, std::string>& retdata);

protected:
    virtual bool serviceInit(const UPnPDeviceDesc& device,
                             const UPnPServiceDesc& service);


private:
    class Internal;
    Internal *m{0};
    TypedService();
};


extern TypedService *findTypedService(const std::string& devname,
                                      const std::string& servicetype,
                                      bool fuzzy);

} // namespace UPnPClient

#endif // _TYPEDSERVICE_H_X_INCLUDED_
