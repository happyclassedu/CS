# This is an include file for all the makefiles which describes system specific
# settings. Also have a look at mk/user.mak.

# Friendly names for building environment
DESCRIPTION.irix = Irix
DESCRIPTION.OS.irix = Irix

ifeq ($(X11.AVAILABLE),yes)
  PLUGINS+=video/canvas/softx
  ifeq ($(GL.AVAILABLE),yes)
    PLUGINS+=video/canvas/openglx
   endif

  # The X-Window plugin
  PLUGINS+=video/canvas/xwindow
  # Shared Memory Plugin
  PLUGINS+=video/canvas/xextshm
  # Video Modes Plugin
  PLUGINS+=video/canvas/xextf86vm
endif

# Uncomment the following to build GGI 2D driver
#PLUGINS+=video/canvas/ggi

# Sound renderer.
PLUGINS+=sound/renderer/software

#--------------------------------------------------- rootdefines & defines ---#
ifneq (,$(findstring defines,$(MAKESECTION)))

# Processor is autodetected and written to config.mak
#PROC=MIPS

# Operating system. Can be one of:
# MACOSX, SOLARIS, LINUX, IRIX, BSD, UNIX, WIN32
OS=IRIX

# Operating system family: UNIX (for Unix or Unix-like platforms), WIN32, etc.
OS_FAMILY=UNIX

# Compiler. Can be one of: GCC, MPWERKS, VC (Visual C++), UNKNOWN
COMP=GCC

endif # ifneq (,$(findstring defines,$(MAKESECTION)))

#----------------------------------------------------------------- defines ---#
ifeq ($(MAKESECTION),defines)

include mk/unix.mak

# Extra libraries needed on this system.
LIBS.EXE=$(LFLAGS.l)dl $(LFLAGS.l)m

# Indicate where special include files can be found.
CFLAGS.INCLUDE=

# General flags for the compiler which are used in any case.
CFLAGS.GENERAL=$(CFLAGS.SYSTEM) $(CSTHREAD.CFLAGS)

# Flags for the compiler which are used when optimizing.
CFLAGS.optimize=-O -fomit-frame-pointer -ffast-math

# Flags for the compiler which are used when debugging.
CFLAGS.debug=-g3 -gstabs

# Flags for the compiler which are used when profiling.
CFLAGS.profile=-pg -O -g

# Flags for the compiler which are used when building a shared library.
CFLAGS.DLL=

# General flags for the linker which are used in any case.
LFLAGS.GENERAL=$(CSTHREAD.LFLAGS)

# Flags for the linker which are used when debugging.
LFLAGS.debug=-g3

# Flags for the linker which are used when profiling.
LFLAGS.profile=-pg

# Flags for the linker which are used when building a shared library.
#LFLAGS.DLL=-Wl,-G -nostdlib $(LFLAGS.l)gcc $(LFLAGS.l)c
LFLAGS.DLL=-shared

# System dependent source files included into CSSYS library
SRC.SYS_CSSYS= $(wildcard libs/cssys/unix/*.cpp) \
  libs/cssys/general/sysroot.cpp \
  libs/cssys/general/findlib.cpp \
  libs/cssys/general/getopt.cpp \
  libs/cssys/general/printf.cpp \
  libs/cssys/general/runloop.cpp \
  libs/cssys/general/sysinit.cpp \
  $(CSTHREAD.SRC)
INC.SYS_CSSYS = $(wildcard libs/cssys/unix/*.h) $(CSTHREAD.INC)

# Use makedep to build dependencies
DEPEND_TOOL=mkdep

endif # ifeq ($(MAKESECTION),defines)

#-------------------------------------------------------------- confighelp ---#
ifeq ($(MAKESECTION),confighelp)

SYSHELP += \
  $(NEWLINE)echo $"  make irix         Prepare for building on $(DESCRIPTION.irix)$"

endif # ifeq ($(MAKESECTION),confighelp)

#--------------------------------------------------------------- configure ---#
ifeq ($(ROOTCONFIG),config)

define SYSCONFIG
  libs/cssys/unix/unixconf.sh irix >>config.tmp
  echo "Don't forget to set USE_MESA=0 in user.mak"
endef

endif # ifeq ($(ROOTCONFIG),config)
