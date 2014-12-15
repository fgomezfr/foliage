#ifndef DXMANAGER_H
#define DXMANAGER_H

#include <windows.h>
#include <d3d11.h>
#include <DirectXMath.h>

using namespace DirectX;

class DxManager
{
	friend class Scene;
	friend class Renderer;
private:

	HWND*							hWnd;

	IDXGISwapChain*					pSwapChain;
	ID3D11RenderTargetView*			pRenderTargetView;
	ID3D11Texture2D*				pDepthStencil;
	ID3D11DepthStencilView*			pDepthStencilView;
	D3D11_VIEWPORT					viewPort;
	ID3D11RasterizerState*			pCullBackRaster;
	ID3D11RasterizerState*			pNoCullRaster;
	ID3D11DepthStencilState*		pDepthState;

	// used to toggle alpha-to-coverage state
	ID3D11BlendState *noBlend;
	ID3D11BlendState *leafBlend;

protected:

	ID3D11Device*					pD3DDevice;
	ID3D11DeviceContext*			pD3DDeviceContext;
	XMMATRIX						viewMatrix;
	XMMATRIX						projectionMatrix;

	void setLeafState();
	void resetState();

public:
	DxManager( );
	~DxManager( );

	bool initialize( HWND* hW, int width, int height, int msaa, bool fullscreen = false );
	void release();

	void setViewMatrix( XMMATRIX & m ) { viewMatrix = m; };
	void setProjMatrix( XMMATRIX & m ) { projectionMatrix = m; };
	void present();
	void clear();

private:

	bool fatalError( const wchar_t *msg );
};

#endif