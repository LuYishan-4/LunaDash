in vec2 uv;
out vec4 fragmentColor;
uniform vec2 resolution;
uniform int palette;
void main() {
    vec2 p = vec2(uv.x, 1.0 - uv.y);
    vec3 skyTop = palette == 1 ? vec3(0.06, 0.20, 0.24) : vec3(0.12, 0.14, 0.25);
    vec3 skyBottom = palette == 1 ? vec3(0.35, 0.55, 0.49) : vec3(0.66, 0.48, 0.55);
    vec3 color = mix(skyTop, skyBottom, p.y);
    vec2 sunPoint = (p - vec2(0.76, 0.28)) * vec2(resolution.x / resolution.y, 1.0);
    float sun = 1.0 - smoothstep(0.13, 0.132, length(sunPoint));
    color = mix(color, palette == 1 ? vec3(0.75, 0.86, 0.70) : vec3(0.94, 0.75, 0.66), sun);
    for (int layer = 0; layer < 6; ++layer) {
        float depth = float(layer);
        float ridge = 0.46 + depth * 0.075 + 0.085 * sin(p.x * 5.5 + depth * 0.55) + 0.04 * sin(p.x * 9.0 + depth);
        vec3 farColor = palette == 1 ? vec3(0.32, 0.53, 0.51) : vec3(0.48, 0.43, 0.56);
        vec3 nearColor = vec3(0.065, 0.12, 0.20);
        color = mix(color, mix(farColor, nearColor, depth / 5.0), smoothstep(ridge, ridge + 0.0015, p.y));
    }
    fragmentColor = vec4(color, 1.0);
}
