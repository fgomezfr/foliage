#include "Camera.h"
#include <Windows.h>

Camera::Camera( ) : speed( 100.f ),
					spin( 3.14159265f/4.f ),
					alpha( 0.f ),
					beta( 0.f ),
					pos( XMVectorSet( 0.f, 1.5f, 0.f, 1.f ) ),
					dir( XMVectorSet( 0.f, 0.f, -1.f, 0.f ) ),
					up( XMVectorSet( 0.f, 1.f, 0.f, 0.f ) )
{
}

Camera::Camera( float speed, float spin ) : speed( speed ),
											spin( spin ),
											alpha( 0.f ), 
											beta( 0.f ), 
											pos( XMVectorSet( 0.f, 1.5f, 0.f, 1.f ) ),
											dir( XMVectorSet( 0.f, 0.f, 1.f, 0.f ) ),
											up( XMVectorSet( 0.f, 1.f, 0.f, 0.f ) )
{
}


Camera::~Camera()
{
}

XMVECTOR Camera::GetEyePos()
{
	return pos;
}

XMVECTOR Camera::GetEyeDir( )
{
	return dir;
}

void Camera::GetTransform( XMMATRIX & m )
{
	m = XMMatrixLookToLH( pos, dir, up );
}

void Camera::HandleKeys( float dt )
{
	float delta_v = speed * dt;
	float delta_a = spin * dt;
	XMVECTOR right = XMVector3Cross( up, dir );
	if ( GetAsyncKeyState( VK_LSHIFT ) != 0 )
	{
		delta_v *= .2f;
		delta_a *= .2f;
	}
	else if ( GetAsyncKeyState( VK_LCONTROL ) != 0 )
	{
		delta_v *= 10.f;
	}
	
	XMVECTOR vel = XMVectorZero();
	if ( GetAsyncKeyState( L'W' ) )
		vel += dir;
	else if ( GetAsyncKeyState( L'S' ) )
		vel -= dir;
	if ( GetAsyncKeyState( L'A' ) )
		vel -= right;
	else if ( GetAsyncKeyState( L'D' ) )
		vel += right;
	if ( GetAsyncKeyState( L'Q' ) )
		vel += up;
	else if ( GetAsyncKeyState( L'E' ) )
		vel -= up;

	pos += delta_v * XMVector3Normalize( vel );

	float alpha = 0.f;
	float beta = 0.f;
	float gamma = 0.f;
	if ( GetAsyncKeyState( VK_LEFT ) )
	{
		if ( !GetAsyncKeyState( VK_RCONTROL ) )
			alpha -= delta_a;
		else
			gamma += delta_a;
	}	
	else if ( GetAsyncKeyState( VK_RIGHT ) )
	{
		if ( !GetAsyncKeyState( VK_RCONTROL ) )
			alpha += delta_a;
		else
			gamma -= delta_a;
	}

	if ( GetAsyncKeyState( VK_UP ) )
		beta -= delta_a;
	else if ( GetAsyncKeyState( VK_DOWN ) )
		beta += delta_a;

	XMMATRIX rot = XMMatrixRotationAxis( dir, gamma ) * XMMatrixRotationAxis( right, beta ) * XMMatrixRotationAxis( up, alpha );
	dir = XMVector3Transform( dir, rot );
	up = XMVector3Transform( up, rot );
}