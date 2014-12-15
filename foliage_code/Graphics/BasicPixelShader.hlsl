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
	float3 V = normalize( eyepos - (float3)input.wp );
	float3 R = reflect( lightDir, N );

	float4 Ia = Ka * ambient;
	float Id = Kd * saturate( dot( N, -lightDir ) );
	float Is = Ks * pow( saturate( dot( R, V ) ), Ns );
	float4 I = Ia + (Id + Is) * lightCol;
	return I * colorMap.Sample(colorSampler, input.t);
}