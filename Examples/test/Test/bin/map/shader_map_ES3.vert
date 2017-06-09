#version 300 es
layout (location = 0) in highp vec3	    inVertex;
layout (location = 1) in mediump vec3	inNormal;
layout (location = 2) in mediump vec2	inTexCoord;

uniform highp   mat4  MVPMatrix;
uniform highp   mat4  WorldViewIT;

out mediump    float  light_dot_norm;
out mediump vec2   uvs;

void main()
{
	gl_Position = MVPMatrix * vec4(inVertex, 1.0);
	
	uvs = inTexCoord;
	
	highp vec3 light_direction = normalize(vec3(0.7,0.7,0.0));
	
	highp vec3 Normals = normalize(mat3(WorldViewIT) * inNormal);
	
	light_dot_norm = clamp(dot(Normals, light_direction) * 0.5 + 0.5, 0.0, 1.0);
}