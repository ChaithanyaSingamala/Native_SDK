#include "OGLESCluster.h"


//order according to enum
const char *shaderSrcFile[SHADER_TOTAL][2] = {
	{ "cluster/shader_phong_ES3.vert",		"cluster/shader_phong_ES3.frag" },
	{ "cluster/shader_phong_cube_ES3.vert", "cluster/shader_phong_cube_ES3.frag" },
	{ "cluster/shader_textured_ES3.vert",	"cluster/shader_textured_ES3.frag" },
};

const char clusterSceneFile[] = "cluster/chrome2.pod";

const pvr::StringHash AttribNames[] =
{
	"POSITION",
	"NORMAL",
	"UV0",
};


Result OGLESCluster::loadTexturePVR(const StringHash& filename, GLuint& outTexHandle, pvr::assets::Texture* outTexture,
	assets::TextureHeader* outDescriptor)
{
	assets::Texture tempTexture;
	Result result;
	std::string fileName = filename;
	fileName = "cluster/" + fileName;
	fileName.replace(fileName.end() - 3, fileName.end(), "pvr");


	Stream::ptr_type assetStream = shell->getAssetStream(fileName);
	native::HTexture_ textureHandle;


	if (!assetStream.get())
	{
		Log(Log.Error, "AssetStore.loadTexture error for filename %s : File not found", filename.c_str());
		return Result::NotFound;
	}
	result = assets::textureLoad(assetStream, assets::TextureFileFormat::PVR, tempTexture);
	if (result == Result::Success)
	{
		bool isDecompressed;
		types::ImageAreaSize areaSize; PixelFormat pixelFmt;
		pvr::utils::textureUpload(shell->getPlatformContext(), tempTexture, textureHandle, areaSize, pixelFmt, isDecompressed);
	}
	if (result != Result::Success)
	{
		Log(Log.Error, "AssetStore.loadTexture error for filename %s : Failed to load texture with code %s.",
			filename.c_str(), Log.getResultCodeString(result));
		return result;
	}
	if (outTexture) { *outTexture = tempTexture; }
	outTexHandle = textureHandle;
	return result;
}

Result loadModel(pvr::IAssetProvider* assetProvider, const char* filename, assets::ModelHandle& outModel)
{
	Stream::ptr_type assetStream = assetProvider->getAssetStream(filename);
	if (!assetStream.get())
	{
		Log(Log.Error, "AssetStore.loadModel  error for filename %s : File not found", filename);
		return Result::NotFound;
	}

	assets::PODReader reader(assetStream);
	assets::ModelHandle handle = assets::Model::createWithReader(reader);

	if (handle.isNull())
	{
		Log(Log.Error, "AssetStore.loadModel error : Failed to load model %s.", filename);
		return pvr::Result::UnableToOpen;
	}
	else
	{
		outModel = handle;
	}
	return Result::Success;
}

bool OGLESCluster::loadTextures()
{
	pvr::uint32 numMaterials = scene->getNumMaterials();
	texDiffuse.resize(numMaterials);
	for (pvr::uint32 i = 0; i < numMaterials; ++i)
	{
		const pvr::assets::Model::Material& material = scene->getMaterial(i);
		if (material.getDiffuseTextureIndex() != -1)
		{
			gl::ActiveTexture(GL_TEXTURE0 + 0);
			if (loadTexturePVR(scene->getTexture(material.getDiffuseTextureIndex()).getName(),
				texDiffuse[i], NULL, 0) != pvr::Result::Success)
			{
				Log("Failed to load texture %s", scene->getTexture(material.getDiffuseTextureIndex()).getName().c_str());
				return false;
			}
			gl::BindTexture(GL_TEXTURE_2D, texDiffuse[i]);
			gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		}


	}

	{
		gl::ActiveTexture(GL_TEXTURE0 + 1);
		if (loadTexturePVR("SkyboxTex.pvr", texCubeMap, NULL, 0) != pvr::Result::Success)
		{
			Log("Failed to load texture SkyboxTex.pvr");
			return false;
		}
		gl::BindTexture(GL_TEXTURE_CUBE_MAP, texCubeMap);
		gl::TexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		gl::TexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		gl::TexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_REPEAT);
		gl::TexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_REPEAT);
	}


	return true;
}

bool OGLESCluster::loadShaders()
{
	const char* attributes[] = { "inVertex", "inNormal", "inTexCoord" };


	for (int i = 0; i < SHADER_TOTAL; i++)
	{
		pvr::assets::ShaderFile fileVersioning;
		fileVersioning.populateValidVersions(shaderSrcFile[i][0], *shell);

		native::HShader_ shaders[2];
		if (!pvr::utils::loadShader(native::HContext_(), *fileVersioning.getBestStreamForApi(pvr::Api::OpenGLES3), ShaderType::VertexShader, 0, 0,
			shaders[0]))
		{
			return false;
		}

		fileVersioning.populateValidVersions(shaderSrcFile[i][1], *shell);
		if (!pvr::utils::loadShader(native::HContext_(), *fileVersioning.getBestStreamForApi(pvr::Api::OpenGLES3), ShaderType::FragmentShader, 0,
			0, shaders[1]))
		{
			return false;
		}
		if (createShaderProgram(shaders, 2, attributes, sizeof(attributes) / sizeof(attributes[0]),
			ShaderProgram[i].handle) != pvr::Result::Success)
		{
			return false;
		}

		gl::UseProgram(ShaderProgram[i].handle);
		int locTex = gl::GetUniformLocation(ShaderProgram[i].handle, "sTexture");
		if (locTex != -1)
			gl::Uniform1i(locTex, 0);
		int locCubeTex = gl::GetUniformLocation(ShaderProgram[i].handle, "sSkybox");
		if(locCubeTex != -1)
			gl::Uniform1i(locCubeTex, 1);

		ShaderProgram[i].uiMVPMatrixLoc = gl::GetUniformLocation(ShaderProgram[i].handle, "MVPMatrix");
		ShaderProgram[i].uiLightDirLoc = gl::GetUniformLocation(ShaderProgram[i].handle, "LightDirection");
		ShaderProgram[i].uiWorldViewITLoc = gl::GetUniformLocation(ShaderProgram[i].handle, "WorldViewIT");
	}

	return true;
}

bool OGLESCluster::LoadVbos()
{
	vbo.resize(scene->getNumMeshes());
	indexVbo.resize(scene->getNumMeshes());
	gl::GenBuffers(scene->getNumMeshes(), &vbo[0]);

	for (unsigned int i = 0; i < scene->getNumMeshes(); ++i)
	{
		const pvr::assets::Mesh& mesh = scene->getMesh(i);
		pvr::uint32 size = (uint32)mesh.getDataSize(0);
		gl::BindBuffer(GL_ARRAY_BUFFER, vbo[i]);
		gl::BufferData(GL_ARRAY_BUFFER, size, mesh.getData(0), GL_STATIC_DRAW);

		indexVbo[i] = 0;
		if (mesh.getFaces().getData())
		{
			gl::GenBuffers(1, &indexVbo[i]);
			size = mesh.getFaces().getDataSize();
			gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexVbo[i]);
			gl::BufferData(GL_ELEMENT_ARRAY_BUFFER, size, mesh.getFaces().getData(), GL_STATIC_DRAW);
		}
	}
	gl::BindBuffer(GL_ARRAY_BUFFER, 0);
	gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	return true;
}

pvr::Result OGLESCluster::initApplication()
{
	pvr::Result rslt = pvr::Result::Success;
	if ((rslt = loadModel(shell, clusterSceneFile, scene)) != pvr::Result::Success)
	{
		shell->setExitMessage("ERROR: Couldn't load the .pod file\n");
		return rslt;
	}

	if (scene->getNumCameras() == 0)
	{
		shell->setExitMessage("ERROR: The scene does not contain a camera. Please add one and re-export.\n");
		return pvr::Result::InvalidData;
	}

	if (scene->getNumLights() == 0)
	{
		shell->setExitMessage("ERROR: The scene does not contain a light. Please add one and re-export.\n");
		return pvr::Result::InvalidData;
	}

	frame = 0;
	return rslt;
}


pvr::Result OGLESCluster::quitApplication() { return pvr::Result::Success; }
pvr::Result OGLESCluster::initView()
{
	pvr::string ErrorStr;
	if (!LoadVbos())
	{
		shell->setExitMessage(ErrorStr.c_str());
		return pvr::Result::UnknownError;
	}

	if (!loadTextures())
	{
		shell->setExitMessage(ErrorStr.c_str());
		return pvr::Result::UnknownError;
	}

	if (!loadShaders())
	{
		shell->setExitMessage(ErrorStr.c_str());
		return pvr::Result::UnknownError;
	}

	gl::CullFace(GL_BACK);
	gl::Enable(GL_CULL_FACE);
	gl::Enable(GL_DEPTH_TEST);
	gl::Enable(GL_BLEND);
	gl::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	gl::ClearColor(0.0, 0.0, 0.0, 1.0f);
	{
		projection = pvr::math::perspectiveFov(pvr::Api::OpenGLES3, scene->getCamera(0).getFOV(), (float)VIEWPORT_WIDTH,
			(float)VIEWPORT_HEIGHT, scene->getCamera(0).getNear(), scene->getCamera(0).getFar());
	}
	return pvr::Result::Success;
}

pvr::Result OGLESCluster::releaseView()
{
	gl::DeleteTextures((GLsizei)texDiffuse.size(), &texDiffuse[0]);

	for (int i = 0; i < SHADER_TOTAL; i++)
	{
		gl::DeleteProgram(ShaderProgram[i].handle);

		gl::DeleteShader(ShaderProgram[i].vertShader);
		gl::DeleteShader(ShaderProgram[i].fragShader);

	}
	scene->destroy();
	gl::DeleteBuffers((GLsizei)vbo.size(), &vbo[0]);
	gl::DeleteBuffers((GLsizei)indexVbo.size(), &indexVbo[0]);
	return pvr::Result::Success;
}

pvr::Result OGLESCluster::renderFrame()
{
	//gl::Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	gl::Clear(GL_DEPTH_BUFFER_BIT);

	frame += (float)shell->getFrameTime() / 90.f; 

	if (frame > scene->getNumFrames() - 2) { frame = 0; }

	scene->setCurrentFrame(frame);

	glm::vec3 lightDirVec3;
	scene->getLightDirection(0, lightDirVec3);
	glm::vec4 lightDirVec4(glm::normalize(lightDirVec3), 1.f);

	glm::mat4 mView;
	glm::vec3	vFrom, vTo, vUp;
	float fFOV;

	scene->getCameraProperties(0, fFOV, vFrom, vTo, vUp);

	mView = glm::lookAt(vFrom, vTo, vUp);

	for (unsigned int i = 0; i < scene->getNumMeshNodes(); ++i)
	{
		const pvr::int32 matId = scene->getMeshNode(i).getMaterialIndex();
		const pvr::assets::Model::Material& material = scene->getMaterial(matId);
		int shaderID = 0;
		bool blendEnable = false;
		//pvr::Log(pvr::Log.Information, "mat: %d %s ", matId, material.getName().c_str());
		if (material.getName() == "black_matte" || material.getName() == "bar_linear")
		{
			//continue;
			shaderID = 0;
		}
		else if (material.getName() == "chrome" || material.getName() == "tickmarks_big" || material.getName() == "copper")
		{
			//continue;
			shaderID = 1;
		}
		else if (material.getName() == "pointer_needle" || material.getName() == "diamond_tile" || material.getName() == "ring" || material.getName() == "h3_texture" || material.getName() == "h3_texture_alpha" || material.getName() == "mask" || material.getName() == "tickmarks_small" || material.getName() == "guilloche")
		{
			//continue;
			if(material.getName() == "mask" || material.getName() == "h3_texture")
				blendEnable = true;
			shaderID = 2;
		}
		else
		{
			//pvr::Log(pvr::Log.Information, "mat: %d %s misssing.. !!!!!!!!!!!!!!!!!!!!!!!! ", matId, material.getName().c_str());
			continue;
		}


		gl::UseProgram(ShaderProgram[shaderID].handle);

		glm::mat4 mWorld = scene->getWorldMatrix(i);

		glm::mat4 mModelView, mMVP;
		mModelView = mView * mWorld;
		mMVP = projection * mModelView;
		glm::mat4 worldIT = glm::inverseTranspose(mModelView);
		gl::UniformMatrix4fv(ShaderProgram[shaderID].uiMVPMatrixLoc, 1, GL_FALSE, glm::value_ptr(mMVP));
		gl::UniformMatrix4fv(ShaderProgram[shaderID].uiWorldViewITLoc, 1, GL_FALSE, glm::value_ptr(worldIT));

		glm::vec4 vLightDir = mView * lightDirVec4;
		glm::vec3 dirModelVec3 = glm::normalize(*(glm::vec3*)&vLightDir);

		gl::Uniform3f(ShaderProgram[shaderID].uiLightDirLoc, dirModelVec3.x, dirModelVec3.y, dirModelVec3.z);

		if (blendEnable)
			gl::Enable(GL_BLEND); 
		else
			gl::Disable(GL_BLEND);

		drawMesh(i);
	}
	return pvr::Result::Success;
}

void OGLESCluster::drawMesh(int nodeIndex)
{
	int meshIndex = scene->getMeshNode(nodeIndex).getObjectId();
	const pvr::assets::Mesh& mesh = scene->getMesh(meshIndex);


	gl::ActiveTexture(GL_TEXTURE0 + 0);
	const pvr::int32 matId = scene->getMeshNode(nodeIndex).getMaterialIndex();
	gl::BindTexture(GL_TEXTURE_2D, texDiffuse[matId]);

	gl::ActiveTexture(GL_TEXTURE0 + 1);
	gl::BindTexture(GL_TEXTURE_2D, texCubeMap);

	gl::BindBuffer(GL_ARRAY_BUFFER, vbo[meshIndex]);
	gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexVbo[meshIndex]);

	gl::EnableVertexAttribArray(VertexArray);
	gl::EnableVertexAttribArray(NormalArray);
	gl::EnableVertexAttribArray(TexCoordArray);

	const pvr::assets::VertexAttributeData* posAttrib = mesh.getVertexAttributeByName(AttribNames[0]);
	const pvr::assets::VertexAttributeData* normalAttrib = mesh.getVertexAttributeByName(AttribNames[1]);
	const pvr::assets::VertexAttributeData* texCoordAttrib = mesh.getVertexAttributeByName(AttribNames[2]);

	gl::VertexAttribPointer(VertexArray, posAttrib->getN(), GL_FLOAT, GL_FALSE, mesh.getStride(0), (void*)(size_t)posAttrib->getOffset());
	gl::VertexAttribPointer(NormalArray, normalAttrib->getN(), GL_FLOAT, GL_FALSE, mesh.getStride(0), (void*)(size_t)normalAttrib->getOffset());
	gl::VertexAttribPointer(TexCoordArray, texCoordAttrib->getN(), GL_FLOAT, GL_FALSE, mesh.getStride(0), (void*)(size_t)texCoordAttrib->getOffset());
	if (mesh.getNumStrips() == 0)
	{
		if (indexVbo[meshIndex])
		{
			GLenum type = (mesh.getFaces().getDataType() == IndexType::IndexType16Bit) ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
			gl::DrawElements(GL_TRIANGLES, mesh.getNumFaces() * 3, type, 0);
		}
		else
		{
			gl::DrawArrays(GL_TRIANGLES, 0, mesh.getNumFaces() * 3);
		}
	}
	else
	{
		pvr::uint32 offset = 0;
		GLenum type = (mesh.getFaces().getDataType() == IndexType::IndexType16Bit) ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;

		for (int i = 0; i < (int)mesh.getNumStrips(); ++i)
		{
			if (indexVbo[meshIndex])
			{
				gl::DrawElements(GL_TRIANGLE_STRIP, mesh.getStripLength(i) + 2, type,
					(void*)(size_t)(offset * mesh.getFaces().getDataSize()));
			}
			else
			{
				gl::DrawArrays(GL_TRIANGLE_STRIP, offset, mesh.getStripLength(i) + 2);
			}
			offset += mesh.getStripLength(i) + 2;
		}
	}

	gl::DisableVertexAttribArray(VertexArray);
	gl::DisableVertexAttribArray(NormalArray);
	gl::DisableVertexAttribArray(TexCoordArray);

	gl::BindBuffer(GL_ARRAY_BUFFER, 0);
	gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}