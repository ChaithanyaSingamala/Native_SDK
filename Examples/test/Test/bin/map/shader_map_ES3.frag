#version 300 es

in mediump vec2   uvs;
in highp    float  light_dot_norm;

layout (location = 0) out lowp vec4 out_colour;

void main()
{
	out_colour = vec4(vec3(light_dot_norm),1.0); 
}