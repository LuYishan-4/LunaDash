in vec2 uv;
uniform sampler2D sourceTexture;
uniform vec2 direction;
uniform float radius;
uniform float opacity;
out vec4 fragColor;
void main() {
    // Adjacent texels keep wide blur kernels smooth instead of repeating sparse taps.
    float sigma = max(radius / 3.0, 0.5);
    vec3 color = vec3(0.0);
    float totalWeight = 0.0;
    for (int offset = -16; offset <= 16; ++offset) {
        float distance = float(offset);
        if (abs(distance) > radius) continue;
        float weight = exp(-0.5 * distance * distance / (sigma * sigma));
        color += texture(sourceTexture, uv + direction * distance).rgb * weight;
        totalWeight += weight;
    }
    fragColor = vec4(color / totalWeight, 1.0) * opacity;
}
