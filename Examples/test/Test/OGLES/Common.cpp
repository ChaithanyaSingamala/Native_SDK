#include "Common.h"



Common::Common()
{
}


Common::~Common()
{
}


Result createShaderProgram(native::HShader_ shaders[], uint32 count, const char** attribs, uint32 attribCount, GLuint& shaderProg)
{
	shaderProg = gl::CreateProgram();
	for (uint32 i = 0; i < count; ++i) { gl::AttachShader(shaderProg, shaders[i]); }

	if (attribs && attribCount)
	{
		for (uint32 i = 0; i < attribCount; ++i) { gl::BindAttribLocation(shaderProg, i, attribs[i]); }
	}
	gl::LinkProgram(shaderProg);
	//check for link success
	GLint glStatus;
	gl::GetProgramiv(shaderProg, GL_LINK_STATUS, &glStatus);
	if (!glStatus)
	{
		std::string infolog;
		int32 infoLogLength, charWriten;
		gl::GetProgramiv(shaderProg, GL_INFO_LOG_LENGTH, &infoLogLength);
		infolog.resize(infoLogLength);
		if (infoLogLength)
		{
			gl::GetProgramInfoLog(shaderProg, infoLogLength, &charWriten, &(infolog)[0]);
			Log(Log.Debug, infolog.c_str());
		}
		return Result::InvalidData;
	}
	return Result::Success;
}
