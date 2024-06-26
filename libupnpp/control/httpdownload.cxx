/***************************************************************************
 *                                  _   _ ____  _
 *  Project                     ___| | | |  _ \| |
 *                             / __| | | | |_) | |
 *                            | (__| |_| |  _ <| |___
 *                             \___|\___/|_| \_\_____|
 *
 * Copyright (C) 1998 - 2013, Daniel Stenberg, <daniel@haxx.se>, et al.
 *
 * This software is licensed as described in the file COPYING, which
 * you should have received as part of this distribution. The terms
 * are also available at http://curl.haxx.se/docs/copyright.html.
 *
 * You may opt to use, copy, modify, merge, publish, distribute and/or sell
 * copies of the Software, and permit persons to whom the Software is
 * furnished to do so, under the terms of the COPYING file.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ***************************************************************************/
#include "config.h"

#include <cstdio>
#include <string>
#include <sys/types.h>

#ifndef _WIN32
#include <netinet/in.h>
#endif

#include <curl/curl.h>

#include "libupnpp/log.hxx"
#include "libupnpp/control/httpdownload.hxx"

using namespace std;

static size_t
write_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    string* out = (string*)userp;

    out->append((const char *)contents, realsize);
    return realsize;
}


namespace UPnPClient {

bool downloadUrlWithCurl(const string& url, string& out, long timeoutsecs,
    struct sockaddr_storage *saddr)
{
    CURL *curl;
    CURLcode res;
    bool ret = false;

    curl = curl_easy_init();
    if(!curl) {
        LOGERR("downloadUrlWithCurl: curl_easy_init failed" << '\n');
        return false;
    }
    long scopeid{-1};
    if (nullptr != saddr && saddr->ss_family == AF_INET6) {
        struct sockaddr_in6 *sa6p = (struct sockaddr_in6 *)saddr;
        scopeid = (long)sa6p->sin6_scope_id;
    }
        
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeoutsecs);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &out);
    if (scopeid != -1) {
        curl_easy_setopt(curl, CURLOPT_ADDRESS_SCOPE, scopeid);
    }
    res = curl_easy_perform(curl);
    if(res != CURLE_OK) {
        LOGERR("downloadUrlWithCurl: curl_easy_perform(): " << curl_easy_strerror(res) << '\n');
    } else {
        ret = true;
    }
    curl_easy_cleanup(curl);

    return ret;
}

}
