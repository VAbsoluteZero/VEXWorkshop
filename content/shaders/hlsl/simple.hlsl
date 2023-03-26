cbuffer cbPerFrame : register(b0)
{
	float4x4 matVP;
	float4x4 matModel;
	float4 color1;
};

struct VSInput
{
	float3 position_local : POSITION;
	float3 normal : NORMAL;
	float2 UV : TEXCOORD;
};

struct VSOutput
{
	float4 position : SV_POSITION;
	float4 color : COLOR;
	float2 UV : TEXCOORD;
};

VSOutput vs_main(VSInput vin)
{
	VSOutput vout = (VSOutput)0;

	vout.position = mul(matVP, mul(matModel, float4(vin.position_local, 1.0f)));
	vout.color = float4(abs(vin.normal), 1);
	vout.UV = vin.UV;

	return vout;
}

/*-----------------------------------------------------------------------*/

// Texture2D objTexture : TEXTURE : register(t0);
// SamplerState objSamplerState : SAMPLER : register(s0);

Texture2D tex : register(t0);
SamplerState smp : register(s0);

float4 ps_main(VSOutput pin) : SV_TARGET
{
	float3 col = tex.Sample(smp, pin.UV);
	return float4(col, 1);
}
