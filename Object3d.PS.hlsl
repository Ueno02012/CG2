
struct PixelShaderOutput
{
  float32_t4 color : SV_TARGET0;
};



PixelShaderOutput main()
{
  PixelShaderOutput output;
  output.color = gMaterial.color;
	return output;
}