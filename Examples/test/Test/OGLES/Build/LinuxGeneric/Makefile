#
# File			Makefile
# Title			Example makefile
#
# Copyright		Copyright (C) Imagination Technologies Limited.
#

# make PLATFORM=Linux_armv8_64
# make PLATFORM=Linux_armv8_64 DEBUG_RELEASE=debug
# make PLATFORM=Linux_armv8_64-fb

.PHONY: clean

SDKDIR  = ../../../../../..
OUTNAME = ../../../../bin/OGLESTestDemo
DEPENDS =  PVRCore PVRAssets PVREgl PVRNativeGles PVRShell 
LIBRARIES =  PVRShell PVRNativeGles PVREgl PVRAssets PVRCore
LIBPATHS =  ../../../../../../Framework/Bin/Linux_$(PLAT_SUFFIX)/$(DEBUG_RELEASE)$(WS)

OBJECTS +=	\
           OGLES/Common.o	\
           OGLES/OGLESTestDemo.o	\
           OGLES/OGLESMap.o	
		   
		   
RESOURCES +=

COMMON_RESOURCES += 

include $(SDKDIR)/Builds/Linux/make_example.mak
