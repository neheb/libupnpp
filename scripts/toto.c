/* Copyright (C) 2016 J.F.Dockes
 *	 This program is free software; you can redistribute it and/or modify
 *	 it under the terms of the GNU General Public License as published by
 *	 the Free Software Foundation; either version 2 of the License, or
 *	 (at your option) any later version.
 *
 *	 This program is distributed in the hope that it will be useful,
 *	 but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	 GNU General Public License for more details.
 *
 *	 You should have received a copy of the GNU General Public License
 *	 along with this program; if not, write to the
 *	 Free Software Foundation, Inc.,
 *	 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "toto.h"

#include <upnp/upnp.h>

#include <functional>
#include <iostream>
#include <map>
#include <utility>

#include "libupnpp/log.hxx"
#include "libupnpp/soaphelp.hxx"
#include "libupnpp/device/device.hxx"

using namespace std;
using namespace std::placeholders;

static const string sTpservice("urn:XXX");
static const string sIdservice("urn:XXX:1");

service::service(UPnPProvider::UpnpDevice *dev)
    : UpnpService(sTpservice, sIdservice, dev)
{
    dev->addActionMapping(
        this, "GetSearchCapabilities",
        bind(&service::actGetSearchCapabilities, this, _1, _2));
    dev->addActionMapping(
        this, "GetSortCapabilities",
        bind(&service::actGetSortCapabilities, this, _1, _2));
    dev->addActionMapping(
        this, "GetSystemUpdateID",
        bind(&service::actGetSystemUpdateID, this, _1, _2));
    dev->addActionMapping(
        this, "Browse",
        bind(&service::actBrowse, this, _1, _2));
    dev->addActionMapping(
        this, "Search",
        bind(&service::actSearch, this, _1, _2));
}


int service::actGetSearchCapabilities(const SoapIncoming& sc, SoapOutgoing& data)
{

    LOGDEB("service::actGetSearchCapabilities: " << endl);
    std::string out_SearchCaps;
    data.addarg("SearchCaps", out_SearchCaps);
    return UPNP_E_SUCCESS;
}

int service::actGetSortCapabilities(const SoapIncoming& sc, SoapOutgoing& data)
{

    LOGDEB("service::actGetSortCapabilities: " << endl);
    std::string out_SortCaps;
    data.addarg("SortCaps", out_SortCaps);
    return UPNP_E_SUCCESS;
}

int service::actGetSystemUpdateID(const SoapIncoming& sc, SoapOutgoing& data)
{

    LOGDEB("service::actGetSystemUpdateID: " << endl);
    std::string out_Id;
    data.addarg("Id", out_Id);
    return UPNP_E_SUCCESS;
}

int service::actBrowse(const SoapIncoming& sc, SoapOutgoing& data)
{
    bool ok = false;
    std::string in_ObjectID;
    ok = sc.get("ObjectID", &in_ObjectID);
    if (!ok) {
        LOGERR("service::actBrowse: no ObjectID in params\n");
        return UPNP_E_INVALID_PARAM;
    }
    std::string in_BrowseFlag;
    ok = sc.get("BrowseFlag", &in_BrowseFlag);
    if (!ok) {
        LOGERR("service::actBrowse: no BrowseFlag in params\n");
        return UPNP_E_INVALID_PARAM;
    }
    std::string in_Filter;
    ok = sc.get("Filter", &in_Filter);
    if (!ok) {
        LOGERR("service::actBrowse: no Filter in params\n");
        return UPNP_E_INVALID_PARAM;
    }
    int in_StartingIndex;
    ok = sc.get("StartingIndex", &in_StartingIndex);
    if (!ok) {
        LOGERR("service::actBrowse: no StartingIndex in params\n");
        return UPNP_E_INVALID_PARAM;
    }
    int in_RequestedCount;
    ok = sc.get("RequestedCount", &in_RequestedCount);
    if (!ok) {
        LOGERR("service::actBrowse: no RequestedCount in params\n");
        return UPNP_E_INVALID_PARAM;
    }
    std::string in_SortCriteria;
    ok = sc.get("SortCriteria", &in_SortCriteria);
    if (!ok) {
        LOGERR("service::actBrowse: no SortCriteria in params\n");
        return UPNP_E_INVALID_PARAM;
    }

    LOGDEB("service::actBrowse: " << " ObjectID " << ObjectID << " BrowseFlag " << BrowseFlag << " Filter " << Filter << " StartingIndex " << StartingIndex << " RequestedCount " << RequestedCount << " SortCriteria " << SortCriteria << endl);
    std::string out_Result;
    std::string out_NumberReturned;
    std::string out_TotalMatches;
    std::string out_UpdateID;
    data.addarg("Result", out_Result);
    data.addarg("NumberReturned", out_NumberReturned);
    data.addarg("TotalMatches", out_TotalMatches);
    data.addarg("UpdateID", out_UpdateID);
    return UPNP_E_SUCCESS;
}

int service::actSearch(const SoapIncoming& sc, SoapOutgoing& data)
{
    bool ok = false;
    std::string in_ContainerID;
    ok = sc.get("ContainerID", &in_ContainerID);
    if (!ok) {
        LOGERR("service::actSearch: no ContainerID in params\n");
        return UPNP_E_INVALID_PARAM;
    }
    std::string in_SearchCriteria;
    ok = sc.get("SearchCriteria", &in_SearchCriteria);
    if (!ok) {
        LOGERR("service::actSearch: no SearchCriteria in params\n");
        return UPNP_E_INVALID_PARAM;
    }
    std::string in_Filter;
    ok = sc.get("Filter", &in_Filter);
    if (!ok) {
        LOGERR("service::actSearch: no Filter in params\n");
        return UPNP_E_INVALID_PARAM;
    }
    int in_StartingIndex;
    ok = sc.get("StartingIndex", &in_StartingIndex);
    if (!ok) {
        LOGERR("service::actSearch: no StartingIndex in params\n");
        return UPNP_E_INVALID_PARAM;
    }
    int in_RequestedCount;
    ok = sc.get("RequestedCount", &in_RequestedCount);
    if (!ok) {
        LOGERR("service::actSearch: no RequestedCount in params\n");
        return UPNP_E_INVALID_PARAM;
    }
    std::string in_SortCriteria;
    ok = sc.get("SortCriteria", &in_SortCriteria);
    if (!ok) {
        LOGERR("service::actSearch: no SortCriteria in params\n");
        return UPNP_E_INVALID_PARAM;
    }

    LOGDEB("service::actSearch: " << " ContainerID " << ContainerID << " SearchCriteria " << SearchCriteria << " Filter " << Filter << " StartingIndex " << StartingIndex << " RequestedCount " << RequestedCount << " SortCriteria " << SortCriteria << endl);
    std::string out_Result;
    std::string out_NumberReturned;
    std::string out_TotalMatches;
    std::string out_UpdateID;
    data.addarg("Result", out_Result);
    data.addarg("NumberReturned", out_NumberReturned);
    data.addarg("TotalMatches", out_TotalMatches);
    data.addarg("UpdateID", out_UpdateID);
    return UPNP_E_SUCCESS;
}


