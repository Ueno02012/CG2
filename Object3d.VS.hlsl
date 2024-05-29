struct TransformationMatrix
{
  float4x4 WVP;
};
ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

struct VertexShadeOutput
{
  float4 position : SV_POSITION;
};

struct VertexShaderInput
{
  float4 position : POSITION0;
};

VertexShadeOutput main(VertexShaderInput input)
{
  VertexShadeOutput output;
  output.position = mul(input.position,gTransformationMatrix.WVP);
	return output;
}

