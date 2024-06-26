QT       -= core gui

TARGET = upnpp
TEMPLATE = lib

# Need this because we have 2 device.cxx files
CONFIG += object_parallel_to_source
CONFIG  += qt warn_on thread
CONFIG += staticlib

INCLUDEPATH += ../

SOURCES += \
../libupnpp/base64.cxx \
../libupnpp/control/avlastchg.cxx \
../libupnpp/control/avtransport.cxx \
../libupnpp/control/cdircontent.cxx \
../libupnpp/control/cdirectory.cxx \
../libupnpp/control/conman.cxx \
../libupnpp/control/description.cxx \
../libupnpp/control/device.cxx \
../libupnpp/control/discovery.cxx \
../libupnpp/control/httpdownload.cxx \
../libupnpp/control/linnsongcast.cxx \
../libupnpp/control/mediarenderer.cxx \
../libupnpp/control/mediaserver.cxx \
../libupnpp/control/ohinfo.cxx \
../libupnpp/control/ohplaylist.cxx \
../libupnpp/control/ohproduct.cxx \
../libupnpp/control/ohradio.cxx \
../libupnpp/control/ohreceiver.cxx \
../libupnpp/control/ohsender.cxx \
../libupnpp/control/ohtime.cxx \
../libupnpp/control/ohvolume.cxx \
../libupnpp/control/renderingcontrol.cxx \
../libupnpp/control/service.cxx \
../libupnpp/control/typedservice.cxx \
../libupnpp/device/device.cxx \
../libupnpp/device/vdir.cxx \
../libupnpp/log.cpp \
../libupnpp/md5.cpp \
../libupnpp/smallut.cpp \
../libupnpp/soaphelp.cxx \
../libupnpp/upnpavutils.cxx \
../libupnpp/upnpplib.cxx

windows {
    DEFINES += UPNP_STATIC_LIB
    DEFINES += CURL_STATICLIB
    DEFINES += PSAPI_VERSION=1

    INCLUDEPATH += c:/users/bill/documents/upnp/npupnp/inc

    LIBS += -liphlpapi
    LIBS += -lwldap32
    LIBS += -lws2_32

    ## W7 with mingw
    contains(QMAKE_CC, gcc){
      INCLUDEPATH += $$PWD/../../../expat-2.1.0/lib
      INCLUDEPATH += $$PWD/../../../curl-7.70.0/include
      QMAKE_CXXFLAGS += -std=c++14 -Wno-unused-parameter
    }


    # W10 with msvc 2017
    contains(QMAKE_CC, cl){
      DEFINES += NOMINMAX
      INCLUDEPATH += $$PWD/../../expat.v140.2.4.1.1/build/native/include
      INCLUDEPATH += $$PWD/../../curl-7.70.0/include
    }
}

mac {
    QMAKE_APPLE_DEVICE_ARCHS = x86_64 arm64
    INCLUDEPATH += ../../npupnp/inc/upnp
}
