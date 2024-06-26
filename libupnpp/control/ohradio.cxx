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
#include "config.h"

#include "libupnpp/control/ohradio.hxx"

#include <cstdlib>
#include <cstring>
#include <upnp.h>

#include <functional>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "libupnpp/upnpavutils.hxx"
#include "libupnpp/control/cdircontent.hxx"
#include "libupnpp/control/service.hxx"
#include "libupnpp/log.hxx"
#include "libupnpp/expatmm.h"
#include "libupnpp/soaphelp.hxx"
#include "libupnpp/upnpp_p.hxx"

using namespace std;
using namespace std::placeholders;
using namespace UPnPP;

namespace UPnPClient {

const string OHRadio::SType("urn:av-openhome-org:service:Radio:1");

// Check serviceType string (while walking the descriptions. We don't
// include a version in comparisons, as we are satisfied with version1
bool OHRadio::isOHRdService(const string& st)
{
    const string::size_type sz(SType.size()-2);
    return !SType.compare(0, sz, st, 0, sz);
}
bool OHRadio::serviceTypeMatch(const std::string& tp)
{
    return isOHRdService(tp);
}

int OHRadio::decodeMetadata(const string& who, const string &didl, UPnPDirObject *dirent)
{
    UPnPDirContent dir;
    if (!dir.parse(didl)) {
        LOGERR("OHRadio::decodeMetadata: " << who << ": didl parse failed: " << didl << '\n');
        return UPNP_E_BAD_RESPONSE;
    }
    if (dir.m_items.size() != 1) {
        LOGERR("OHRadio::decodeMetadata: " << who << ": " << dir.m_items.size()
               << " items in response: [" << didl << "]" << '\n');
        return UPNP_E_BAD_RESPONSE;
    }
    *dirent = dir.m_items[0];
    return 0;
}

void OHRadio::evtCallback(const std::unordered_map<std::string, std::string>& props)
{
    LOGDEB1("OHRadio::evtCallback: getReporter(): " << getReporter() << '\n');
    for (const auto& [propname, propvalue] : props) {
        if (!getReporter()) {
            LOGDEB1("OHRadio::evtCallback: " << propname << " -> " << propvalue << '\n');
            continue;
        }

        if (propname == "Id" || propname == "ChannelsMax") {
            getReporter()->changed(propname.c_str(), atoi(propvalue.c_str()));
        } else if (propname == "IdArray") {
            // Decode IdArray. See how we call the client
            vector<int> v;
            ohplIdArrayToVec(propvalue, &v);
            getReporter()->changed(propname.c_str(), v);
        } else if (propname == "ProtocolInfo" || propname == "Uri") {
            getReporter()->changed(propname.c_str(), propvalue.c_str());
        } else if (propname == "Metadata") {
            /* Metadata is a didl-lite string */
            UPnPDirObject dirent;
            if (decodeMetadata("evt", propvalue, &dirent) == 0) {
                getReporter()->changed(propname.c_str(), dirent);
            } else {
                LOGDEB("OHRadio:evtCallback: bad metadata in event\n");
            }
        } else if (propname == "TransportState") {
            OHPlaylist::TPState tp;
            OHPlaylist::stringToTpState(propvalue, &tp);
            getReporter()->changed(propname.c_str(), int(tp));
        } else {
            LOGERR("OHRadio event: unknown variable: name [" << propname << "] value [" <<
                   propvalue << '\n');
            getReporter()->changed(propname.c_str(), propvalue.c_str());
        }
    }
}

void OHRadio::registerCallback()
{
    Service::registerCallback(bind(&OHRadio::evtCallback, this, _1));
}

int OHRadio::channel(std::string* urip, UPnPDirObject *dirent)
{
    SoapOutgoing args(getServiceType(), "Channel");
    SoapIncoming data;
    int ret = runAction(args, data);
    if (ret != UPNP_E_SUCCESS) {
        return ret;
    }
    if (!data.get("Uri", urip)) {
        LOGERR("OHRadio::Read: missing Uri in response" << '\n');
        return UPNP_E_BAD_RESPONSE;
    }
    string didl;
    if (!data.get("Metadata", &didl)) {
        LOGERR("OHRadio::Read: missing Uri in response" << '\n');
        return UPNP_E_BAD_RESPONSE;
    }
    return decodeMetadata("channel", didl, dirent);
}

int OHRadio::channelsMax(int *value)
{
    return runSimpleGet("ChannelsMax", "Value", value);
}

int OHRadio::id(int *value, int timeoutms)
{
    ActionOptions opts;
    if (timeoutms >= 0) {
        opts.active_options |= AOM_TIMEOUTMS;
        opts.timeoutms = timeoutms;
    }
    return runSimpleGet("Id", "Value", value, &opts);
}

int OHRadio::idArray(vector<int> *ids, int *tokp)
{
    SoapOutgoing args(getServiceType(), "IdArray");
    SoapIncoming data;
    int ret = runAction(args, data);
    if (ret != UPNP_E_SUCCESS) {
        return ret;
    }
    if (!data.get("Token", tokp)) {
        LOGERR("OHRadio::idArray: missing Token in response" << '\n');
        return UPNP_E_BAD_RESPONSE;
    }
    string arraydata;
    if (!data.get("Array", &arraydata)) {
        LOGINF("OHRadio::idArray: missing Array in response" << '\n');
        // We get this for an empty array ? This would need to be investigated
    }
    ohplIdArrayToVec(arraydata, ids);
    return 0;
}

int OHRadio::idArrayChanged(int token, bool *changed)
{
    SoapOutgoing args(getServiceType(), "IdArrayChanged");
    args("Token", SoapHelp::i2s(token));
    SoapIncoming data;
    int ret = runAction(args, data);
    if (ret != UPNP_E_SUCCESS) {
        return ret;
    }
    if (!data.get("Value", changed)) {
        LOGERR("OHRadio::idArrayChanged: missing Value in response" << '\n');
        return UPNP_E_BAD_RESPONSE;
    }
    return 0;
}

int OHRadio::pause()
{
    return runTrivialAction("Pause");
}

int OHRadio::play()
{
    return runTrivialAction("Play");
}

int OHRadio::protocolInfo(std::string *proto)
{
    SoapOutgoing args(getServiceType(), "ProtocolInfo");
    SoapIncoming data;
    int ret = runAction(args, data);
    if (ret != UPNP_E_SUCCESS) {
        return ret;
    }
    if (!data.get("Value", proto)) {
        LOGERR("OHRadio::protocolInfo: missing Value in response" << '\n');
        return UPNP_E_BAD_RESPONSE;
    }
    return 0;
}

int OHRadio::read(int id, UPnPDirObject *dirent)
{
    SoapOutgoing args(getServiceType(), "Read");
    args("Id", SoapHelp::i2s(id));
    SoapIncoming data;
    int ret = runAction(args, data);
    if (ret != UPNP_E_SUCCESS) {
        return ret;
    }
    string didl;
    if (!data.get("Metadata", &didl)) {
        LOGERR("OHRadio::Read: missing Metadata in response" << '\n');
        return UPNP_E_BAD_RESPONSE;
    }
    return decodeMetadata("read", didl, dirent);
}

// Tracklist format
// <TrackList>
//   <Entry>
//     <Id>10</Id>
//     <Uri>http://blabla</Uri>
//     <Metadata>(xmlencoded didl)</Metadata>
//   </Entry>
// </TrackList>


class OHTrackListParser : public inputRefXMLParser {
public:
    OHTrackListParser(const string& input, vector<OHPlaylist::TrackListEntry>* vp)
        : inputRefXMLParser(input), m_v(vp)
    {
        //LOGDEB("OHTrackListParser: input: " << input << '\n');
    }

protected:
    void EndElement(const XML_Char* name) override
    {
        if (!strcmp(name, "Entry")) {
            UPnPDirContent dir;
            if (!dir.parse(m_tdidl)) {
                LOGERR("OHRadio::ReadList: didl parse failed: "
                       << m_tdidl << '\n');
                return;
            }
            if (dir.m_items.size() != 1) {
                LOGERR("OHRadio::ReadList: " << dir.m_items.size() << " in response!" << '\n');
                return;
            }
            m_tt.dirent = dir.m_items[0];
            m_v->push_back(m_tt);
            m_tt.clear();
            m_tdidl.clear();
        }
    }
    void CharacterData(const XML_Char* s, int len) override
    {
        if (s == 0 || *s == 0)
            return;
        string str(s, len);
        if (m_path.back().name == "Id")
            m_tt.id = atoi(str.c_str());
        else if (m_path.back().name == "Uri")
            m_tt.url = str;
        else if (m_path.back().name == "Metadata")
            m_tdidl += str;
    }

private:
    vector<OHPlaylist::TrackListEntry>* m_v;
    OHPlaylist::TrackListEntry m_tt;
    string m_tdidl;
};

int OHRadio::readList(const std::vector<int>& ids, vector<OHPlaylist::TrackListEntry>* entsp)
{
    string idsparam;
    for (int id : ids) {
        idsparam += SoapHelp::i2s(id) + " ";
    }
    entsp->clear();

    SoapOutgoing args(getServiceType(), "ReadList");
    args("IdList", idsparam);
    SoapIncoming data;
    int ret = runAction(args, data);
    if (ret != UPNP_E_SUCCESS) {
        return ret;
    }
    string xml;
    if (!data.get("ChannelList", &xml)) {
        LOGERR("OHRadio::readlist: missing TrackList in response" << '\n');
        return UPNP_E_BAD_RESPONSE;
    }
    OHTrackListParser mparser(xml, entsp);
    if (!mparser.Parse())
        return UPNP_E_BAD_RESPONSE;
    return 0;
}

int OHRadio::setChannel(const std::string& uri, const std::string& didl)
{
    SoapOutgoing args(getServiceType(), "SetChannel");
    args("Uri", uri)
    ("Metadata", didl);
    SoapIncoming data;
    return runAction(args, data);
}

int OHRadio::setId(int id, const std::string& uri)
{
    SoapOutgoing args(getServiceType(), "SetId");
    args("Value", SoapHelp::val2s(id))
    ("Uri", uri);
    SoapIncoming data;
    return runAction(args, data);
}

int OHRadio::stop()
{
    return runTrivialAction("Stop");
}

int OHRadio::transportState(OHPlaylist::TPState* tpp)
{
    string value;
    int ret;

    if ((ret = runSimpleGet("TransportState", "Value", &value)))
        return ret;

    return OHPlaylist::stringToTpState(value, tpp);
}

} // End namespace UPnPClient
