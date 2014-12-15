#include "Scene.h"
#include "..\CoreLib\Basic.h"
#include "..\CoreLib\LibString.h"
#include "..\CoreLib\LibIO.h"
#include "..\DirectXTK\Inc\WICTextureLoader.h"
#include <d3dcompiler.h>

#define checkResult(succ) if (!(succ)){printf("Format error reading from file.\n"); return false;}

Scene::Scene()
{
	stable_struct.lightDir = XMFLOAT4( 0.f, -1.f, 0.f, 0.f );
	stable_struct.lightCol = XMFLOAT4( 1.f, 1.f, 1.f, 1.f );
	stable_struct.ambient = XMFLOAT4( 1.f, 1.f, 1.f, 1.f );

	linear_falloff_count = 50;
}

Scene::~Scene()
{
}

// loads an individual model from text (.fmt) file
bool Model::LoadFromFile( const char *filename )
{
	CoreLib::Basic::String name( filename );
	CoreLib::Basic::String directory = CoreLib::IO::Path::GetDirectoryName( name );
	if ( name.EndsWith( CoreLib::Basic::String( ".fmt" ) ) )
	{
		FILE* f = 0;
		if ( fopen_s( &f, filename, "rt" ) != 0 )
		{
			printf( "Error: could not open file: %s\n", filename );
			return false;
		}
		const int bufferSize = 4096;
		char buf[bufferSize];

		while ( !feof( f ) )
		{
			// get the oriented bounding box
			checkResult( fscanf_s( f, "%s", buf, bufferSize ) );
			if ( _stricmp( buf, "*BOUNDS" ) == 0 )
			{
				checkResult( fscanf_s( f, " %f %f %f %f %f %f", &obb.Center.x, &obb.Center.y, &obb.Center.z,
																&obb.Extents.x, &obb.Extents.y, &obb.Extents.z ) );
				while ( !feof( f ) && fgetc( f ) != '\n' );
			}
			// look for a regular mesh declaration
			else if ( _stricmp( buf, "*MESH" ) == 0 )
			{
				Mesh mesh;

				// get the mesh name for debugging
				checkResult( fscanf_s( f, " %s", buf, bufferSize ) );
				CoreLib::Basic::String meshName( buf );
				while ( !feof( f ) && fgetc( f ) != '\n' );

				checkResult( fscanf_s( f, "%s", buf, bufferSize ) );
				if ( _stricmp( buf, "*MATERIAL" ) == 0 )
				{
					checkResult( fscanf_s( f, " %f %f %f %f", &mesh.material.Ka, &mesh.material.Kd, &mesh.material.Ks, &mesh.material.Ns ) );
					while ( !feof( f ) && fgetc( f ) != '\n' );
				}
				else
				{
					printf( "Error: mesh %s has no material\n", meshName.Buffer() );
					return false;
				}

				// load the texture for the mesh
				checkResult( fscanf_s( f, "%s", buf, bufferSize ) );
				if ( _stricmp( buf, "*TEXTURE" ) == 0 )
				{
					checkResult( fscanf_s( f, " %s", buf, bufferSize ) );

					CoreLib::Basic::String texfile = CoreLib::IO::Path::Combine( directory, CoreLib::Basic::String( buf ) );
					int index = texfiles.IndexOf( texfile );
					if ( index >= 0 )
					{
						mesh.material.textureID = index;
					}
					else
					{
						mesh.material.textureID = texfiles.Count( );
						texfiles.Add( texfile );
					}
						
					while ( !feof( f ) && fgetc( f ) != '\n' );
				}
				else
				{
					printf( "Error: mesh \"%s\" has no texture!\n", meshName.Buffer() );
					return false;
				}


				checkResult( fscanf_s( f, "%s", buf, bufferSize ) );

				if ( _stricmp( buf, "*VERTICES" ) == 0 )
				{
					// get the number of vertices
					checkResult( fscanf_s( f, " %d", &mesh.vertexCount ) );
					MeshVertex * vertices = new MeshVertex[mesh.vertexCount];
					while ( !feof( f ) && fgetc( f ) != '\n' );
					if ( feof( f ) )
					{
						printf( "Error: mesh \"%s\" incomplete (missing vertex list)\n", meshName.Buffer() );
						delete(vertices);
						return false;
					}

					// read in the vertex data
					for ( MeshVertex * vertex = vertices; vertex < vertices + mesh.vertexCount; vertex++ )
					{
						if ( !fscanf_s( f, "%f %f %f %f %f %f %f %f", 
							&vertex->position.x,
							&vertex->position.y,
							&vertex->position.z,
							&vertex->normal.x,
							&vertex->normal.y,
							&vertex->normal.z,
							&vertex->texcoord.x,
							&vertex->texcoord.y ) )
						{
							printf( "Error: invalid vertex in mesh \"%s\"\n", meshName.Buffer() );
							delete(vertices);
							return false;
						}
						while ( !feof( f ) && fgetc( f ) != '\n' );
					}
					mesh.vertices = vertices;
					
					checkResult( fscanf_s( f, "%s", buf, bufferSize ) );
					if ( _stricmp( buf, "*INDICES" ) == 0 )
					{
						// get the number of indices
						checkResult( fscanf_s( f, " %d", &mesh.indexCount ) );
						UINT32 * indices = new UINT32[mesh.indexCount];
						while ( !feof( f ) && fgetc( f ) != '\n' );

						for ( UINT32 * index = indices; index < indices + mesh.indexCount; index++ )
						{
							if ( !fscanf_s( f, "%d", index ) || *index >= mesh.vertexCount )
							{
								printf( "Error: invalid index in mesh \"%s\"\n", meshName.Buffer() );
								return false;
							}
							while ( !feof( f ) )
							{
								char c = fgetc( f );
								if ( (c != '\n') && (c != ' ') && (c != '\t') )
								{
									ungetc( c, f );
									break;
								}
							}
						}
						mesh.indices = indices;
					}
					else
					{
						printf( "Error: mesh \"%s\" missing index list.\n", meshName.Buffer() );
						return false;
					}

					// finished parsing the mesh, add it to model's list
					meshes.Add( mesh );
				}
				else
				{
					printf( "Error: mesh \"%s\" missing vertex list.\n", meshName.Buffer() );
				}
			}
			else if ( _stricmp( buf, "*LEAFMESH" ) == 0 )
			{
				checkResult( fscanf_s( f, " %s", buf, bufferSize ) );
				CoreLib::Basic::String meshName( buf );
				while ( !feof( f ) && fgetc( f ) != '\n' );
				
				InstancedMesh mesh;

				checkResult( fscanf_s( f, "%s", buf, bufferSize ) );
				if ( _stricmp( buf, "*MATERIAL" ) == 0 )
				{
					checkResult( fscanf_s( f, " %f %f %f %f", &mesh.material.Ka, &mesh.material.Kd, &mesh.material.Ks, &mesh.material.Ns ) );
					while ( !feof( f ) && fgetc( f ) != '\n' );
				}
				else
				{
					printf( "Error: mesh %s has no material\n", meshName.Buffer( ) );
					return false;
				}

				// load the texture for the mesh
				checkResult( fscanf_s( f, "%s", buf, bufferSize ) );
				if ( _stricmp( buf, "*TEXTURE" ) == 0 )
				{
					// TODO: filter duplicate textures
					checkResult( fscanf_s( f, " %s", buf, bufferSize ) );
					CoreLib::Basic::String texfile = CoreLib::IO::Path::Combine( directory, CoreLib::Basic::String( buf ) );
					int index = texfiles.IndexOf( texfile );
					if ( index >= 0 )
					{
						mesh.material.textureID = index;
					}
					else
					{
						mesh.material.textureID = texfiles.Count( );
						texfiles.Add( texfile );
					}
					while ( !feof( f ) && fgetc( f ) != '\n' );
				}
				else
				{
					printf( "Error: mesh \"%s\" has no texture!\n", meshName.Buffer( ) );
					return false;
				}

				checkResult( fscanf_s( f, "%s", buf, bufferSize ) );
				if ( _stricmp( buf, "*LOD" ) == 0 )
				{
					checkResult( fscanf_s( f, " %f %f", &mesh.d0, &mesh.h ) );
					mesh.h = -1.f / log2f( mesh.h ); // log_h(1/2) = log_2(1/2) / log_2(h) = -1 / log_2(h)
					while ( !feof( f ) && fgetc( f ) != '\n' );
				}
				else
				{
					printf( "Error: leaf mesh %s missing lod values.\n", meshName.Buffer( ) );
					return false;
				}

				checkResult( fscanf_s( f, "%s", buf, bufferSize ) );

				if ( _stricmp( buf, "*VERTICES" ) == 0 )
				{
					// get the number of vertices
					checkResult( fscanf_s( f, " %d", &mesh.vertexCount ) );
					MeshVertex * vertices = new MeshVertex[mesh.vertexCount];
					while ( !feof( f ) && fgetc( f ) != '\n' );
					if ( feof( f ) )
					{
						printf( "Error: mesh \"%s\" incomplete (missing vertex list)\n", meshName.Buffer() );
						delete(vertices);
						return false;
					}

					// read in the vertex data
					for ( MeshVertex * vertex = vertices; vertex < vertices + mesh.vertexCount; vertex++ )
					{
						if ( !fscanf_s( f, "%f %f %f %f %f %f %f %f",
							&vertex->position.x,
							&vertex->position.y,
							&vertex->position.z,
							&vertex->normal.x,
							&vertex->normal.y,
							&vertex->normal.z,
							&vertex->texcoord.x,
							&vertex->texcoord.y ) )
						{
							printf( "Error: invalid vertex in mesh \"%s\"\n", meshName.Buffer() );
							delete(vertices);
							return false;
						}
						while ( !feof( f ) && fgetc( f ) != '\n' );
					}
					mesh.vertices = vertices;


					checkResult( fscanf_s( f, "%s", buf, bufferSize ) );
					if ( _stricmp( buf, "*INDICES" ) == 0 )
					{
						// get the number of indices
						checkResult( fscanf_s( f, " %d", &mesh.indexCount ) );
						UINT32 * indices = new UINT32[mesh.indexCount];
						while ( !feof( f ) && fgetc( f ) != '\n' );

						for ( UINT32 * index = indices; index < indices + mesh.indexCount; index++ )
						{
							if ( !fscanf_s( f, "%d", index ) || *index >= mesh.vertexCount )
							{
								printf( "Error: invalid index in mesh \"%s\"\n", meshName.Buffer() );
								return false;
							}
							while ( !feof( f ) )
							{
								char c = fgetc( f );
								if ( (c != '\n') && (c != ' ') && (c != '\t') )
								{
									ungetc( c, f );
									break;
								}
							}
						}
						mesh.indices = indices;
					}
					else
					{
						printf( "Error: mesh \"%s\" missing index list.\n", meshName.Buffer() );
						return false;
					}

					checkResult( fscanf_s( f, "%s", buf, bufferSize ) );
					if ( _stricmp( buf, "*INSTANCES" ) == 0 )
					{
						checkResult( fscanf_s( f, " %d", &mesh.instanceCount ) );
						while ( !feof( f ) && fgetc( f ) != '\n' );
						MeshInstance* instances = new MeshInstance[mesh.instanceCount];
						for ( MeshInstance* instance = instances; instance < instances + mesh.instanceCount; instance++ )
						{
							float x, y, z, roll, pitch, yaw;
							checkResult( fscanf_s( f, "%f %f %f %f %f %f", &x, &y, &z, &pitch, &yaw, &roll ) );
							XMStoreFloat3x3( &instance->rotation, XMMatrixTranspose( XMMatrixRotationRollPitchYaw( pitch, yaw, roll ) ) );
							instance->translation = XMFLOAT3( x, y, z );
						}
						mesh.instances = instances;
					}
					else
					{
						printf( "Error: leaf mesh %s missing instance list\n", meshName.Buffer() );
						return false;
					}

					instancedMeshes.Add( mesh );
				}
			}
			else
			{
				while ( !feof( f ) && fgetc( f ) != '\n' );

			}
		}

		return true;
	}
	else
	{
		printf( "Error: unsupported file type. Please provide models in .fmt format\n" );
		return false;
	}
}

// loads the scene models and materials from a text (.fst) file
bool Scene::LoadFromFile( const char *filename )
{
	CoreLib::Basic::String name( filename );
	CoreLib::Basic::String path = CoreLib::IO::Path::GetDirectoryName( name );
	if ( name.EndsWith( CoreLib::Basic::String( ".fst" ) ) )
	{
		FILE* f = 0;
		fopen_s( &f, filename, "rt" );
		if ( f == 0 )
		{
			printf( "Error: could not open file: %s\n", filename );
			return false;
		}
		const int bufferSize = 4096;
		char buf[bufferSize];

		while ( !feof( f ) )
		{
			
			checkResult( fscanf_s( f, "%s", buf, bufferSize ) );
			// read in a model and its world placement
			if ( _stricmp( buf, "*MODEL" ) == 0 )
			{
				// TODO: maintain a map of names, so we can re-use models
				checkResult( fscanf_s( f, " %s", buf, bufferSize ) );
				CoreLib::Basic::String modelfile = CoreLib::Basic::String( buf );
				models.GrowToSize( models.Count() + 1 );

				Model& mdl = models.Last();

				int index = modelnames.IndexOf( modelfile );
				if ( index >= 0 )
				{
					mdl = models[index]; // shallow copy
				}
				else
				{
					mdl.modelID = modelnames.Count();
					modelnames.Add( modelfile );
					if ( !mdl.LoadFromFile( CoreLib::IO::Path::Combine( path, modelfile ).ToMultiByteString() ) )
						 return false;
				}

				// get the model transform
				float roll, pitch, yaw;
				while ( !feof( f ) && fgetc( f ) != '\n' );
				checkResult( fscanf_s( f, "%s", buf, bufferSize ) );
				if ( _stricmp( buf, "*POSITION" ) == 0 )
				{
					checkResult( fscanf_s( f, " %f %f %f", &mdl.position.x, &mdl.position.y, &mdl.position.z ) );
					while ( !feof( f ) && fgetc( f ) != '\n' );
				}
				checkResult( fscanf_s( f, "%s", buf, bufferSize ) );
				if ( _stricmp( buf, "*ORIENTATION" ) == 0 )
				{
					checkResult( fscanf_s( f, " %f %f %f", &pitch, &yaw, &roll ) );
					while ( !feof( f ) && fgetc( f ) != '\n' );
				}
				XMVECTOR q = XMQuaternionRotationRollPitchYaw( -pitch * XM_PI / 180, -yaw * XM_PI / 180, -roll * XM_PI / 180 );
				XMStoreFloat4( &mdl.obb.Orientation, q );
				XMStoreFloat4x4( &mdl.transform, XMMatrixMultiplyTranspose( XMMatrixRotationQuaternion( q ), XMMatrixTranslation( mdl.position.x, mdl.position.y, mdl.position.z ) ) );
			}
			// read in custom directional+ambient light for the scene
			else if ( _stricmp( buf, "*SUNLIGHT" ) == 0 )
			{
				while ( !feof( f ) && fgetc( f ) != '\n' );

				if ( _stricmp( buf, "*DIRECTION" ) == 0 )
				{
					checkResult( fscanf_s( f, " %f %f %f", stable_struct.lightDir.x, stable_struct.lightDir.y, stable_struct.lightDir.z ) );
					while ( !feof( f ) && fgetc( f ) != '\n' );
				}

				if ( _stricmp( buf, "*COLOR" ) == 0 )
				{
					checkResult( fscanf_s( f, " %f %f %f", stable_struct.lightCol.x, stable_struct.lightCol.y, stable_struct.lightCol.z ) );
					while ( !feof( f ) && fgetc( f ) != '\n' );
				}

				if ( _stricmp( buf, "*AMBIENT" ) == 0 )
				{
					checkResult( fscanf_s( f, " %f %f %f", stable_struct.ambient.x, stable_struct.ambient.y, stable_struct.ambient.z ) );
					while ( !feof( f ) && fgetc( f ) != '\n' );
				}
			}
			// custom model LOD cutoff
			else if ( _stricmp( buf, "*LODCOUNT" ) == 0 )
			{
				checkResult( fscanf_s( f, " %u", &linear_falloff_count ) );
				while ( !feof( f ) && fgetc( f ) != '\n' );
			}
			else
			{
				while ( !feof( f ) && fgetc( f ) != '\n' );
			}
		}

		// fix up obb positions, and sort models by resource data
		models.Sort();
		for ( Model * m = models.begin(); m != models.end(); m++ )
		{
			m->obb.Center.x += m->position.x;
			m->obb.Center.y += m->position.y;
			m->obb.Center.z += m->position.z;
		}
		return true;
	}
	else
	{
		printf( "Error: unsupported file type. Please provide sceness in .fst format\n" );
		return false;
	}
}

// this method should be called after LoadFromFile
bool Scene::initializeD3D( const DxManager & dxManager )
{
	// compile the shaders and create input layouts
	if ( FAILED( D3DCompileFromFile( L"..\\Graphics\\BasicVertexShader.hlsl", 
									 NULL, D3D_COMPILE_STANDARD_FILE_INCLUDE, 
									 "main",
									 "vs_5_0", 
									 D3DCOMPILE_OPTIMIZATION_LEVEL2, 
									 0, 
									 &basicVertexShaderBlob, 
									 NULL ) ) )
		return false;
	else if ( FAILED( dxManager.pD3DDevice->CreateVertexShader( basicVertexShaderBlob->GetBufferPointer(),
																basicVertexShaderBlob->GetBufferSize(),
																NULL, 
																&basicVertexShader ) ) )
		return false;
	else if ( FAILED( dxManager.pD3DDevice->CreateInputLayout(  vertexLayout,
																ARRAYSIZE(vertexLayout),
																basicVertexShaderBlob->GetBufferPointer(),
																basicVertexShaderBlob->GetBufferSize(),
																&vertexInputLayout ) ) )
		return false;

	if ( FAILED( D3DCompileFromFile( L"..\\Graphics\\BasicPixelShader.hlsl", 
									 NULL, 
									 D3D_COMPILE_STANDARD_FILE_INCLUDE, 
									 "main",
									 "ps_5_0", 
									 D3DCOMPILE_OPTIMIZATION_LEVEL2, 
									 0, 
									 &basicPixelShaderBlob, 
									 NULL ) ) )
		return false;
	else if ( FAILED( dxManager.pD3DDevice->CreatePixelShader( basicPixelShaderBlob->GetBufferPointer(),
															   basicPixelShaderBlob->GetBufferSize(),
															   NULL, 
															   &basicPixelShader ) ) )
		return false;

	

	if ( FAILED( D3DCompileFromFile( L"..\\Graphics\\LeafVertexShader.hlsl",
									NULL,
									D3D_COMPILE_STANDARD_FILE_INCLUDE,
									"main",
									"vs_5_0",
									D3DCOMPILE_OPTIMIZATION_LEVEL2,
									0,
									&leafVertexShaderBlob,
									NULL ) ) )
		return false;
	else if ( FAILED( dxManager.pD3DDevice->CreateVertexShader( leafVertexShaderBlob->GetBufferPointer(),
																leafVertexShaderBlob->GetBufferSize(),
																NULL,
																&leafVertexShader ) ) )
		return false;
	else if ( FAILED( dxManager.pD3DDevice->CreateInputLayout( instanceLayout,
															   ARRAYSIZE( instanceLayout ),
															   leafVertexShaderBlob->GetBufferPointer( ),
															   leafVertexShaderBlob->GetBufferSize( ),
															   &instanceInputLayout ) ) )
		return false;

	if ( FAILED( D3DCompileFromFile( L"..\\Graphics\\LeafPixelShader.hlsl",
									 NULL,
									 D3D_COMPILE_STANDARD_FILE_INCLUDE,
									 "main",
									 "ps_5_0",
									 D3DCOMPILE_OPTIMIZATION_LEVEL2,
									 0,
									 &leafPixelShaderBlob,
									 NULL ) ) )
		return false;
	else if ( FAILED( dxManager.pD3DDevice->CreatePixelShader( leafPixelShaderBlob->GetBufferPointer(),
															   leafPixelShaderBlob->GetBufferSize(),
															   NULL,
															   &leafPixelShader ) ) )
		return false;

	// create the constant, vertex, and index buffers for the models

	D3D11_BUFFER_DESC constantBufferDesc;
	ZeroMemory( &constantBufferDesc, sizeof(D3D11_BUFFER_DESC) );
	constantBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	constantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	constantBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	constantBufferDesc.MiscFlags = 0;
	constantBufferDesc.ByteWidth = sizeof(StableBuffer);

	if ( FAILED( dxManager.pD3DDevice->CreateBuffer( &constantBufferDesc, NULL, &stableBuffer ) ) ) return false;

	constantBufferDesc.ByteWidth = sizeof(PerMdlBuffer);

	if ( FAILED( dxManager.pD3DDevice->CreateBuffer( &constantBufferDesc, NULL, &perMdlBuffer ) ) ) return false;

	constantBufferDesc.ByteWidth = sizeof(PerMeshBuffer);

	if ( FAILED( dxManager.pD3DDevice->CreateBuffer( &constantBufferDesc, NULL, &perMshBuffer ) ) ) return false;
	
	D3D11_BUFFER_DESC vertexBufferDesc;
	ZeroMemory( &vertexBufferDesc, sizeof(D3D11_BUFFER_DESC) );
	vertexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
	vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vertexBufferDesc.CPUAccessFlags = 0;
	vertexBufferDesc.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA vertexBufferData;
	ZeroMemory( &vertexBufferData, sizeof(D3D11_SUBRESOURCE_DATA) );
	vertexBufferData.SysMemPitch = 0;
	vertexBufferData.SysMemSlicePitch = 0;

	D3D11_BUFFER_DESC indexBufferDesc;
	ZeroMemory( &indexBufferDesc, sizeof(D3D11_BUFFER_DESC) );
	indexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
	indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	indexBufferDesc.CPUAccessFlags = 0;
	indexBufferDesc.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA indexBufferData;
	ZeroMemory( &indexBufferData, sizeof(D3D11_SUBRESOURCE_DATA) );
	indexBufferData.SysMemPitch = 0;
	indexBufferData.SysMemSlicePitch = 0;

	for ( Model * model = models.begin(); model != models.end(); model++ )
	{
		// initialize the first model
		for ( Mesh * mesh = model->meshes.begin(); mesh != model->meshes.end(); mesh++ )
		{
			vertexBufferData.pSysMem = mesh->vertices;
			vertexBufferDesc.ByteWidth = mesh->vertexCount * sizeof(MeshVertex);

			if ( FAILED( dxManager.pD3DDevice->CreateBuffer( &vertexBufferDesc, &vertexBufferData, &mesh->vertexBuffer ) ) )
				return false;

			indexBufferData.pSysMem = mesh->indices;
			indexBufferDesc.ByteWidth = mesh->indexCount * sizeof(UINT32);

			if ( FAILED( dxManager.pD3DDevice->CreateBuffer( &indexBufferDesc, &indexBufferData, &mesh->indexBuffer ) ) )
				return false;
		}

		for ( InstancedMesh * mesh = model->instancedMeshes.begin(); mesh != model->instancedMeshes.end(); mesh++ )
		{
			vertexBufferData.pSysMem = mesh->vertices;
			vertexBufferDesc.ByteWidth = mesh->vertexCount * sizeof(MeshVertex);

			if ( FAILED( dxManager.pD3DDevice->CreateBuffer( &vertexBufferDesc, &vertexBufferData, &mesh->vertexBuffer ) ) )
				return false;

			vertexBufferData.pSysMem = mesh->instances;
			vertexBufferDesc.ByteWidth = mesh->instanceCount * sizeof(MeshInstance);

			if ( FAILED( dxManager.pD3DDevice->CreateBuffer( &vertexBufferDesc, &vertexBufferData, &mesh->instanceBuffer ) ) )
				return false;

			indexBufferData.pSysMem = mesh->indices;
			indexBufferDesc.ByteWidth = mesh->indexCount * sizeof(UINT32);

			if ( FAILED( dxManager.pD3DDevice->CreateBuffer( &indexBufferDesc, &indexBufferData, &mesh->indexBuffer ) ) )
				return false;
		}

		for ( CoreLib::Basic::String * texfile = model->texfiles.begin( ); texfile != model->texfiles.end( ); texfile++ )
		{
			Texture texture;

			HRESULT hr = CreateWICTextureFromFileEx( dxManager.pD3DDevice,
													 dxManager.pD3DDeviceContext,
													 texfile->Buffer( ),
													 0,
													 D3D11_USAGE_DEFAULT,
													 D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET,
													 0,
													 D3D11_RESOURCE_MISC_GENERATE_MIPS,
													 false,
													 &texture.texture,
													 &texture.view );
			if ( FAILED( hr ) )
			{
				printf( "Error: failed to load texture %s\n", texfile->Buffer( ) );
				return false;
			}

			model->textures.Add( texture );
		}

		// copy the pointers for identical models
		Model * first = model;
		for ( model++; model != models.end() && model->modelID == first->modelID; model++ )
		{
			for ( int i = 0; i < model->meshes.Count(); i++ )
			{
				model->meshes[i].vertexBuffer = first->meshes[i].vertexBuffer;
				model->meshes[i].indexBuffer = first->meshes[i].indexBuffer;
			}

			for ( int i = 0; i < model->instancedMeshes.Count(); i++ )
			{
				model->instancedMeshes[i].vertexBuffer = first->instancedMeshes[i].vertexBuffer;
				model->instancedMeshes[i].indexBuffer = first->instancedMeshes[i].indexBuffer;
				model->instancedMeshes[i].instanceBuffer = first->instancedMeshes[i].instanceBuffer;
			}

			model->textures = first->textures;
		}
		model--;
	}

	// sampler definitions
	D3D11_SAMPLER_DESC basicSamplerDesc;
	ZeroMemory( &basicSamplerDesc, sizeof(D3D11_SAMPLER_DESC) );
	basicSamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	basicSamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	basicSamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	basicSamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	basicSamplerDesc.MipLODBias = 0.0f;
	basicSamplerDesc.MaxAnisotropy = 16;
	basicSamplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	basicSamplerDesc.MinLOD = 0.0f;
	basicSamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

	if ( FAILED( dxManager.pD3DDevice->CreateSamplerState( &basicSamplerDesc, &basicSampler ) ) ) return false;

	return true;
}

void Scene::releaseD3D()
{
	for ( Model * mdl = models.begin(); mdl != models.end(); mdl++ )
	{
		for ( Mesh * m = mdl->meshes.begin(); m != mdl->meshes.end(); m++ )
		{
			if ( m->vertexBuffer ) m->vertexBuffer->Release();
			if ( m->indexBuffer ) m->indexBuffer->Release();
			if ( m->vertices ) delete( m->vertices );
			if ( m->indices ) delete( m->indices );
		}

		for ( InstancedMesh * m = mdl->instancedMeshes.begin(); m != mdl->instancedMeshes.end(); m++ )
		{
			if ( m->vertexBuffer ) m->vertexBuffer->Release( );
			if ( m->indexBuffer ) m->indexBuffer->Release( );
			if ( m->instanceBuffer ) m->instanceBuffer->Release();
			if ( m->instances ) delete(m->instances);
			if ( m->vertices ) delete(m->vertices);
			if ( m->indices ) delete(m->indices);
		}

		for ( Texture * texture = mdl->textures.begin( ); texture != mdl->textures.end( ); texture++ )
		{
			texture->texture->Release();
			texture->view->Release();
		}

		while ( mdl + 1 < models.end() && (mdl + 1)->modelID == mdl->modelID )
			mdl++;
	}

	if ( stableBuffer) stableBuffer->Release();
	if ( perMdlBuffer ) perMdlBuffer->Release();
	if ( perMshBuffer ) perMshBuffer->Release();
	if ( vertexInputLayout ) vertexInputLayout->Release();
	if ( instanceInputLayout ) instanceInputLayout->Release();
	if ( basicVertexShaderBlob ) basicVertexShaderBlob->Release();
	if ( basicVertexShader ) basicVertexShader->Release();
	if ( basicPixelShaderBlob ) basicPixelShaderBlob->Release();
	if ( basicPixelShader ) basicPixelShader->Release();
	if ( leafVertexShaderBlob ) leafVertexShaderBlob->Release();
	if ( leafVertexShader ) leafVertexShader->Release();
	if ( leafPixelShaderBlob ) leafPixelShaderBlob->Release();
	if ( leafPixelShader ) leafPixelShader->Release();
	if ( basicSampler ) basicSampler->Release();
}

// perform OBB-frustum culling
UINT Scene::computePVS( const BoundingFrustum & frustum )
{
	UINT count = 0;
	for ( Model * model = models.begin(); model != models.end(); model++ )
	{
		model->visible = frustum.Contains( model->obb ) > 0;
		count++;
	}

	return count;
}

// update lambda values for all leaf meshes
UINT Scene::computeLODs( XMVECTOR eyepos, XMVECTOR eyedir, float z_far )
{
	UINT count = 0;
	float d_max = 0.f, d_min = z_far;

	// invidual lambda computation
	for ( Model * model = models.begin(); model != models.end(); model++ )
	{
		if ( !model->visible )
			continue;

		XMVECTOR delta = XMVectorSet( model->position.x, model->position.y, model->position.z, 1.f ) - eyepos;
		float d = sqrtf( XMVector3Dot( delta, delta ).m128_f32[0] );
		model->d = d < z_far ? d : z_far;

		if ( d < d_min )
			d_min = d;
		if ( d > d_max )
			d_max = d;

		for ( InstancedMesh * mesh = model->instancedMeshes.begin(); mesh != model->instancedMeshes.end(); mesh++ )
		{				
			count++;
			mesh->lambda = mesh->d0 > d ? 1.f : powf( mesh->d0 / d, mesh->h );
		}
	}

	// approximate depth-complexity lambda adjustment	

	float d_range = d_max - d_min;
	if ( d_range > 0.f )
	{	
		float n = linear_falloff_count / (float) count;
		float w = d_range / z_far;
		for ( Model * model = models.begin(); model != models.end(); model++ )
		{
			float dc_lambda = 1.f - 0.5f * w * powf( (model->d - d_min) / d_range, n );

			for ( InstancedMesh * mesh = model->instancedMeshes.begin(); mesh != model->instancedMeshes.end(); mesh++ )
			{
				mesh->lambda *= dc_lambda;

				// scaling breaks at extremely aggressive simplification
				// models/scenes should be tuned so that at this point the meshes can be ignored or replaced by billboards
				if ( mesh->lambda < 0.005f )
				{
					mesh->lambda = 0.f;
					model->visible = false; // since i don't have lods for the branches, toggle everything
					count--;
				}	
			}
		}
	}

	return count;
}

// render the scene to the currently bound viewport and render target
UINT Scene::Render( DxManager & dxManager, XMVECTOR eyepos )
{
	// re-apply general render state - gets reset by SpriteBatch
	dxManager.pD3DDeviceContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
	ID3D11Buffer* constantBuffers[3];
	constantBuffers[0] = stableBuffer;
	constantBuffers[1] = perMdlBuffer;
	constantBuffers[2] = perMshBuffer;
	dxManager.pD3DDeviceContext->VSSetConstantBuffers( 0, 3, constantBuffers );
	constantBuffers[1] = perMshBuffer;
	dxManager.pD3DDeviceContext->PSSetConstantBuffers( 0, 2, constantBuffers );
	dxManager.pD3DDeviceContext->PSSetSamplers( 0, 1, &basicSampler );

	// update the stable buffer with camera view and projection
	XMMATRIX viewproj = XMMatrixMultiply( dxManager.viewMatrix, dxManager.projectionMatrix );
	XMStoreFloat4x4( &stable_struct.viewproj, XMMatrixTranspose( viewproj ) );
	XMStoreFloat4( &stable_struct.eyepos, eyepos );
	D3D11_MAPPED_SUBRESOURCE msr;
	ZeroMemory( &msr, sizeof(D3D11_MAPPED_SUBRESOURCE) );
	dxManager.pD3DDeviceContext->Map( stableBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr );
	memcpy( msr.pData, &stable_struct, sizeof(StableBuffer) );
	dxManager.pD3DDeviceContext->Unmap( stableBuffer, 0 );

	// set shaders for basic meshes
	dxManager.pD3DDeviceContext->IASetInputLayout( vertexInputLayout );
	dxManager.pD3DDeviceContext->VSSetShader( basicVertexShader, NULL, 0 );
	dxManager.pD3DDeviceContext->PSSetShader( basicPixelShader, NULL, 0 );

	// render all basic meshes
	for ( Model * mdl = models.begin(); mdl != models.end(); mdl++ )
	{
		if ( !mdl->visible )
			continue;

		// update the per-model buffer
		ZeroMemory( &msr, sizeof(D3D11_MAPPED_SUBRESOURCE) );
		dxManager.pD3DDeviceContext->Map( perMdlBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr );
		memcpy( msr.pData, &mdl->transform, sizeof(XMFLOAT4X4) );
		dxManager.pD3DDeviceContext->Unmap( perMdlBuffer, 0 );

		for ( Mesh * m = mdl->meshes.begin(); m != mdl->meshes.end(); m++ )
		{
			// update the per-mesh buffer
			PerMeshBuffer buf;
			buf.material.x = m->material.Ka;
			buf.material.y = m->material.Kd;
			buf.material.z = m->material.Ks;
			buf.material.w = m->material.Ns;
			ZeroMemory( &msr, sizeof(D3D11_MAPPED_SUBRESOURCE) );
			dxManager.pD3DDeviceContext->Map( perMshBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr );
			memcpy( msr.pData, &buf, sizeof(PerMeshBuffer) );
			dxManager.pD3DDeviceContext->Unmap( perMshBuffer, 0 );

			UINT stride = sizeof(MeshVertex);
			UINT offset = 0;
			dxManager.pD3DDeviceContext->IASetVertexBuffers( 0, 1, &m->vertexBuffer, &stride, &offset );
			dxManager.pD3DDeviceContext->IASetIndexBuffer( m->indexBuffer, DXGI_FORMAT_R32_UINT, 0 );
			dxManager.pD3DDeviceContext->PSSetShaderResources( 0, 1, &mdl->textures[m->material.textureID].view );

			dxManager.pD3DDeviceContext->DrawIndexed( m->indexCount, 0, 0 );
		}
	}

	// set shaders for instanced meshes
	dxManager.pD3DDeviceContext->IASetInputLayout( instanceInputLayout );
	dxManager.pD3DDeviceContext->VSSetShader( leafVertexShader, NULL, 0 );
	dxManager.pD3DDeviceContext->PSSetShader( leafPixelShader, NULL, 0 );

	// set blend state to alpha-to-coverage
	dxManager.setLeafState();

	// render all instanced meshes
	UINT leafcount = 0;
	for ( Model * mdl = models.begin( ); mdl != models.end( ); mdl++ )
	{
		if ( !mdl->visible )
			continue;

		// update the per-model buffer
		ZeroMemory( &msr, sizeof(D3D11_MAPPED_SUBRESOURCE) );
		dxManager.pD3DDeviceContext->Map( perMdlBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr );
		memcpy( msr.pData, &mdl->transform, sizeof(XMFLOAT4X4) );
		dxManager.pD3DDeviceContext->Unmap( perMdlBuffer, 0 );

		for ( InstancedMesh * m = mdl->instancedMeshes.begin( ); m != mdl->instancedMeshes.end( ); m++ )
		{
			if ( m->lambda > 0.f )
			{
				UINT numLeaves = (UINT) (m->lambda * m->instanceCount);

				// update the per-mesh buffers
				PerMeshBuffer buf;
				buf.material.x = m->material.Ka;
				buf.material.y = m->material.Kd;
				buf.material.z = m->material.Ks;
				buf.material.w = m->material.Ns;
				buf.scale = 1.f / m->lambda;
				buf.scale_cutoff_index = (UINT) (0.95f * m->lambda * m->instanceCount);
				buf.leafcount = numLeaves;
				ZeroMemory( &msr, sizeof(D3D11_MAPPED_SUBRESOURCE) );
				dxManager.pD3DDeviceContext->Map( perMshBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr );
				memcpy( msr.pData, &buf, sizeof(PerMeshBuffer) );
				dxManager.pD3DDeviceContext->Unmap( perMshBuffer, 0 );

				UINT strides[2] = { sizeof(MeshVertex), sizeof(MeshInstance) };
				UINT offsets[2] = { 0, 0 };
				ID3D11Buffer *buffers[2];
				buffers[0] = m->vertexBuffer;
				buffers[1] = m->instanceBuffer;
				dxManager.pD3DDeviceContext->IASetVertexBuffers( 0, 2, buffers, strides, offsets );
				dxManager.pD3DDeviceContext->IASetIndexBuffer( m->indexBuffer, DXGI_FORMAT_R32_UINT, 0 );
				dxManager.pD3DDeviceContext->PSSetShaderResources( 0, 1, &mdl->textures[m->material.textureID].view );
				
				dxManager.pD3DDeviceContext->DrawIndexedInstanced( m->indexCount, numLeaves, 0, 0, 0 );
				leafcount += numLeaves;
			}		
		}
	}

	return leafcount;
}