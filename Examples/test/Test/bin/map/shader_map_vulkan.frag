#version 450


layout(location = 0) in highp vec2 uvs;
layout(location = 1) in highp float light_dot_norm;

layout(location = 0) out highp vec4 out_colour;

void main()
{
	out_colour = vec4(vec3(light_dot_norm),1.0); 
}


