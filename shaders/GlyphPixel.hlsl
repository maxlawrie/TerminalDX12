// GlyphPixel.hlsl - Pixel shader for glyph rendering

// Atlas texture
Texture2D<float4> atlasTexture : register(t0);
SamplerState atlasSampler : register(s0);

struct PSInput {
    float4 position : SV_Position;
    float2 texCoord : TEXCOORD;
    float4 color : COLOR;
};

float4 main(PSInput input) : SV_Target {
    // Sample atlas texture
    float4 texColor = atlasTexture.Sample(atlasSampler, input.texCoord);

    // Multiply by text color
    // The atlas stores white glyphs with alpha, so we modulate by the color
    float4 finalColor = input.color * texColor.a;

    // Discard fully transparent pixels (optional optimization)
    if (finalColor.a < 0.01) {
        discard;
    }

    return finalColor;
}
