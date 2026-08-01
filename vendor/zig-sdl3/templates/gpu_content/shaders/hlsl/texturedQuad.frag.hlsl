Texture2D<float4> Texture : register(t0, space2);
SamplerState Sampler : register(s0, space2);

float4 main(float2 tex_coord : TEXCOORD0) : SV_Target0
{
    return Texture.Sample(Sampler, tex_coord);
}
