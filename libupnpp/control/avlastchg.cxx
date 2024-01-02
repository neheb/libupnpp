/* Copyright (C) 2006-2023 J.F.Dockes
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
#include "libupnpp/config.h"

#include <string.h>

#include "libupnpp/control/avlastchg.hxx"
#include "libupnpp/expatmm.h"
#include "libupnpp/log.h"

using namespace std;

namespace UPnPClient {


class LastchangeParser : public inputRefXMLParser {
public:
    LastchangeParser(const string& input, std::unordered_map<string,string>& props)
        : inputRefXMLParser(input), m_props(props) {}

protected:
    virtual void StartElement(const XML_Char *name, const XML_Char **attrs) {
        if (!strcmp(name, "Event"))
            return;
        LOGDEB1("LastchangeParser: " << name << "\n");
        std::string value, channel;
        for (int i = 0; attrs[i] != 0; i += 2) {
            LOGDEB1("    " << attrs[i] << " -> " << attrs[i+1] << "\n");
            if (!strcmp("val", attrs[i])) {
                value = attrs[i+1];
            } else if (!strcmp("channel", attrs[i])) {
                channel = attrs[i+1];
            }
        }
        if (channel.empty() || "Master" == channel) {
            m_props[name] = value;
        } else {
            m_props[std::string(name) + "-" + channel] = value;
        }
    }
private:
    std::unordered_map<string, string>& m_props;
};


bool decodeAVLastChange(const string& xml, std::unordered_map<string, string>& props)
{
    LastchangeParser mparser(xml, props);
    if (!mparser.Parse())
        return false;
    return true;
}


}
