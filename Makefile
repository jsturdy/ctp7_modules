ifndef PETA_STAGE
$(error "Error: PETA_STAGE environment variable not set.")
endif

BUILD_HOME   := $(shell dirname `pwd`)
Project      := ctp7_modules
Package      := ctp7_modules
ShortPackage := ctp7_modules
LongPackage  := ctp7_modules
PackageName  := $(ShortPackage)
PackagePath  := $(shell pwd)
PackageDir   := pkg/$(ShortPackage)
Arch         := arm
Packager     := Mykhailo Dalchenko

CTP7_MODULES_VER_MAJOR:=$(shell ./config/tag2rel.sh | awk '{split($$0,a," "); print a[1];}' | awk '{split($$0,b,":"); print b[2];}')
CTP7_MODULES_VER_MINOR:=$(shell ./config/tag2rel.sh | awk '{split($$0,a," "); print a[2];}' | awk '{split($$0,b,":"); print b[2];}')
CTP7_MODULES_VER_PATCH:=$(shell ./config/tag2rel.sh | awk '{split($$0,a," "); print a[3];}' | awk '{split($$0,b,":"); print b[2];}')

INSTALL_PREFIX=/mnt/persistent/ctp7_modules

include $(BUILD_HOME)/$(Package)/config/mfZynq.mk
include $(BUILD_HOME)/$(Package)/config/mfCommonDefs.mk
include $(BUILD_HOME)/$(Package)/config/mfRPMRules.mk

PackageBase = $(BUILD_HOME)/$(Package)
ProjectBase = $(BUILD_HOME)/$(Project)
IncludeDirs = $(PackageBase)/include
IncludeDirs+= /opt/xhal/include
# IncludeDirs+= /opt/cactus/include
IncludeDirs+= /opt/wiscrpcsvc/include
IncludeDirs+= /opt/reedmuller/include
INC=$(IncludeDirs:%=-I%)

ifndef GEM_VARIANT
GEM_VARIANT = ge11
endif

CFLAGS+= -DGEM_VARIANT="$(GEM_VARIANT)"
CFLAGS+= -std=c++1y -O3 -pthread -fPIC

LDFLAGS+= -Wl,--as-needed

LibraryDirs = $(PackageBase)/lib
LibraryDirs+= /opt/xhal/lib/arm
LibraryDirs+= /opt/wiscrpcsvc/lib
LibraryDirs+= /opt/reedmuller/lib/arm
Libraries=$(LibraryDirs:%=-L%)

.PHONY: clean rpc prerpm

default: build
	@echo "Running default target"
	$(MakeDir) $(PackageDir)

_rpmprep: preprpm
	@echo "Running _rpmprep target"

preprpm: default
	@echo "Running preprpm target"
	@cp -rf lib $(PackageDir)

PackageSourceDir=$(ProjectBase)/src
PackageTestSourceDir=$(ProjectBase)/test
PackageIncludeDir=$(ProjectBase)/include
PackageLibraryDir=$(ProjectBase)/lib
PackageExecDir=$(ProjectBase)/bin
PackageObjectDir=$(PackageSourceDir)/linux/$(Arch)
# PackageObjectDir=$(PackageSourceDir)/linux
Sources      := $(wildcard $(PackageSourceDir)/*.cpp) $(wildcard $(PackageSourceDir)/*/*.cpp)
TestSources  := $(wildcard $(PackageTestSourceDir)/*.cxx) $(wildcard $(PackageTestSourceDir)/*.cpp)
Dependencies := $(patsubst $(PackageSourceDir)/%.cpp, $(PackageObjectDir)/%.d, $(Sources))
TargetObjects:= $(patsubst %.d,%.o,$(Dependencies))

TargetLibraries:= memhub memory optical utils extras amc daq_monitor vfat3 optohybrid calibration_routines gbt

# Everything links against these three
BASE_LINKS = -lxhal -llmdb -lwisci2c

## Generic shared object creation rule, need to accomodate cases where we have lib.o lib/sub.o
pc:=%
.SECONDEXPANSION:
$(PackageLibraryDir)/%.so: $$(filter $(PackageObjectDir)/$$*$$(pc).o, $(TargetObjects))
	$(MakeDir) $(@D)
	$(CXX) $(CFLAGS) $(LDFLAGS) $(Libraries) -shared -Wl,-soname,$(*F).so -o $@ $^ $(EXTRA_LINKS) $(BASE_LINKS)

## adapted from http://make.mad-scientist.net/papers/advanced-auto-dependency-generation/
## Generic object creation rule, generate dependencies and use them later
$(PackageObjectDir)/%.o: $(PackageSourceDir)/%.cpp
	$(MakeDir) $(@D)
	$(CXX) $(CFLAGS) -c $(INC) -MT $@ -MMD -MP -MF $(@D)/$(*F).Td -o $@ $<
	mv $(@D)/$(*F).Td $(@D)/$(*F).d
# this was to prevent an older object than dependency file (for some versions of gcc)
	touch $@

## dummy rule for dependencies
$(PackageObjectDir)/%.d:

## mark dependencies and objects as not auto-removed
.PRECIOUS: $(PackageObjectDir)/%.d
.PRECIOUS: $(PackageObjectDir)/%.o

## Force rule for all target library names
$(TargetLibraries):

## Define the target library dependencies
memhub:
	$(eval export EXTRA_LINKS=-lmemsvc)
	$(MAKE) $(PackageLibraryDir)/memhub.so EXTRA_LINKS="$(EXTRA_LINKS)"

memory: memhub
	$(eval export EXTRA_LINKS=$(^:%=-l:%.so))
	$(MAKE) $(PackageLibraryDir)/memory.so EXTRA_LINKS="$(EXTRA_LINKS)"

optical: memhub
	$(eval export EXTRA_LINKS=$(^:%=-l:%.so))
	$(MAKE) $(PackageLibraryDir)/optical.so EXTRA_LINKS="$(EXTRA_LINKS)"

utils: memhub
	$(eval export EXTRA_LINKS=$(^:%=-l:%.so))
	$(MAKE) $(PackageLibraryDir)/utils.so EXTRA_LINKS="$(EXTRA_LINKS)"

extras: memhub utils
	$(eval export EXTRA_LINKS=$(^:%=-l:%.so))
	$(MAKE) $(PackageLibraryDir)/extras.so EXTRA_LINKS="$(EXTRA_LINKS)"

amc: utils extras
	$(eval export EXTRA_LINKS=$(^:%=-l:%.so))
	$(MAKE) $(PackageLibraryDir)/amc.so EXTRA_LINKS="$(EXTRA_LINKS)"

daq_monitor: amc extras utils
	$(eval export EXTRA_LINKS=$(^:%=-l:%.so))
	$(MAKE) $(PackageLibraryDir)/daq_monitor.so EXTRA_LINKS="$(EXTRA_LINKS)"

vfat3: optohybrid amc extras utils
	$(eval export EXTRA_LINKS=$(^:%=-l:%.so) -lreedmuller)
	$(MAKE) $(PackageLibraryDir)/vfat3.so EXTRA_LINKS="$(EXTRA_LINKS)"

optohybrid: amc extras utils
	$(eval export EXTRA_LINKS=$(^:%=-l:%.so))
	$(MAKE) $(PackageLibraryDir)/optohybrid.so EXTRA_LINKS="$(EXTRA_LINKS)"

calibration_routines: optohybrid vfat3 amc extras utils
	$(eval export EXTRA_LINKS=$(^:%=-l:%.so))
	$(MAKE) $(PackageLibraryDir)/calibration_routines.so EXTRA_LINKS="$(EXTRA_LINKS)"

gbt: amc utils
	$(eval export EXTRA_LINKS=$(^:%=-l:%.so))
	$(MAKE) $(PackageLibraryDir)/gbt.so EXTRA_LINKS="$(EXTRA_LINKS)"

build: $(TargetLibraries)
	@echo Executing build stage

_all: build
	@echo Executing _all stage

### local (PC) test functions, need standard gcc toolchain, dirs, and flags
.PHONY: test
# test: test/tester.cpp
TestExecs := $(patsubst $(PackageTestSourceDir)/%.cxx, $(PackageExecDir)/%, $(TestSources))
$(TestExecs):

$(PackageExecDir)/%: $(PackageTestSourceDir)/%.cxx
	$(MakeDir) $(@D)
	g++ -O0 -g3 -fno-inline -std=c++11 -c $(INC) -MT $@ -MMD -MP -MF $(@D)/$(*F).Td -o $@ $<
	mv $(@D)/$(*F).Td $(@D)/$(*F).d
	touch $@
	g++ -O0 -g3 -fno-inline -std=c++11 -o $@ $< $(INC) $(LDFLAGS) -L/opt/wiscrpcsvc/lib -lwiscrpcsvc

test: $(TestExecs)

clean: cleanrpm
	@echo Cleaning up all generated files
	-rm -rf $(PackageDir)
	-rm -rf $(Dependencies)
	-rm -rf $(TargetObjects)
	-rm -rf $(PackageObjectDir)
	-rm -rf $(PackageLibraryDir)

cleandoc:
	@echo "TO DO"

-include $(Dependencies)
