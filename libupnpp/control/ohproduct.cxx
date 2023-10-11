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
#include "libupnpp/control/ohproduct.hxx"

#include <string.h>
#include <upnp.h>
#include <ostream>
#include <string>
#include <vector>

#include "libupnpp/expatmm.h"
#include "libupnpp/log.hxx"
#include "libupnpp/soaphelp.hxx"
#include "libupnpp/upnpp_p.hxx"
#include "libupnpp/smallut.h"

using namespace std::placeholders;
using namespace UPnPP;

namespace UPnPClient {

// SourceList XML format
// <SourceList>
//    <Source>
//        <Name>[user name for source]</Name>
//        <Type>[Type of the source from closed list of Supported types]</Type>
//        <Visible>[Boolean.]</Visible>
//    </sourcetag>
// </SourceList>
// - Playlist - the av.openhome.org:Playlist:1 service must be available
// - Radio - the av.openhome.org:Radio:1 service must be available
// - Receiver - the av.openhome.org:Receiver:1 service must be available
// - UpnpAv - the upnp.org:MediaRenderer:1 device must be available
// - NetAux - Specifies 3rd party, non OpenHome controllable, network
//   protocols such as AirPlay
// - Analog - Specifies an analog external input
// - Digital - Specifies a digital external input
// - Hdmi - Specifies a HDMI external input
class OHSourceParser : public inputRefXMLParser {
public:
    OHSourceParser(const std::string& input, std::vector<OHProduct::Source>& sources)
        : inputRefXMLParser(input), m_sources(sources) {}

protected:
    virtual void EndElement(const XML_Char *name) {
        if (!strcmp(name, "Source")) {
            m_sources.push_back(m_tsrc);
            m_tsrc = OHProduct::Source();
        }
    }
    virtual void CharacterData(const XML_Char *s, int len) {
        if (s == 0 || *s == 0)
            return;
        std::string str(s, len);
        trimstring(str);
        switch (m_path.back().name[0]) {
        case 'N':
            if (m_path.back().name == "Name")
                m_tsrc.name = str;
            break;
        case 'T':
            if (m_path.back().name == "Type")
                m_tsrc.type = str;
            break;
        case 'V':
            if (m_path.back().name == "Visible")
                stringToBool(str, &m_tsrc.visible);
            break;
        }
    }

private:
    std::vector<OHProduct::Source>& m_sources;
    OHProduct::Source m_tsrc;
};

const std::string OHProduct::SType("urn:av-openhome-org:service:Product:1");

// Check serviceType string (while walking the descriptions. We don't
// include a version in comparisons, as we are satisfied with version1
bool OHProduct::isOHPrService(const std::string& st)
{
    const std::string::size_type sz(SType.size()-2);
    return !SType.compare(0, sz, st, 0, sz);
}
bool OHProduct::serviceTypeMatch(const std::string& tp)
{
    return isOHPrService(tp);
}


void OHProduct::evtCallback(const std::unordered_map<std::string, std::string>& props)
{
    LOGDEB1("OHProduct::evtCallback: getReporter(): " << getReporter() << "\n");
    for (const auto& prop : props) {
        if (!getReporter()) {
            LOGDEB1("OHProduct::evtCallback: " << prop.first << " -> " << prop.second << "\n");
            continue;
        }
        if (prop.first == "SourceIndex") {
            getReporter()->changed(prop.first.c_str(), atoi(prop.second.c_str()));
        } else if (prop.first == "Standby") {
            bool val = false;
            stringToBool(prop.second, &val);
            getReporter()->changed(prop.first.c_str(), val ? 1 : 0);
        } else {
            LOGDEB1("OHProduct event: unknown variable: name [" <<
                    prop.first << "] value [" << prop.second << "\n");
            getReporter()->changed(prop.first.c_str(), prop.second.c_str());
        }
    }
}

void OHProduct::registerCallback()
{
    Service::registerCallback(bind(&OHProduct::evtCallback, this, _1));
}

int OHProduct::getSources(std::vector<Source>& sources)
{
    SoapOutgoing args(getServiceType(), "SourceXml");
    SoapIncoming data;
    int ret = runAction(args, data);
    if (ret != UPNP_E_SUCCESS) {
        return ret;
    }
    std::string sxml;
    if (!data.get("Value", &sxml)) {
        LOGERR("OHProduct:getSources: missing Value in response" << "\n");
        return UPNP_E_BAD_RESPONSE;
    }
    return parseSourceXML(sxml, sources) ? UPNP_E_SUCCESS : UPNP_E_BAD_RESPONSE;
}

bool OHProduct::parseSourceXML(std::string& sxml, std::vector<Source>& sources)
{
    OHSourceParser mparser(sxml, sources);
    if (!mparser.Parse())
        return false;
    return true;
}

int OHProduct::sourceIndex(int *index)
{
    std::string value;
    int ret;

    if ((ret = runSimpleGet("SourceIndex", "Value", &value)))
        return ret;

    *index = atoi(value.c_str());
    return 0;
}

int OHProduct::setSourceIndex(int index)
{
    return runSimpleAction("SetSourceIndex", "Value", index);
}

int OHProduct::setSourceIndexByName(const std::string& name)
{
    return runSimpleAction("SetSourceIndexByName", "Value", name);
}

int OHProduct::standby(bool *value)
{
    return runSimpleGet("Standby", "Value", value);
}

int OHProduct::setStanby(bool value)
{
    return runSimpleAction("SetStandby", "Value", value);
}


} // End namespace UPnPClient
