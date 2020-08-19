QT       -= core gui

TARGET = upnpp
TEMPLATE = lib

# Need this because we have 2 device.cxx files
CONFIG += object_parallel_to_source
CONFIG  += qt warn_on thread
CONFIG += staticlib

INCLUDEPATH += ../../
# Note that the following needs a pseudo install in the build dir:
# cd inc;mkdir upnp;cp *.h upnp;
INCLUDEPATH += ../../../npupnp/inc/upnp


SOURCES += \
../../libupnpp/base64.cxx \
../../libupnpp/control/avlastchg.cxx \
../../libupnpp/control/avtransport.cxx \
../../libupnpp/control/cdircontent.cxx \
../../libupnpp/control/cdirectory.cxx \
../../libupnpp/control/conman.cxx \
../../libupnpp/control/description.cxx \
../../libupnpp/control/device.cxx \
../../libupnpp/control/discovery.cxx \
../../libupnpp/control/httpdownload.cxx \
../../libupnpp/control/linnsongcast.cxx \
../../libupnpp/control/mediarenderer.cxx \
../../libupnpp/control/mediaserver.cxx \
../../libupnpp/control/ohinfo.cxx \
../../libupnpp/control/ohplaylist.cxx \
../../libupnpp/control/ohproduct.cxx \
../../libupnpp/control/ohradio.cxx \
../../libupnpp/control/ohreceiver.cxx \
../../libupnpp/control/ohsender.cxx \
../../libupnpp/control/ohtime.cxx \
../../libupnpp/control/ohvolume.cxx \
../../libupnpp/control/renderingcontrol.cxx \
../../libupnpp/control/service.cxx \
../../libupnpp/control/typedservice.cxx \
../../libupnpp/device/device.cxx \
../../libupnpp/device/vdir.cxx \
../../libupnpp/log.cpp \
../../libupnpp/md5.cpp \
../../libupnpp/smallut.cpp \
../../libupnpp/soaphelp.cxx \
../../libupnpp/upnpavutils.cxx \
../../libupnpp/upnpplib.cxx
