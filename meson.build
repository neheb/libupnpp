project(
  'libupnpp',
  'cpp',
  license: 'LGPL 2.1+',
  version: '0.26.4',
  default_options: 'cpp_std=c++17',
  meson_version: '>=0.49',
)

# Change this only when the library interface would become incompatible with a binary linked with a
# previous version. A change should also result in changing the Debian binary package name so that
# both library versions can cohexist on a system.
libupnpp_soversion = 16

cpp = meson.get_compiler('cpp')
deps = []
if host_machine.system() == 'windows'
  deps += cpp.find_library('ws2_32')
  deps += cpp.find_library('iphlpapi')
elif host_machine.system() == 'sunos'
  deps += cpp.find_library('nsl')
  deps += cpp.find_library('socket')
endif

deps += dependency('threads')
deps += dependency('libnpupnp')
deps += dependency('libcurl')
deps += dependency('expat')

# Note: meson sets -D_FILE_OFFSET_BITS=64 by default
if get_option('default_library') != 'static'
  add_project_arguments('-DUPNPP_DLL', language: 'cpp')
endif

add_project_arguments(
  cpp.get_supported_arguments(
    '-Wno-deprecated-declarations',
    '/D_CRT_SECURE_NO_WARNINGS',
    '/wd4251',
  ),
  language: 'cpp',
)

# We need different build flags to export symbols from log.cpp because we don't want to use
# the ones specific to libupnpp in this shared file. The "simplest" way to do this is to use a
# separate static library.
libupnpputil_sources = files(
    'libupnpp/log.cpp',
    'libupnpp/log.h',
    'libupnpp/log.hxx',
)

libupnpputil = static_library(
    'libupnpputil',
    libupnpputil_sources,
    gnu_symbol_visibility: 'default',
    name_prefix: '',
    install: false,
    cpp_args : '-fPIC',
    cpp_args : '-DPIC',
)

libupnpp_incdir = include_directories('libupnpp')

libupnpp_sources = files(
    'libupnpp/base64.cxx',
    'libupnpp/base64.hxx',
    'libupnpp/conf_post.h',
    'libupnpp/control/avlastchg.cxx',
    'libupnpp/control/avlastchg.hxx',
    'libupnpp/control/avtransport.cxx',
    'libupnpp/control/avtransport.hxx',
    'libupnpp/control/cdircontent.cxx',
    'libupnpp/control/cdircontent.hxx',
    'libupnpp/control/cdirectory.cxx',
    'libupnpp/control/cdirectory.hxx',
    'libupnpp/control/conman.cxx',
    'libupnpp/control/conman.hxx',
    'libupnpp/control/description.cxx',
    'libupnpp/control/description.hxx',
    'libupnpp/control/device.cxx',
    'libupnpp/control/device.hxx',
    'libupnpp/control/discovery.cxx',
    'libupnpp/control/discovery.hxx',
    'libupnpp/control/httpdownload.cxx',
    'libupnpp/control/httpdownload.hxx',
    'libupnpp/control/linnsongcast.cxx',
    'libupnpp/control/linnsongcast.hxx',
    'libupnpp/control/mediarenderer.cxx',
    'libupnpp/control/mediarenderer.hxx',
    'libupnpp/control/mediaserver.cxx',
    'libupnpp/control/mediaserver.hxx',
    'libupnpp/control/ohinfo.cxx',
    'libupnpp/control/ohinfo.hxx',
    'libupnpp/control/ohplaylist.cxx',
    'libupnpp/control/ohplaylist.hxx',
    'libupnpp/control/ohproduct.cxx',
    'libupnpp/control/ohproduct.hxx',
    'libupnpp/control/ohradio.cxx',
    'libupnpp/control/ohradio.hxx',
    'libupnpp/control/ohreceiver.cxx',
    'libupnpp/control/ohreceiver.hxx',
    'libupnpp/control/ohsender.cxx',
    'libupnpp/control/ohsender.hxx',
    'libupnpp/control/ohtime.cxx',
    'libupnpp/control/ohtime.hxx',
    'libupnpp/control/ohvolume.cxx',
    'libupnpp/control/ohvolume.hxx',
    'libupnpp/control/renderingcontrol.cxx',
    'libupnpp/control/renderingcontrol.hxx',
    'libupnpp/control/service.cxx',
    'libupnpp/control/service.hxx',
    'libupnpp/control/typedservice.cxx',
    'libupnpp/control/typedservice.hxx',
    'libupnpp/device/device.cxx',
    'libupnpp/device/device.hxx',
    'libupnpp/device/service.cxx',
    'libupnpp/device/service.hxx',
    'libupnpp/device/vdir.cxx',
    'libupnpp/device/vdir.hxx',
    'libupnpp/expatmm.h',
    'libupnpp/log.h',
    'libupnpp/log.hxx',
    'libupnpp/md5.cpp',
    'libupnpp/md5.h',
    'libupnpp/smallut.cpp',
    'libupnpp/smallut.h',
    'libupnpp/soaphelp.cxx',
    'libupnpp/soaphelp.hxx',
    'libupnpp/upnpavutils.cxx',
    'libupnpp/upnpavutils.hxx',
    'libupnpp/upnperrcodes.hxx',
    'libupnpp/upnppexports.hxx',
    'libupnpp/upnpplib.cxx',
    'libupnpp/upnpplib.hxx',
    'libupnpp/upnpp_p.hxx',
    'libupnpp/upnpputils.hxx',
    'libupnpp/workqueue.h',
)

auto = configuration_data()
ver = meson.project_version().split('.')
auto.set('LIBUPNPP_VERSION_MAJOR', ver[0])
auto.set('LIBUPNPP_VERSION_MINOR', ver[1])
auto.set('LIBUPNPP_VERSION_REVISION', ver[2])
auto.set_quoted('PACKAGE_VERSION', meson.project_version())
cfile = configure_file(input: 'libupnpp/config.h.in', output: 'config.h',
                       configuration: auto)


libupnpp = library(
  'libupnpp',
  libupnpp_sources,
  gnu_symbol_visibility: 'hidden',
  name_prefix: '',
  soversion: host_machine.system() == 'windows' ? '' : libupnpp_soversion,
  include_directories: libupnpp_incdir,
  dependencies: deps,
  install: true,
  link_with: libupnpputil,
)

pkg = import('pkgconfig')
pkg.generate(
  libupnpp,
  description: 'Another SDK for UPnP Devices',
  subdirs: 'libupnpp',
)

install_headers(
    'libupnpp/base64.hxx',
    'libupnpp/log.h',
    'libupnpp/log.hxx',
    'libupnpp/soaphelp.hxx',
    'libupnpp/upnpavutils.hxx',
    'libupnpp/upnperrcodes.hxx',
    'libupnpp/upnppexports.hxx',
    'libupnpp/upnpplib.hxx',
    'libupnpp/upnpputils.hxx',
    subdir: 'libupnpp',
)

install_headers(
    'libupnpp/device/device.hxx',
    'libupnpp/device/service.hxx',
    'libupnpp/device/vdir.hxx',
    subdir: 'libupnpp/device',
)

install_headers(
    'libupnpp/control/avtransport.hxx',
    'libupnpp/control/cdircontent.hxx',
    'libupnpp/control/cdirectory.hxx',
    'libupnpp/control/conman.hxx',
    'libupnpp/control/description.hxx',
    'libupnpp/control/device.hxx',
    'libupnpp/control/discovery.hxx',
    'libupnpp/control/linnsongcast.hxx',
    'libupnpp/control/mediarenderer.hxx',
    'libupnpp/control/mediaserver.hxx',
    'libupnpp/control/ohinfo.hxx',
    'libupnpp/control/ohplaylist.hxx',
    'libupnpp/control/ohproduct.hxx',
    'libupnpp/control/ohradio.hxx',
    'libupnpp/control/ohreceiver.hxx',
    'libupnpp/control/ohsender.hxx',
    'libupnpp/control/ohtime.hxx',
    'libupnpp/control/ohvolume.hxx',
    'libupnpp/control/renderingcontrol.hxx',
    'libupnpp/control/service.hxx',
    'libupnpp/control/typedservice.hxx',
    subdir: 'libupnpp/control',
)

libuppnpp_dep = declare_dependency(
  include_directories: libupnpp_incdir,
  link_with: libupnpp,
)
