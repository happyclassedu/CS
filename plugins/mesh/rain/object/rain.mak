DESCRIPTION.rain = Rain mesh object plug-in

#------------------------------------------------------------- rootdefines ---#
ifeq ($(MAKESECTION),rootdefines)

PLUGINHELP += \
  $(NEWLINE)echo $"  make rain         Make the $(DESCRIPTION.rain)$"

endif # ifeq ($(MAKESECTION),rootdefines)
#------------------------------------------------------------- roottargets ---#
ifeq ($(MAKESECTION),roottargets)

.PHONY: rain rainclean
plugins meshes all: rain

rainclean:
	$(MAKE_CLEAN)
rain:
	$(MAKE_TARGET) MAKE_DLL=yes

endif # ifeq ($(MAKESECTION),roottargets)
#------------------------------------------------------------- postdefines ---#
ifeq ($(MAKESECTION),postdefines)

vpath %.cpp plugins/mesh/rain/object plugins/mesh/partgen

ifeq ($(USE_PLUGINS),yes)
  RAIN = $(OUTDLL)/rain$(DLL)
  LIB.RAIN = $(foreach d,$(DEP.RAIN),$($d.LIB))
  TO_INSTALL.DYNAMIC_LIBS += $(RAIN)
else
  RAIN = $(OUT)/$(LIB_PREFIX)rain$(LIB)
  DEP.EXE += $(RAIN)
  SCF.STATIC += rain
  TO_INSTALL.STATIC_LIBS += $(RAIN)
endif

INC.RAIN = $(wildcard plugins/mesh/rain/object/*.h plugins/mesh/partgen/*.h)
SRC.RAIN = $(wildcard plugins/mesh/rain/object/*.cpp plugins/mesh/partgen/*.cpp)
OBJ.RAIN = $(addprefix $(OUT)/,$(notdir $(SRC.RAIN:.cpp=$O)))
DEP.RAIN = CSTOOL CSGEOM CSUTIL CSSYS CSUTIL

MSVC.DSP += RAIN
DSP.RAIN.NAME = rain
DSP.RAIN.TYPE = plugin

endif # ifeq ($(MAKESECTION),postdefines)
#----------------------------------------------------------------- targets ---#
ifeq ($(MAKESECTION),targets)

.PHONY: rain rainclean
rain: $(OUTDIRS) $(RAIN)

$(RAIN): $(OBJ.RAIN) $(LIB.RAIN)
	$(DO.PLUGIN)

clean: rainclean
rainclean:
	-$(RM) $(RAIN) $(OBJ.RAIN)

ifdef DO_DEPEND
dep: $(OUTOS)/rain.dep
$(OUTOS)/rain.dep: $(SRC.RAIN)
	$(DO.DEP)
else
-include $(OUTOS)/rain.dep
endif

endif # ifeq ($(MAKESECTION),targets)
