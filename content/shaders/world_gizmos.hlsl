cbuffer cbPerFrame : register(b0)
{
	float4x4 matVP;
	float4x4 matModel;
};

/* vertex attributes go here to input to the vertex shader */
struct VSinput
{
	float3 Position : POS;
};

/* outputs from vertex shader go here. can be interpolated to pixel shader */
struct VSOutput
{
	float4 Position : SV_POSITION; // required output of VS
};

VSOutput vs_main(VSinput input)
{
	VSOutput output = (VSOutput)0; 

	output.Position = mul(mul(float4(vin.Position, 1.0f), matModel), matVP);

	return output;
}

float4 ps_main(VSOutput input) : SV_TARGET
{
	return float4(0.4, 0.8, 0.4, 1.0); // must return an RGBA colour
}