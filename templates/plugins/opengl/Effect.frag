#version 440
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float strength;
    vec2 resolution;
    vec4 parameters;
};
layout(binding = 1) uniform sampler2D source;
void main() {
    vec4 original = texture(source, qt_TexCoord0);
    vec3 tinted = original.rgb * vec3(0.72, 0.86, 1.0);
    fragColor = vec4(mix(original.rgb, tinted, clamp(strength, 0.0, 1.0)), original.a) * qt_Opacity;
}
