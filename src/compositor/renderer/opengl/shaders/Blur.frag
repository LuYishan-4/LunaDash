in vec2 uv;
uniform sampler2D sourceTexture;
uniform vec2 direction;
uniform int kernelPairs;
uniform float weights[9];
uniform float offsets[9];
uniform float opacity;
out vec4 fragColor;
void main() {
    // Linear filtering combines adjacent Gaussian taps without sparse sampling.
    vec3 color = texture(sourceTexture, uv).rgb * weights[0];
    for (int index = 1; index <= 8; ++index) {
        if (index > kernelPairs) break;
        vec2 step = direction * offsets[index];
        color += (texture(sourceTexture, uv + step).rgb
                + texture(sourceTexture, uv - step).rgb) * weights[index];
    }
    fragColor = vec4(color, 1.0) * opacity;
}
