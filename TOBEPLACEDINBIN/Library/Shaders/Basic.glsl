#type vertex
#version 460 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;

uniform mat4 u_Model;
uniform mat4 u_View;
uniform mat4 u_Projection;

out vec3 v_Normal;
out vec2 v_TexCoord;

void main() {
    v_Normal = mat3(u_Model) * a_Normal;
    v_TexCoord = a_TexCoord;
    gl_Position = u_Projection * u_View * u_Model * vec4(aPos, 1.0);
}

#type fragment
#version 460 core

in vec3 v_Normal;
in vec2 v_TexCoord;

out vec4 FragColor;

void main() {
    //FragColor = vec4(1.0, 0.5, 0.2, 1.0);
    FragColor = vec4(normalize(v_Normal) * 0.5 + 0.5, 1.0); 
}
