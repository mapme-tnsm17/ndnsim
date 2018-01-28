## -*- Mode: python; py-indent-offset: 4; indent-tabs-mode: nil; coding: utf-8; -*-

import os
from waflib import Logs, Utils, Options, TaskGen, Task
from waflib.Errors import WafError

import wutils

REQUIRED_BOOST_LIBS = ['graph', 'thread', 'unit_test_framework',
                       'system', 'random', 'date_time', 'iostreams', 'regex', 'program_options', 'chrono', 'filesystem']

def required_boost_libs(conf):
    conf.env.REQUIRED_BOOST_LIBS += REQUIRED_BOOST_LIBS

def options(opt):

    # BEGIN EXTENSIONS (1/2)
    #
    # We need to duplicate here both NFD and ndn-cxx extensions since their
    # wscript's are not used by ndnSIM.

    opt.add_option('--without-mapme', help='Disable MAPME mobility support',
        dest='without_mapme', action='store_true', default=False)
    opt.add_option('--without-kite', help='Disable KITE mobility support',
        dest='without_kite', action='store_true', default=False)
    opt.add_option('--without-anchor', help='Disable anchor mobility support',
        dest='without_anchor', action='store_true', default=False)
    opt.add_option('--without-path-labelling', help='Disable path labelling',
        dest='without_path_labelling', action='store_true', default=False)
    opt.add_option('--without-raaqm', help='Disable RAAQM congestion control',
        dest='without_raaqm', action='store_true', default=False)
    opt.add_option('--without-conf-file', help='Disable configuration file for forwarder',
        dest='without_conf_file', action='store_true', default=False)
    opt.add_option('--without-lb-strategy', help='Disable load balance strategy',
        dest='without_lb_strategy', action='store_true', default=False)
    opt.add_option('--without-fix-random', help='Disable fix for randomness',
        dest='without_fix_random', action='store_true', default=False)
    opt.add_option('--with-hop-count', help='Enable hop count',
        dest='with_hop_count', action='store_true', default=False)
    opt.add_option('--without-unicast-ethernet', help='Enable unicast ethernet support',
        dest='without_unicast_ethernet', action='store_true', default=False)
    opt.add_option('--without-bugfixes', help='Disable bugfixes',
        dest='without_bugfixes', action='store_true', default=False)
    opt.add_option('--without-cache-extensions', help='Disable cache extensions',
        dest='without_cache_extensions', action='store_true', default=False)
    opt.add_option('--without-fib-extensions', help='Disable FIB extensions',
        dest='without_fib_extensions', action='store_true', default=False)
    opt.add_option('--without-wldr', help='Disable WLDR',
        dest='without_wldr', action='store_true', default=False)
    opt.add_option('--without-mldr', help='Disable MLDR',
        dest='without_mldr', action='store_true', default=False)

    # Simulation-specific extensions
    opt.add_option('--without-network-dynamics', help='Disable additions to support dynamic simulations',
        dest='without_network_dynamics', action='store_true', default=False)
    opt.add_option('--without-face-up-down', help='Disable face up/down support',
        dest='without_face_up_down', action='store_true', default=False)
    opt.add_option('--without-globalrouting-updates', help='Disable GlobalRouting updates',
        dest='without_globalrouting_updates', action='store_true', default=False)

    # END EXTENSIONS (1/2)

    opt.load(['version'], tooldir=['%s/.waf-tools' % opt.path.abspath()])
    opt.load(['doxygen', 'sphinx_build', 'type_traits', 'compiler-features', 'cryptopp', 'sqlite3'],
             tooldir=['%s/ndn-cxx/.waf-tools' % opt.path.abspath()])

def configure(conf):
    conf.load(['doxygen', 'sphinx_build', 'type_traits', 'compiler-features', 'version', 'cryptopp', 'sqlite3'])

    conf.env['ENABLE_NDNSIM']=False

    if not os.environ.has_key('PKG_CONFIG_PATH'):
        os.environ['PKG_CONFIG_PATH'] = ':'.join([
            '/usr/local/lib/pkgconfig',
            '/usr/local/lib64/pkgconfig',
            '/usr/local/lib32/pkgconfig',
            '/opt/local/lib/pkgconfig'])

    conf.check_cxx(lib='pthread', uselib_store='PTHREAD', define_name='HAVE_PTHREAD', mandatory=False)
    conf.check_sqlite3(mandatory=True)
    conf.check_cryptopp(mandatory=True, use='PTHREAD')

    if not conf.env['LIB_BOOST']:
        conf.report_optional_feature("ndnSIM", "ndnSIM", False,
                                     "Required boost libraries not found")
        Logs.error ("ndnSIM will not be build as it requires boost libraries of version at least 1.48")
        conf.env['MODULES_NOT_BUILT'].append('ndnSIM')
        return
    else:
        present_boost_libs = []
        for boost_lib_name in conf.env['LIB_BOOST']:
            if boost_lib_name.startswith("boost_"):
                boost_lib_name = boost_lib_name[6:]
            if boost_lib_name.endswith("-mt"):
                boost_lib_name = boost_lib_name[:-3]
            present_boost_libs.append(boost_lib_name)

        missing_boost_libs = [lib for lib in REQUIRED_BOOST_LIBS if lib not in present_boost_libs]

        if missing_boost_libs != []:
            conf.report_optional_feature("ndnSIM", "ndnSIM", False,
                                         "ndnSIM requires boost libraries: %s" % ' '.join(missing_boost_libs))
            conf.env['MODULES_NOT_BUILT'].append('ndnSIM')

            Logs.error ("ndnSIM will not be build as it requires boost libraries: %s" % ' '.join(missing_boost_libs))
            Logs.error ("Please upgrade your distribution or install custom boost libraries (http://ndnsim.net/faq.html#boost-libraries)")
            return

        boost_version = conf.env.BOOST_VERSION.split('_')
        if int(boost_version[0]) < 1 or int(boost_version[1]) < 48:
            conf.report_optional_feature("ndnSIM", "ndnSIM", False,
                                         "ndnSIM requires at least boost version 1.48")
            conf.env['MODULES_NOT_BUILT'].append('ndnSIM')

            Logs.error ("ndnSIM will not be build as it requires boost libraries of version at least 1.48")
            Logs.error ("Please upgrade your distribution or install custom boost libraries (http://ndnsim.net/faq.html#boost-libraries)")
            return

    conf.env['ENABLE_NDNSIM']=True;
    conf.env['MODULES_BUILT'].append('ndnSIM')

    # BEGIN EXTENSIONS (2/2)

    conf.env['WITH_MAPME']            = not Options.options.without_mapme
    conf.env['WITH_KITE']             = not Options.options.without_kite
    conf.env['WITH_ANCHOR']           = not Options.options.without_anchor
    conf.env['WITH_PATH_LABELLING']   = not Options.options.without_path_labelling
    conf.env['WITH_RAAQM']            = not Options.options.without_raaqm
    conf.env['WITH_CONF_FILE']        = not Options.options.without_conf_file
    conf.env['WITH_LB_STRATEGY']      = not Options.options.without_lb_strategy
    conf.env['WITH_FIX_RANDOM']       = not Options.options.without_fix_random
    conf.env['WITH_HOP_COUNT']        = False # existing feature in ndnSIM
    conf.env['WITH_UNICAST_ETHERNET'] = not Options.options.without_unicast_ethernet
    conf.env['WITH_BUGFIXES']         = not Options.options.without_bugfixes
    conf.env['WITH_CACHE_EXTENSIONS'] = not Options.options.without_cache_extensions
    conf.env['WITH_FIB_EXTENSIONS']   = not Options.options.without_fib_extensions
    conf.env['WITH_WLDR']             = not Options.options.without_wldr
    conf.env['WITH_MLDR']             = not Options.options.without_mldr

    conf.env['WITH_NETWORK_DYNAMICS'] = not Options.options.without_network_dynamics
    conf.env['WITH_FACE_UP_DOWN']     = not Options.options.without_face_up_down
    conf.env['WITH_GLOBALROUTING_UPDATES'] = not Options.options.without_globalrouting_updates

    extensions = ['MAPME', 'KITE', 'ANCHOR', 'PATH_LABELLING', 'RAAQM', 'CONF_FILE',
                  'LB_STRATEGY', 'FIX_RANDOM', 'HOP_COUNT', 'UNICAST_ETHERNET',
                  'BUGFIXES', 'CACHE_EXTENSIONS', 'FIB_EXTENSIONS', 'WLDR', 'MLDR',
                  'NETWORK_DYNAMICS', 'FACE_UP_DOWN', 'GLOBALROUTING_UPDATES']

    for extension in extensions:
      if conf.env['WITH_%s' % extension]:
        conf.env.append_value('DEFINES', extension)
        conf.env.append_value('DEFINES', 'NDN_CXX_%s' % extension)
        conf.msg('Compilation flag %s set' % extension, 'yes')
      else:
        conf.msg('Compilation flag %s set' % extension, 'not', color='YELLOW')

    # END EXTENSIONS (2/2)

    # Simulation-specific code
    conf.env.append_value('DEFINES', 'NDNSIM')

    conf.report_optional_feature("ndnSIM", "ndnSIM", True, "")

    conf.write_config_header('../../ns3/ndnSIM/ndn-cxx/ndn-cxx-config.hpp', define_prefix='NDN_CXX_', remove=False)
    conf.write_config_header('../../ns3/ndnSIM/NFD/config.hpp', remove=False)

def build(bld):
    (base, build, split) = bld.getVersion('NFD')
    bld(features="subst",
        name="version-NFD",
        source='NFD/version.hpp.in', target='../../ns3/ndnSIM/NFD/version.hpp',
        install_path=None,
        VERSION_STRING=base,
        VERSION_BUILD="%s-ndnSIM" % build,
        VERSION=int(split[0]) * 1000000 + int(split[1]) * 1000 + int(split[2]),
        VERSION_MAJOR=split[0], VERSION_MINOR=split[1], VERSION_PATCH=split[2])

    (base, build, split) = bld.getVersion('ndn-cxx')
    bld(features="subst",
        name="version-ndn-cxx",
        source='ndn-cxx/src/version.hpp.in', target='../../ns3/ndnSIM/ndn-cxx/version.hpp',
        install_path=None,
        VERSION_STRING=base,
        VERSION_BUILD="%s-ndnSIM" % build,
        VERSION=int(split[0]) * 1000000 + int(split[1]) * 1000 + int(split[2]),
        VERSION_MAJOR=split[0], VERSION_MINOR=split[1], VERSION_PATCH=split[2])

    deps = ['core', 'network', 'point-to-point', 'topology-read', 'mobility', 'internet']
    if 'ns3-visualizer' in bld.env['NS3_ENABLED_MODULES']:
        deps.append('visualizer')

    if bld.env.ENABLE_EXAMPLES:
        deps += ['point-to-point-layout', 'csma', 'applications', 'wifi']

    ndnCxxSrc = bld.path.ant_glob('ndn-cxx/src/**/*.cpp',
                                  excl=['ndn-cxx/src/**/*-osx.cpp',
                                        'ndn-cxx/src/util/dummy-client-face.cpp'])

    nfdSrc = bld.path.ant_glob(['%s/**/*.cpp' % dir for dir in ['NFD/core', 'NFD/daemon', 'NFD/rib']],
                               excl=['NFD/core/network-interface.cpp',
                                     'NFD/daemon/main.cpp',
                                     'NFD/daemon/nfd.cpp',
                                     'NFD/daemon/face/ethernet*',
                                     'NFD/daemon/face/multicast-udp*',
                                     'NFD/daemon/face/tcp*',
                                     'NFD/daemon/face/udp*',
                                     'NFD/daemon/face/unix-stream*',
                                     'NFD/daemon/face/websocket*',
                                     'NFD/rib/nrd.cpp'])

    module = bld.create_ns3_module('ndnSIM', deps)
    module.module = 'ndnSIM'
    module.features += ' ns3fullmoduleheaders ndncxxheaders'
    module.use += ['version-ndn-cxx', 'version-NFD', 'BOOST', 'CRYPTOPP', 'SQLITE3', 'RT', 'PTHREAD']
    module.includes = ['../..', '../../ns3/ndnSIM/NFD', './NFD/core', './NFD/daemon', './NFD/rib', '../../ns3/ndnSIM', '../../ns3/ndnSIM/ndn-cxx']
    module.export_includes = ['../../ns3/ndnSIM/NFD', './NFD/core', './NFD/daemon', './NFD/rib', '../../ns3/ndnSIM']

    headers = bld(features='ns3header')
    headers.module = 'ndnSIM'
    headers.source = ["ndn-all.hpp"]

    if not bld.env['ENABLE_NDNSIM']:
        bld.env['MODULES_NOT_BUILT'].append('ndnSIM')
        return

    module_dirs = ['apps', 'helper', 'model', 'utils']
    module.source = bld.path.ant_glob(['%s/**/*.cpp' % dir for dir in module_dirs],
                                      excl=['model/ip-faces/*']) + ndnCxxSrc + nfdSrc

    module_dirs = ['NFD/core', 'NFD/daemon', 'NFD/rib', 'apps', 'helper', 'model', 'utils']
    module.full_headers = bld.path.ant_glob(['%s/**/*.hpp' % dir for dir in module_dirs])
    module.full_headers += bld.path.ant_glob('NFD/common.hpp')

    module.ndncxx_headers = bld.path.ant_glob(['ndn-cxx/src/**/*.hpp'],
                                              excl=['src/**/*-osx.hpp', 'src/detail/**/*'])
    if bld.env.ENABLE_EXAMPLES:
        bld.recurse('examples')

    if bld.env.ENABLE_TESTS:
        bld.recurse('tests')

    bld.ns3_python_bindings()

@TaskGen.feature('ns3fullmoduleheaders')
@TaskGen.after_method('process_rule')
def apply_ns3fullmoduleheaders(self):
    # ## get all of the ns3 headers
    ns3_dir_node = self.bld.path.find_or_declare("ns3")

    mode = getattr(self, "mode", "install")

    for src_node in set(self.full_headers):
        dst_node = ns3_dir_node.find_or_declare(src_node.path_from(self.bld.path.find_dir('src')))
        assert dst_node is not None

        relpath = src_node.parent.path_from(self.bld.path.find_dir('src'))

        task = self.create_task('ns3header')
        task.mode = getattr(self, 'mode', 'install')
        if task.mode == 'install':
            self.bld.install_files('${INCLUDEDIR}/%s%s/ns3/%s' % (wutils.APPNAME, wutils.VERSION, relpath),
                                   [src_node])
            task.set_inputs([src_node])
            task.set_outputs([dst_node])
        else:
            task.header_to_remove = dst_node

@TaskGen.feature('ndncxxheaders')
@TaskGen.after_method('process_rule')
def apply_ndnsim_moduleheaders(self):
    # ## get all of the ns3 headers
    ndncxx_dir_node = self.bld.path.find_or_declare("ns3/ndnSIM/ndn-cxx")

    mode = getattr(self, "mode", "install")

    for src_node in set(self.ndncxx_headers):
        dst_node = ndncxx_dir_node.find_or_declare(src_node.path_from(self.bld.path.find_dir('src/ndnSIM/ndn-cxx/src')))
        assert dst_node is not None

        relpath = src_node.parent.path_from(self.bld.path.find_dir('src/ndnSIM/ndn-cxx/src'))

        task = self.create_task('ns3header')
        task.mode = getattr(self, 'mode', 'install')
        if task.mode == 'install':
            self.bld.install_files('${INCLUDEDIR}/%s%s/ns3/ndnSIM/ndn-cxx/%s' % (wutils.APPNAME, wutils.VERSION, relpath),
                                   [src_node])
            task.set_inputs([src_node])
            task.set_outputs([dst_node])
        else:
            task.header_to_remove = dst_node
    # 
