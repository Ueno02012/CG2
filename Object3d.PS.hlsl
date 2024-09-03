#include "Object3d.hlsli"

struct Material
{
    float4 color;
    int endbleLighting;
};

struct DirectionalLight
{
    float4 color; //!< ライトの色
    float3 direction; //!< ライトの向き
    float intensity; //!< 輝度
};

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);

struct PixeShaderOutput
{
    float4 color : SV_TARGET0;
    
};

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

PixeShaderOutput main(VertexShaderOutput input)
{
    //TextureをSampling
    float4 textureColor = gTexture.Sample(gSampler, input.texcoord);
    
    PixeShaderOutput output;
    
    if (gMaterial.endbleLighting != 0){ // Linhthingする場合
        // half lambert
        float Ndont = dot(normalize(input.normal), -gDirectionalLight.direction);
        float cos = pow(Ndont * 0.5f + 0.5f, 2.0f);
        output.color = gMaterial.color * textureColor * gDirectionalLight.color * cos * gDirectionalLight.intensity;
    }
    else{ // Linhthingしない場合、前回までと同じ演算
        output.color = gMaterial.color * textureColor;
    }
    return output;
}