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
#ifndef _OHPRODUCT_HXX_INCLUDED_
#define _OHPRODUCT_HXX_INCLUDED_

#include <unordered_map>
#include <string>
#include <vector>

#include "service.hxx"


namespace UPnPClient {

class OHProduct;
class UPnPDeviceDesc;
class UPnPServiceDesc;

typedef std::shared_ptr<OHProduct> OHPRH;

/**
 * OHProduct Service client class.
 *
 */
class UPNPP_API OHProduct : public Service {
public:
    using Service::Service;

    /** Test service type from discovery message */
    static bool isOHPrService(const std::string& st);
    bool serviceTypeMatch(const std::string& tp) override;

    struct Source {
        std::string name;
        std::string type;
        bool visible{false};
    };

    static bool parseSourceXML(std::string& sxml, std::vector<Source>& sources);

    /** @return 0 for success, upnp error else */
    int getSources(std::vector<Source>& sources);
    int sourceIndex(int *index);
    int setSourceIndex(int index);
    int setSourceIndexByName(const std::string& name);
    int standby(bool *value);
    int setStanby(bool value);

protected:
    /* My service type string */
    static const std::string SType;
private:
    void UPNPP_LOCAL evtCallback(const std::unordered_map<std::string, std::string>&);
    void UPNPP_LOCAL registerCallback() override;
};

} // namespace UPnPClient

#endif /* _OHPRODUCT_HXX_INCLUDED_ */
