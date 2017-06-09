#include "Common.h"
//#include "PVRCore\AxisAlignedBox.h"


#define MAP_TOTAL_EFFECTS 1

enum CONSTANTS
{
	MAX_NUMBER_OF_SWAP_IMAGES = 4,
	MAX_NUMBER_OF_THREADS = 16,
	TILE_SIZE_X = 152,
	TILE_GAP_X = 0,
	TILE_SIZE_Y = 140,
	TILE_SIZE_Z = 152,
	TILE_GAP_Z = 0,
	NUM_TILES_X = 8,
	NUM_TILES_Z = 20,
	NUM_OBJECTS_PER_TILE = 122,
	NUM_UNIQUE_OBJECTS_PER_TILE = 4,
	TOTAL_NUMBER_OF_OBJECTS = NUM_TILES_X * NUM_TILES_Z * NUM_OBJECTS_PER_TILE,
};

struct Mesh
{
	pvr::assets::MeshHandle mesh;
	GLuint vbo;
	GLuint indexVbo;
	GLuint ibo;
};
typedef std::vector<Mesh> MeshLod;

struct Meshes
{
	MeshLod map_part[NUM_OBJECTS_PER_TILE];
	void clearAll()
	{
		for (int i = 0; i < NUM_OBJECTS_PER_TILE; i++)
			clearMesh(map_part[i], true);
	}

	void clearObjects()
	{
		for (int i = 0; i < NUM_OBJECTS_PER_TILE; i++)
			clearMesh(map_part[i], true);
	}
	void createObjects()
	{
		for (int i = 0; i < NUM_OBJECTS_PER_TILE; i++)
			createMesh(map_part[i]);
	}
private:
	static void clearMesh(MeshLod& mesh, bool deleteAll)
	{
		for (MeshLod::iterator it = mesh.begin(); it != mesh.end(); ++it)
		{
			if (deleteAll) { it->mesh.reset(); }
		}
	}
	static void createMesh(MeshLod& mesh)
	{
		for (MeshLod::iterator it = mesh.begin(); it != mesh.end(); ++it)
		{
			//utils::createSingleBuffersFromMesh(ctx, *it->mesh, it->vbo, it->ibo);
			const pvr::assets::Mesh& mesh = *it->mesh;
			gl::GenBuffers(1, &it->vbo);
			pvr::uint32 size = (uint32)mesh.getDataSize(0);
			gl::BindBuffer(GL_ARRAY_BUFFER, it->vbo);
			gl::BufferData(GL_ARRAY_BUFFER, size, mesh.getData(0), GL_STATIC_DRAW);

			it->indexVbo = 0;
			if (mesh.getFaces().getData())
			{
				gl::GenBuffers(1, &it->indexVbo);
				size = mesh.getFaces().getDataSize();
				gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, it->indexVbo);
				gl::BufferData(GL_ELEMENT_ARRAY_BUFFER, size, mesh.getFaces().getData(), GL_STATIC_DRAW);
			}
		}
	}
};

class OGLESMap
{
	pvr::Shell *shell;
	pvr::assets::ModelHandle scene;

	void LoadMesh(const StringHash & filename, uint32_t num_lods);

	MeshLod loadLodMesh(const StringHash& filename, const StringHash& mesh, uint32_t max_lods);

	bool loadShaders();

	Meshes meshes;

	glm::mat4 projMtx, viewMtx;

	struct
	{
		pvr::uint32 vertShader;
		pvr::uint32 fragShader;
		GLuint handle;
		pvr::uint32 uiMVPMatrixLoc;
		pvr::uint32 uiLightDirLoc;
		pvr::uint32 uiWorldViewITLoc;
	} ShaderProgram;
	
	pvr::GraphicsContext context;

public:
	float frame;
	OGLESMap(pvr::Shell *_shell):shell(_shell){}
	virtual pvr::Result initApplication();
	virtual pvr::Result initView();
	virtual pvr::Result releaseView();
	virtual pvr::Result quitApplication();
	virtual pvr::Result renderFrame();

	int state = 0;
};
