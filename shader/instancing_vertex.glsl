#version 330

layout(location = 0) in vec3 vertexPosition;
layout(location = 6) in vec3 instancePosition;
layout(location = 5) in vec4 instanceColor;

uniform mat4 mvp;
uniform float vertexScale;

out vec4 fragColor;

void main()
{
    fragColor = instanceColor;
    gl_Position = mvp * vec4(vertexPosition * vertexScale + instancePosition, 1.0);
}
