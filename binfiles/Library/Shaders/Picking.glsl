#type vertex
#version 460 core

layout(location = 0) in vec3 aPos;
layout(location = 3) in mat4 i_Model;
layout(location = 7) in vec3 i_IDRGB;

uniform mat4 u_View;
uniform mat4 u_Projection;

out vec3 v_ID;

void main() {
	gl_Position = u_Projection * u_View * i_Model * vec4(aPos, 1.0);
	v_ID = i_IDRGB;
}

#type fragment
#version 460 core

in vec3 v_ID;

out vec4 FragColor;

void main() {
    FragColor = vec4(v_ID, 1.0);
}