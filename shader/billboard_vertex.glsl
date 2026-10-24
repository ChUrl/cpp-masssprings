#version 330

// The base mesh is a unit quad in the XZ plane (GenMeshPlane). Each instance is expanded in
// camera space using the camera's right/up vectors, so the quad always faces the viewer.
layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec2 vertexTexCoord;
layout(location = 6) in vec3 instancePosition;
layout(location = 5) in vec4 instanceColor;

uniform mat4 mvp;
uniform float vertexScale;
uniform vec3 cameraRight;
uniform vec3 cameraUp;

out vec4 fragColor;
out vec2 fragUV;

void main()
{
    fragColor = instanceColor;
    fragUV = vertexTexCoord;

    vec3 worldPosition = instancePosition
        + cameraRight * (vertexPosition.x * vertexScale)
        + cameraUp * (vertexPosition.z * vertexScale);

    gl_Position = mvp * vec4(worldPosition, 1.0);
}
