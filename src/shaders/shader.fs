#version 430

uniform sampler2D tex;

in vec2 vPosition;
in vec2 vUV;
in vec4 vColor;

out vec4 outColor;

void main() {
    outColor = vColor * texture(tex, vUV);
}
