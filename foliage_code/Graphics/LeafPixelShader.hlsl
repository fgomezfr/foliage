cbuffer stable : register(b0)
{
	float4x4 viewproj;
	float3 eyepos;
	float3 lightDir;
	float4 lightCol;
	float4 ambient;
};

cbuffer perMesh : register(b1)
{
	float4 material;
}

struct PS_INPUT
{
	float4 p : SV_POSITION;
	float4 wp : WORLDPOSITION;
	float3 n : NORMAL;
	float2 t : TEXCOORD;
};

sampler colorSampler;
Texture2D<float4> colorMap;

float4 main( PS_INPUT input ) : SV_TARGET
{
	// simple phong lighting
	float Ka = material.x;
	float Kd = material.y;
	float Ks = material.z;
	float Ns = material.w;

	float3 N = normalize( input.n );

	// make sure leaves are facing the sun
	// rendering back-faces the same as front-faces
	float d = dot( N, -lightDir );
	if ( d < 0.f )
	{
		N *= -1.f;
		d *= -1.f;
	}

	float3 V = normalize( eyepos - (float3)input.wp );
	float3 R = reflect( lightDir, N );

	float3 Ia = Ka * (float3)ambient;
	float Id = Kd * saturate( d );
	float Is = Ks * pow( saturate( dot( R, V ) ), Ns );
	float3 I = Ia + (Id + Is) * (float3)lightCol;
	float4 texcolor = colorMap.Sample( colorSampler, input.t );
	return float4(I * (float3)texcolor, texcolor.w);
}