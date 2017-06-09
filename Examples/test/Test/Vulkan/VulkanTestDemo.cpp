#include "VulkanTestDemo.h"
#ifdef RENDER_CLUSTER
#include "VulkanMainScene.h"
#endif
#ifdef RENDER_MAP
#include "VulkanMap.h"
#endif


pvr::Result VulkanTestDemo::initApplication()
{
	//setAASamples(8);
#ifdef _WIN32
	setDimensions(1920, 720);
#endif

	deviceResources.reset(new DeviceResources());
	deviceResources->assetManager.init(*this);

#ifdef RENDER_CLUSTER
	mainScene = new VulkanMainScene(this);
	mainScene->initApplication();
#endif
#ifdef RENDER_MAP
	map = new VulkanMap(this);
	map->initApplication();
#endif

	return pvr::Result::Success;
}

pvr::Result VulkanTestDemo::quitApplication() 
{
#ifdef RENDER_CLUSTER
	mainScene->quitApplication();
	delete mainScene;
	mainScene = nullptr;
#endif
#ifdef RENDER_MAP
	map->quitApplication();
	delete map;
	map = nullptr;
#endif
	return pvr::Result::Success; 
}

pvr::Result VulkanTestDemo::initView()
{
	deviceResources->context = getGraphicsContext();
	//deviceResources->fboOnScreen = deviceResources->context->createOnScreenFboSet(pvr::types::LoadOp::Load, pvr::types::StoreOp::Store, pvr::types::LoadOp::Load, pvr::types::StoreOp::Ignore, pvr::types::LoadOp::Load, pvr::types::StoreOp::Ignore);
	deviceResources->fboOnScreen = deviceResources->context->createOnScreenFboSet();

	int totalSwapChains = getPlatformContext().getSwapChainLength();
	deviceResources->commandBuffers.resize(totalSwapChains);

	for (int i = 0; i < getPlatformContext().getSwapChainLength(); ++i)
	{
		deviceResources->commandBuffers[i] = deviceResources->context->createCommandBufferOnDefaultPool();
	}


#ifdef RENDER_CLUSTER
	mainScene->initView();
#endif
#ifdef RENDER_MAP
	map->initView();
#endif

	Log(Log.Information, "window width=%d height=%d", getWidth(), getHeight());
	Log(Log.Information, "viewport width=%d height=%d", VIEWPORT_WIDTH, VIEWPORT_HEIGHT);

	return pvr::Result::Success;
}

pvr::Result VulkanTestDemo::releaseView()
{
#ifdef RENDER_CLUSTER
	mainScene->releaseView();
#endif
#ifdef RENDER_MAP
	map->releaseView();
#endif

	activeCB->reset();

	deviceResources.reset();
	return pvr::Result::Success;
}
pvr::Result VulkanTestDemo::renderFrame()
{
	int currentSwapChain = getSwapChainIndex();

	activeCB = &deviceResources->commandBuffers[currentSwapChain];

	activeCB->get()->beginRecording();
	activeCB->get()->beginRenderPass(deviceResources->fboOnScreen[currentSwapChain], false, glm::vec4(0.0, 0.00, 0.00, 1.0f));
	
	//activeCB->get()->clearColorAttachment(deviceResources->fboOnScreen[currentSwapChain], glm::vec4(0.0, 0.00, 0.00, 1.0f));

#ifdef RENDER_MAP
	map->renderFrame();
#endif
#ifdef RENDER_CLUSTER
	mainScene->renderFrame();
#endif

	activeCB->get()->endRenderPass();
	activeCB->get()->endRecording();


	deviceResources->commandBuffers[getSwapChainIndex()]->submit();
	return pvr::Result::Success;
}

void VulkanTestDemo::eventKeyStroke(Keys key)
{
	switch (key)
	{
	case Keys::A:
		map->state = 0;
		break;
	case Keys::S:
		map->state = 1;
		break;
	case Keys::D:
		map->state = 2;
		break;
	case Keys::F:
		map->state = 3;
		break;
	case Keys::G:
		map->state = 4;
		break;
	case Keys::H:
		map->state = 5;
		break;
	}
}

std::auto_ptr<pvr::Shell> pvr::newDemo() { return std::auto_ptr<pvr::Shell>(new VulkanTestDemo()); }
