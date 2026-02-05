#version 460 core

out vec4 FragColor;
uniform sampler2D screenTexture;
in vec2 fragUVs;

void main()
{
    FragColor = texture(screenTexture, fragUVs);
}