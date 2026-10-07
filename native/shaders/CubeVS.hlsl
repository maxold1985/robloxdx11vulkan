cbuffer ObjectBuffer : register(b0)
{
	row_major float4x4 World;
	row_major float4x4 ViewProjection;
	float4 ObjectColor;
};

struct VSInput
{
	float3 Position : POSITION;
	float3 Normal : NORMAL;
};

struct VSOutput
{
	float4 Position : SV_POSITION;
	float3 Normal : TEXCOORD0;
};

VSOutput main(VSInput input)
{
	VSOutput output;

	float4 worldPosition =
		mul(
			float4(input.Position, 1.0f),
			World
		);

	output.Position =
		mul(
			worldPosition,
			ViewProjection
		);

	output.Normal =
		normalize(
			mul(
				float4(input.Normal, 0.0f),
				World
			).xyz
		);

	return output;
}
