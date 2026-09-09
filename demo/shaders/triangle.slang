// A minimal raster pass: one vertex entry point, one fragment entry point.
// The triangle is built from SV_VertexID, so there is nothing to bind at draw time.

struct VertexOutput {
    float4 position : SV_Position;
    float3 color : COLOR;
};

static const float2 POSITIONS[3] = {
    float2(0.0, -0.6),
    float2(0.6, 0.6),
    float2(-0.6, 0.6),
};

static const float3 COLORS[3] = {
    float3(1.0, 0.2, 0.2),
    float3(0.2, 1.0, 0.2),
    float3(0.2, 0.4, 1.0),
};

[shader("vertex")]
VertexOutput vertex_main(uint vertex_id: SV_VertexID) {
    VertexOutput output;
    output.position = float4(POSITIONS[vertex_id], 0.0, 1.0);
    output.color = COLORS[vertex_id];
    return output;
}

[shader("fragment")]
float4 fragment_main(VertexOutput input) : SV_Target {
    return float4(input.color, 1.0);
}
