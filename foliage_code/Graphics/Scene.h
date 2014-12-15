// defines a simple scene containing instanced foliage meshes with a simple, unified material model

#include "DxManager.h"
#include <DirectXCollision.h>
#include "..\CoreLib\List.h"
#include "..\CoreLib\LibString.h"

using namespace DirectX;

// memory layouts for non-instanced meshes (e.g. trunk, branches)
struct MeshVertex
{
	XMFLOAT3 position;
	XMFLOAT3 normal;
	XMFLOAT2 texcoord;
};

const D3D11_INPUT_ELEMENT_DESC vertexLayout[] =
{
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 }
};

// memory layouts for instanced meshes (leaves)

// per-instance data
struct MeshInstance
{
	XMFLOAT3 translation;
	XMFLOAT3X3 rotation;
};

const D3D11_INPUT_ELEMENT_DESC instanceLayout[] =
{
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },

	{ "TRANSLATION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
	{ "ROTATION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 12, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
	{ "ROTATION", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 24, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
	{ "ROTATION", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 36, D3D11_INPUT_PER_INSTANCE_DATA, 1 }
};

// phong material data
struct Material
{
	float Ka, Kd, Ks, Ns;
	UINT textureID;
};

// base mesh data
struct Mesh
{
	UINT32 vertexCount;
	UINT32 indexCount;
	const MeshVertex *vertices;
	const UINT32 *indices;
	ID3D11Buffer *vertexBuffer;
	ID3D11Buffer *indexBuffer;
	Material material;
};

// additional data for alpha-mapped, instanced meshes
struct InstancedMesh : Mesh
{
	UINT32 instanceCount;
	float d0; // base distance for simplification
	float h; // 0 <= h <= 1/2, lod exponent (store log_h(1/2))
	const MeshInstance *instances;
	ID3D11Buffer *instanceBuffer;

	// updated per-frame
	float lambda;
};

struct Texture
{
	ID3D11Resource *texture;
	ID3D11ShaderResourceView *view;
};

// a simple description of a foliage model with instanced leaves
struct Model
{
	// per-frame data
	bool visible;
	float d;

	// persistent data
	UINT modelID;
	XMFLOAT3 position;
	BoundingOrientedBox obb;
	XMFLOAT4X4 transform;
	CoreLib::Basic::List<Mesh> meshes;
	CoreLib::Basic::List<InstancedMesh> instancedMeshes;
	CoreLib::Basic::List<Texture> textures;
	CoreLib::Basic::List<CoreLib::Basic::String> texfiles;

	bool LoadFromFile( const char *filename );
	bool operator<(const Model& rhs) const
	{
		return modelID < rhs.modelID;
	}
};

// the overall scene is a simple list of foliage models
// future optimizations may be explored at scene level, but for now we are interested in one model at a time
class Scene
{
	friend class Renderer;
public:
	Scene();
	~Scene();
	bool LoadFromFile( const char *filename );
	bool initializeD3D( const DxManager & dxManager );
	void releaseD3D();
	UINT computePVS( const BoundingFrustum& frustum );
	UINT computeLODs( XMVECTOR eyepos, XMVECTOR eyedir, float z_far );
	UINT Render( DxManager & dxManager, XMVECTOR eyepos );
protected:
	CoreLib::Basic::List<Model> models;
	UINT linear_falloff_count;
private:
	CoreLib::Basic::List<CoreLib::Basic::String> modelnames;
	CoreLib::Basic::List<MeshVertex *> vertices;
	CoreLib::Basic::List<ID3D11Buffer *> vertexBuffers;
	CoreLib::Basic::List<UINT32 *> indices;
	CoreLib::Basic::List<ID3D11Buffer *> indexBuffers;
	CoreLib::Basic::List<MeshInstance *> instances;
	CoreLib::Basic::List<ID3D11Buffer *> instanceBuffers;

	struct StableBuffer
	{
		XMFLOAT4X4 viewproj;
		XMFLOAT4 eyepos;
		XMFLOAT4 lightDir;
		XMFLOAT4 lightCol;
		XMFLOAT4 ambient;
	} stable_struct;

	struct PerMdlBuffer
	{
		XMFLOAT4X4 world;
	};

	struct PerMeshBuffer
	{
		XMFLOAT4 material;
		float scale;
		UINT	 scale_cutoff_index;
		UINT	 leafcount;
		UINT	 padding;
	};

	// buffer input layouts
	ID3D11InputLayout *vertexInputLayout;
	ID3D11InputLayout *instanceInputLayout;

	// constant buffers
	ID3D11Buffer* stableBuffer; // changes once per frame
	ID3D11Buffer* perMdlBuffer; // changes once per model
	ID3D11Buffer* perMshBuffer; // changes once per mesh

	// shaders for non-leaf meshes
	ID3DBlob *basicVertexShaderBlob;
	ID3D11VertexShader *basicVertexShader;
	ID3DBlob *basicPixelShaderBlob;
	ID3D11PixelShader *basicPixelShader;

	// shaders for leaf meshes
	ID3DBlob *leafVertexShaderBlob;
	ID3D11VertexShader *leafVertexShader;
	ID3DBlob *leafPixelShaderBlob;
	ID3D11PixelShader *leafPixelShader;

	// texture sampler
	ID3D11SamplerState *basicSampler;
};