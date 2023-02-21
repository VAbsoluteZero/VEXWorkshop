cbuffer cbPerFrame : register(b0)
{
	float4x4 matVP;
	float4x4 matModel;
	float4 color1; 
};

/* vertex attributes go here to input to the vertex shader */
struct VSinput
{
	float3 position_local : POS;
};

/* outputs from vertex shader go here. can be interpolated to pixel shader */
struct VSOutput
{
	float4 position_local : SV_POSITION; // required output of VS
	float4 col : COLOR;
};

VSOutput vs_main(VSinput input)
{
	VSOutput output = (VSOutput)0; 

	output.position_local = mul(matVP, mul(matModel, float4(input.position_local, 1.0f)));
	output.col = color1;

	return output;
}

float4 ps_main(VSOutput input) : SV_TARGET
{
	return input.col; // must return an RGBA colour
}