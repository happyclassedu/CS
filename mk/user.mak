#=============================================================================
# This is an configuration file which describes user options global to all
# Crystal Space programs and libraries.  Edit it to suit your taste.  Also
# have a look at the platform-dependent makefile for system settings.
#
# Documentation copyright (C)1999-2001 by Eric Sunshine.
#=============================================================================

# Include a local makefile where you can add additional targets or whatever.
# This comes in handy, if you don't want to alter the original CS makefiles.
-include mk/local.mak

#-----------------------------------------------------------------------------
# Dynamic Settings
# Changes to these settings will be reflected in the makefile system
# immediately.
#-----------------------------------------------------------------------------

# Default list of plugins to build.  Note that you'll link all plugins into
# executable in the case of static build.  If you want additional plugins to
# be built either define the environment variable PLUGINS or put a line similar
# to those below into config.mak. The plugins listed in PLUGINS variable
# are always built; PLUGINS.DYNAMIC are built only if plugins are compiled
# as shared libraries. Please think twice before adding anything to PLUGINS;
# in most cases you will want to add to PLUGINS.DYNAMIC.

ifeq ($(ZLIB.AVAILABLE),yes)
PLUGINS += filesys/vfs
endif
PLUGINS += video/renderer/software
ifeq ($(GL.AVAILABLE),yes)
PLUGINS += video/renderer/opengl
endif
PLUGINS += video/loader/mplex
PLUGINS += video/loader/gif
ifeq ($(PNG.AVAILABLE),yes)
PLUGINS += video/loader/png
endif
ifeq ($(JPEG.AVAILABLE),yes)
PLUGINS += video/loader/jpg
endif
PLUGINS += video/loader/bmp
PLUGINS += video/loader/tga
PLUGINS += video/effects
PLUGINS += sound/loader/mplex
PLUGINS += sound/loader/au
PLUGINS += sound/loader/aiff
PLUGINS += sound/loader/iff
PLUGINS += sound/loader/wav
PLUGINS += font/server/csfont 
PLUGINS += console/output/simple
PLUGINS += console/input/standard
PLUGINS += collide/rapid
PLUGINS += collide/opcode
PLUGINS += perfstat
PLUGINS += bugplug
PLUGINS += reporter stdrep
PLUGINS += mesh/crossbld
PLUGINS += mesh/impexp/ieplex
PLUGINS += mesh/spr2d/object       mesh/spr2d/persist/standard
PLUGINS += mesh/spr3d/object       mesh/spr3d/persist/standard
PLUGINS +=                         mesh/spr3d/persist/binary
PLUGINS += mesh/fountain/object    mesh/fountain/persist/standard
PLUGINS += mesh/explo/object       mesh/explo/persist/standard
PLUGINS += mesh/fire/object        mesh/fire/persist/standard
PLUGINS += mesh/snow/object        mesh/snow/persist/standard
PLUGINS += mesh/rain/object        mesh/rain/persist/standard
PLUGINS += mesh/spiral/object      mesh/spiral/persist/standard
PLUGINS += mesh/ball/object        mesh/ball/persist/standard
PLUGINS += mesh/genmesh/object     mesh/genmesh/persist/standard
PLUGINS +=                         mesh/genmesh/persist/tree
PLUGINS += mesh/emit/object        mesh/emit/persist/standard
PLUGINS += mesh/metaball/object    mesh/metaball/persist/standard
PLUGINS += mesh/terrfunc/object    mesh/terrfunc/persist/standard
PLUGINS += mesh/terrbig
PLUGINS += mesh/null/object
PLUGINS += mesh/bcterr/object      mesh/bcterr/persist/standard
PLUGINS += mesh/metagen/object
PLUGINS += mesh/stars/object       mesh/stars/persist/standard
PLUGINS += mesh/haze/object        mesh/haze/persist/standard
PLUGINS += mesh/thing/object       mesh/thing/persist/standard
PLUGINS += mesh/cloth/object
PLUGINS += motion/standard/object  motion/standard/persist/standard
PLUGINS += engine/3d
PLUGINS += culling/dynavis
PLUGINS += culling/frustvis
PLUGINS += csparser
PLUGINS += csparser/services
PLUGINS += cssaver
PLUGINS += sequence
PLUGINS += engseq

PLUGINS.DYNAMIC += engine/iso
PLUGINS.DYNAMIC += isoldr
PLUGINS.DYNAMIC += video/renderer/line
PLUGINS.DYNAMIC += video/renderer/null
PLUGINS.DYNAMIC += video/renderer/inf
PLUGINS.DYNAMIC += video/canvas/null2d
PLUGINS.DYNAMIC += video/canvas/memory
PLUGINS.DYNAMIC += video/loader/sgi
PLUGINS.DYNAMIC += video/loader/wal
ifeq ($(MNG.AVAILABLE),yes)
PLUGINS.DYNAMIC += video/loader/jng
endif
ifeq ($(SOCKET.AVAILABLE),yes)
PLUGINS.DYNAMIC += net/driver/socket
PLUGINS.DYNAMIC += net/driver/ensocket
endif
PLUGINS.DYNAMIC += net/manager
PLUGINS.DYNAMIC += console/output/standard
PLUGINS.DYNAMIC += console/output/fancy
PLUGINS.DYNAMIC += mesh/impexp/ase
PLUGINS.DYNAMIC += mesh/impexp/md2
PLUGINS.DYNAMIC += mesh/impexp/mdl
PLUGINS.DYNAMIC += mesh/impexp/obj
PLUGINS.DYNAMIC += mesh/impexp/pov
PLUGINS.DYNAMIC += mesh/impexp/spr
#PLUGINS.DYNAMIC += mesh/impexp/3ds
PLUGINS.DYNAMIC += font/server/fontplex
#PLUGINS.DYNAMIC += font/server/freefnt2
PLUGINS.DYNAMIC += aws
#PLUGINS.DYNAMIC += sound/loader/ogg
#PLUGINS.DYNAMIC += sound/loader/mod
#PLUGINS.DYNAMIC += sound/renderer/openal
#PLUGINS.DYNAMIC += physics/odedynam
#PLUGINS.DYNAMIC += physics/loader
#PLUGINS.DYNAMIC += video/render3d/opengl
#PLUGINS.DYNAMIC += video/render3d/shadermgr
#PLUGINS.DYNAMIC += video/render3d/shaderplugins/glshader_arb

ifeq ($(PYTHON.AVAILABLE),yes)
PLUGINS.DYNAMIC += cscript/cspython
endif

#PLUGINS.DYNAMIC += cscript/cslua

# Unfortunately, we can not yet enable this module automatically -- even if
# the configuration script detects its presence -- since it fails to build on
# most platforms.
ifeq ($(PERL.AVAILABLE),yes)
#PLUGINS.DYNAMIC += cscript/csperl5
endif


#-----------------------------------------------------------------------------
# Static Settings            *** TAKE NOTE ***
# After changing the settings in this section, you must re-run the makefile
# configuration step (for example, "make unknown" followed by "make linux")
# in order for your changes to become effective.
#-----------------------------------------------------------------------------

# Should we build drivers/plugins as loadable modules?
ifndef USE_PLUGINS
  USE_PLUGINS=yes
endif

# Should we build libraries as shared/dynamic libraries?
# Currently only supported on Unix
# NOTE! Be careful enabling this option. It hasn't been tested well
# and it will not work in some situations (like with 'make install').
ifndef USE_SHARED_LIBS
  USE_SHARED_LIBS=no
endif

# Default build mode (either "optimize", "debug", or "profile").
ifndef MODE
  MODE=optimize
endif

# If MODE is "debug", should we use the extensive heap validating machinery?
# Do not enable this if you platform or your own code uses an overloaded `new'
# operator.
ifndef EXTENSIVE_MEMDEBUG
  EXTENSIVE_MEMDEBUG=no
endif

# Enable strict smart pointers. If this is set to yes then csPtr return
# values can ONLY be assigned to csRef's. So casting of csPtr to normal pointers
# does not work. This is a good way to ensure that your code is fully
# clean with regards to smart pointer usage. Note that this flag does NOT
# affect the ability to cast csRef's to normal pointers. Only for csPtr.
# In addition to the removal of the cast operator, setting this flag in
# combination with debug mode also causes some tests to be added for good
# usage of csPtr (i.e. calling a function that returns csPtr and not using
# the result will be detected at runtime).
ifndef STRICT_SMART_POINTERS
  STRICT_SMART_POINTERS=yes
endif

# Should we cache makefile information?  Caching information speeds up the
# build process by avoiding the time-consuming scanning which locates and then
# reads makefiles throughout the project.  Usually caching is desirable.
ifndef USE_MAKEFILE_CACHE
  USE_MAKEFILE_CACHE=yes
endif

# Should we monitor and automatically refresh an outdated makefile cache?
# The makefile cache is created when the makefile system is first configured,
# and then automatically refreshed when the chosen platform-specific makefile
# changes, user.mak changes, or one of the source makefiles for the cache
# changes.  Unfortunately, monitoring the source makefiles for changes adds
# some overhead and slows down the build process slightly.  If you have a slow
# disk subsystem, then you may want to disable the cache monitoring facility
# since even this small amount of extra overhead may become annoying.  In most
# cases, it is safe to disable this facility since makefiles rarely change,
# thus it is unlikely that the cache will become outdated.  Even if it does
# become outdated, you can still rebuild it manually by invoking the `recache'
# makefile target (i.e. "make recache").  Beware, however, that if you update
# the project frequently from the CVS repository, it is probably safest to
# leave the cache monitor enabled, just in case other developers modify
# makefiles in the repository.
ifndef MONITOR_MAKEFILE_CACHE
  MONITOR_MAKEFILE_CACHE=yes
endif

# Should we use NASM for assembly?  For most environments, this setting is
# automatically enabled if nasm is available.
ifndef NASM.AVAILABLE
  NASM.AVAILABLE=no
endif

# If 'yes' include assembly optimizations in Crystal Space.  On systems that
# don't support this, setting this option to 'yes' does nothing.
ifndef DO_ASM
  DO_ASM=yes
endif

# If "yes" include MMX support in software renderer
ifndef DO_MMX
  DO_MMX=yes
endif

# Set to 1 to use Mesa instead of "real" OpenGL.  You can define MESA_PATH
# variable in environment to point to MesaGL base path. If Mesa is not
# used then you can use OPENGL_PATH to point to the base of the OpenGL
# libraries and includes.
ifndef USE_MESA
  USE_MESA=0
endif

# The tool used to build dependencies. The possible values are:
# none  - Cannot build dependencies on this platform
# cc    - Use the C compiler (gcc -MM) for this
# mkdep - Use the makedep tool provided in the apps/makedep directory
ifndef DEPEND_TOOL
  DEPEND_TOOL=cc
endif
