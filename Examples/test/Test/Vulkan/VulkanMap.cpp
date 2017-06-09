#include "VulkanMap.h"
#include "VulkanTestDemo.h"

#define CAMERA_ID	1
#define VIEWPORT_WIDTH	1920
#define VIEWPORT_HEIGHT	720


const char mapSceneFileName[] = "map/map_with_camera_imx8.pod";


utils::VertexBindings attributeBindings[] =
{
	{ "POSITION", 0 },
	{ "NORMAL", 1 },
	{ "UV0", 2 },
};






void GnomeHordeTileThreadData::garbageCollectPreviousFrameFreeCommandBuffers(uint8 swapIndex)
{
	auto& freeCmd = apiObj->freeCmdBuffers[swapIndex];
	auto& prefreeCmd = apiObj->preFreeCmdBuffers[swapIndex];

	std::move(prefreeCmd.begin(), prefreeCmd.end(), std::back_inserter(freeCmd));
	prefreeCmd.clear();
	if (freeCmd.size() > 10)
	{
		freeCmd.clear();
	}
}

api::SecondaryCommandBuffer GnomeHordeTileThreadData::getFreeCommandBuffer(uint8 swapIndex)
{
	{
		std::unique_lock<std::mutex> lock(cmdMutex);
		if (apiObj->lastSwapIndex != app->swapIndex)
		{
			apiObj->lastSwapIndex = app->swapIndex;
			garbageCollectPreviousFrameFreeCommandBuffers(app->swapIndex);
		}
	}

	api::SecondaryCommandBuffer retval;
	{
		std::unique_lock<std::mutex> lock(cmdMutex);
		if (!apiObj->freeCmdBuffers[swapIndex].empty())
		{
			retval = apiObj->freeCmdBuffers[swapIndex].back();
			apiObj->freeCmdBuffers[swapIndex].pop_back();
		}
	}
	if (retval.isNull())
	{
		retval = apiObj->cmdPools.back()->allocateSecondaryCommandBuffer();
		if (retval.isNull())
		{
			Log(Log.Error, "[THREAD %d] Command buffer allocation failed, . Trying to create additional command buffer pool.", id);
			apiObj->cmdPools.push_back(app->demo->getGraphicsContext()->createCommandPool());
			retval = apiObj->cmdPools.back()->allocateSecondaryCommandBuffer();
			if (retval.isNull())
			{
				Log(Log.Critical, "COMMAND BUFFER ALLOCATION FAILED ON FRESH COMMAND POOL.");
			}
		}
	}
	return retval;
}

void GnomeHordeTileThreadData::freeCommandBuffer(const api::SecondaryCommandBuffer& cmdBuff, uint8 swapIndex)
{
	std::unique_lock<std::mutex> lock(cmdMutex);
	if (apiObj->lastSwapIndex != app->swapIndex)
	{
		apiObj->lastSwapIndex = app->swapIndex;
		garbageCollectPreviousFrameFreeCommandBuffers(app->swapIndex);
	}
	apiObj->preFreeCmdBuffers[swapIndex].push_back(cmdBuff);
}

bool GnomeHordeTileThreadData::doWork()
{
	int32 batch_size = 4;
	glm::ivec2 workItem[4];
	int32 result;
	if ((result = app->tilesToProcessQ.consume(apiObj->processQConsumerToken, workItem[0])))
	{
		generateTileBuffer(workItem, result);
	}
	return result != 0;
}

void GnomeHordeTileThreadData::generateTileBuffer(const ivec2* tileIdxs, uint32 numTiles)
{
	// Lambda to create the tiles secondary cmdbs
	utils::StructuredMemoryView& uboAllObj = app->apiObj->uboPerObject;
	const api::DescriptorSet& descSetAllObj = app->apiObj->descSetAllObjects;

	for (uint32 tilenum = 0; tilenum < numTiles; ++tilenum)
	{
		ivec2 tileId2d = tileIdxs[tilenum];
		uint32 x = tileId2d.x, y = tileId2d.y;
		uint32 tileIdx = y * NUM_TILES_X + x;

		TileInfo& tile = app->apiObj->tileInfos[y][x];

		// Recreate the cmdb
		for (uint32 swapIdx = 0; swapIdx < app->numSwapImages; ++swapIdx)
		{
			MultiBuffering& multi = app->apiObj->multiBuffering[swapIdx];
			tile.cbs[swapIdx] = getFreeCommandBuffer(swapIdx);
			tile.threadId = id;

			auto& cb = tile.cbs[swapIdx];

			cb->beginRecording(app->demo->deviceResources->fboOnScreen[swapIdx]);

			for (uint32_t objId = 0; objId < NUM_OBJECTS_PER_TILE; ++objId)
			{
				TileObject& obj = tile.objects[objId];
				if (obj.mesh == nullptr)
					continue;
				uint32 lod = std::min<uint32>(static_cast<pvr::uint32>(obj.mesh->size()) - 1, tile.lod);

				cb->bindPipeline(tile.pipeline);

				Mesh& mesh = (*obj.mesh)[lod];

				uint32 offset = uboAllObj.getDynamicOffset(0, tileIdx * NUM_OBJECTS_PER_TILE + objId);

				cb->bindDescriptorSet(app->apiObj->pipeLayout, 0, descSetAllObj, &offset, 1);
				cb->bindDescriptorSet(app->apiObj->pipeLayout, 1, multi.descSetPerFrame, 0, 0);

				//If different than before?
				cb->bindVertexBuffer(mesh.vbo, 0, 0);
				cb->bindIndexBuffer(mesh.ibo, 0, mesh.mesh->getFaces().getDataType());

				// Offset in the per-object transformation matrices UBO - these do not change frame-to-frame
				//getArrayOffset, will return the actual byte offset of item #(first param) that is in an
				//array of items, at array index #(second param).
				cb->drawIndexed(0, mesh.mesh->getNumIndices());
			}
			cb->endRecording();
		}
		app->tilesToDrawQ.produce(apiObj->drawQProducerToken, tileId2d);
		++app->itemsToDraw;
		//Add the item to the "processed" queue and mark the count. If it's the last item, mark that the main
		//thread "must unblock"
		if (--app->itemsRemaining == 0)
		{
			//For good measure... The above will unblock the main Q, but even though it appears that the main thread will
			//be able to avoid it, we must still signal the "all done" for purposes of robustness.
			++app->poisonPill;
			app->tilesToDrawQ.unblockOne();
		}
	}
}

void GnomeHordeWorkerThread::addlog(const std::string& str)
{
	std::unique_lock<std::mutex> lock(app->logMutex);
	app->multiThreadLog.push_back(str);
}

void GnomeHordeWorkerThread::run()
{
	addlog(strings::createFormatted("=== Tile Visibility Thread [%d] ===            Starting", id));
	running = true;
	while (doWork()) { continue; } // grabs a piece of work as long as the queue is not empty.
	running = false;
	addlog(strings::createFormatted("=== Tile Visibility Thread [%d] ===            Exiting", id));
}

bool GnomeHordeVisibilityThreadData::doWork()
{
	int32 batch_size = 4;
	int32 workItem[4];
	int32 result;
	if ((result = app->linesToProcessQ.consume(apiObj->linesQConsumerToken, workItem[0])))
	{
		determineLineVisibility(workItem, result);
	}
	return result != 0;
}

void GnomeHordeVisibilityThreadData::determineLineVisibility(const int32* lineIdxs, uint32 numLines)
{
	auto& tileInfos = app->apiObj->tileInfos;
	math::ViewingFrustum frustum; utils::memCopyFromVolatile(frustum, app->frustum);
	glm::vec3 camPos; utils::memCopyFromVolatile(camPos, app->cameraPosition);

	TileTasksQueue& processQ = app->tilesToProcessQ;
	TileTasksQueue& drawQ = app->tilesToDrawQ;

	uint8 numSwapImages = app->numSwapImages;
	for (uint32 line = 0; line < numLines; ++line)
	{
		glm::ivec2 id2d(0, lineIdxs[line]);
		for (id2d.x = 0; id2d.x < NUM_TILES_X; ++id2d.x)
		{
			tileInfos[id2d.y][id2d.x].visibility = math::aabbInFrustum(tileInfos[id2d.y][id2d.x].aabb, frustum);

			TileInfo& tile = tileInfos[id2d.y][id2d.x];

			if (tile.visibility != tile.oldVisibility) // || tile.lod != tile.oldLod) // The tile has some change. Will need to do something.
			{
				for (uint32 i = 0; i < numSwapImages; ++i) //First, free its pre-existing command buffers (just mark free)
				{
					if (tile.cbs[i].isValid())
					{
						app->apiObj->tileThreadData[tile.threadId].freeCommandBuffer(tile.cbs[i], i);
						tile.cbs[i].reset();
					}
				}

				//// PRODUCER CONSUMER QUEUE ////
				if (tile.visibility) //If the tile is visible, it will need to be generated.
				{
					// COMMAND BUFFER GENERATION BEGINS ** IMMEDIATELY ** on a worker thread
					processQ.produce(apiObj->processQproducerToken, id2d);
					//// PRODUCE ///
					//The producer thread must signal the unblock...
				}
				//Otherwise, no further action is required.
			}
			else if (tile.visibility) // Tile had no change, but was visible - just add it to the drawing queue.
			{
				++app->itemsToDraw;
				drawQ.produce(apiObj->drawQproducerToken, id2d);
				//Add the item to the "processed" queue and mark the count. If it's the last item, mark that the main
				//thread "must unblock"
				if (--app->itemsRemaining == 0)
				{
					//Signal that we have unblocked the main thread
					++app->poisonPill;
					drawQ.unblockOne();
				}
			}

			tile.oldVisibility = tile.visibility;
			tile.oldLod = tile.lod;

			if (!tile.visibility)
			{
				//Remove the item from the total expected number of items if it was not visible
				//If it was the last one, make sure the main thread is not blocked forever. Since there is an actual race
				//condition (but we wanted to avoid using very expensive synchronization for the exit condition), we are
				//just making sure the main thread will not block forever (In a sort of "poison pill" technique - we are
				//in a sense putting an item in the queue that will tell the main thread to stop).
				if (--app->itemsRemaining == 0)
				{
					//Make sure the main thread does not remain blocked forever
					++app->poisonPill;
					drawQ.unblockOne();
				}
			}
		}
	}

}

///// UTILS (local) /////
inline vec3 getTrackPosition(float32 time, const vec3& world_size)
{
	const float32 angle = time * 0.02f;
	const vec3 centre = world_size * 0.5f;
	const vec3 radius = world_size * 0.2f;
	// Main circle
	float a1 = time * 0.07f;
	float a2 = time * 0.1f;
	float a3 = angle;

	const float32 h = glm::sin(a1) * 15.f + 100.f;
	const float32 radius_factor = 0.95f + 0.1f * glm::sin(a2);
	const glm::vec3 circle(glm::sin(a3)*radius.x * radius_factor, h, glm::cos(a3)*radius.z * radius_factor);

	return centre + circle;
}

float32 initializeGridPosition(std::vector<float>& grid, uint32 numItemsPerRow)
{
	//| x | x | x |
	//| x | x | x |
	//| x | x | x |
	//Jittered Grid - each object is placed on the center of a normal grid, and then moved randomly around.
	const float32 MIN_DISTANCE_FACTOR = -.2f; // Minimum item distance is 1/5 th their starting distance

	grid.resize(numItemsPerRow);
	float32 distance = 1.f / numItemsPerRow;
	grid[0] = .5f * distance;
	for (uint32 i = 1; i < numItemsPerRow; ++i)
	{
		grid[i] = grid[i - 1] + distance;
	}
	float32 deviation = distance * .5f * (1.f - MIN_DISTANCE_FACTOR);
	return deviation;
}

inline void generatePositions(vec3* points, vec3 minBound, vec3 maxBound)
{
	static std::vector<float> normalGridPositions;
	static const uint32 numItemsPerRow = (uint32)(sqrtf(NUM_UNIQUE_OBJECTS_PER_TILE));
	static const float deviation = initializeGridPosition(normalGridPositions, numItemsPerRow);

	for (int i = 0; i < numItemsPerRow * numItemsPerRow; ++i)
	{
		points[i] = glm::mix(minBound, maxBound, vec3(0.5f, 0.5f, 0)); // vec3(randomrange(-1.f, 1.f), 0, randomrange(-1.f, 1.f)));
	}
}


/////////// CLASS VulkanMap ///////////
///// CALLBACKS - IMPLEMENTATION OF pvr::Shell /////
Result VulkanMap::initApplication()
{
	if ((scene = pvr::assets::Model::createWithReader(pvr::assets::PODReader(demo->getAssetStream(mapSceneFileName)))).isNull())
	{
		demo->setExitMessage("ERROR: Couldn't load the %s file\n", mapSceneFileName);
		exit(0);
	}

	if (scene->getNumCameras() <= CAMERA_ID)
	{
		demo->setExitMessage("ERROR: The scene does not contain a camera\n");
		return pvr::Result::InvalidData;
	}

	int num_cores = (int)std::thread::hardware_concurrency();
	int THREAD_FACTOR_RELAXATION = 1;

	int thread_factor = std::max(num_cores - THREAD_FACTOR_RELAXATION, 1);

	numVisibilityThreads = std::min(thread_factor, (int)MAX_NUMBER_OF_THREADS);
	numTileThreads = std::min(thread_factor, (int)MAX_NUMBER_OF_THREADS);
	Log(Log.Information,
		"Hardware concurreny reported: %u cores. Enabling %u visibility threads plus %u tile processing threads\n",
		num_cores, numVisibilityThreads, numTileThreads);

	// Meshes
	//for (int i = 0; i < NUM_OBJECTS_PER_TILE; i++)
	//{
	//	char objName[50];
	//	sprintf(objName, "Object%02d", i+1);
	//	meshes.map_part[i] = loadLodMesh("map/map_parts", objName, 1);
	//}

	LoadMesh("map/map_parts", 1);

	return Result::Success;
}

Result VulkanMap::quitApplication()
{
	meshes.clearAll();
	return Result::Success;
}

void VulkanMap::setUpUI()
{

}

Result VulkanMap::initView()
{
	apiObj.reset(new ApiObjects(linesToProcessQ, tilesToDrawQ));

	numSwapImages = demo->getSwapChainLength();

	for (uint32 i = 0; i < numSwapImages; ++i)
	{
		apiObj->multiBuffering[i].fence = demo->deviceResources->context->createFence(true);
	}

	initUboStructuredObjects();

	//Create Descriptor set layouts
	api::DescriptorSetLayoutCreateParam dynamicUboDescParam;
	dynamicUboDescParam.setBinding(0, types::DescriptorType::UniformBufferDynamic, 1, types::ShaderStageFlags::Vertex);
	api::DescriptorSetLayout descLayoutUboDynamic = demo->deviceResources->context->createDescriptorSetLayout(dynamicUboDescParam);

	api::DescriptorSetLayoutCreateParam uboDescParam;
	uboDescParam.setBinding(0, types::DescriptorType::UniformBuffer, 1, types::ShaderStageFlags::Vertex);
	api::DescriptorSetLayout descLayoutUboStatic = demo->deviceResources->context->createDescriptorSetLayout(uboDescParam);

	//Create Pipelines
	{
		apiObj->pipeLayout = demo->deviceResources->context->createPipelineLayout(
			api::PipelineLayoutCreateParam()
			.setDescSetLayout(0, descLayoutUboDynamic)
			.setDescSetLayout(1, descLayoutUboStatic));

		// Must not assume the cache will always work

		api::Shader map_vert = demo->deviceResources->context->createShader(*demo->getAssetStream("map/shader_map_vulkan.vert.spv"), types::ShaderType::VertexShader);
		api::Shader map_frag = demo->deviceResources->context->createShader(*demo->getAssetStream("map/shader_map_vulkan.frag.spv"), types::ShaderType::FragmentShader);

		api::GraphicsPipelineCreateParam pipeCreate;
		types::BlendingConfig cbStateNoBlend;
		types::BlendingConfig cbStateBlend(true, types::BlendFactor::OneMinusSrcAlpha, types::BlendFactor::SrcAlpha, types::BlendOp::Add);
		types::BlendingConfig cbStatePremulAlpha(true, types::BlendFactor::One, types::BlendFactor::OneMinusSrcAlpha, types::BlendOp::Add);

		utils::createInputAssemblyFromMesh(*meshes.map_part[0][0].mesh, &attributeBindings[0], 3, pipeCreate);
		pipeCreate.rasterizer.setFrontFaceWinding(types::PolygonWindingOrder::FrontFaceCCW);
		pipeCreate.rasterizer.setCullFace(types::Face::Back);
		pipeCreate.depthStencil.setDepthTestEnable(true);
		pipeCreate.depthStencil.setDepthCompareFunc(pvr::types::ComparisonMode::Less);
		pipeCreate.depthStencil.setDepthWrite(true);
		pipeCreate.renderPass = demo->deviceResources->fboOnScreen[0]->getRenderPass();
		pipeCreate.pipelineLayout = apiObj->pipeLayout;

		pipeCreate.vertexShader = map_vert;
		pipeCreate.fragmentShader = map_frag;
		pipeCreate.colorBlend.setAttachmentState(0, cbStateNoBlend);
		if ((apiObj->pipelines.solid = demo->deviceResources->context->createGraphicsPipeline(pipeCreate)).isNull())
		{
			demo->setExitMessage("Failed to create Opaque rendering pipeline");
			return Result::UnknownError;
		}
	}

	createDescSetsAndTiles(descLayoutUboDynamic, descLayoutUboStatic);

	animDetails.logicTime = 0.f;
	animDetails.gameTime = 0.f;
	{
		for (uint32 i = 0; i < numVisibilityThreads; ++i)
		{
			apiObj->visibilityThreadData[i].id = i;
			apiObj->visibilityThreadData[i].app = this;
			apiObj->visibilityThreadData[i].apiObj.reset(new GnomeHordeVisibilityThreadData::ApiObjects(linesToProcessQ, tilesToProcessQ, tilesToDrawQ));
			apiObj->visibilityThreadData[i].thread = std::thread(&GnomeHordeVisibilityThreadData::run, (GnomeHordeVisibilityThreadData*)&apiObj->visibilityThreadData[i]);
		}
		for (uint32 i = 0; i < numTileThreads; ++i)
		{
			apiObj->tileThreadData[i].id = i;
			apiObj->tileThreadData[i].app = this;
			apiObj->tileThreadData[i].apiObj.reset(new GnomeHordeTileThreadData::ApiObjects(tilesToProcessQ, tilesToDrawQ));
			apiObj->tileThreadData[i].apiObj->cmdPools.clear();
			apiObj->tileThreadData[i].apiObj->cmdPools.push_back(demo->deviceResources->context->createCommandPool());
			apiObj->tileThreadData[i].thread = std::thread(&GnomeHordeTileThreadData::run, (GnomeHordeTileThreadData*)&apiObj->tileThreadData[i]);
		}
	}
	printLog();
	return Result::Success;
}

Result VulkanMap::releaseView()
{
	Log(Log.Information, "Signalling all worker threads: Signal drain empty queues...");
	//Done will allow the queue to finish its work if it has any, but then immediately
	//afterwards it will free any and all threads waiting. Any threads attempting to
	//dequeue work from the queue will immediately return "false".
	linesToProcessQ.done();
	tilesToProcessQ.done();
	tilesToDrawQ.done();

	//waitIdle is being called to make sure the command buffers we will be destroying
	//are not being referenced.
	demo->getGraphicsContext()->waitIdle();

	Log(Log.Information, "Joining all worker threads...");

	//Finally, tear down everything.
	for (uint32 i = 0; i < numVisibilityThreads; ++i)
	{
		apiObj->visibilityThreadData[i].thread.join();
	}
	for (uint32 i = 0; i < numTileThreads; ++i)
	{
		apiObj->tileThreadData[i].thread.join();
	}

	//Clear all objects. This will also free the command buffers that were allocated
	//from the worker thread's command pools, but are currently only held by the
	//tiles.
	apiObj.reset();
	meshes.clearApiObjects();

	Log(Log.Information, "All worker threads done!");
	return Result::Success;
}

Result VulkanMap::renderFrame()
{
	static float frame = 0;
	static int oldSubmittedCmdCount = 0;

	swapIndex = demo->getSwapChainIndex();

	frame += (float)demo->getFrameTime() / 30.f;

	if (frame >= scene->getNumFrames() - 1) { frame = 0; }
	if (frame < 0) { frame = scene->getNumFrames() - 1; }

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


	glm::mat4 projMtx, viewMtx;
	{
		{
			projMtx = glm::perspective(scene->getCamera(CAMERA_ID).getFOV(),
				(float)VIEWPORT_WIDTH / (float)VIEWPORT_HEIGHT, scene->getCamera(CAMERA_ID).getNear(),
				scene->getCamera(CAMERA_ID).getFar());
		}
		{
			glm::vec3 from, to, up(0.0f, 1.0f, 0.0f);
			pvr::float32 fov;
			glm::vec3 cameraPos, cameraTarget, cameraUp;

			scene->getCameraProperties(CAMERA_ID, fov, cameraPos, cameraTarget, cameraUp);
			viewMtx = glm::lookAt(cameraPos, cameraTarget, cameraUp);
		}
	}

	glm::vec3 from, to, up(0.0f, 1.0f, 0.0f);
	pvr::float32 fov;
	glm::vec3 cameraPos, cameraTarget, cameraUp;

	scene->getCameraProperties(CAMERA_ID, fov, cameraPos, cameraTarget, cameraUp);
	viewMtx = glm::lookAt(cameraPos, cameraTarget, cameraUp);

	//mat4 cameraMat = projMtx * viewMtx;

	//mat4 cameraMat2 = projMtx * viewMtx;

	mat4 cameraMat = math::perspectiveFov(pvr::Api::Vulkan, scene->getCamera(CAMERA_ID).getFOV(), (float)VIEWPORT_WIDTH, (float)VIEWPORT_HEIGHT, scene->getCamera(CAMERA_ID).getNear(), scene->getCamera(CAMERA_ID).getFar()) *
		glm::lookAt(cameraPos, cameraTarget, cameraUp);

	mat4 cameraMat2 = math::perspectiveFov(pvr::Api::Vulkan, scene->getCamera(CAMERA_ID).getFOV(), (float)VIEWPORT_WIDTH, (float)VIEWPORT_HEIGHT, scene->getCamera(CAMERA_ID).getNear(), scene->getCamera(CAMERA_ID).getFar()) *
		glm::lookAt(cameraPos, cameraTarget, cameraUp);

	updateCameraUbo(cameraMat);

	math::ViewingFrustum frustumTmp;
	math::getFrustumPlanes(cameraMat2, frustumTmp);
	utils::memCopyToVolatile(frustum, frustumTmp);

	itemsRemaining.store(NUM_TILES_X * NUM_TILES_Z);
	itemsToDraw.store(0);
	itemsDrawn.store(0);
	poisonPill.store(0);
	submittedCmd.store(0);

	linesToProcessQ.produceMultiple(apiObj->lineQproducerToken, allLines, NUM_TILES_Z);

	enum ItemsTotal { itemsTotal = NUM_TILES_X * NUM_TILES_Z };
	demo->activeCB->get()->enqueueSecondaryCmds_BeginMultiple(255);
	{
		uint32 result;
		glm::ivec2 tileId[256];

		uint32 loop = 0;
		while (itemsDrawn != itemsToDraw || itemsRemaining)
		{

			if ((itemsDrawn > itemsToDraw) && !itemsRemaining)
			{
				if ((result == 0) && (loop > 0)) //NOT THE FIRST TIME?
				{
					Log(Log.Error, "Blocking is not released");
					poisonPill.store(0);
					break;
				}
			}


			result = (uint32)tilesToDrawQ.consumeMultiple(apiObj->drawQconsumerToken, tileId, 256);
			if (!result) { --poisonPill; }
			itemsDrawn += result;
			for (uint32 i = 0; i < result; ++i)
			{
				submittedCmd++;
				demo->activeCB->get()->enqueueSecondaryCmds_EnqueueMultiple(&(apiObj->tileInfos[tileId[i].y][tileId[i].x].cbs[swapIndex]), 1);
			}
		}

		demo->activeCB->get()->enqueueSecondaryCmds_SubmitMultiple(); 
		if (poisonPill >= 0)
		{
			while (poisonPill--)
			{
				tilesToDrawQ.consume(apiObj->drawQconsumerToken, tileId[255]); //Make sure it is in a consistent state
			}
		}
		else
		{
			Log(Log.Error, "poisonPill is less than 0");
		}
	}

	assertion(linesToProcessQ.isEmpty(), "Initial Line Processing Queue was not empty after work done!");
	assertion(tilesToProcessQ.isEmpty(), "Worker Tile Processing Queue was not empty after work done!");

	//if (oldSubmittedCmdCount != submittedCmd)
	//{
	//	oldSubmittedCmdCount = submittedCmd;
	//	Log(Log.Information, "visible tiles = %d", oldSubmittedCmdCount);
	//}
	printLog();

	return Result::Success;
}

void VulkanMap::kickReleaseCommandBuffers()
{
}

void VulkanMap::updateCameraUbo(const glm::mat4& matrix)
{
	apiObj->multiBuffering[swapIndex].uboPerFrame.map(0);
	apiObj->multiBuffering[swapIndex].uboPerFrame.setValue(0, matrix);
	apiObj->multiBuffering[swapIndex].uboPerFrame.unmap(0);
}

void VulkanMap::createDescSetsAndTiles(const api::DescriptorSetLayout& layoutPerObject,
	const api::DescriptorSetLayout& layoutPerFrameUbo)
{
	apiObj->uboPerObject.connectWithBuffer(
		0, demo->deviceResources->context->createBufferView(
			demo->deviceResources->context->createBuffer(apiObj->uboPerObject.getAlignedTotalSize(), types::BufferBindingUse::UniformBuffer, true),
			0, apiObj->uboPerObject.getAlignedElementSize()));

	apiObj->descSetAllObjects = demo->deviceResources->context->createDescriptorSetOnDefaultPool(layoutPerObject);
	apiObj->descSetAllObjects->update(api::DescriptorSetUpdate().setDynamicUbo(0, apiObj->uboPerObject.getConnectedBuffer(0)));

	for (uint32 i = 0; i < numSwapImages; ++i)
	{
		auto& current = apiObj->multiBuffering[i];
		current.descSetPerFrame = demo->deviceResources->context->createDescriptorSetOnDefaultPool(layoutPerFrameUbo);
		current.uboPerFrame.connectWithBuffer(0, demo->deviceResources->context->createBufferAndView(current.uboPerFrame.getAlignedElementSize(),
			types::BufferBindingUse::UniformBuffer, true));
		current.descSetPerFrame->update(api::DescriptorSetUpdate().setUbo(0, current.uboPerFrame.getConnectedBuffer(0)));
	}
	meshes.createApiObjects(demo->deviceResources->context);

	utils::StructuredMemoryView& perObj = apiObj->uboPerObject;
	uint32 mvIndex = perObj.getIndex("modelView");
	uint32 mvITIndex = perObj.getIndex("modelViewIT");

	perObj.mapMultipleArrayElements(0, 0, TOTAL_NUMBER_OF_OBJECTS, types::MapBufferFlags::Write);

	glm::float32 grid_start_x = (1.0 - NUM_TILES_X) * 0.5;
	glm::float32 grid_start_z = (1.0 - NUM_TILES_Z) * 0.5;

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

			TileInfo& thisTile = apiObj->tileInfos[z][x];

			thisTile.aabb.setMinMax(tileBL, tileTR);

			thisTile.visibility = false;
			thisTile.lod = uint8(0xFFu);
			thisTile.oldVisibility = false;
			thisTile.oldLod = (uint8)0xFFu - (uint8)1;

			for (int i = 0; i < NUM_OBJECTS_PER_TILE; i++)
			{
				thisTile.objects[i].mesh = &meshes.map_part[i];
				thisTile.pipeline = apiObj->pipelines.solid;
			}

			std::array<vec3, NUM_OBJECTS_PER_TILE> points;
			uint32 tileBaseIndex = (z * NUM_TILES_X + x) * NUM_OBJECTS_PER_TILE;

			for (uint32 obj = 0; obj < NUM_OBJECTS_PER_TILE; ++obj)
			{
				glm::mat4 mWorld = glm::mat4(1);
				mat4 xform = glm::translate(mWorld, glm::vec3(pos_x, 0.0, pos_z));;
				mat4 xformIT = glm::transpose(glm::inverse(xform));

				perObj.getDynamicOffset(mvITIndex, tileBaseIndex + obj);
				perObj.setArrayValue(mvIndex, tileBaseIndex + obj, xform);
				perObj.setArrayValue(mvITIndex, tileBaseIndex + obj, xformIT);

			}
		}
	}
	perObj.unmap(0);
}

MeshLod VulkanMap::loadLodMesh(const StringHash& filename, const StringHash& mesh, uint32_t num_lods)
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
		Stream::ptr_type str = demo->getAssetStream(path);

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

void VulkanMap::LoadMesh(const StringHash& filename, uint32_t num_lods)
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
		Stream::ptr_type str = demo->getAssetStream(path);

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
			}
			if (j == model->getNumMeshNodes()) { assertion(false, strings::createFormatted("Could not find mesh %s in model file %s", objName, path.c_str()).c_str()); }
		}
	}
}

void VulkanMap::initUboStructuredObjects()
{
	for (uint32 i = 0; i < numSwapImages; ++i)
	{
		apiObj->multiBuffering[i].uboPerFrame.addEntryPacked("projectionMat", types::GpuDatatypes::mat4x4);
		apiObj->multiBuffering[i].uboPerFrame.finalize(demo->getGraphicsContext(), 1, types::BufferBindingUse::UniformBuffer);
	}
	apiObj->uboPerObject.addEntryPacked("modelView", types::GpuDatatypes::mat4x4);
	apiObj->uboPerObject.addEntryPacked("modelViewIT", types::GpuDatatypes::mat4x4);
	apiObj->uboPerObject.finalize(demo->getGraphicsContext(), TOTAL_NUMBER_OF_OBJECTS, types::BufferBindingUse::UniformBuffer, true, false);
}