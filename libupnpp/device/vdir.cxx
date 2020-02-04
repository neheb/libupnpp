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
#include "libupnpp/config.h"

#include "vdir.hxx"

#include <string.h>
#include <upnp/upnp.h>

#include <iostream>
#include <utility>
#include <unordered_map>

#include "libupnpp/log.hxx"
#include "libupnpp/upnpp_p.hxx"

using namespace std;
using namespace UPnPP;

typedef struct File_Info UpnpFileInfo;

namespace UPnPProvider {

static VirtualDir *theDir;

class FileEnt {
public:
    time_t mtime;
    std::string mimetype;
    std::string content;
};

struct DirEnt {
public:
    DirEnt(bool vd = false) : isvd(vd) {}
    bool isvd;
    unordered_map<string, FileEnt> files;
    VirtualDir::FileOps ops;
};
static unordered_map<string,  DirEnt> m_dirs;

static void pathcatslash(string& path)
{
    if (path.empty() || path[path.size() - 1] != '/') {
        path += '/';
    }
}

// Look up entry for pathname
static FileEnt *vdgetentry(const char *pathname, DirEnt **de)
{
    //LOGDEB("vdgetentry: [" << pathname << "]" << endl);
    *de = nullptr;
    
    VirtualDir *thedir = VirtualDir::getVirtualDir();
    if (thedir == 0) {
        return 0;
    }

    string path = path_getfather(pathname);
    string name= path_getsimple(pathname);

    pathcatslash(path);
    
    auto dir = m_dirs.find(path);
    if (dir == m_dirs.end()) {
        LOGERR("VirtualDir::vdgetentry: no dir: " << path << endl);
        return 0;
    }
    *de = &dir->second;
    if (dir->second.isvd) {
        return 0;
    } else {
        auto f = dir->second.files.find(name);
        if (f == dir->second.files.end()) {
            LOGERR("VirtualDir::vdgetentry: no file: " << path << endl);
            return 0;
        }
        return &(f->second);
    }
}

bool VirtualDir::addFile(const string& _path, const string& name,
                         const string& content, const string& mimetype)
{
    string path(_path);
    pathcatslash(path);
    
    //LOGDEB("VirtualDir::addFile: path " << path << " name " << name << endl);

    if (m_dirs.find(path) == m_dirs.end()) {
        m_dirs[path] = DirEnt();
        
        UpnpAddVirtualDir(path.c_str()
#if defined(PUPNP_VDIR_1COOKIE) || defined(PUPNP_VDIR_2COOKIES)
                          , 0, 0
#endif
            );
    }

    FileEnt entry;
    entry.mtime = time(0);
    entry.mimetype = mimetype;
    entry.content = content;
    m_dirs[path].files[name] = entry;
    // LOGDEB("VirtualDir::addFile: added entry for dir " <<
    // path << " name " << name << endl);
    return true;
}

bool VirtualDir::addVDir(const std::string& _path, FileOps fops)
{
    string path(_path);
    pathcatslash(path);
    if (m_dirs.find(path) == m_dirs.end()) {
        m_dirs[path] = DirEnt(true);
        UpnpAddVirtualDir(path.c_str()
#if defined(PUPNP_VDIR_1COOKIE) || defined(PUPNP_VDIR_2COOKIES)
                          , 0, 0
#endif
            );
    }
    m_dirs[path].ops = fops;
    return true;
}


//////////
// Methods for the libupnp interface:

struct Handle {
    Handle(FileEnt *e, DirEnt *d = nullptr, void *vh = 0)
        : entry(e), dir(d), vhandle(vh), offset(0) {
    }
    FileEnt *entry;
    DirEnt *dir;
    void *vhandle;
    size_t offset;
};

static int vdclose(UpnpWebFileHandle fileHnd
#if defined(PUPNP_VDIR_1COOKIE) || defined(PUPNP_VDIR_2COOKIES)
                   , const void*
#endif
#if defined(PUPNP_VDIR_2COOKIES)
                   , const void*
#endif
    )
{
    Handle *h = (Handle*)fileHnd;
    if (h->vhandle) {
        h->dir->ops.close(h->vhandle);
    }
    delete h;
    return 0;
}

static int vdgetinfo(const char *fn, UpnpFileInfo* info
#if defined(PUPNP_VDIR_1COOKIE) || defined(PUPNP_VDIR_2COOKIES)
                     , const void*
#endif
#if defined(PUPNP_VDIR_2COOKIES)
                     , const void**
#endif
    )
{
    //LOGDEB("vdgetinfo: [" << fn << "] off_t " << sizeof(off_t) <<
    // " time_t " << sizeof(time_t) << endl);
    DirEnt *dir;
    FileEnt *entry = vdgetentry(fn, &dir);
    if (dir && dir->isvd) {
        VirtualDir::FileInfo inf;
        int ret = dir->ops.getinfo(fn, &inf);
        if (ret >= 0) {
            UpnpFileInfo_set_FileLength(info, inf.file_length);
            UpnpFileInfo_set_LastModified(info, inf.last_modified);
            UpnpFileInfo_set_IsDirectory(info, inf.is_directory);
            UpnpFileInfo_set_IsReadable(info, inf.is_readable);
            UpnpFileInfo_set_ContentType(info, inf.mime);
        }
        return ret;
    }
    if (entry == 0) {
        LOGERR("vdgetinfo: no entry for " << fn << endl);
        return -1;
    }

    UpnpFileInfo_set_FileLength(info, entry->content.size());
    UpnpFileInfo_set_LastModified(info, entry->mtime);
    UpnpFileInfo_set_IsDirectory(info, 0);
    UpnpFileInfo_set_IsReadable(info, 1);
    UpnpFileInfo_set_ContentType(info, entry->mimetype);

    return 0;
}

static UpnpWebFileHandle vdopen(const char* fn, enum UpnpOpenFileMode
#if defined(PUPNP_VDIR_1COOKIE) || defined(PUPNP_VDIR_2COOKIES)
                                , const void*
#endif
#if defined(PUPNP_VDIR_2COOKIES)
                                , const void*
#endif
    )
{
    //LOGDEB("vdopen: " << fn << endl);
    DirEnt *dir;
    FileEnt *entry = vdgetentry(fn, &dir);

    if (dir && dir->isvd) {
        void *vh = dir->ops.open(fn);
        if (vh) {
            return new Handle(nullptr, dir, vh);
        } else {
            return NULL;
        }
    }

    if (entry == 0) {
        LOGERR("vdopen: no entry for " << fn << endl);
        return NULL;
    }
    return new Handle(entry);
}

static int vdread(UpnpWebFileHandle fileHnd, char* buf, size_t buflen
#if defined(PUPNP_VDIR_1COOKIE) || defined(PUPNP_VDIR_2COOKIES)
                  , const void*
#endif
#if defined(PUPNP_VDIR_2COOKIES)
                  , const void*
#endif
    )
{
    // LOGDEB("vdread: " << endl);
    if (buflen == 0) {
        return 0;
    }
    Handle *h = (Handle *)fileHnd;
    if (h->vhandle) {
        return h->dir->ops.read(h->vhandle, buf, buflen);
    }

    if (h->offset >= h->entry->content.size()) {
        return 0;
    }
    size_t toread = buflen > h->entry->content.size() - h->offset ?
        h->entry->content.size() - h->offset : buflen;
    memcpy(buf, h->entry->content.c_str() + h->offset, toread);
    h->offset += toread;
    return (int)toread;
}

static int vdseek(UpnpWebFileHandle fileHnd, off_t offset, int origin
#if defined(PUPNP_VDIR_1COOKIE) || defined(PUPNP_VDIR_2COOKIES)
                  , const void*
#endif
#if defined(PUPNP_VDIR_2COOKIES)
                  , const void*
#endif
    )
{
    // LOGDEB("vdseek: " << endl);
    Handle *h = (Handle *)fileHnd;
    if (h->vhandle) {
        return (int)h->dir->ops.seek(h->vhandle, offset, origin);
    }
    if (origin == 0) {
        h->offset = offset;
    } else if (origin == 1) {
        h->offset += offset;
    } else if (origin == 2) {
        h->offset = h->entry->content.size() + offset;
    } else {
        return -1;
    }
    // Note that the upnp prototype is incorrect if sizeof(int)>sizeof(off_t
    // but there is nothing we can do about it.
    return (int)offset;
}

static int vdwrite(UpnpWebFileHandle, char*, size_t
#if defined(PUPNP_VDIR_1COOKIE) || defined(PUPNP_VDIR_2COOKIES)
                   , const void*
#endif
#if defined(PUPNP_VDIR_2COOKIES)
                   , const void*
#endif
    )
{
    LOGERR("vdwrite" << endl);
    return -1;
}

VirtualDir *VirtualDir::getVirtualDir()
{
    if (theDir == 0) {
        theDir = new VirtualDir();
        if (UpnpVirtualDir_set_GetInfoCallback(vdgetinfo) || 
            UpnpVirtualDir_set_OpenCallback(vdopen) ||
            UpnpVirtualDir_set_ReadCallback(vdread) || 
            UpnpVirtualDir_set_WriteCallback(vdwrite) ||
            UpnpVirtualDir_set_SeekCallback(vdseek) ||
            UpnpVirtualDir_set_CloseCallback(vdclose)) {
            LOGERR("SetVirtualDirCallbacks failed" << endl);
            delete theDir;
            theDir = 0;
            return 0;
        }
    }
    return theDir;
}

}
