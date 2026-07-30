struct Input
{
    float3 pos : TEXCOORD0;
    float2 tex_coord : TEXCOORD1;
};

struct Output
{
    float2 tex_coord : TEXCOORD0;
    float4 pos : SV_Position;
};

Output main(Input input)
{
    Output output;
    output.tex_coord = input.tex_coord;
    output.pos = float4(input.pos, 1.0f);
    return output;
}
