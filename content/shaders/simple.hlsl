cbuffer cbPerFrame : register(b0)
{
	float4x4 matVP;
	float4x4 matGeo;
};

struct VSInput
{
	float3 Position : POSITION;
	float3 Normal : NORMAL;
	float2 UV : TEXCOORD;
};

struct VSOutput
{
	float4 Position : SV_POSITION;
	float4 Color : COLOR;
	float2 UV : TEXCOORD;
};

VSOutput vs_main(VSInput vin)
{
	VSOutput vout = (VSOutput)0;

	vout.Position = mul(mul(float4(vin.Position, 1.0f), matGeo), matVP);
	vout.Color = float4(abs(vin.Normal), 1);
	vout.UV = vin.UV;

	return vout;
}
/*-----------------------------------------------------------------------*/

struct PSInput
{
	float4 Color : COLOR;
	float2 UV : TEXCOORD;
};

Texture2D tex : register(t0);
SamplerState smp : register(s0);

float4 ps_main(PSInput pin) : SV_TARGET
{
	return tex.Sample(smp, pin.UV);
}
