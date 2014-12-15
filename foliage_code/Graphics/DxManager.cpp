/*
 * Simple manager class for DirecX device
 * Ported to D3D11 from Bobby Anguelov's D3D10 tutorials
 */

#include "DxManager.h"

DxManager::DxManager( ) :	pD3DDevice(NULL ),
							pSwapChain( NULL ),
							pRenderTargetView( NULL ),
							viewMatrix( XMMatrixIdentity() ),
							projectionMatrix( XMMatrixIdentity() ),
							pD3DDeviceContext( NULL ),
							pDepthStencil( NULL ),
							pDepthStencilView( NULL ),
							pCullBackRaster( NULL ),
							pNoCullRaster( NULL ),
							pDepthState( NULL )
{
}


DxManager::~DxManager()
{
}

void DxManager::release()
{
	BOOL fullscreen;
	if ( FAILED( pSwapChain->GetFullscreenState( &fullscreen, NULL ) ) || fullscreen )
	{
		pSwapChain->SetFullscreenState( false, NULL );
	}

	if ( pRenderTargetView ) pRenderTargetView->Release( );
	if ( pSwapChain ) pSwapChain->Release( );
	if ( pD3DDevice ) pD3DDevice->Release( );
	if ( pD3DDeviceContext ) pD3DDeviceContext->Release( );
	if ( pDepthStencil ) pDepthStencil->Release();
	if ( pDepthStencilView ) pDepthStencilView->Release();
	if ( pCullBackRaster ) pCullBackRaster->Release( );
	if ( pNoCullRaster ) pNoCullRaster->Release();
	if ( noBlend ) noBlend->Release( );
	if ( leafBlend ) leafBlend->Release( );
}

bool DxManager::initialize( HWND* hW, int width, int height, int msaa, bool fullscreen )
{
	UINT sampleCount = 1;
	BOOL antialias = false;

	if ( msaa > 0 )
	{
		antialias = true;
		sampleCount = 2 << msaa;
	}

	//window handle
	hWnd = hW;

	//Set up DX swap chain
	//--------------------------------------------------------------
	DXGI_SWAP_CHAIN_DESC swapChainDesc;
	ZeroMemory( &swapChainDesc, sizeof(swapChainDesc) );
	swapChainDesc.BufferDesc.Width = width;
	swapChainDesc.BufferDesc.Height = height;
	swapChainDesc.BufferDesc.RefreshRate.Numerator = 144;
	swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
	swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE;
	swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	swapChainDesc.SampleDesc.Count = sampleCount;
	swapChainDesc.SampleDesc.Quality = 0;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = 2;
	swapChainDesc.OutputWindow = *hWnd;
	swapChainDesc.Windowed = true;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD; // TODO: DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL gives black screen
	swapChainDesc.Flags = 0;

	// we will need at least 10.0 for geometry shaders
	const D3D_FEATURE_LEVEL featureLevels[4] = { 
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
	};

	D3D_FEATURE_LEVEL selectedLevel;

	//Create the D3D device
	//--------------------------------------------------------------
	if ( E_INVALIDARG == D3D11CreateDeviceAndSwapChain( NULL,
														D3D_DRIVER_TYPE_HARDWARE,
														NULL,
														D3D11_CREATE_DEVICE_SINGLETHREADED,
														(const D3D_FEATURE_LEVEL *) &featureLevels,
														4,
														D3D11_SDK_VERSION,
														&swapChainDesc,
														&pSwapChain,
														&pD3DDevice,
														&selectedLevel,
														&pD3DDeviceContext ) )
	{
		// means no 11_1 support on device, try again without
		if ( FAILED( D3D11CreateDeviceAndSwapChain( NULL,
													D3D_DRIVER_TYPE_HARDWARE,
													NULL,
													D3D11_CREATE_DEVICE_SINGLETHREADED,
													(const D3D_FEATURE_LEVEL *) &featureLevels[1],
													3,
													D3D11_SDK_VERSION,
													&swapChainDesc,
													&pSwapChain,
													&pD3DDevice,
													&selectedLevel,
													&pD3DDeviceContext ) ) )
			return fatalError( L"D3D device creation failed" );
	}
	
	// Set rasterizer state
	//--------------------------------------------------------------
	D3D11_RASTERIZER_DESC rasterDesc;
	ZeroMemory( &rasterDesc, sizeof(D3D11_RASTERIZER_DESC) );
	rasterDesc.FillMode = D3D11_FILL_SOLID;
	rasterDesc.CullMode = D3D11_CULL_BACK;
	rasterDesc.FrontCounterClockwise = false;
	rasterDesc.DepthBias = 0;
	rasterDesc.DepthBiasClamp = 0.f;
	rasterDesc.SlopeScaledDepthBias = 0.f;
	rasterDesc.DepthClipEnable = true;
	rasterDesc.ScissorEnable = false;
	rasterDesc.MultisampleEnable = antialias;
	rasterDesc.AntialiasedLineEnable = false;
	if ( FAILED( pD3DDevice->CreateRasterizerState( &rasterDesc, &pCullBackRaster ) ) ) return false;
	pD3DDeviceContext->RSSetState( pCullBackRaster );

	rasterDesc.CullMode = D3D11_CULL_NONE;
	if ( FAILED( pD3DDevice->CreateRasterizerState( &rasterDesc, &pNoCullRaster ) ) ) return false;
	

	// Create render target view
	//--------------------------------------------------------------

	// try to get the back buffer
	ID3D11Texture2D* pBackBuffer;
	if ( FAILED( pSwapChain->GetBuffer( 0, __uuidof(ID3D11Texture2D), (LPVOID*) &pBackBuffer ) ) ) return fatalError( L"Could not get back buffer" );
	// try to create render target view
	if ( FAILED( pD3DDevice->CreateRenderTargetView( pBackBuffer, NULL, &pRenderTargetView ) ) ) return fatalError( L"Could not create render target view" );
	//release the back buffer
	pBackBuffer->Release( );

	// create the depth stencil
	D3D11_TEXTURE2D_DESC descDepth;
	ZeroMemory( &descDepth, sizeof(D3D11_TEXTURE2D_DESC) );
	descDepth.Width = width;
	descDepth.Height = height;
	descDepth.MipLevels = 1;
	descDepth.ArraySize = 1;
	descDepth.Format = DXGI_FORMAT_D32_FLOAT;
	descDepth.SampleDesc.Count = sampleCount;
	descDepth.SampleDesc.Quality = 0;
	descDepth.Usage = D3D11_USAGE_DEFAULT;
	descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	descDepth.CPUAccessFlags = 0;
	descDepth.MiscFlags = 0;

	if ( FAILED( pD3DDevice->CreateTexture2D( &descDepth, NULL, &pDepthStencil ) ) ) return fatalError( L"Could not create depth stencil." );

	D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
	ZeroMemory( &descDSV, sizeof(D3D11_DEPTH_STENCIL_VIEW_DESC) );
	descDSV.Format = descDepth.Format;
	descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;
	descDSV.Flags = 0;
	descDSV.Texture2D.MipSlice = 0;

	if ( FAILED( pD3DDevice->CreateDepthStencilView( pDepthStencil, &descDSV, &pDepthStencilView ) ) ) return fatalError( L"Could not create depth stencil view." );
	
	D3D11_DEPTH_STENCIL_DESC descDepthState;
	ZeroMemory( &descDepthState, sizeof(D3D11_DEPTH_STENCIL_DESC) );
	descDepthState.DepthEnable = true;
	descDepthState.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	descDepthState.DepthFunc = D3D11_COMPARISON_LESS;
	descDepthState.StencilEnable = false;
	descDepthState.StencilReadMask = 0;
	descDepthState.StencilWriteMask = 0;

	if ( FAILED( pD3DDevice->CreateDepthStencilState( &descDepthState, &pDepthState ) ) ) return fatalError( L"Could not create depth stencil state." );
	pD3DDeviceContext->OMSetDepthStencilState( pDepthState, 0 );

	//set the render target
	pD3DDeviceContext->OMSetRenderTargets( 1, &pRenderTargetView, pDepthStencilView );

	//Create viewport
	//--------------------------------------------------------------

	//create viewport structure	
	ZeroMemory( &viewPort, sizeof(D3D11_VIEWPORT) );
	viewPort.TopLeftX = 0;
	viewPort.TopLeftY = 0;
	viewPort.Width = (float)width;
	viewPort.Height = (float)height;
	viewPort.MinDepth = 0.0f;
	viewPort.MaxDepth = 1.0f;

	pD3DDeviceContext->RSSetViewports( 1, &viewPort );

	// enable fullscreen once everything is initialized
	if ( fullscreen )
	{
		if ( FAILED( pSwapChain->SetFullscreenState( true, NULL ) ) )
			return fatalError( L"Failed to enable fullscreen." );
	}

	// set up alpha-to-coverage states for leaf rendering
	D3D11_BLEND_DESC blendDesc;
	ZeroMemory( &blendDesc, sizeof(D3D11_BLEND_DESC) );
	blendDesc.AlphaToCoverageEnable = false;
	blendDesc.IndependentBlendEnable = false;
	blendDesc.RenderTarget[0].BlendEnable = false;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

	if ( FAILED( pD3DDevice->CreateBlendState( &blendDesc, &noBlend ) ) ) return fatalError( L"Could not create default blend state." );

	blendDesc.AlphaToCoverageEnable = true;

	if ( FAILED( pD3DDevice->CreateBlendState( &blendDesc, &leafBlend ) ) ) return fatalError( L"Could not create alpha-to-coverage blend state." );

	pD3DDeviceContext->OMSetBlendState( noBlend, NULL, 0xffffffff );

	return true;
}

void DxManager::setLeafState()
{
	pD3DDeviceContext->RSSetState( pCullBackRaster );
	pD3DDeviceContext->OMSetBlendState( leafBlend, NULL, 0xffffffff );
}

void DxManager::resetState()
{
	pD3DDeviceContext->RSSetState( pCullBackRaster );
	pD3DDeviceContext->OMSetBlendState( noBlend, NULL, 0xffffffff );
	pD3DDeviceContext->OMSetDepthStencilState( pDepthState, 0 );
}

void DxManager::present()
{
	pSwapChain->Present( 0, 0 );
}

void DxManager::clear()
{
	static float clearColor[] = { 0.f, 0.f, 0.f, 0.f };
	pD3DDeviceContext->ClearRenderTargetView( pRenderTargetView, clearColor );
	pD3DDeviceContext->ClearDepthStencilView( pDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0 );
}

bool DxManager::fatalError( const wchar_t *msg )
{
	MessageBox( *hWnd, msg, L"Fatal Error!", MB_ICONERROR );
	return false;
}