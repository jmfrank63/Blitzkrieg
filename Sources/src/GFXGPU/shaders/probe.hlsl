struct ProbeVertex {
    float3 position : POSITION0;
    float4 color : COLOR0;
};

struct ProbeOutput {
    float4 position : SV_Position;
    float4 color : COLOR0;
};

ProbeOutput VSMain(ProbeVertex input) {
    ProbeOutput output;
    output.position = float4(input.position, 1.0f);
    output.color = input.color;
    return output;
}

float4 PSMain(float4 color : COLOR0) : SV_Target0 {
    return color;
}
