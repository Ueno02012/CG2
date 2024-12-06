#include "Particle.hlsli"

struct Material
{
  float32_t4 color;
  float32_t4x4 uvTransform;
  int32_t enbleLighting;
};
struct PixelShaderOutput
{
  float32_t4 color : SV_TARGET0;
};
struct DirectionalLight
{
  float32_t4 color;
  float32_t3 direction;
  float intensity;
};

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

PixelShaderOutput main(VertexShaderOutput input)
{
  PixelShaderOutput output;
  float4 transformedUV = mul(float32_t4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
  float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
  output.color = gMaterial.color * textureColor;

  if (output.color.a == 0)
  {
    discard;
  }
 
  
  //if (gMaterial.enbleLighting != 0)
  //{
  //  float cos = saturate(dot(normalize(input.normal), -gDirectionalLight.direction));
  //  output.color = gMaterial.color * textureColor * gDirectionalLight.color * cos * gDirectionalLight.intensity;
    
  //} else {
  //  output.color = gMaterial.color * textureColor;
  //}

  
	return output;
}