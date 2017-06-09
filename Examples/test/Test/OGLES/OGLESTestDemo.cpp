#include "OGLESTestDemo.h"
#include <iostream>
#include <sstream>
#ifdef RENDER_MAIN_SCENE
#include "OGLESMainScene.h"
#endif
#ifdef RENDER_MAP
#include "OGLESMap.h"
#endif
#include "PVRShell/PVRShellNoPVRApi.h"

pvr::Result OGLESTestDemo::initApplication()
{
#ifdef _WIN32
	setDimensions(1920, 720);
#endif
	setAASamples(4);

#ifdef RENDER_MAIN_SCENE
	mainScene = new OGLESMainScene(this);
	mainScene->initApplication();
#endif

#ifdef RENDER_MAP
	map = new OGLESMap(this);
	map->initApplication();
#endif


	return pvr::Result::Success;
}

pvr::Result OGLESTestDemo::quitApplication()
{
#ifdef RENDER_MAIN_SCENE
	mainScene->quitApplication();
#endif
#ifdef RENDER_MAP
	map->quitApplication();
#endif
	return pvr::Result::Success;
}

pvr::Result OGLESTestDemo::initView()
{
	gl::initGl();

#ifdef RENDER_MAIN_SCENE
	mainScene->initView();
#endif
#ifdef RENDER_MAP
	map->initView();
#endif

	Log(Log.Information, "window width=%d height=%d", getWidth(), getHeight());
	Log(Log.Information, "viewport width=%d height=%d", VIEWPORT_WIDTH, VIEWPORT_HEIGHT);

	return pvr::Result::Success;
}

pvr::Result OGLESTestDemo::releaseView()
{
#ifdef RENDER_MAIN_SCENE
	mainScene->releaseView();
#endif
#ifdef RENDER_MAP
	map->releaseView();
#endif
	return pvr::Result::Success;
}

pvr::Result OGLESTestDemo::renderFrame()
{
#ifdef RENDER_MAP
	map->renderFrame();
#endif
#ifdef RENDER_MAIN_SCENE
	mainScene->renderFrame();
#endif
	return pvr::Result::Success;
}

void OGLESTestDemo::eventKeyStroke(Keys key)
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

std::auto_ptr<pvr::Shell> pvr::newDemo() { return std::auto_ptr<pvr::Shell>(new OGLESTestDemo()); }
