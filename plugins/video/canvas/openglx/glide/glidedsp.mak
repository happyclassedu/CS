# This is a subinclude file used to define the rules needed
# to build the Glide Displaydriver for GLX 2D driver -- oglglide

# Driver description
DESCRIPTION.oglglide=Glide Displaydriver for Crystal Space GL/X 2D driver

#-------------------------------------------------------------- rootdefines ---#
ifeq ($(MAKESECTION),rootdefines)

# Driver-specific help commands
DRIVERHELP += $(NEWLINE)echo $"  make oglglide     Make the $(DESCRIPTION.oglglide)$"

endif # ifeq ($(MAKESECTION),rootdefines)

#-------------------------------------------------------------- roottargets ---#
ifeq ($(MAKESECTION),roottargets)

.PHONY: oglglide

all plugins glxdisp: oglglide

oglglide:
	$(MAKE_TARGET) MAKE_DLL=yes

oglglideclean:
	$(MAKE_CLEAN)

endif # ifeq ($(MAKESECTION),roottargets)

#-------------------------------------------------------------- postdefines ---#
ifeq ($(MAKESECTION),postdefines)

# Local CFLAGS and libraries
#LIBS._oglglide+=-L$(X11_PATH)/lib -lXext -lX11

CFLAGS.OGLGLIDE+=-I/usr/include/glide -I/usr/local/glide/include
LIBS._oglglide+=-lglide2x

# The driver
ifeq ($(USE_SHARED_PLUGINS),yes)
  oglglide=$(OUTDLL)oglglide$(DLL)
  LIBS.OGLGLIDE=$(LIBS._oglglide)
#  LIBS.OGLGLIDE=$(LIBS._oglglide) $(CSUTIL.LIB) $(CSSYS.LIB)
  DEP.OGLGLIDE=$(CSUTIL.LIB) $(CSSYS.LIB)
else
  oglglide=$(OUT)$(LIB_PREFIX)oglglide$(LIB)
  DEP.EXE+=$(oglglide)
  LIBS.EXE+=$(LIBS._oglglide) $(CSUTIL.LIB) $(CSSYS.LIB)
  CFLAGS.STATIC_SCF+=$(CFLAGS.D)SCL_oglglide
endif
DESCRIPTION.$(oglglide) = $(DESCRIPTION.OGLGLIDE)
SRC.OGLGLIDE = $(wildcard libs/cs2d/openglx/glide/*.cpp)
OBJ.OGLGLIDE = $(addprefix $(OUT),$(notdir $(SRC.OGLGLIDE:.cpp=$O)))

endif # ifeq ($(MAKESECTION),postdefines)

#------------------------------------------------------------------ targets ---#
ifeq ($(MAKESECTION),targets)

.PHONY: oglglide oglglideclean

# Chain rules
clean: oglglideclean

oglglide: $(OUTDIRS) $(oglglide)

$(OUT)%$O: libs/cs2d/openglx/glide/%.cpp
	$(DO.COMPILE.CPP) $(CFLAGS.OGLGLIDE)
 
$(oglglide): $(OBJ.OGLGLIDE) $(DEP.OGLGLIDE)
	$(DO.PLUGIN) $(LIBS.OGLGLIDE)

oglglideclean:
	$(RM) $(oglglide) $(OBJ.OGLGLIDE)
 
ifdef DO_DEPEND
depend: $(OUTOS)oglglide.dep
$(OUTOS)oglglide.dep: $(SRC.OGLGLIDE)
	$(DO.DEP) $(CFLAGS.OGLGLIDE)
else
-include $(OUTOS)oglglide.dep
endif

endif # ifeq ($(MAKESECTION),targets)
