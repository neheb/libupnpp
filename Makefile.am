
AM_CPPFLAGS = -DDEBUG -g -Wall -DUPNPP_DLL \
	            ${upnp_CFLAGS} ${curl_CFLAGS} ${expat_CFLAGS} \
	            -DDATADIR=\"${pkgdatadir}\" -DCONFIGDIR=\"${sysconfdir}\"

AM_CXXFLAGS = -std=c++11

lib_LTLIBRARIES = libupnpp.la


if LINUX
libupnpp_la_CXXFLAGS = -fvisibility=hidden
# log.cpp needs a different visibility setting because we don't want to
# include upnppexports.h in this generic file, but we do want to export the
# log functions. Tried with a make target-specific variable:
#   libupnpp/libupnpp_la_log.lo: libupnpp_la_CXXFLAGS = -fvisibility=default
# and could not make it work. So, as automake only supports per-target
# flags, have to use a separate utility static lib for compiling log.cpp
# This generates a bogus warning about linking a .a into a .so
instdir=/usr/lib
inst_LIBRARIES = libutility.a
libutility_a_SOURCES = \
    libupnpp/log.cpp \
    libupnpp/log.h \
    libupnpp/log.hxx
libutility_a_CXXFLAGS = -fPIC -DPIC
endif

libupnpp_la_SOURCES = \
    libupnpp/control/avlastchg.cxx \
    libupnpp/control/avlastchg.hxx \
    libupnpp/control/avtransport.cxx \
    libupnpp/control/avtransport.hxx \
    libupnpp/control/cdircontent.cxx \
    libupnpp/control/cdircontent.hxx \
    libupnpp/control/cdirectory.cxx \
    libupnpp/control/cdirectory.hxx \
    libupnpp/control/conman.cxx \
    libupnpp/control/conman.hxx \
    libupnpp/control/description.cxx \
    libupnpp/control/description.hxx \
    libupnpp/control/device.hxx \
    libupnpp/control/device.cxx \
    libupnpp/control/discovery.cxx \
    libupnpp/control/discovery.hxx \
    libupnpp/control/httpdownload.cxx \
    libupnpp/control/httpdownload.hxx \
    libupnpp/control/linnsongcast.cxx \
    libupnpp/control/linnsongcast.hxx \
    libupnpp/control/mediarenderer.cxx \
    libupnpp/control/mediarenderer.hxx \
    libupnpp/control/mediaserver.cxx \
    libupnpp/control/mediaserver.hxx \
    libupnpp/control/ohplaylist.cxx \
    libupnpp/control/ohplaylist.hxx \
    libupnpp/control/ohproduct.cxx \
    libupnpp/control/ohproduct.hxx \
    libupnpp/control/ohradio.cxx \
    libupnpp/control/ohradio.hxx \
    libupnpp/control/ohinfo.cxx \
    libupnpp/control/ohinfo.hxx \
    libupnpp/control/ohreceiver.cxx \
    libupnpp/control/ohreceiver.hxx \
    libupnpp/control/ohsender.cxx \
    libupnpp/control/ohsender.hxx \
    libupnpp/control/ohtime.cxx \
    libupnpp/control/ohtime.hxx \
    libupnpp/control/ohvolume.cxx \
    libupnpp/control/ohvolume.hxx \
    libupnpp/control/renderingcontrol.cxx \
    libupnpp/control/renderingcontrol.hxx \
    libupnpp/control/service.cxx \
    libupnpp/control/service.hxx \
    libupnpp/control/typedservice.cxx \
    libupnpp/control/typedservice.hxx \
    libupnpp/device/device.cxx \
    libupnpp/device/device.hxx \
    libupnpp/device/service.cxx \
    libupnpp/device/service.hxx \
    libupnpp/device/vdir.cxx \
    libupnpp/device/vdir.hxx \
    libupnpp/base64.hxx \
    libupnpp/base64.cxx \
    libupnpp/conf_post.h \
    libupnpp/expatmm.hxx \
    libupnpp/md5.cpp \
    libupnpp/md5.h \
    libupnpp/smallut.cpp \
    libupnpp/smallut.h \
    libupnpp/soaphelp.cxx \
    libupnpp/soaphelp.hxx \
    libupnpp/upnpavutils.cxx \
    libupnpp/upnpavutils.hxx \
    libupnpp/upnpp_p.hxx \
    libupnpp/upnppexports.hxx \
    libupnpp/upnpplib.cxx \
    libupnpp/upnpplib.hxx \
    libupnpp/workqueue.h

if ! LINUX
libupnpp_la_SOURCES += \
    libupnpp/log.cpp \
    libupnpp/log.h \
    libupnpp/log.hxx
endif    

nobase_include_HEADERS = \
    libupnpp/base64.hxx \
    libupnpp/control/avtransport.hxx \
    libupnpp/control/cdircontent.hxx \
    libupnpp/control/cdirectory.hxx \
    libupnpp/control/conman.hxx \
    libupnpp/control/description.hxx \
    libupnpp/control/device.hxx \
    libupnpp/control/discovery.hxx \
    libupnpp/control/linnsongcast.hxx \
    libupnpp/control/mediarenderer.hxx \
    libupnpp/control/mediaserver.hxx \
    libupnpp/control/ohinfo.hxx \
    libupnpp/control/ohplaylist.hxx \
    libupnpp/control/ohproduct.hxx \
    libupnpp/control/ohradio.hxx \
    libupnpp/control/ohreceiver.hxx \
    libupnpp/control/ohsender.hxx \
    libupnpp/control/ohtime.hxx \
    libupnpp/control/ohvolume.hxx \
    libupnpp/control/renderingcontrol.hxx \
    libupnpp/control/service.hxx \
    libupnpp/control/typedservice.hxx \
    libupnpp/device/device.hxx \
    libupnpp/device/service.hxx \
    libupnpp/device/vdir.hxx \
    libupnpp/log.h \
    libupnpp/log.hxx \
    libupnpp/soaphelp.hxx \
    libupnpp/upnperrcodes.hxx \
    libupnpp/upnpavutils.hxx \
    libupnpp/upnppexports.hxx \
    libupnpp/upnpplib.hxx \
    libupnpp/upnpputils.hxx

EXTRA_DIST = autogen.sh

if LINUX
# Curiously, -no-undefined seems to do nothing?? -Wl,-zdefs works though.
# automake 1.14.1
libupnpp_la_LDFLAGS = -Wl,-zdefs -no-undefined -version-info $(VERSION_INFO) 
endif

libupnpp_la_LIBADD = $(LIBUPNPP_LIBS) libutility.a

if FREEBSD
pkgconfigdir=${prefix}/libdata/pkgconfig
else
pkgconfigdir = $(libdir)/pkgconfig
endif

pkgconfig_DATA = libupnpp.pc

dist-hook:
if LINUX
	nm -g --defined-only --demangle .libs/libupnpp.so | grep ' T ' | \
           awk '{print $$3}' | diff - $(top_srcdir)/symbols-reference
endif
	test -z "`git status -s | egrep -v '?? libupnpp-.*/|?? libutility.a'`" 
	vers=`echo $(VERSION) | sed -e 's/~/_/g'`;\
	git tag -a libupnpp-v$$vers -m "version $$vers"
