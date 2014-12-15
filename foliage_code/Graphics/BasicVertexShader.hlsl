cbuffer stable : register( b0 )
{
	float4x4 viewproj;
	float4 lightDir;
	float4 lightCol;
	float4 ambient;
};


cbuffer perMdl : register(b1)
{
	float4x4 world;
}

cbuffer perMesh : register(b2)
{
	float4 material;
	float scale;
	uint	scale_index;
	uint	leafcount;
}

struct VS_INPUT
{
	float3 position : POSITION;
	float3 normal : NORMAL;
	float2 texcoord : TEXCOORD;
};

struct VS_OUTPUT
{
	float4 p : SV_POSITION;
	float4 wp : WORLDPOSITION;
	float3 n : NORMAL;
	float2 t : TEXCOORD;
};

VS_OUTPUT main( VS_INPUT input )
{
	VS_OUTPUT output;
	output.wp = mul( float4(input.position, 1.f), world );
	output.p = mul( output.wp, viewproj );
	output.n = normalize( mul( input.normal, (float3x3) world ) );
	output.t = input.texcoord;
	return output;
}