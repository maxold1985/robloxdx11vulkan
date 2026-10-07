cbuffer ObjectBuffer : register(b0)
{
	row_major float4x4 World;
	row_major float4x4 ViewProjection;
	float4 ObjectColor;
};

struct PSInput
{
	float4 Position : SV_POSITION;
	float3 Normal : TEXCOORD0;
};

float4 main(PSInput input) : SV_TARGET
{
	const float3 lightDirection =
		normalize(
			float3(
				-0.4f,
				0.8f,
				-0.6f
			)
		);

	float lighting =
		saturate(
			dot(
				normalize(input.Normal),
				lightDirection
			)
		);

	lighting =
		0.35f +
		lighting * 0.65f;

	return float4(
		ObjectColor.rgb * lighting,
		ObjectColor.a
	);
}
