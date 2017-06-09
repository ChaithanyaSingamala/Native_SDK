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
OUTNAME = ../../../../bin/VulkanTestDemo
DEPENDS =  PVRCore PVRAssets PVRVulkanGlue PVRNativeVulkan PVRVulkan PVRShell 
LIBRARIES =  PVRShell PVRVulkan PVRNativeVulkan PVRVulkanGlue PVRAssets PVRCore
LIBPATHS =  ../../../../../../Framework/Bin/Linux_$(PLAT_SUFFIX)/$(DEBUG_RELEASE)$(WS)

OBJECTS +=	\
           Vulkan/VulkanTestDemo.o	\
		   Vulkan/VulkanMap.o	


include $(SDKDIR)/Builds/Linux/make_example.mak
