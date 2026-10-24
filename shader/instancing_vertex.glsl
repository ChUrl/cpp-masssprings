#version 330

layout(location = 0) in vec3 vertexPosition;
layout(location = 6) in vec3 instancePosition;
layout(location = 5) in vec4 instanceColor;
layout(location = 7) in float instanceScale;

uniform mat4 mvp;

out vec4 fragColor;

void main()
{
    fragColor = instanceColor;
    gl_Position = mvp * vec4(vertexPosition * instanceScale + instancePosition, 1.0);
}
