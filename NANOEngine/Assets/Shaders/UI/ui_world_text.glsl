#type vertex
#version 460 core
// UI World Space Text Shader - 3D positioned text rendering with font atlas
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aColor;

out vec2 vUV;
out vec4 vColor;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;

void main() {
    gl_Position = uProj * uView * uModel * vec4(aPos, 1.0);
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
layout(bindless_sampler) uniform sampler2D uFontAtlas;

void main() {
    // Sample single-channel (red) font atlas for alpha
    float alpha = texture(uFontAtlas, vUV).r;
    FragColor = vec4(uColor.rgb * vColor.rgb, uColor.a * vColor.a * alpha);
}
