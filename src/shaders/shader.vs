#version 430

in vec2 pixelPosition;
in vec2 uv;
in vec4 color;

out vec2 vPosition;
out vec2 vUV;
out vec4 vColor;

uniform sampler2D tex;
uniform uint width;
uniform uint height;

void main() {
    vPosition = 2 * pixelPosition / vec2(width, height) - 1;
    vPosition.y = -vPosition.y;
    
    gl_Position = vec4(vPosition, 0, 1);
    vColor = color;
    vUV = uv;
}
