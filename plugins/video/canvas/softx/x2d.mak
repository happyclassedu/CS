# This is a subinclude file used to define the rules needed
# to build the X-windows 2D driver -- x2d

# Driver description
DESCRIPTION.x2d = Crystal Space XLib 2D driver

#------------------------------------------------------------- rootdefines ---#
ifeq ($(MAKESECTION),rootdefines)

# Driver-specific help commands
DRIVERHELP += \
  $(NEWLINE)echo $"  make x2d          Make the $(DESCRIPTION.x2d)$"

endif # ifeq ($(MAKESECTION),rootdefines)

#------------------------------------------------------------- roottargets ---#
ifeq ($(MAKESECTION),roottargets)

.PHONY: x2d x2dclean

all plugins drivers drivers2d: x2d

x2d:
	$(MAKE_TARGET) MAKE_DLL=yes
x2dclean:
	$(MAKE_CLEAN)

endif # ifeq ($(MAKESECTION),roottargets)

#------------------------------------------------------------- postdefines ---#
ifeq ($(MAKESECTION),postdefines)
 
ifeq ($(USE_XFREE86VM),yes)
  CFLAGS.X2D+=-DXFREE86VM
  # Place the xf86vm library before -lX11, this is required on some platforms
  LIBS.X2D+=-lXxf86vm
endif

# We need also the X libs
CFLAGS.X2D+=-I$(X11_PATH)/include
LIBS.X2D+=-L$(X11_PATH)/lib -lXext -lX11 $(X11_EXTRA_LIBS)

# The 2D Xlib driver
ifeq ($(USE_SHARED_PLUGINS),yes)
  XLIB2D=$(OUTDLL)x2d$(DLL)
  LIBS.LOCAL.X2D=$(LIBS.X2D)
  DEP.X2D=$(CSUTIL.LIB) $(CSSYS.LIB)
else
  XLIB2D=$(OUT)$(LIB_PREFIX)x2d$(LIB)
  DEP.EXE+=$(XLIB2D)
  LIBS.EXE+=$(LIBS.X2D)
  CFLAGS.STATIC_SCF+=$(CFLAGS.D)SCL_X2D
endif
DESCRIPTION.$(XLIB2D) = $(DESCRIPTION.x2d)
SRC.XLIB2D = $(wildcard plugins/video/canvas/softx/*.cpp \
  plugins/video/canvas/common/x11comm.cpp \
  plugins/video/canvas/common/x11-keys.cpp \
  $(SRC.COMMON.DRV2D))
OBJ.XLIB2D = $(addprefix $(OUT),$(notdir $(SRC.XLIB2D:.cpp=$O)))

endif # ifeq ($(MAKESECTION),postdefines)

#----------------------------------------------------------------- targets ---#
ifeq ($(MAKESECTION),targets)

.PHONY: x2d x2dclean

# Chain rules
clean: x2dclean

x2d: $(OUTDIRS) $(XLIB2D)

$(OUT)%$O: plugins/video/canvas/softx/%.cpp
	$(DO.COMPILE.CPP) $(CFLAGS.X2D)
 
$(XLIB2D): $(OBJ.XLIB2D) $(DEP.X2D)
	$(DO.PLUGIN) $(LIBS.LOCAL.X2D)

x2dclean:
	$(RM) $(XLIB2D) $(OBJ.XLIB2D) $(OUTOS)x2d.dep

ifdef DO_DEPEND
dep: $(OUTOS)x2d.dep
$(OUTOS)x2d.dep: $(SRC.XLIB2D)
	$(DO.DEP1) $(CFLAGS.X2D) $(DO.DEP2)
else
-include $(OUTOS)x2d.dep
endif

endif # ifeq ($(MAKESECTION),targets)

#------------------------------------------------------------------ config ---#
ifeq ($(ROOTCONFIG)/$(MAKESECTION),volatile/rootdefines)

# Default value for DO_SHM
ifndef DO_SHM
  DO_SHM = yes
endif

ifeq ($(DO_SHM)$(findstring DO_SHM,$(MAKE_VOLATILE_H)),yes)
  MAKE_VOLATILE_H+=$(NEWLINE)echo $"\#define DO_SHM$">>volatile.tmp
endif

endif # ifeq ($(ROOTCONFIG)/$(MAKESECTION),volatile/rootdefines)
