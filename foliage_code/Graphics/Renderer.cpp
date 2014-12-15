#include "Renderer.h"

bool Renderer::initialize( const char *scenefile, HWND * hWnd, int width, int height, float z_far, int msaa, bool fullscreen )
{
	// setup DirectX controls
	if ( !dxManager.initialize( hWnd, width, height, msaa, fullscreen ) ) return false;
	XMMATRIX m;
	camera.GetTransform( m );
	dxManager.setViewMatrix( m );
	XMMATRIX proj = XMMatrixPerspectiveFovLH( .4f * CoreLib::Basic::Math::Pi, (float) width / height, 1.f, z_far );
	dxManager.setProjMatrix( proj );
	BoundingFrustum::CreateFromMatrix( camera_frustum, proj );
	this->z_far = z_far;

	// Load the scene file
	if ( scenefile != NULL )
	{
		if ( !scene.LoadFromFile( scenefile ) ) return false;
	}

	if ( !scene.initializeD3D( dxManager ) )
		return false;

	spriteBatch.reset( new SpriteBatch( dxManager.pD3DDeviceContext ) );
	spriteFont.reset( new SpriteFont( dxManager.pD3DDevice, L"Calibri.spritefont" ) );

	time = CoreLib::Diagnostics::PerformanceCounter::Start( );
	return true;
}

void Renderer::run()
{
	// clear render target and depth buffer
	dxManager.resetState();
	dxManager.clear();

	// update the camera
	float dtime = (float) CoreLib::Diagnostics::PerformanceCounter::ToSeconds( CoreLib::Diagnostics::PerformanceCounter::End( time ) );
	time = CoreLib::Diagnostics::PerformanceCounter::Start();
	camera.HandleKeys( dtime );
	XMMATRIX mat;
	camera.GetTransform( mat );
	dxManager.setViewMatrix( mat );

	BoundingFrustum frustum;
	camera_frustum.Transform( frustum, XMMatrixInverse( nullptr, mat ) );

#ifdef _DEBUG
	static bool j = true, k = true;
	if ( GetAsyncKeyState( L'J' ) )
	{
		if ( j )
		{
			scene.models.begin( )->instancedMeshes.begin( )->d0 += 100.f;
			j = false;
		}		
	}
	else
	{
		j = true;
		if ( GetAsyncKeyState( L'K' ) )
		{
			if ( k )
			{
				k = false;
				float d0 = scene.models.begin( )->instancedMeshes.begin( )->d0 - 100.f;
				scene.models.begin( )->instancedMeshes.begin( )->d0 = d0 < 0.f ? 0.f : d0;
			}			
		}
		else
		{
			k = true;
		}
	}

	static float h = 0.5f;
	static bool n = true, m = true;
	if ( GetAsyncKeyState( L'N' ) )
	{
		if ( n )
		{
			n = false;
			h += 0.05f;
			h = h > 0.5f ? 0.5f : h;
			scene.models.begin( )->instancedMeshes.begin( )->h = -1.f / log2f( h );
		}		
	}
	else
	{
		n = true;
		if ( GetAsyncKeyState( L'M' ) )
		{
			if ( m )
			{
				m = false;
				h -= 0.05f;
				h = h < 0.f ? 0.f : h;
				scene.models.begin( )->instancedMeshes.begin( )->h = -1.f / log2f( h );
			}	
		}
		else
		{
			m = true;
		}
	}
	
#endif

	// draw the scene to the D3D device
	float fps = 1.f / dtime;
	UINT modelCount = scene.computePVS( frustum );
	UINT meshCount = scene.computeLODs( camera.GetEyePos(), camera.GetEyeDir(), z_far );
	UINT leafcount = scene.Render( dxManager, camera.GetEyePos() );

	wchar_t buf[100];
	swprintf( buf, 100, L"FPS: %3.3f", fps );
	spriteBatch->Begin( DirectX::SpriteSortMode_Immediate );
	spriteFont->DrawString( spriteBatch.get(), buf, XMFLOAT2( 0, 0 ) );
	swprintf( buf, 100, L"Leaves: %u", leafcount );
	spriteFont->DrawString( spriteBatch.get(), buf, XMFLOAT2( 0, spriteFont->GetLineSpacing() ) );
	spriteBatch->End();

	// swap buffers
	dxManager.present();
}

void Renderer::release()
{
	if( textBlend ) textBlend->Release();
	spriteBatch.release();
	spriteFont.release();
	scene.releaseD3D();
	dxManager.release();
}