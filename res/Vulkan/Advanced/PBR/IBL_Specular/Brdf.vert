#version 450 core
layout (location = 0) in vec4 aPos;
layout (location = 1) in vec4 inColor;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec4 normal;

out vec2 TexCoords;

void main()
{
    TexCoords = aTexCoords;
	gl_Position = aPos;
}