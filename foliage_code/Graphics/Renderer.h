#include "Scene.h"
#include "DxManager.h"
#include "Camera.h"
#include "..\CoreLib\PerformanceCounter.h"
#include "..\DirectXTK\Inc\SpriteFont.h"

class Renderer
{
private:
	DxManager dxManager;
	Scene scene;
	Camera camera;
	BoundingFrustum camera_frustum;
	float z_far;
	CoreLib::Diagnostics::TimePoint time;

	ID3D11BlendState *textBlend;
	std::unique_ptr<SpriteBatch> spriteBatch;
	std::unique_ptr<SpriteFont> spriteFont;

public:
	bool initialize( const char *scenefile, HWND * hWnd, int width, int height, float z_far, int msaa, bool fullscreen = false);
	void run();
	void release();
};