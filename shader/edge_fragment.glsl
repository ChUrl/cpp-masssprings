#version 330

uniform vec4 colDiffuse;
out vec4 finalColor;

void main()
{
    finalColor = colDiffuse;
}