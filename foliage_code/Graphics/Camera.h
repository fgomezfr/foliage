#pragma once

#include <DirectXMath.h>
using namespace DirectX;

class Camera
{
private:
	XMVECTOR pos;
	XMVECTOR dir;
	XMVECTOR up;
	float alpha, beta;
	float speed, spin;
public:
	Camera();
	Camera( float speed, float spin );
	~Camera();

	XMVECTOR GetEyePos();
	XMVECTOR GetEyeDir();
	void GetTransform( XMMATRIX & m );
	void HandleKeys( float dt );
};

