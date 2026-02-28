#version 330

layout(location=0) in vec3 vertexPosition;
layout(location=1) in mat4 instanceTransform;
layout(location=5) in vec4 instanceColor;

uniform mat4 mvp;

out vec4 fragColor;

void main() {
     fragColor = instanceColor;
    gl_Position = mvp * instanceTransform * vec4(vertexPosition, 1.0);
}