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

#include "libupnpp/control/device.hxx"

#include "libupnpp/log.hxx"
#include "libupnpp/control/description.hxx"

using namespace std;

namespace UPnPClient {


class Device::Internal {
public:
    UPnPDeviceDesc desc;
};


Device::Device()
    : m(new Internal())
{
}

Device::Device(const UPnPDeviceDesc& desc)
    : m(new Internal())
{
    m->desc = desc;
}


const UPnPDeviceDesc *Device::desc() const
{
    return m ? &m->desc : nullptr;
}

}
