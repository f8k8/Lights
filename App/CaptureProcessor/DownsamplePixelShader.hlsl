Texture2D tx : register(t0);
SamplerState samLinear : register(s0);

cbuffer constants : register(b0)
{
	float SampleWidth;
	float SampleHeight;
	float3 ColourScale;
};

struct PS_INPUT
{
	float4 Pos : SV_POSITION;
	float2 Tex : TEXCOORD;
};

#define NUM_ROWS 15
#define NUM_COLS 3

//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------
float4 DownSamplePS(PS_INPUT input) : SV_Target
{
	// Average out samples of the pixels
	float4 outVal = { 0, 0, 0, 0 };
	float scale = 1.0 / (NUM_ROWS * NUM_COLS);
	float rowScale = 1.0 / NUM_ROWS;
	float colScale = 1.0 / NUM_COLS;
	float xStep = SampleWidth / NUM_COLS;
	float yStep = SampleHeight / NUM_ROWS;
	
	float yPos = input.Tex.y - (0.5 * SampleHeight);
	
	[unroll(NUM_ROWS)]
	for (uint row = 0; row < NUM_ROWS; ++row, yPos += yStep)
	{
		float xPos = input.Tex.x - (0.5 * SampleWidth);

		[unroll(NUM_COLS)]
		for (uint col = 0; col < NUM_COLS; ++col, xStep, xPos += xStep)
		{
			outVal += tx.Sample(samLinear, float2(xPos, yPos));
		}
	}

	// Scale the output value for averaging
	outVal *= scale;
	
	// Scale the colour to include our tint
	return outVal * float4(ColourScale.x, ColourScale.y, ColourScale.z, 1.0);
}