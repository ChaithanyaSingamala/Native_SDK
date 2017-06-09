#pragma once

#include "PVRApi/PVRApi.h"
#include "PVRCore/Threading.h"
#include "PVRShell/PVRShell.h"
#include "PVREngineUtils/PVREngineUtils.h"

using glm::vec2;
using glm::vec3;
using glm::vec4;
using glm::ivec2;
using glm::ivec3;
using glm::ivec4;
using glm::mat3;
using glm::mat4;

using namespace pvr;

enum CONSTANTS
{
	MAX_NUMBER_OF_SWAP_IMAGES = 4,
	MAX_NUMBER_OF_THREADS = 16,
	TILE_SIZE_X = 152,
	TILE_GAP_X = 0,
	TILE_SIZE_Y = 100,
	TILE_SIZE_Z = 152,
	TILE_GAP_Z = 0,
	NUM_TILES_X = 8,
	NUM_TILES_Z = 20,
	NUM_OBJECTS_PER_TILE = 122,
	NUM_UNIQUE_OBJECTS_PER_TILE = 4,
	TOTAL_NUMBER_OF_OBJECTS = NUM_TILES_X * NUM_TILES_Z * NUM_OBJECTS_PER_TILE,
};

class VulkanTestDemo;

class VulkanMap;
typedef LockedQueue<int32> LineTasksQueue;

typedef LockedQueue<glm::ivec2> TileTasksQueue;

class GnomeHordeWorkerThread
{
public:
	GnomeHordeWorkerThread() : id(-1), running(false) {}
	std::thread thread;
	VulkanMap* app;
	volatile uint8 id;
	volatile bool running;
	void addlog(const std::string& str);
	void run();
	virtual bool doWork() = 0;
};

class GnomeHordeTileThreadData : public GnomeHordeWorkerThread
{
public:
	struct ApiObjects
	{
		std::vector<api::CommandPool> cmdPools;
		TileTasksQueue::ConsumerToken processQConsumerToken;
		TileTasksQueue::ProducerToken drawQProducerToken;
		uint8 lastSwapIndex;
		std::array<std::vector<api::SecondaryCommandBuffer>, MAX_NUMBER_OF_SWAP_IMAGES> preFreeCmdBuffers;
		std::array<std::vector<api::SecondaryCommandBuffer>, MAX_NUMBER_OF_SWAP_IMAGES> freeCmdBuffers;
		ApiObjects(TileTasksQueue& processQ, TileTasksQueue& drawQ) :
			processQConsumerToken(processQ.getConsumerToken()), drawQProducerToken(drawQ.getProducerToken()), lastSwapIndex(-1)
		{
		}
	};
	std::mutex cmdMutex;
	std::auto_ptr<ApiObjects> apiObj;

	bool doWork();

	api::SecondaryCommandBuffer getFreeCommandBuffer(uint8 swapIndex);

	void garbageCollectPreviousFrameFreeCommandBuffers(uint8 swapIndex);

	void freeCommandBuffer(const api::SecondaryCommandBuffer& cmdBuff, uint8 swapIndex);

	void generateTileBuffer(const ivec2* tiles, uint32 numTiles);

};

class GnomeHordeVisibilityThreadData : public GnomeHordeWorkerThread
{
public:
	struct ApiObjects
	{
		LineTasksQueue::ConsumerToken linesQConsumerToken;
		TileTasksQueue::ProducerToken processQproducerToken;
		TileTasksQueue::ProducerToken drawQproducerToken;
		ApiObjects(LineTasksQueue& linesQ, TileTasksQueue& processQ, TileTasksQueue& drawQ) :
			linesQConsumerToken(linesQ.getConsumerToken()), processQproducerToken(processQ.getProducerToken()), drawQproducerToken(drawQ.getProducerToken())
		{ }
	};
	std::auto_ptr<ApiObjects> apiObj;

	bool doWork();

	void determineLineVisibility(const int32* lines, uint32 numLines);
};


struct MultiBuffering
{
	utils::StructuredMemoryView uboPerFrame;
	api::DescriptorSet descSetPerFrame;
	api::Fence fence;
};
struct Mesh
{
	assets::MeshHandle mesh;
	api::Buffer vbo;
	api::Buffer ibo;
};
typedef std::vector<Mesh> MeshLod;

struct Meshes
{
	MeshLod map_part[NUM_OBJECTS_PER_TILE];
	void clearAll()
	{
		for(int i = 0; i < NUM_OBJECTS_PER_TILE; i ++)
			clearApiMesh(map_part[i], true);
	}

	void clearApiObjects()
	{
		for (int i = 0; i < NUM_OBJECTS_PER_TILE; i++)
			clearApiMesh(map_part[i], true);
	}
	void createApiObjects(GraphicsContext& ctx)
	{
		for (int i = 0; i < NUM_OBJECTS_PER_TILE; i++)
			createApiMesh(map_part[i], ctx);
	}
private:
	static void clearApiMesh(MeshLod& mesh, bool deleteAll)
	{
		for (MeshLod::iterator it = mesh.begin(); it != mesh.end(); ++it)
		{
			it->vbo.reset();
			it->ibo.reset();
			if (deleteAll) { it->mesh.reset(); }
		}
	}
	static void createApiMesh(MeshLod& mesh, GraphicsContext& ctx)
	{
		for (MeshLod::iterator it = mesh.begin(); it != mesh.end(); ++it)
		{
			utils::createSingleBuffersFromMesh(ctx, *it->mesh, it->vbo, it->ibo);
		}
	}
};


struct Pipelines
{
	api::GraphicsPipeline solid;
};
struct TileObject
{
	MeshLod* mesh;
	//api::GraphicsPipeline pipeline;
};

struct TileInfo
{
	// Per tile info
	std::array<TileObject, NUM_OBJECTS_PER_TILE> objects;
	api::GraphicsPipeline pipeline;
	std::array<api::SecondaryCommandBuffer, MAX_NUMBER_OF_SWAP_IMAGES> cbs;
	math::AxisAlignedBox aabb;
	uint8 threadId;
	uint8 lod;
	uint8 oldLod;
	bool visibility;
	bool oldVisibility;
};

struct ApiObjects
{
	utils::StructuredMemoryView uboPerObject;
	api::PipelineLayout pipeLayout;

	api::Sampler trilinear;
	api::Sampler nonMipmapped;

	api::DescriptorSet descSetAllObjects;
	Pipelines pipelines;


	std::array<GnomeHordeTileThreadData, MAX_NUMBER_OF_THREADS> tileThreadData;
	std::array<GnomeHordeVisibilityThreadData, MAX_NUMBER_OF_THREADS> visibilityThreadData;

	std::array<std::array<TileInfo, NUM_TILES_X>, NUM_TILES_Z> tileInfos;
	MultiBuffering multiBuffering[MAX_NUMBER_OF_SWAP_IMAGES];

	std::array<std::thread, 16> threads;
	LineTasksQueue::ProducerToken lineQproducerToken;
	TileTasksQueue::ConsumerToken drawQconsumerToken;

	ApiObjects(LineTasksQueue& lineQ, TileTasksQueue& drawQ)
		: lineQproducerToken(lineQ.getProducerToken()), drawQconsumerToken(drawQ.getConsumerToken())
	{ }
};



class VulkanMap
{
	pvr::assets::ModelHandle scene;

public:
	VulkanTestDemo *demo;
	std::deque<std::string> multiThreadLog;
	std::mutex logMutex;
	std::atomic<int32> itemsRemaining;
	std::atomic<int32> itemsToDraw;
	std::atomic<int32> itemsDrawn;
	std::atomic<int32> poisonPill; //Technique used to break threads out of their waiting

	uint32 numSwapImages;
	Meshes meshes;
	std::auto_ptr<ApiObjects> apiObj;
	LineTasksQueue linesToProcessQ;
	TileTasksQueue tilesToProcessQ;
	TileTasksQueue tilesToDrawQ;

	std::atomic<int32> submittedCmd;

	uint32 allLines[NUM_TILES_Z]; //Stores the line #. Used to kick initial work in the visibility threads
								  //as each thread will be processing one line

	volatile glm::vec3 cameraPosition;
	volatile math::ViewingFrustum frustum;
	volatile uint8 swapIndex;

	bool isPaused;
	uint8 numVisibilityThreads;
	uint8 numTileThreads;
	VulkanMap(VulkanTestDemo *_demo) : isPaused(false), numVisibilityThreads(0), numTileThreads(0)
	{
		demo = _demo;
		for (int i = 0; i < NUM_TILES_Z; ++i)
		{
			allLines[i] = i;
		}
	}

	Result initApplication();
	Result initView();
	Result releaseView();
	Result quitApplication();
	Result renderFrame();

	void setUpUI();

	MeshLod loadLodMesh(const StringHash& filename, const StringHash& mesh, uint32_t max_lods);
	void LoadMesh(const StringHash & filename, uint32_t num_lods);
	void initUboStructuredObjects();
	void createDescSetsAndTiles(const api::DescriptorSetLayout& layoutPerObject, const api::DescriptorSetLayout& layoutPerFrameUbo);
	void kickReleaseCommandBuffers();
	void updateCameraUbo(const glm::mat4& matrix);

	void printLog()
	{
		std::unique_lock<std::mutex> lock(logMutex);
		while (!multiThreadLog.empty())
		{
			Log(Log.Information, multiThreadLog.front().c_str());
			multiThreadLog.pop_front();
		}
	}

	struct DemoDetails
	{
		// Time tracking
		float32 logicTime; //!< Total time that has elapsed for the application (Conceptual: Clock at start - Clock time now - Paused time)
		float32 gameTime; //!< Time that has elapsed for the application (Conceptual: Integration of logicTime * the demo's speed factor at each point)
		bool isManual;
		uint32_t currentMode;
		uint32_t previousMode;
		float modeSwitchTime;
		DemoDetails() : logicTime(0), gameTime(0), isManual(false), currentMode(0), previousMode(0), modeSwitchTime(0.f) {}
	} animDetails;

	int state = 0;
};