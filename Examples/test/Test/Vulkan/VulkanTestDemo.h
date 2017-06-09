#pragma once

#include "PVRShell/PVRShell.h"
#include "PVRApi/PVRApi.h"
#include "PVREngineUtils/PVREngineUtils.h"
#include "PVREngineUtils/AssetStore.h"

#define RENDER_MAP
//#define RENDER_CLUSTER

#ifdef RENDER_CLUSTER
class VulkanMainScene;
#endif
#ifdef RENDER_MAP
class VulkanMap;
#endif

#define VIEWPORT_WIDTH	1920
#define VIEWPORT_HEIGHT	720

using namespace pvr;


class VulkanTestDemo : public pvr::Shell
{
#ifdef RENDER_MAP
	VulkanMap *map;
#endif
#ifdef RENDER_CLUSTER
	VulkanMainScene *mainScene;
#endif

	struct DeviceResources
	{
		pvr::api::FboSet fboOnScreen;

		pvr::Multi<pvr::api::CommandBuffer> commandBuffers;

		pvr::utils::AssetStore assetManager;
		pvr::GraphicsContext context;
	};

public:
	std::auto_ptr<DeviceResources> deviceResources;
	pvr::api::CommandBuffer *activeCB;
	virtual pvr::Result initApplication();
	virtual pvr::Result initView();
	virtual pvr::Result releaseView();
	virtual pvr::Result quitApplication();
	virtual pvr::Result renderFrame();

	virtual void eventKeyStroke(Keys key);

};
