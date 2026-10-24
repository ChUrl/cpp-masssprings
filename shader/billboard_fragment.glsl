#version 330

in vec4 fragColor;
in vec2 fragUV;

out vec4 finalColor;

void main()
{
    // Discard the corners so the quad renders as a circle.
    vec2 p = fragUV * 2.0 - 1.0;
    if (dot(p, p) > 1.0) {
        discard;
    }

    finalColor = fragColor;
}
