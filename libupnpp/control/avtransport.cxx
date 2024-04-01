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

#include "libupnpp/control/avtransport.hxx"

#include <stdlib.h>
#include <upnp.h>

#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "libupnpp/control/avlastchg.hxx"
#include "libupnpp/control/cdircontent.hxx"
#include "libupnpp/log.hxx"
#include "libupnpp/soaphelp.hxx"
#include "libupnpp/upnpavutils.hxx"
#include "libupnpp/upnpp_p.hxx"
#include "libupnpp/smallut.h" 

#ifdef _WIN32
#undef DeviceCapabilities
#endif

using namespace std;
using namespace std::placeholders;
using namespace UPnPP;

namespace UPnPClient {

const string AVTransport::SType("urn:schemas-upnp-org:service:AVTransport:1");

// Check serviceType string (while walking the descriptions. We don't
// include a version in comparisons, as we are satisfied with version1
bool AVTransport::isAVTService(const string& st)
{
    const string::size_type sz(SType.size()-2);
    return !SType.compare(0, sz, st, 0, sz);
}
bool AVTransport::serviceTypeMatch(const std::string& tp)
{
    return isAVTService(tp);
}

static AVTransport::TransportState stringToTpState(const string& s)
{
    if (!stringuppercmp("STOPPED", s)) {
        return AVTransport::Stopped;
    } else if (!stringuppercmp("PLAYING", s)) {
        return AVTransport::Playing;
    } else if (!stringuppercmp("TRANSITIONING", s)) {
        return AVTransport::Transitioning;
    } else if (!stringuppercmp("PAUSED_PLAYBACK", s)) {
        return AVTransport::PausedPlayback;
    } else if (!stringuppercmp("PAUSED_RECORDING", s)) {
        return AVTransport::PausedRecording;
    } else if (!stringuppercmp("RECORDING", s)) {
        return AVTransport::Recording;
    } else if (!stringuppercmp("NO_MEDIA_PRESENT", s)) {
        return AVTransport::NoMediaPresent;
    } else {
        LOGINF("AVTransport event: bad value for TransportState: "
               << s << "\n");
        return AVTransport::Unknown;
    }
}

static AVTransport::TransportStatus stringToTpStatus(const string& s)
{
    if (!stringuppercmp("OK", s)) {
        return AVTransport::TPS_Ok;
    } else if (!stringuppercmp("ERROR_OCCURRED", s)) {
        return  AVTransport::TPS_Error;
    } else {
        LOGERR("AVTransport event: bad value for TransportStatus: "
               << s << "\n");
        return  AVTransport::TPS_Unknown;
    }
}

static AVTransport::PlayMode stringToPlayMode(const string& s)
{
    if (!stringuppercmp("NORMAL", s)) {
        return AVTransport::PM_Normal;
    } else if (!stringuppercmp("SHUFFLE", s)) {
        return AVTransport::PM_Shuffle;
    } else if (!stringuppercmp("REPEAT_ONE", s)) {
        return AVTransport::PM_RepeatOne;
    } else if (!stringuppercmp("REPEAT_ALL", s)) {
        return AVTransport::PM_RepeatAll;
    } else if (!stringuppercmp("RANDOM", s)) {
        return AVTransport::PM_Random;
    } else if (!stringuppercmp("DIRECT_1", s)) {
        return AVTransport::PM_Direct1;
    } else {
        LOGERR("AVTransport event: bad value for PlayMode: "
               << s << "\n");
        return AVTransport::PM_Unknown;
    }
}

void AVTransport::evtCallback(
    const std::unordered_map<std::string, std::string>& props)
{
    LOGDEB1("AVTransport::evtCallback:" << "\n");
    auto reporter = getReporter();
    if (nullptr == reporter) {
        LOGDEB1("AVTransport::evtCallback: " << varchg.first << " -> " << varchg.second << "\n");
        return;
    }

    for (const auto& prop : props) {
        if (prop.first.compare("LastChange")) {
            LOGINF("AVTransport:event: var not lastchange: "
                   << prop.first << " -> " << prop.second << "\n");
            continue;
        }
        LOGDEB1("AVTransport:event: " << prop.first << " -> " << prop.second << "\n");

        std::unordered_map<std::string, std::string> changes;
        if (!decodeAVLastChange(prop.second, changes)) {
            LOGERR("AVTransport::evtCallback: bad LastChange value: " << prop.second << "\n");
            return;
        }
        for (const auto& varchg : changes) {
            const std::string& varnm{varchg.first};
            const std::string& varvalue{varchg.second};

            if (!varnm.compare("TransportState")) {
                reporter->changed(varnm.c_str(), stringToTpState(varvalue));

            } else if (!varnm.compare("TransportStatus")) {
                reporter->changed(varnm.c_str(), stringToTpStatus(varvalue));

            } else if (!varnm.compare("CurrentPlayMode")) {
                reporter->changed(varnm.c_str(), stringToPlayMode(varvalue));

            } else if (!varnm.compare("CurrentTransportActions")) {
                int iacts;
                if (!CTAStringToBits(varvalue, iacts))
                    reporter->changed(varnm.c_str(), iacts);

            } else if (!varnm.compare("CurrentTrackURI") ||
                       !varnm.compare("AVTransportURI") ||
                       !varnm.compare("NextAVTransportURI")) {
                reporter->changed(varnm.c_str(), varvalue.c_str());

            } else if (!varnm.compare("TransportPlaySpeed") ||
                       !varnm.compare("CurrentTrack") ||
                       !varnm.compare("NumberOfTracks") ||
                       !varnm.compare("RelativeCounterPosition") ||
                       !varnm.compare("AbsoluteCounterPosition") ||
                       !varnm.compare("InstanceID")) {
                reporter->changed(varnm.c_str(), atoi(varvalue.c_str()));

            } else if (!varnm.compare("CurrentMediaDuration") ||
                       !varnm.compare("CurrentTrackDuration") ||
                       !varnm.compare("RelativeTimePosition") ||
                       !varnm.compare("AbsoluteTimePosition")) {
                reporter->changed(varnm.c_str(), upnpdurationtos(varvalue));

            } else if (!varnm.compare("AVTransportURIMetaData") ||
                       !varnm.compare("NextAVTransportURIMetaData") ||
                       !varnm.compare("CurrentTrackMetaData")) {
                UPnPDirContent meta;
                if (!varvalue.empty() && !meta.parse(varvalue)) {
                    LOGERR("AVTransport event: bad metadata: [" << varvalue << "]" << "\n");
                } else {
                    LOGDEB1("AVTransport event: good metadata: [" << varvalue << "]" << "\n");
                    if (meta.m_items.size() > 0) {
                        reporter->changed(varnm.c_str(), meta.m_items[0]);
                    }
                }
            } else if (!varnm.compare("PlaybackStorageMedium") ||
                       !varnm.compare("PossiblePlaybackStorageMedia") ||
                       !varnm.compare("RecordStorageMedium") ||
                       !varnm.compare("PossibleRecordStorageMedia") ||
                       !varnm.compare("RecordMediumWriteStatus") ||
                       !varnm.compare("CurrentRecordQualityMode") ||
                       !varnm.compare("PossibleRecordQualityModes")) {
                reporter->changed(varnm.c_str(), varvalue.c_str());

            } else {
                LOGDEB1("AVTransport event: unknown variable: name [" <<
                        varnm << "] value [" << varvalue << "\n");
                reporter->changed(varnm.c_str(), varvalue.c_str());
            }
        }
    }
}


int AVTransport::setURI(const string& uri, const string& metadata,
                        int instanceID, bool next)
{
    SoapOutgoing args(getServiceType(), next ? "SetNextAVTransportURI" : "SetAVTransportURI");
    args("InstanceID", SoapHelp::i2s(instanceID))
        (next ? "NextURI" : "CurrentURI", uri)
        (next ? "NextURIMetaData" : "CurrentURIMetaData", metadata);

    SoapIncoming data;
    return runAction(args, data);
}

int AVTransport::setPlayMode(PlayMode pm, int instanceID)
{
    SoapOutgoing args(getServiceType(), "SetPlayMode");
    string playmode;
    switch (pm) {
    case PM_Normal:
        playmode = "NORMAL";
        break;
    case PM_Shuffle:
        playmode = "SHUFFLE";
        break;
    case PM_RepeatOne:
        playmode = "REPEAT_ONE";
        break;
    case PM_RepeatAll:
        playmode = "REPEAT_ALL";
        break;
    case PM_Random:
        playmode = "RANDOM";
        break;
    case PM_Direct1:
        playmode = "DIRECT_1";
        break;
    default:
        playmode = "NORMAL";
        break;
    }

    args("InstanceID", SoapHelp::i2s(instanceID))
        ("NewPlayMode", playmode);

    SoapIncoming data;
    return runAction(args, data);
}

int AVTransport::getMediaInfo(MediaInfo& info, int instanceID)
{
    SoapOutgoing args(getServiceType(), "GetMediaInfo");
    args("InstanceID", SoapHelp::i2s(instanceID));
    SoapIncoming data;
    int ret = runAction(args, data);
    if (ret != UPNP_E_SUCCESS) {
        return ret;
    }
    string s;
    data.get("NrTracks", &info.nrtracks);
    data.get("MediaDuration", &s);
    info.mduration = upnpdurationtos(s);
    data.get("CurrentURI", &info.cururi);
    data.get("CurrentURIMetaData", &s);
    UPnPDirContent meta;
    if (!s.empty())
        meta.parse(s);
    if (meta.m_items.size() > 0)
        info.curmeta = meta.m_items[0];
    meta.clear();
    data.get("NextURI", &info.nexturi);
    data.get("NextURIMetaData", &s);
    if (meta.m_items.size() > 0)
        info.nextmeta = meta.m_items[0];
    data.get("PlayMedium", &info.pbstoragemed);
    data.get("RecordMedium", &info.pbstoragemed);
    data.get("WriteStatus", &info.ws);
    return 0;
}

int AVTransport::getTransportInfo(TransportInfo& info, int instanceID)
{
    SoapOutgoing args(getServiceType(), "GetTransportInfo");
    args("InstanceID", SoapHelp::i2s(instanceID));
    SoapIncoming data;
    int ret = runAction(args, data);
    if (ret != UPNP_E_SUCCESS) {
        return ret;
    }
    string s;
    data.get("CurrentTransportState", &s);
    info.tpstate = stringToTpState(s);
    data.get("CurrentTransportStatus", &s);
    info.tpstatus = stringToTpStatus(s);
    data.get("CurrentSpeed", &info.curspeed);
    return 0;
}

int AVTransport::getPositionInfo(PositionInfo& info, int instanceID, int timeoutms)
{
    SoapOutgoing args(getServiceType(), "GetPositionInfo");
    args("InstanceID", SoapHelp::i2s(instanceID));
    SoapIncoming data;
    ActionOptions opts;
    if (timeoutms >= 0) {
        opts.active_options |= AOM_TIMEOUTMS;
        opts.timeoutms = timeoutms;
    }
    int ret = runAction(args, data, &opts);
    if (ret != UPNP_E_SUCCESS) {
        return ret;
    }
    string s;
    data.get("Track", &info.track);
    data.get("TrackDuration", &s);
    info.trackduration = upnpdurationtos(s);
    data.get("TrackMetaData", &s);
    if (!s.empty()) {
        UPnPDirContent meta;
        meta.parse(s);
        if (meta.m_items.size() > 0) {
            info.trackmeta = meta.m_items[0];
            LOGDEB1("AVTransport::getPositionInfo: size " <<
                    meta.m_items.size() << " current title: " << meta.m_items[0].m_title << "\n");
        }
    }
    data.get("TrackURI", &info.trackuri);
    data.get("RelTime", &s);
    info.reltime = upnpdurationtos(s);
    data.get("AbsTime", &s);
    info.abstime = upnpdurationtos(s);
    data.get("RelCount", &info.relcount);
    data.get("AbsCount", &info.abscount);
    return 0;
}

int AVTransport::getDeviceCapabilities(DeviceCapabilities& info, int iID)
{
    SoapOutgoing args(getServiceType(), "GetDeviceCapabilities");
    args("InstanceID", SoapHelp::i2s(iID));
    SoapIncoming data;
    int ret = runAction(args, data);
    if (ret != UPNP_E_SUCCESS) {
        return ret;
    }
    data.get("PlayMedia", &info.playmedia);
    data.get("RecMedia", &info.recmedia);
    data.get("RecQualityModes", &info.recqualitymodes);
    return 0;
}

int AVTransport::getTransportSettings(TransportSettings& info, int instanceID)
{
    SoapOutgoing args(getServiceType(), "GetTransportSettings");
    args("InstanceID", SoapHelp::i2s(instanceID));
    SoapIncoming data;
    int ret = runAction(args, data);
    if (ret != UPNP_E_SUCCESS) {
        return ret;
    }
    string s;
    data.get("PlayMedia", &s);
    info.playmode = stringToPlayMode(s);
    data.get("RecQualityMode", &info.recqualitymode);
    return 0;
}

int AVTransport::getCurrentTransportActions(int& iacts, int iID)
{
    SoapOutgoing args(getServiceType(), "GetCurrentTransportActions");
    args("InstanceID", SoapHelp::i2s(iID));
    SoapIncoming data;
    int ret = runAction(args, data);
    if (ret != UPNP_E_SUCCESS) {
        return ret;
    }
    string actions;
    if (!data.get("Actions", &actions)) {
        LOGERR("AVTransport:getCurrentTransportActions: no actions in answer" << "\n");
        return UPNP_E_BAD_RESPONSE;
    }
    return CTAStringToBits(actions, iacts);
}

int AVTransport::CTAStringToBits(const string& actions, int& iacts)
{
    vector<string> sacts;
    if (!csvToStrings(actions, sacts)) {
        LOGERR("AVTransport::CTAStringToBits: bad actions string:" << actions << "\n");
        return UPNP_E_BAD_RESPONSE;
    }
    iacts = 0;
    for (auto& act : sacts) {
        trimstring(act);
        // It's not clear if the values are case sensitive, they are
        // as below in the doc. gmediarender for one, uses
        // all-caps. So let's compare insensitively.
        if (!stringicmp(act, "Next")) {
            iacts |= TPA_Next;
        } else if (!stringicmp(act, "Pause")) {
            iacts |= TPA_Pause;
        } else if (!stringicmp(act, "Play")) {
            iacts |= TPA_Play;
        } else if (!stringicmp(act, "Previous")) {
            iacts |= TPA_Previous;
        } else if (!stringicmp(act, "Seek")) {
            iacts |= TPA_Seek;
        } else if (!stringicmp(act, "Stop")) {
            iacts |= TPA_Stop;
        } else if (act.empty()) {
            continue;
        } else {
            LOGINF("AVTransport::CTAStringToBits: unknown action in " <<
                   actions << " : [" << act << "]" << "\n");
        }
    }
    return 0;
}

int AVTransport::stop(int instanceID)
{
    SoapOutgoing args(getServiceType(), "Stop");
    args("InstanceID", SoapHelp::i2s(instanceID));
    SoapIncoming data;
    return runAction(args, data);
}

int AVTransport::pause(int instanceID)
{
    SoapOutgoing args(getServiceType(), "Pause");
    args("InstanceID", SoapHelp::i2s(instanceID));
    SoapIncoming data;
    return runAction(args, data);
}

int AVTransport::play(int speed, int instanceID)
{
    SoapOutgoing args(getServiceType(), "Play");
    args("InstanceID", SoapHelp::i2s(instanceID))
        ("Speed", SoapHelp::i2s(speed));
    SoapIncoming data;
    return runAction(args, data);
}

int AVTransport::seek(SeekMode mode, int target, int instanceID)
{
    string sm;
    string value = SoapHelp::i2s(target);
    switch (mode) {
    case SEEK_TRACK_NR:
        sm = "TRACK_NR";
        break;
    case SEEK_ABS_TIME:
        sm = "ABS_TIME";
        value = upnpduration(target*1000);
        break;
    case SEEK_REL_TIME:
        sm = "REL_TIME";
        value = upnpduration(target*1000);
        break;
    case SEEK_ABS_COUNT:
        sm = "ABS_COUNT";
        break;
    case SEEK_REL_COUNT:
        sm = "REL_COUNT";
        break;
    case SEEK_CHANNEL_FREQ:
        sm = "CHANNEL_FREQ";
        break;
    case SEEK_TAPE_INDEX:
        sm = "TAPE-INDEX";
        break;
    case SEEK_FRAME:
        sm = "FRAME";
        break;
    default:
        return UPNP_E_INVALID_PARAM;
    }

    SoapOutgoing args(getServiceType(), "Seek");
    args("InstanceID", SoapHelp::i2s(instanceID))
        ("Unit", sm)
        ("Target", value);
    SoapIncoming data;
    return runAction(args, data);
}

int AVTransport::next(int instanceID)
{
    SoapOutgoing args(getServiceType(), "Next");
    args("InstanceID", SoapHelp::i2s(instanceID));
    SoapIncoming data;
    return runAction(args, data);
}

int AVTransport::previous(int instanceID)
{
    SoapOutgoing args(getServiceType(), "Previous");
    args("InstanceID", SoapHelp::i2s(instanceID));
    SoapIncoming data;
    return runAction(args, data);
}

void AVTransport::registerCallback()
{
    Service::registerCallback(bind(&AVTransport::evtCallback, this, _1));
}

} // End namespace UPnPClient
