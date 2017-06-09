#pragma once
class Common
{
public:
	Common();
	~Common();
};


#include "PVRAssets/PVRAssets.h"
#include "PVRNativeApi/OGLES/OpenGLESBindings.h"
#include "PVRNativeApi/OGLES/NativeObjectsGles.h"
#include "PVRNativeApi/NativeGles.h"
#include "PVRNativeApi\EGL\NativeLibraryEgl.h"
#include "PVRShell/PVRShell.h"

using namespace pvr;
using namespace pvr::types;

#define VIEWPORT_WIDTH	1920
#define VIEWPORT_HEIGHT	720

const pvr::uint32 VertexArray = 0;
const pvr::uint32 NormalArray = 1;
const pvr::uint32 TexCoordArray = 2;


Result createShaderProgram(native::HShader_ shaders[], uint32 count, const char** attribs, uint32 attribCount, GLuint& shaderProg);
