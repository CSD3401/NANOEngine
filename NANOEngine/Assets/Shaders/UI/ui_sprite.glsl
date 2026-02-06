#type vertex
#version 460 core
// UI Sprite Shader - Screen space rendering (pixel coordinates)
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aColor;

out vec2 vUV;
out vec4 vColor;

uniform vec2 uScreenSize;

void main() {
    // Convert pixel coordinates to NDC (-1 to 1)
    float ndcX = (aPos.x / uScreenSize.x) * 2.0 - 1.0;
    float ndcY = 1.0 - (aPos.y / uScreenSize.y) * 2.0;  // Flip Y (top-left origin)

    gl_Position = vec4(ndcX, ndcY, aPos.z, 1.0);
    vUV = aUV;
    vColor = aColor;
}

#type fragment
#version 460 core
#extension GL_ARB_bindless_texture : require

in vec2 vUV;
in vec4 vColor;
out vec4 FragColor;

uniform vec4 uColor;
uniform int uHasTexture;
layout(bindless_sampler) uniform sampler2D uTexture;

void main() {
    if (uHasTexture != 0) {
        vec4 texColor = texture(uTexture, vUV);
        FragColor = texColor * uColor * vColor;  // Texture tinted by color
    } else {
        FragColor = uColor * vColor;  // Solid color only
    }
}
