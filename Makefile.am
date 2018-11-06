
AM_CPPFLAGS = -DDEBUG -g -Wall \
	            ${upnp_CFLAGS} ${curl_CFLAGS} ${expat_CFLAGS} \
	            -DDATADIR=\"${pkgdatadir}\" -DCONFIGDIR=\"${sysconfdir}\"

AM_CXXFLAGS = -std=c++11

lib_LTLIBRARIES = libupnpp.la

libupnpp_la_SOURCES = \
    libupnpp/control/avlastchg.cxx \
    libupnpp/control/avlastchg.hxx \
    libupnpp/control/avtransport.cxx \
    libupnpp/control/avtransport.hxx \
    libupnpp/control/cdircontent.cxx \
    libupnpp/control/cdircontent.hxx \
    libupnpp/control/cdirectory.cxx \
    libupnpp/control/cdirectory.hxx \
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
    libupnpp/device/vdir.cxx \
    libupnpp/device/vdir.hxx \
    libupnpp/base64.hxx \
    libupnpp/base64.cxx \
    libupnpp/expatmm.hxx \
    libupnpp/getsyshwaddr.c \
    libupnpp/getsyshwaddr.h \
    libupnpp/ixmlwrap.cxx \
    libupnpp/ixmlwrap.hxx \
    libupnpp/log.cpp \
    libupnpp/log.h \
    libupnpp/log.hxx \
    libupnpp/md5.cpp \
    libupnpp/md5.h \
    libupnpp/smallut.cpp \
    libupnpp/smallut.h \
    libupnpp/soaphelp.cxx \
    libupnpp/soaphelp.hxx \
    libupnpp/upnpavutils.cxx \
    libupnpp/upnpavutils.hxx \
    libupnpp/upnpp_p.hxx \
    libupnpp/upnpplib.cxx \
    libupnpp/upnpplib.hxx \
    libupnpp/workqueue.h

nobase_include_HEADERS = \
    libupnpp/base64.hxx \
    libupnpp/conf_post.h \
    libupnpp/config.h \
    libupnpp/control/avtransport.hxx \
    libupnpp/control/cdircontent.hxx \
    libupnpp/control/cdirectory.hxx \
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
    libupnpp/device/vdir.hxx \
    libupnpp/log.h \
    libupnpp/log.hxx \
    libupnpp/soaphelp.hxx \
    libupnpp/upnpavutils.hxx \
    libupnpp/upnpplib.hxx \
    libupnpp/upnpputils.hxx \
    libupnpp/workqueue.h

EXTRA_DIST = autogen.sh

if LINUX
# Curiously, -no-undefined seems to do nothing?? -Wl,-zdefs works though.
# automake 1.14.1
libupnpp_la_LDFLAGS = -Wl,-zdefs -no-undefined -version-info $(VERSION_INFO)
endif

libupnpp_la_LIBADD = $(LIBUPNPP_LIBS)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libupnpp.pc

dist-hook:
	test -z "`git status -s | egrep -v '?? libupnpp-.*/'`" 
	git tag -a libupnpp-v$(VERSION) -m 'version $(VERSION)'

