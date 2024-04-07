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

#include <cstring>

#include <unordered_map>
#include <string>
#include <vector>
#include <iostream>

#include "libupnpp/control/cdircontent.hxx"
#include "libupnpp/expatmm.h"
#include "libupnpp/log.hxx"
#include "libupnpp/upnpp_p.hxx"
#include "libupnpp/soaphelp.hxx"

using namespace std;
using namespace UPnPP;

namespace UPnPClient {

string UPnPDirObject::nullstr;

// An XML parser which builds directory contents from DIDL-lite input.
class UPnPDirParser : public inputRefXMLParser {
public:
    UPnPDirParser(UPnPDirContent& dir, const string& input, bool detailed)
        : inputRefXMLParser(input), m_dir(dir), m_detailed(detailed) {
        //LOGDEB("UPnPDirParser: input: " << input << endl);
        m_okitems["object.item.audioItem"] = UPnPDirObject::ITC_audioItem;
        m_okitems["object.item.audioItem.musicTrack"] =
            UPnPDirObject::ITC_audioItem;
        m_okitems["object.item.audioItem.audioBroadcast"] =
            UPnPDirObject::ITC_audioItem;
        m_okitems["object.item.audioItem.audioBook"] =
            UPnPDirObject::ITC_audioItem;
        m_okitems["object.item.playlistItem"] = UPnPDirObject::ITC_playlist;
        m_okitems["object.item.videoItem"] = UPnPDirObject::ITC_videoItem;
    }
    UPnPDirContent& m_dir;
protected:
    void StartElement(const XML_Char* name, const XML_Char**) override
    {
        //LOGDEB("startElement: name [" << name << "]" << " bpos " <<
        //             XML_GetCurrentByteIndex(expat_parser) << endl);
        auto& mapattrs = m_path.back().attributes;
        switch (name[0]) {
        case 'c':
            if (!strcmp(name, "container")) {
                m_tobj.clear(m_detailed);
                m_tobj.m_type = UPnPDirObject::container;
                m_tobj.m_id = mapattrs["id"];
                m_tobj.m_pid = mapattrs["parentID"];
            }
            break;
        case 'i':
            if (!strcmp(name, "item")) {
                m_tobj.clear(m_detailed);
                m_tobj.m_type = UPnPDirObject::item;
                m_tobj.m_id = mapattrs["id"];
                m_tobj.m_pid = mapattrs["parentID"];
            }
            break;
        default:
            break;
        }
    }

    virtual bool checkobjok() {
        // We used to check id and pid not empty, but this is ok if we
        // are parsing a didl fragment sent from a control point. So
        // lets all ok and hope for the best.
        bool ok =  true; /*!m_tobj.m_id.empty() && !m_tobj.m_pid.empty() &&
                           !m_tobj.m_title.empty();*/

        if (ok && m_tobj.m_type == UPnPDirObject::item) {
            const auto& it = m_okitems.find(m_tobj.m_props["upnp:class"]);
            if (it == m_okitems.end()) {
                // Only log this if the record comes from an MS as e.g. naims
                // send records with empty classes (and empty id/pid)
                if (!m_tobj.m_id.empty()) {
                    LOGINF("checkobjok: found object of unknown class: [" << m_tobj.m_props["upnp:class"] << "]" << '\n');
                }
                m_tobj.m_iclass = UPnPDirObject::ITC_unknown;
            } else {
                m_tobj.m_iclass = it->second;
            }
        }

        if (!ok) {
            LOGINF("checkobjok:skip: id [" << m_tobj.m_id << "] pid [" << m_tobj.m_pid << "] clss [" << m_tobj.m_props["upnp:class"]
                                           << "] tt [" << m_tobj.m_title << "]" << '\n');
        }
        return ok;
    }

    void EndElement(const XML_Char* name) override
    {
        string parentname;
        if (m_path.size() == 1) {
            parentname = "root";
        } else {
            parentname = m_path[m_path.size()-2].name;
        }
        //LOGDEB("Closing element " << name << " inside element " <<
        //       parentname << " data " << m_path.back().data << endl);
        if (!strcmp(name, "container")) {
            if (checkobjok()) {
                m_dir.m_containers.push_back(m_tobj);
            }
        } else if (!strcmp(name, "item")) {
            if (checkobjok()) {
                size_t len = XML_GetCurrentByteIndex(expat_parser) -
                    m_path.back().start_index;
                if (len > 0) {
                    m_tobj.m_didlfrag = m_input.substr(
                        m_path.back().start_index, len) +  "</item>";
                }
                m_dir.m_items.push_back(m_tobj);
            }
        } else if (!parentname.compare("item") ||
                   !parentname.compare("container")) {
            switch (name[0]) {
            case 'd':
                if (!strcmp(name, "dc:title")) {
                    m_tobj.m_title = m_path.back().data;
                } else {
                    addprop(name, m_path.back().data);
                }
                break;
            case 'r':
                if (!strcmp(name, "res")) {
                    // <res protocolInfo="http-get:*:audio/mpeg:*" size="517149"
                    // bitrate="24576" duration="00:03:35"
                    // sampleFrequency="44100" nrAudioChannels="2">
                    UPnPResource res;
                    if (LibUPnP::getLibUPnP()->m->reSanitizeURLs()) {
                        res.m_uri = reSanitizeURL(m_path.back().data);
                    } else {
                        res.m_uri = m_path.back().data;
                    }
                    res.m_props = m_path.back().attributes;
                    m_tobj.m_resources.push_back(res);
                } else {
                    addprop(name, m_path.back().data);
                }
                break;
            case 'u':
                if (!strcmp(name, "upnp:albumArtURI")) {
                    if (LibUPnP::getLibUPnP()->m->reSanitizeURLs()) {
                        addprop(name, reSanitizeURL(m_path.back().data));
                    } else {
                        addprop(name, m_path.back().data);
                    }
                } else {
                    addprop(name, m_path.back().data);
                }
                break;
            default:
                addprop(name, m_path.back().data);
                break;
            }
        }
    }

    void CharacterData(const XML_Char* s, int len) override
    {
        if (s == 0 || *s == 0)
            return;
        string str(s, len);
        m_path.back().data += str;
    }

private:
    UPnPDirObject m_tobj;
    map<string, UPnPDirObject::ItemClass> m_okitems;
    bool m_detailed;

    void addprop(const string& nm, const string& data) {
        // e.g <upnp:artist role="AlbumArtist">Jojo</upnp:artist>
        auto& mapattrs = m_path.back().attributes;

        if (m_tobj.m_allprops) {
            (*m_tobj.m_allprops)[nm].emplace_back(data, mapattrs);
            return;
        }
        // "old" format with concatenated string output
        auto roleit = mapattrs.find("role");
        string rolevalue;
        if (roleit != mapattrs.end()) {
            if (roleit->second.compare("AlbumArtist")) {
                // AlbumArtist is not useful for the user
                rolevalue = string(" (") + roleit->second + string(")");
            }
        }
        auto it = m_tobj.m_props.find(nm);
        if (it == m_tobj.m_props.end()) {
            m_tobj.m_props[nm] = data + rolevalue;
        } else {
            // Only add the value if it's not already there. Actually,
            // to do this properly we'd need to only build the string
            // at the end when we have all the entries
            string &current(it->second);
            if (current.compare(data)) {
                current += string(", ") + data + rolevalue;
            }
        }
    }

};

bool UPnPDirContent::parse(const std::string& input, bool detailed)
{
    if (input.empty()) {
        return false;
    }
    const string *ipp = &input;

    // Double-quoting happens. Just deal with it...
    string unquoted;
    if (input[0] == '&') {
        LOGDEB0("UPnPDirContent::parse: unquoting over-quoted input: " << input << '\n');
        unquoted = SoapHelp::xmlUnquote(input);
        ipp = &unquoted;
    }

    UPnPDirParser parser(*this, *ipp, detailed);
    bool ret = parser.Parse();
    if (ret == false) {
        LOGERR("UPnPDirContent::parse: parser failed: " << parser.getLastErrorMessage() << " for:\n"
                                                        << *ipp << '\n');
    }
    return ret;
}

static const string didl_header(
    "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
    "<DIDL-Lite xmlns=\"urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/\""
    " xmlns:dc=\"http://purl.org/dc/elements/1.1/\""
    " xmlns:upnp=\"urn:schemas-upnp-org:metadata-1-0/upnp/\""
    " xmlns:dlna=\"urn:schemas-dlna-org:metadata-1-0/\">");
static const string didl_close("</DIDL-Lite>");

// Maybe we'll do something about building didl from scratch if this
// proves necessary.
string UPnPDirObject::getdidl() const
{
    return didl_header + m_didlfrag + didl_close;
}

} // namespace
