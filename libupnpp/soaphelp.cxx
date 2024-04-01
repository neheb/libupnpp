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

#include "libupnpp/upnpp_p.hxx"
#include "libupnpp/soaphelp.hxx"

#include <stdio.h>
#include <stdlib.h>

#include <iostream>
#include <vector>

#include "libupnpp/log.hxx"
#include "libupnpp/smallut.h"

using namespace std;

namespace UPnPP {

SoapIncoming::SoapIncoming()
    : m(new Internal())
{
}

SoapIncoming::~SoapIncoming()
{
    deleteZ(m);
}

void SoapIncoming::getMap(unordered_map<string, string>& out)
{
    if (m) {
        out = m->args;
    }
}

const string& SoapIncoming::getName() const
{
    return m->name;
}

bool SoapIncoming::get(const char *nm, bool *value) const
{
    auto it = m->args.find(nm);
    if (it == m->args.end() || it->second.empty()) {
        return false;
    }
    return stringToBool(it->second, value);
}

bool SoapIncoming::get(const char *nm, int *value) const
{
    auto it = m->args.find(nm);
    if (it == m->args.end() || it->second.empty()) {
        return false;
    }
    *value = atoi(it->second.c_str());
    return true;
}

bool SoapIncoming::get(const char *nm, string *value) const
{
    auto it = m->args.find(nm);
    if (it == m->args.end()) {
        return false;
    }
    *value = it->second;
    return true;
}

string SoapHelp::xmlQuote(const string& in)
{
    string out;
    for (auto c : in) {
        switch (c) {
        case '"':
            out += "&quot;";
            break;
        case '&':
            out += "&amp;";
            break;
        case '<':
            out += "&lt;";
            break;
        case '>':
            out += "&gt;";
            break;
        case '\'':
            out += "&apos;";
            break;
        default:
            out += c;
        }
    }
    return out;
}

string SoapHelp::xmlUnquote(const string& in)
{
    string out;
    for (unsigned int i = 0; i < in.size(); i++) {
        if (in[i] == '&') {
            unsigned int j;
            for (j = i; j < in.size(); j++) {
                if (in[j] == ';') {
                    break;
                }
            }
            if (in[j] != ';') {
                out += in.substr(i);
                return out;
            }
            string entname = in.substr(i + 1, j - i - 1);
            //cerr << "entname [" << entname << "]" << endl;
            if (!entname.compare("quot")) {
                out += '"';
            } else if (!entname.compare("amp")) {
                out += '&';
            } else if (!entname.compare("lt")) {
                out += '<';
            } else if (!entname.compare("gt")) {
                out += '>';
            } else if (!entname.compare("apos")) {
                out += '\'';
            } else {
                out += in.substr(i, j - i + 1);
            }
            i = j;
        } else {
            out += in[i];
        }
    }
    return out;
}

string SoapHelp::i2s(int val)
{
    return lltodecstr(val);
}


SoapOutgoing::SoapOutgoing()
    : m(new Internal())
{
}

SoapOutgoing::SoapOutgoing(const string& st, const string& nm)
    : m(new Internal(st, nm))
{
}

SoapOutgoing::~SoapOutgoing()
{
    deleteZ(m);
}

const string& SoapOutgoing::getName() const
{
    return m->name;
}

SoapOutgoing& SoapOutgoing::addarg(const string& k, const string& v)
{
    m->data.push_back(pair<string, string>(k, v));
    return *this;
}

SoapOutgoing& SoapOutgoing::operator()(const string& k, const string& v)
{
    m->data.push_back(pair<string, string>(k, v));
    return *this;
}

} // namespace
