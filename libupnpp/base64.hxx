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
#ifndef _BASE64_H_INCLUDED_
#define _BASE64_H_INCLUDED_
#include <string>

#include "upnppexports.hxx"

namespace UPnPP {

void UPNPP_API base64_encode(const std::string& in, std::string& out);
bool UPNPP_API base64_decode(const std::string& in, std::string& out);
inline std::string base64_encode(const std::string& in)
{
    std::string o;
    base64_encode(in, o);
    return o;
}
inline std::string base64_decode(const std::string& in)
{
    std::string o;
    if (base64_decode(in, o))
        return o;
    return std::string();
}

} // namespace

#endif /* _BASE64_H_INCLUDED_ */
