#include "OGLESMap.h"
#include <iostream>
#include <sstream>

#define CAMERA_ID	1

const char *mapShaderSrcFile[2] = {
	"map/shader_map_ES3.vert", "map/shader_map_ES3.frag"
};

const char mapSceneFileName[] = "map/map_with_camera_imx8.pod"; 

const pvr::StringHash mapAttribNames[] =
{
	"POSITION",
	"NORMAL",
	"UV0",
};
void OGLESMap::LoadMesh(const StringHash& filename, uint32_t num_lods)
{
	MeshLod meshLod;
	meshLod.resize(num_lods);

	for (uint32_t i = 0; i < num_lods; ++i)
	{
		std::stringstream ss;
		ss << i;
		ss << ".pod";

		std::string path = filename.str() + ss.str();
		Log(Log.Information, "Loading model:%s \n", path.c_str());
		pvr::assets::ModelHandle model;
		Stream::ptr_type str = shell->getAssetStream(path);

		if ((model = pvr::assets::Model::createWithReader(pvr::assets::PODReader(str))).isNull())
		{
			assertion(false, strings::createFormatted("Failed to load model file %s", path.c_str()).c_str());
		}
		for (uint32 j = 0; j < model->getNumMeshNodes(); ++j)
		{
			char objName[50];
			sprintf(objName, "Object%02d", j + 1);

			if (model->getMeshNode(j).getName() == objName)
			{
				uint32 meshId = model->getMeshNode(j).getObjectId();
				meshes.map_part[j].resize(num_lods);
				meshes.map_part[j][i].mesh = assets::getMeshHandle(model, meshId);
				//break;
			}
			if (j == model->getNumMeshNodes()) { assertion(false, strings::createFormatted("Could not find mesh %s in model file %s", objName, path.c_str()).c_str()); }
		}
	}
}

MeshLod OGLESMap::loadLodMesh(const StringHash& filename, const StringHash& mesh, uint32_t num_lods)
{
	MeshLod meshLod;
	meshLod.resize(num_lods);

	for (uint32_t i = 0; i < num_lods; ++i)
	{
		std::stringstream ss;
		ss << i;
		ss << ".pod";

		std::string path = filename.str() + ss.str();
		Log(Log.Information, "Loading model:%s mesh:%s\n", path.c_str(), mesh.c_str());
		pvr::assets::ModelHandle model;
		Stream::ptr_type str = shell->getAssetStream(path);

		if ((model = pvr::assets::Model::createWithReader(pvr::assets::PODReader(str))).isNull())
		{
			assertion(false, strings::createFormatted("Failed to load model file %s", path.c_str()).c_str());
		}
		for (uint32 j = 0; j < model->getNumMeshNodes(); ++j)
		{
			if (model->getMeshNode(j).getName() == mesh)
			{
				uint32 meshId = model->getMeshNode(j).getObjectId();
				meshLod[i].mesh = assets::getMeshHandle(model, meshId);
				break;
			}
			if (j == model->getNumMeshNodes()) { assertion(false, strings::createFormatted("Could not find mesh %s in model file %s", mesh.c_str(), path.c_str()).c_str()); }
		}
	}
	return meshLod;
}

bool OGLESMap::loadShaders()
{
	const char* attributes[] = { "inVertex", "inNormal", "inTexCoord" };
	{
		pvr::assets::ShaderFile fileVersioning;
		fileVersioning.populateValidVersions(mapShaderSrcFile[0], *shell);

		native::HShader_ shaders[2];
		if (!pvr::nativeGles::loadShader(*fileVersioning.getBestStreamForApi(pvr::Api::OpenGLES31), ShaderType::VertexShader, 0, 0,
			shaders[0])) 
		{
			return false;
		}

		fileVersioning.populateValidVersions(mapShaderSrcFile[1], *shell);
		if (!pvr::nativeGles::loadShader(*fileVersioning.getBestStreamForApi(pvr::Api::OpenGLES31), ShaderType::FragmentShader, 0,
			0, shaders[1]))
		{
			return false;
		}
		if (createShaderProgram(shaders, 2, attributes, sizeof(attributes) / sizeof(attributes[0]),
			ShaderProgram.handle) != pvr::Result::Success)
		{
			return false;
		}

		gl::UseProgram(ShaderProgram.handle);
		int locTex = gl::GetUniformLocation(ShaderProgram.handle, "sTexture");
		if (locTex != -1)
			gl::Uniform1i(locTex, 0);
		int locCubeTex = gl::GetUniformLocation(ShaderProgram.handle, "sSkybox");
		if (locCubeTex != -1)
			gl::Uniform1i(locCubeTex, 1);

		ShaderProgram.uiMVPMatrixLoc = gl::GetUniformLocation(ShaderProgram.handle, "MVPMatrix");
		ShaderProgram.uiLightDirLoc = gl::GetUniformLocation(ShaderProgram.handle, "LightDirection");
		ShaderProgram.uiWorldViewITLoc = gl::GetUniformLocation(ShaderProgram.handle, "WorldViewIT");
	}

	return true;
}


pvr::Result OGLESMap::initApplication()
{
	pvr::Result rslt = pvr::Result::Success;
	if ((scene = pvr::assets::Model::createWithReader(pvr::assets::PODReader(shell->getAssetStream(mapSceneFileName)))).isNull())
	{
		shell->setExitMessage("ERROR: Couldn't load the %s file\n", mapSceneFileName);
		return rslt;
	}

	if (scene->getNumCameras() <= CAMERA_ID)
	{
		shell->setExitMessage("ERROR: The scene does not contain a camera\n");
		return pvr::Result::InvalidData;
	}

	frame = 0;

	LoadMesh("map/map_parts", 1);

	return pvr::Result::Success;
}

pvr::Result OGLESMap::quitApplication() 
{
	return pvr::Result::Success; 
}

pvr::Result OGLESMap::initView()
{
	pvr::string ErrorStr;
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

	gl::ClearColor(0.0, 0.0, 0.0, 0.0f);
	gl::ClearDepthf(1.0f);
	
	{
		projMtx = glm::perspective(scene->getCamera(CAMERA_ID).getFOV(),
		                           (float)VIEWPORT_WIDTH / (float)VIEWPORT_HEIGHT, scene->getCamera(CAMERA_ID).getNear(),
		                           scene->getCamera(CAMERA_ID).getFar());
	}
	glm::vec3 from, to, up(0.0f, 1.0f, 0.0f);
	pvr::float32 fov;
	glm::vec3 cameraPos, cameraTarget, cameraUp;

	scene->getCameraProperties(CAMERA_ID, fov, cameraPos, cameraTarget, cameraUp);
	viewMtx = glm::lookAt(cameraPos, cameraTarget, cameraUp);

	
	meshes.createObjects();


	return pvr::Result::Success;
}

pvr::Result OGLESMap::releaseView()
{
	return pvr::Result::Success;
}

pvr::Result OGLESMap::renderFrame()
{
	gl::Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	frame += (float)shell->getFrameTime() / 30.f;

	if (frame >= scene->getNumFrames() - 1)	{	frame = 0;	}
	if (frame < 0 ) { frame = scene->getNumFrames() - 1; }

	//frame = 500;
	switch (state)
	{
	case 0: frame = 0;	break;
	case 1: frame = 300;	break;
	case 2: frame = 400;	break;
	case 3: frame = 450;	break;
	case 4: frame = 500;	break;
	case 5: frame = 0;	break;
	}

	scene->setCurrentFrame(frame);

	glm::vec3 from, to, up(0.0f, 1.0f, 0.0f);
	pvr::float32 fov;
	glm::vec3 cameraPos, cameraTarget, cameraUp;
	scene->getCameraProperties(CAMERA_ID, fov, cameraPos, cameraTarget, cameraUp);
	viewMtx = glm::lookAt(cameraPos, cameraTarget, cameraUp);

	glm::mat4 cameraMat2 = projMtx * viewMtx;
	math::ViewingFrustum frustumTmp;
	math::getFrustumPlanes(cameraMat2, frustumTmp);

	glm::vec4 lightDirVec4(glm::normalize(cameraTarget), 1.f);

	glm::float32 grid_start_x = (1.0 - NUM_TILES_X) * 0.5;
	glm::float32 grid_start_z = (1.0 - NUM_TILES_Z) * 0.5;

	static int oldVisibleTileCount = 0;
	int count = 0;
	for (uint32 z = 0; z < NUM_TILES_Z; ++z)
	{
		for (uint32 x = 0; x < NUM_TILES_X; ++x)
		{
			glm::float32 pos_x = (grid_start_x + x) * TILE_SIZE_X;
			glm::float32 pos_z = (grid_start_z + z) * TILE_SIZE_Z;

			glm::vec3 tileBL(pos_x, 0, pos_z);
			tileBL += glm::vec3(-TILE_SIZE_X * 0.5, 0, -TILE_SIZE_Z * 0.5);
			glm::vec3 tileTR = tileBL + glm::vec3(TILE_SIZE_X, 0, TILE_SIZE_Z);

			pvr::math::AxisAlignedBox aabb;
			aabb.setMinMax(tileBL, tileTR);

			bool visibility = math::aabbInFrustum(aabb, frustumTmp);
			if (visibility)
			{
				count++;
				gl::UseProgram(ShaderProgram.handle);

				gl::Enable(GL_BLEND);

				glm::mat4 mWorld = glm::mat4(1);
				mWorld = glm::translate(mWorld, glm::vec3(pos_x, 0.0, pos_z));

				glm::mat4 mModelView, mMVP;
				mModelView = viewMtx * mWorld;
				mMVP = projMtx * mModelView;
				glm::mat4 worldIT = glm::inverseTranspose(mModelView);
				gl::UniformMatrix4fv(ShaderProgram.uiMVPMatrixLoc, 1, GL_FALSE, glm::value_ptr(mMVP));
				gl::UniformMatrix4fv(ShaderProgram.uiWorldViewITLoc, 1, GL_FALSE, glm::value_ptr(worldIT));

				glm::vec4 vLightDir = viewMtx * lightDirVec4;
				glm::vec3 dirModelVec3 = glm::normalize(*(glm::vec3*)&vLightDir);

				gl::Uniform3f(ShaderProgram.uiLightDirLoc, dirModelVec3.x, dirModelVec3.y, dirModelVec3.z);

				for (int i = 0; i < NUM_OBJECTS_PER_TILE; i++)
				{
					Mesh *mesh = &meshes.map_part[i][0];

					gl::BindBuffer(GL_ARRAY_BUFFER, mesh->vbo);
					gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->indexVbo);

					gl::EnableVertexAttribArray(VertexArray);
					gl::EnableVertexAttribArray(NormalArray);
					//gl::EnableVertexAttribArray(TexCoordArray);

					const pvr::assets::VertexAttributeData* posAttrib = mesh->mesh->getVertexAttributeByName(mapAttribNames[0]);
					const pvr::assets::VertexAttributeData* normalAttrib = mesh->mesh->getVertexAttributeByName(mapAttribNames[1]);
					const pvr::assets::VertexAttributeData* texCoordAttrib = mesh->mesh->getVertexAttributeByName(mapAttribNames[2]);

					gl::VertexAttribPointer(VertexArray, posAttrib->getN(), GL_FLOAT, GL_FALSE, mesh->mesh->getStride(0), (void*)(size_t)posAttrib->getOffset());
					gl::VertexAttribPointer(NormalArray, normalAttrib->getN(), GL_FLOAT, GL_FALSE, mesh->mesh->getStride(0), (void*)(size_t)normalAttrib->getOffset());
				//	gl::VertexAttribPointer(TexCoordArray, texCoordAttrib->getN(), GL_FLOAT, GL_FALSE, mesh->mesh->getStride(0), (void*)(size_t)texCoordAttrib->getOffset());
					if (mesh->mesh->getNumStrips() == 0)
					{
						if (mesh->indexVbo)
						{
							GLenum type = (mesh->mesh->getFaces().getDataType() == IndexType::IndexType16Bit) ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
							gl::DrawElements(GL_TRIANGLES, mesh->mesh->getNumFaces() * 3, type, 0);
						}
						else
						{
							gl::DrawArrays(GL_TRIANGLES, 0, mesh->mesh->getNumFaces() * 3);
						}
					}
					else
					{
						pvr::uint32 offset = 0;
						GLenum type = (mesh->mesh->getFaces().getDataType() == IndexType::IndexType16Bit) ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;

						for (int i = 0; i < (int)mesh->mesh->getNumStrips(); ++i)
						{
							if (mesh->indexVbo)
							{
								gl::DrawElements(GL_TRIANGLE_STRIP, mesh->mesh->getStripLength(i) + 2, type,
									(void*)(size_t)(offset * mesh->mesh->getFaces().getDataSize()));
							}
							else
							{
								gl::DrawArrays(GL_TRIANGLE_STRIP, offset, mesh->mesh->getStripLength(i) + 2);
							}
							offset += mesh->mesh->getStripLength(i) + 2;
						}
					}

					gl::DisableVertexAttribArray(VertexArray);
					gl::DisableVertexAttribArray(NormalArray);
					//gl::DisableVertexAttribArray(TexCoordArray);

					gl::BindBuffer(GL_ARRAY_BUFFER, 0);
					gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
				}
			}
			else
			{
				int sss = 0;
			}
		}
	}

	if (oldVisibleTileCount != count)
	{
		oldVisibleTileCount = count;
		pvr::Log(pvr::Log.Information, "visible tile: %d", oldVisibleTileCount);
	}

	return pvr::Result::Success;
}