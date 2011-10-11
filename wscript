#!/usr/bin/env python

import Options
import sys
from os import unlink, removedirs, symlink, popen, environ, makedirs
from os.path import exists, lexists, abspath
from shutil import copy2, copytree, rmtree

srcdir = "."
top = "release"
blddir = "build"
APPNAME = "eegdriver"
VERSION = "0.1.0"

def canonical_cpu_type(arch):
	m = {'x86': 'ia32', 'i386':'ia32', 'x86_64':'x64', 'amd64':'x64'}
	if arch in m: arch = m[arch]
	if not arch in supported_archs:
		raise Exception("supported architectures are "+', '.join(supported_archs)+" but NOT '" + arch + "'.")
	return arch


def set_options(opt):
	opt.tool_options("compiler_cc")
	opt.tool_options("compiler_cxx")
	opt.add_option( '--debug'
								, action='store_true'
								, default=False
								, help='Build debug variant [Default: False]'
								, dest='debug'
								)	

def configure(conf):
	if 'DEST_CPU' in environ and environ['DEST_CPU']:
		conf.env['DEST_CPU'] = canonical_cpu_type(os.environ['DEST_CPU'])
	elif 'DEST_CPU' in conf.env and conf.env['DEST_CPU']:
		conf.env['DEST_CPU'] = canonical_cpu_type(conf.env['DEST_CPU'])
	
	conf.check_tool("compiler_cc")
	conf.check_tool("compiler_cxx")
	conf.check_tool("node_addon")
	#conf.env.append_value('CXXFLAGS', ['-DDEBUG', '-g', '-O0'])
	#conf.env.append_value('CXXFLAGS', ['-Wall', '-Wextra'])
	conf.env.append_value('CFLAGS', ['-Os', '-ffunction-sections', '-fdata-sections', '-fPIC'])
	conf.env.append_value('CXXFLAGS', ['-Os', '-ffunction-sections', '-fdata-sections', '-fPIC'])
	
	if sys.platform.startswith("darwin"):
		conf.env.append_value('LINKFLAGS', ['-Wl,-dead_strip'])
		conf.env.append_value('LINKFLAGS', ['-Wl,-bind_at_load'])
	elif sys.platform.startswith("linux"):
		conf.env.append_value('LINKFLAGS', ['-Wl,--gc-sections', '-Wl,--as-needed'])
		conf.env.append_value('CXXFLAGS', ['-fno-rtti'])
	
	conf.define("HAVE_CONFIG_H", 1)

	if sys.platform.startswith("sunos"):
		conf.env.append_value ('CCFLAGS', '-threads')
		conf.env.append_value ('CXXFLAGS', '-threads')
		#conf.env.append_value ('LINKFLAGS', ' -threads')
	elif not sys.platform.startswith("cygwin"):
		conf.env.append_value ('CCFLAGS', '-pthread')
		conf.env.append_value ('CXXFLAGS', '-pthread')
		conf.env.append_value ('LINKFLAGS', '-pthread')

	# conf.check(lib='node', libpath=['/usr/lib', '/usr/local/lib'], uselib_store='NODE')


def build(bld):
	eegdriver = bld.new_task_gen("cxx", "shlib", "node_addon", install_path=None)
	eegdriver.name = "eegdriver"
	eegdriver.target = "eegdriver"
	eegdriver.source = ["src/eegdriver.cc"] #"src/nsser.c", "src/nsutil.c", "src/openedf.c", "src/nsnet.c", "src/monitor.c",
	#["src/monitor.h", "src/config.h", "src/edfmacros.h", "src/nsnet.h", "src/nsser.h", "src/nsutil.h", "src/openedf.h"]
	eegdriver.includes = ['/opt/local/include']
	eegdriver.cflags = ['-Wall', '-I../src']
	eegdriver.cxxflags = ['-Wall', '-I../src']

def shutdown(ctx):
	print "done"
