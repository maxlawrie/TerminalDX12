// GlyphVertex.hlsl - Vertex shader for instanced glyph rendering

// Constants for screen transformation
cbuffer ScreenConstants : register(b0) {
    float2 screenSize;
    float2 padding;
};

struct VSInput {
    float2 position : POSITION;
    float2 size : SIZE;
    float2 uvMin : UVMIN;
    float2 uvMax : UVMAX;
    float4 color : COLOR;
};

struct VSOutput {
    float4 position : SV_Position;
    float2 texCoord : TEXCOORD;
    float4 color : COLOR;
};

VSOutput main(uint vertexID : SV_VertexID, VSInput instance) {
    VSOutput output;

    // Generate quad vertices (triangle strip: 0=TL, 1=TR, 2=BL, 3=BR)
    float2 quadPositions[4] = {
        float2(0.0, 0.0),  // Top-left
        float2(1.0, 0.0),  // Top-right
        float2(0.0, 1.0),  // Bottom-left
        float2(1.0, 1.0)   // Bottom-right
    };

    float2 quadUVs[4] = {
        float2(0.0, 0.0),  // Top-left
        float2(1.0, 0.0),  // Top-right
        float2(0.0, 1.0),  // Bottom-left
        float2(1.0, 1.0)   // Bottom-right
    };

    // Get vertex position in quad
    float2 quadPos = quadPositions[vertexID];
    float2 quadUV = quadUVs[vertexID];

    // Calculate screen position (pixels)
    float2 screenPos = instance.position + quadPos * instance.size;

    // Convert to NDC (-1 to 1)
    float2 ndc = (screenPos / screenSize) * 2.0 - 1.0;
    ndc.y = -ndc.y;  // Flip Y axis (screen space is top-down, NDC is bottom-up)

    output.position = float4(ndc, 0.0, 1.0);

    // Interpolate UV coordinates
    output.texCoord = lerp(instance.uvMin, instance.uvMax, quadUV);

    // Pass color through
    output.color = instance.color;

    return output;
}
