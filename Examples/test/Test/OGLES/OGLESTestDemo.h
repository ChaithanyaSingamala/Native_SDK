#pragma once

#include "PVRShell/PVRShell.h"
#include "PVRApi/PVRApi.h"

using namespace pvr;
using namespace pvr::types;

#define RENDER_MAP
//#define RENDER_MAIN_SCENE

#ifdef RENDER_MAIN_SCENE
class OGLESMainScene;
#endif
#ifdef RENDER_MAP
class OGLESMap;
#endif

class OGLESTestDemo : public pvr::Shell
{
#ifdef RENDER_MAIN_SCENE
	OGLESMainScene *mainScene;
#endif
#ifdef RENDER_MAP
	OGLESMap *map;
#endif
public:
	virtual pvr::Result initApplication();
	virtual pvr::Result initView();
	virtual pvr::Result releaseView();
	virtual pvr::Result quitApplication();
	virtual pvr::Result renderFrame();

	virtual void eventKeyStroke(Keys key);
};
