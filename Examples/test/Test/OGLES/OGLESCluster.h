#pragma once

#include "Common.h"

enum _SHADER
{
	SHADER_PHONG,
	SHADER_PHONG_CUBEMAP,
	SHADER_TEXTURE,

	SHADER_TOTAL,
};

class OGLESCluster
{
	pvr::Shell *shell;
	pvr::assets::ModelHandle	scene;

	std::vector<GLuint> vbo;
	std::vector<GLuint> indexVbo;
	std::vector<GLuint> texDiffuse;
	GLuint texCubeMap;
	struct
	{
		pvr::uint32 vertShader;
		pvr::uint32 fragShader;
		GLuint handle;
		pvr::uint32 uiMVPMatrixLoc;
		pvr::uint32 uiLightDirLoc;
		pvr::uint32 uiWorldViewITLoc;
	} ShaderProgram[SHADER_TOTAL];

	pvr::float32 frame;
	glm::mat4 projection;
public:
	OGLESCluster(pvr::Shell *_shell) :shell(_shell){}
	virtual pvr::Result initApplication();
	virtual pvr::Result initView();
	virtual pvr::Result releaseView();
	virtual pvr::Result quitApplication();
	virtual pvr::Result renderFrame();

	bool loadTextures();
	bool loadShaders();
	bool LoadVbos();

	Result loadTexturePVR(const StringHash& filename, GLuint& outTexHandle,
		pvr::assets::Texture* outTexture, assets::TextureHeader* outDescriptor);

	void drawMesh(int i32NodeIndex);
};


