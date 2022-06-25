#version 430

uniform sampler2D tex;

in vec2 vPosition;
in vec2 vUV;
in vec3 vColor;

out vec4 outColor;

void main() {
    outColor = vec4(vColor, 1) * texture(tex, vUV);
}
