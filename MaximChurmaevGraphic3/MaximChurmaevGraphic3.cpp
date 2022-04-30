﻿#include <iostream>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include "GL/glew.h";
#include "GL/glut.h";
#include "glm/glm.hpp";
#include "GL/freeglut.h";
#include <Magick++.h>;
#include <string>;


#define M_PI 3.14159265358979323846
#define ToRadian(x) ((x) * M_PI / 180.0f)
#define ToDegree(x) ((x) * 180.0f / M_PI)
#define MAX_POINT_LIGHTS 3
class Texture
{
public:
	Texture(GLenum TextureTarget, const std::string& FileName)
	{
		m_textureTarget = TextureTarget;
		m_fileName = FileName;
		m_pImage = NULL;
	}

	bool Load()
	{
		try {
			m_pImage = new Magick::Image(m_fileName);
			m_pImage->write(&m_blob, "RGBA");
		}
		catch (Magick::Error& Error) {
			std::cout << "Error loading texture '" << m_fileName << "': " << Error.what() << std::endl;
			return false;
		}

		glGenTextures(1, &m_textureObj);
		glBindTexture(m_textureTarget, m_textureObj);
		glTexImage2D(m_textureTarget, 0, GL_RGB, m_pImage->columns(), m_pImage->rows(), -0.5, GL_RGBA, GL_UNSIGNED_BYTE, m_blob.data());
		glTexParameterf(m_textureTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterf(m_textureTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		return true;
	}

	void Bind(GLenum TextureUnit)
	{
		glActiveTexture(TextureUnit);
		glBindTexture(m_textureTarget, m_textureObj);
	}
private:
	std::string m_fileName;
	GLenum m_textureTarget;
	GLuint m_textureObj;
	Magick::Image* m_pImage;
	Magick::Blob m_blob;
};

struct Vertex
{
	glm::fvec3 m_pos;
	glm::fvec2 m_tex;
	glm::fvec3 m_normal;

	Vertex() {}

	Vertex(glm::fvec3 pos, glm::fvec2 tex)
	{
		m_pos = pos;
		m_tex = tex;
		m_normal = glm::fvec3(0.0f, 0.0f, 0.0f);
	}
};

struct{
	glm::fvec3 Pos;
	glm::fvec3 Target;
	glm::fvec3 Up;
} m_camera;
void SetCamera(const glm::fvec3& Pos, const glm::fvec3& Target, const glm::fvec3& Up){
	m_camera.Pos = Pos;
	m_camera.Target = Target;
	m_camera.Up = Up;
}
void CameraTransform(const glm::fvec3& Target, const glm::fvec3& Up, glm::fmat4& m){
	glm::fvec3 N = Target;
	N = glm::normalize(N);
	//N.Normalize();
	glm::fvec3 U = Up;
	U = glm::normalize(U);
	//U.Normalize();
	U = cross(U, Target);
	glm::fvec3 V = cross(N, U);

	m[0][0] = U.x; m[0][1] = U.y; m[0][2] = U.z; m[0][3] = 0.0f;
	m[1][0] = V.x; m[1][1] = V.y; m[1][2] = V.z; m[1][3] = 0.0f;
	m[2][0] = N.x; m[2][1] = N.y; m[2][2] = N.z; m[2][3] = 0.0f;
	m[3][0] = 0.0f; m[3][1] = 0.0f; m[3][2] = 0.0f; m[3][3] = 1.0f;
}
struct BaseLight
{
	glm::fvec3 Color;
	float AmbientIntensity;
	float DiffuseIntensity;

	BaseLight()
	{
		Color = glm::fvec3(0.0f, 0.0f, 0.0f);
		AmbientIntensity = 0.0f;
		DiffuseIntensity = 0.0f;
	}
};
struct DirectionalLight : public BaseLight{
	glm::fvec3 Direction;
};
struct PointLight : public BaseLight {
	glm::fvec3 Position;

	struct
	{
		float Constant;
		float Linear;
		float Exp;
	} Attenuation;

	PointLight()
	{
		Position = glm::fvec3(0.0f, 0.0f, 0.0f);
		Attenuation.Constant = 1.0f;
		Attenuation.Linear = 0.0f;
		Attenuation.Exp = 0.0f;
	}
};
struct SpotLight : public PointLight
{
	glm::fvec3 Direction;
	float Cutoff;

	SpotLight()
	{
		Direction = glm::fvec3(0.0f, 0.0f, 0.0f);
		Cutoff = 0.0f;
	}
};
struct {
	GLuint Color;
	GLuint AmbientIntensity;
	GLuint DiffuseIntensity;
	GLuint Position;
	struct
	{
		GLuint Constant;
		GLuint Linear;
		GLuint Exp;
	} Atten;
} m_pointLightsLocation[MAX_POINT_LIGHTS];

struct {
	GLuint Color;
	GLuint AmbientIntensity;
	GLuint DiffuseIntensity;
	GLuint Position;
	GLuint Direction;
	GLuint Cutoff;
	struct {
		GLuint Constant;
		GLuint Linear;
		GLuint Exp;
	} Atten;
} m_spotLightsLocation[MAX_POINT_LIGHTS];

void CalcNormals(const unsigned int* pIndices, unsigned int IndexCount, Vertex* pVertices, unsigned int VertexCount){
	for(unsigned int i = 0; i < IndexCount; i += 3){
		unsigned int Index0 = pIndices[i];
		unsigned int Index1 = pIndices[i + 1];
		unsigned int Index2 = pIndices[i + 2];
		glm::fvec3 v1 = pVertices[Index1].m_pos - pVertices[Index0].m_pos;
		glm::fvec3 v2 = pVertices[Index2].m_pos - pVertices[Index0].m_pos;
		glm::fvec3 Normal = cross(v1, v2);
		Normal = glm::normalize(Normal);

		pVertices[Index0].m_normal += Normal;
		pVertices[Index1].m_normal += Normal;
		pVertices[Index2].m_normal += Normal;
	}

	for(unsigned int i = 0; i < VertexCount; i++){
		pVertices[i].m_normal = glm::normalize(pVertices[i].m_normal);
	}
}


GLuint VBO;
GLuint IBO;
GLuint gWVPLocation;
GLuint m_WorldMatrixLocation;
GLuint m_dirLightColorLocation;
GLuint m_dirLightAmbientIntensityLocation;
GLuint m_dirLightLocationDirection;
GLuint m_dirLightLocationDiffuseIntensity;
GLuint m_eyeWorldPosition;
GLuint m_matSpecularIntensityLocation;
GLuint m_matSpecularPowerLocation;
GLuint m_numPointLightsLocation;
GLuint m_numSpotLightsLocation;
Texture* pTexture = NULL;
GLuint gSampler;
glm::fmat4* m_transformation = new glm::fmat4();
glm::fmat4* World = new glm::fmat4();


static const char* pVS = "                                                          \n\
#version 330                                                                        \n\
layout(location = 0) in vec3 Position;												\n\
layout(location = 1) in vec2 TexCoord;												\n\
layout(location = 2) in vec3 Normal;												\n\
																					\n\
uniform mat4 gWVP;																	\n\
uniform mat4 gWorld;																\n\
																					\n\
out vec2 TexCoord0;																	\n\
out vec3 Normal0;                                                                   \n\
out vec3 WorldPos0;																	\n\
																					\n\
void main()																			\n\
{																					\n\
	gl_Position = gWVP * vec4(Position, 1.0);										\n\
	TexCoord0 = TexCoord;															\n\
	Normal0 = (gWorld * vec4(Normal, 0.0)).xyz;										\n\
	WorldPos0   = (gWorld * vec4(Position, 1.0)).xyz;								\n\
};";

static const char* pFS = "                                                          \n\
#version 330                                                                        \n\
																					\n\
in vec2 TexCoord0;																	\n\
in vec3 Normal0;                                                                    \n\
in vec3 WorldPos0;																	\n\
const int MAX_POINT_LIGHTS = 3;                                                     \n\
																					\n\
out vec4 FragColor;																	\n\
																					\n\
struct BaseLight                                                                    \n\
{                                                                                   \n\
	vec3 Color;																		\n\
	float AmbientIntensity;															\n\
	float DiffuseIntensity;															\n\
};                                                                                  \n\
																					\n\
struct DirectionalLight                                                             \n\
{                                                                                   \n\
	BaseLight Base;																	\n\
	vec3 Direction;                                                                 \n\
};                                                                                  \n\
																					\n\
struct Attenuation                                                                  \n\
{                                                                                   \n\
	float Constant;																	\n\
	float Linear;																	\n\
	float Exp;																		\n\
};                                                                                  \n\
																					\n\
struct PointLight                                                                   \n\
{                                                                                   \n\
	BaseLight Base;							                                        \n\
	vec3 Position;                                                                  \n\
	Attenuation Atten;                                                              \n\
};																					\n\
																					\n\
struct SpotLight																	\n\
{																					\n\
	PointLight Base;																\n\
	vec3 Direction;																	\n\
	float Cutoff;																	\n\
};																					\n\
																					\n\
uniform int gNumPointLights;                                                        \n\
uniform int gNumSpotLights;															\n\
uniform DirectionalLight gDirectionalLight;                                         \n\
uniform PointLight gPointLights[MAX_POINT_LIGHTS];									\n\
uniform SpotLight gSpotLights[MAX_POINT_LIGHTS];									\n\
uniform sampler2D gSampler;															\n\
                                                                                    \n\
uniform vec3 gEyeWorldPos;                                                          \n\
uniform float gMatSpecularIntensity;                                                \n\
uniform float gSpecularPower;                                                       \n\
																					\n\
vec4 CalcLightInternal(BaseLight Light, vec3 LightDirection, vec3 Normal)							\n\
{																									\n\
	vec4 AmbientColor = vec4(Light.Color, 1.0f) * Light.AmbientIntensity;							\n\
	float DiffuseFactor = dot(Normal, -LightDirection);												\n\
																									\n\
	vec4 DiffuseColor = vec4(0, 0, 0, 0);															\n\
	vec4 SpecularColor = vec4(0, 0, 0, 0);															\n\
																									\n\
	if (DiffuseFactor > 0) {																		\n\
		DiffuseColor = vec4(Light.Color, 1.0f) * Light.DiffuseIntensity * DiffuseFactor;			\n\
		vec3 VertexToEye = normalize(gEyeWorldPos - WorldPos0);										\n\
		vec3 LightReflect = normalize(reflect(LightDirection, Normal));								\n\
		float SpecularFactor = dot(VertexToEye, LightReflect);										\n\
		SpecularFactor = pow(SpecularFactor, gSpecularPower);										\n\
		if (SpecularFactor > 0) {																	\n\
			SpecularColor = vec4(Light.Color, 1.0f) *												\n\
				gMatSpecularIntensity * SpecularFactor;												\n\
		}																							\n\
	}																								\n\
																									\n\
	return (AmbientColor + DiffuseColor + SpecularColor);											\n\
}																									\n\
vec4 CalcDirectionalLight(vec3 Normal)																\n\
{																									\n\
    return CalcLightInternal(gDirectionalLight.Base, gDirectionalLight.Direction, Normal);			\n\
}																									\n\
vec4 CalcPointLight(PointLight l, vec3 Normal)														\n\
{																									\n\
	vec3 LightDirection = WorldPos0 - l.Position;													\n\
	float Distance = length(LightDirection);														\n\
	LightDirection = normalize(LightDirection);														\n\
																									\n\
	vec4 Color = CalcLightInternal(l.Base, LightDirection, Normal);									\n\
	float Attenuation = l.Atten.Constant +															\n\
		l.Atten.Linear * Distance +																	\n\
		l.Atten.Exp * Distance * Distance;															\n\
																									\n\
	return Color / Attenuation;																		\n\
}																									\n\
vec4 CalcSpotLight(SpotLight l, vec3 Normal)														\n\
{																									\n\
	vec3 LightToPixel = normalize(WorldPos0 - l.Base.Position);										\n\
	float SpotFactor = dot(LightToPixel, l.Direction);												\n\
																									\n\
	if (SpotFactor > l.Cutoff) {																	\n\
		vec4 Color = CalcPointLight(l.Base, Normal);												\n\
		return Color * (1.0 - (1.0 - SpotFactor) * 1.0 / (1.0 - l.Cutoff));							\n\
	}																								\n\
	else {																							\n\
		return vec4(0, 0, 0, 0);																	\n\
	}																								\n\
}																									\n\
void main()																							\n\
{																									\n\
	vec3 Normal = normalize(Normal0);																\n\
    vec4 TotalLight = CalcDirectionalLight(Normal);													\n\
																									\n\
    for (int i = 0 ; i < gNumPointLights ; i++) {													\n\
        TotalLight += CalcPointLight(gPointLights[i], Normal);										\n\
    }																								\n\
	for (int i = 0 ; i < gNumSpotLights ; i++) {													\n\
		TotalLight += CalcSpotLight(gSpotLights[i], Normal);										\n\
	}																								\n\
																									\n\
    FragColor = texture2D(gSampler, TexCoord0.xy) * TotalLight;										\n\
};";


void Pers(glm::fmat4& m, float zNear, float zFar, float width, float height, float fov){
	const float ar = width / height;
	const float zRange = zNear - zFar;
	const float tanHalfFOV = tanf(ToRadian(fov / 2.0));

	m[0][0] = 1.0f / (tanHalfFOV * ar);	m[1][0] = 0.0f;				 m[2][0] = 0.0f;							m[3][0] = 0.0f;
	m[0][1] = 0.0f;						m[1][1] = 1.0f / tanHalfFOV; m[2][1] = 0.0f;							m[3][1] = 0.0f;
	m[0][2] = 0.0f;						m[1][2] = 0.0f;				 m[2][2] = (-zNear - zFar) / zRange;		m[3][2] = 1.0f;
	m[0][3] = 0.0f;						m[1][3] = 0.0f;				 m[2][3] = 2.0f * zFar * zNear / zRange;	m[3][3] = 0.0f;
	m = glm::transpose(m);
}
void Translate(glm::fmat4& WorldPos, GLfloat x, GLfloat y, GLfloat z){
	WorldPos[0][0] = 1.0f; WorldPos[0][1] = 0.0f; WorldPos[0][2] = 0.0f; WorldPos[0][3] = x;
	WorldPos[1][0] = 0.0f; WorldPos[1][1] = 1.0f; WorldPos[1][2] = 0.0f; WorldPos[1][3] = y;
	WorldPos[2][0] = 0.0f; WorldPos[2][1] = 0.0f; WorldPos[2][2] = 1.0f; WorldPos[2][3] = z;
	WorldPos[3][0] = 0.0f; WorldPos[3][1] = 0.0f; WorldPos[3][2] = 0.0f; WorldPos[3][3] = 1.0f;
	WorldPos = glm::transpose(WorldPos);
}
void RotateY(glm::fmat4& WorldRot, GLfloat y){
	WorldRot[0][0] = cosf(ToRadian(y));	WorldRot[0][1] = 0.0f;	WorldRot[0][2] = -sinf(ToRadian(y));	WorldRot[0][3] = 0.0f;
	WorldRot[1][0] = 0.0f;				WorldRot[1][1] = 1.0f;	WorldRot[1][2] = 0.0f;					WorldRot[1][3] = 0.0f;
	WorldRot[2][0] = sinf(ToRadian(y)); WorldRot[2][1] = 0.0f;  WorldRot[2][2] = cosf(ToRadian(y));		WorldRot[2][3] = 0.0f;
	WorldRot[3][0] = 0.0f;				WorldRot[3][1] = 0.0f;  WorldRot[3][2] = 0.0f;					WorldRot[3][3] = 1.0f;
	WorldRot = glm::transpose(WorldRot);
}
void Rotate(glm::fmat4& WorldRot, GLfloat _x, GLfloat _y, GLfloat _z){
	glm::fmat4 rx, ry, rz;
	const float x = ToRadian(_x);
	const float y = ToRadian(_y);
	const float z = ToRadian(_z);
	rx[0][0] = 1.0f;	rx[0][1] = 0.0f;	rx[0][2] = 0.0f;	rx[0][3] = 0.0f;
	rx[1][0] = 0.0f;	rx[1][1] = cosf(x);	rx[1][2] = -sinf(x); rx[1][3] = 0.0f;
	rx[2][0] = 0.0f;	rx[2][1] = sinf(x);	rx[2][2] = cosf(x);	rx[2][3] = 0.0f;
	rx[3][0] = 0.0f;	rx[3][1] = 0.0f;	rx[3][2] = 0.0f;	rx[3][3] = 1.0f;
	ry[0][0] = cosf(y); ry[0][1] = 0.0f;	ry[0][2] = -sinf(y); ry[0][3] = 0.0f;
	ry[1][0] = 0.0f;	ry[1][1] = 1.0f;	ry[1][2] = 0.0f;	ry[1][3] = 0.0f;
	ry[2][0] = sinf(y); ry[2][1] = 0.0f;	ry[2][2] = cosf(y);	ry[2][3] = 0.0f;
	ry[3][0] = 0.0f;	ry[3][1] = 0.0f;	ry[3][2] = 0.0f;	ry[3][3] = 1.0f;
	rz[0][0] = cosf(z); rz[0][1] = -sinf(z); rz[0][2] = 0.0f;	rz[0][3] = 0.0f;
	rz[1][0] = sinf(z); rz[1][1] = cosf(z); rz[1][2] = 0.0f;	rz[1][3] = 0.0f;
	rz[2][0] = 0.0f;	rz[2][1] = 0.0f;	rz[2][2] = 1.0f;	rz[2][3] = 0.0f;
	rz[3][0] = 0.0f;	rz[3][1] = 0.0f;	rz[3][2] = 0.0f;	rz[3][3] = 1.0f;
	WorldRot = glm::transpose(rz * ry * rx);
}
void Scale(glm::fmat4& WorldScl, GLfloat x, GLfloat y, GLfloat z){
	WorldScl[0][0] = x;		WorldScl[0][1] = 0.0f;	WorldScl[0][2] = 0.0f;	WorldScl[0][3] = 0.0f;
	WorldScl[1][0] = 0.0f;	WorldScl[1][1] = y;		WorldScl[1][2] = 0.0f;	WorldScl[1][3] = 0.0f;
	WorldScl[2][0] = 0.0f;	WorldScl[2][1] = 0.0f;	WorldScl[2][2] = z;		WorldScl[2][3] = 0.0f;
	WorldScl[3][0] = 0.0f;	WorldScl[3][1] = 0.0f;	WorldScl[3][2] = 0.0f;	WorldScl[3][3] = 1.0f;
}
void RenderSceneCB(){
	glClear(GL_COLOR_BUFFER_BIT);
	static float rotate = 0.0f;
	static float m_scale = 0.0f;
	rotate += 0.01f;
	m_scale += 0.001f;


	glm::fmat4 WorldScl;
	glm::fmat4 WorldRot;
	glm::fmat4 WorldPos;
	glm::fmat4 CameraPos;
	glm::fmat4 CameraRot;
	glm::fmat4 WorldPers;
	Scale(WorldScl, 1.0f, 1.0f, 1.0f);
	RotateY(WorldRot, rotate);
	Translate(WorldPos, 0, 0, 5.0f);
	Pers(WorldPers, 1.0f, 100.0f, 1024, 768, 30);

	Translate(CameraPos, -m_camera.Pos.x, -m_camera.Pos.y, -m_camera.Pos.z);
	CameraTransform(m_camera.Target, m_camera.Up, CameraRot);

	*World = glm::transpose(WorldPos * WorldRot * WorldScl);
	*m_transformation = glm::transpose(WorldPers * CameraRot * CameraPos * glm::transpose(*World));
	
	glUniformMatrix4fv(gWVPLocation, 1, GL_TRUE, (const GLfloat*)m_transformation);
	glUniformMatrix4fv(m_WorldMatrixLocation, 1, GL_TRUE, (const GLfloat*)World);

	DirectionalLight Light;
	Light.Color = glm::vec3(1.0f, 1.0f, 1.0f);
	Light.AmbientIntensity = 0.0f;
	Light.Direction = glm::vec3(1.0f, 1.0f, 0.0f);
	Light.DiffuseIntensity = 0.75f;
	glUniform3f(m_dirLightColorLocation, Light.Color.x, Light.Color.y, Light.Color.z);
	glUniform1f(m_dirLightAmbientIntensityLocation, Light.AmbientIntensity);
	glm::fvec3 Direction = Light.Direction;
	Direction = glm::normalize(Direction);
	glUniform3f(m_dirLightLocationDirection, Direction.x, Direction.y, Direction.z);
	glUniform1f(m_dirLightLocationDiffuseIntensity, Light.DiffuseIntensity);

	float ReflectIntensity = 1.0f;
	float ReflectPower = 32.0f;
	glUniform1f(m_matSpecularIntensityLocation, ReflectIntensity);
	glUniform1f(m_matSpecularPowerLocation, ReflectPower);
	glUniform3f(m_eyeWorldPosition, m_camera.Pos.x, m_camera.Pos.y, m_camera.Pos.z);


	/*PointLight pl[3];
	pl[0].DiffuseIntensity = 0.5f;
	pl[0].Color = glm::fvec3(1.0f, 0.0f, 0.0f);
	pl[0].Position = glm::fvec3(sinf(m_scale) * 10, 1.0f, cosf(m_scale) * 10);
	pl[0].Attenuation.Linear = 0.1f;

	pl[1].DiffuseIntensity = 0.5f;
	pl[1].Color = glm::fvec3(0.0f, 1.0f, 0.0f);
	pl[1].Position = glm::fvec3(sinf(m_scale + 2.1f) * 10, 1.0f, cosf(m_scale + 2.1f) * 10);
	pl[1].Attenuation.Linear = 0.1f;

	pl[2].DiffuseIntensity = 0.5f;
	pl[2].Color = glm::fvec3(0.0f, 0.0f, 1.0f);
	pl[2].Position = glm::fvec3(sinf(m_scale + 4.2f) * 10, 1.0f, cosf(m_scale + 4.2f) * 10);
	pl[2].Attenuation.Linear = 0.1f;
	unsigned int NumLights = 3;
	//PointLight* pl;
	glUniform1i(m_numPointLightsLocation, NumLights);

	for (unsigned int i = 0; i < NumLights; i++) {
		glUniform3f(m_pointLightsLocation[i].Color, pl[i].Color.x, pl[i].Color.y, pl[i].Color.z);
		glUniform1f(m_pointLightsLocation[i].AmbientIntensity, pl[i].AmbientIntensity);
		glUniform1f(m_pointLightsLocation[i].DiffuseIntensity, pl[i].DiffuseIntensity);
		glUniform3f(m_pointLightsLocation[i].Position, pl[i].Position.x, pl[i].Position.y, pl[i].Position.z);
		glUniform1f(m_pointLightsLocation[i].Atten.Constant, pl[i].Attenuation.Constant);
		glUniform1f(m_pointLightsLocation[i].Atten.Linear, pl[i].Attenuation.Linear);
		glUniform1f(m_pointLightsLocation[i].Atten.Exp, pl[i].Attenuation.Exp);
	}*/

	SpotLight sl[2];
	sl[0].DiffuseIntensity = 5.0f;
	sl[0].Color = glm::fvec3(1.0f, 1.0f, 0.7f);
	sl[0].Position = glm::fvec3(0.3f, -0.5f, -0.0f);
	sl[0].Direction = glm::fvec3(sinf(m_scale), 0.0f, cosf(m_scale));
	sl[0].Direction = glm::fvec3(0.0f, 0.0f, 1.0f);
	sl[0].Attenuation.Linear = 0.1f;
	sl[0].Cutoff = 10.0f;

	/*sl[1].DiffuseIntensity = 15.0f;
	sl[1].Color = glm::fvec3(0.0f, 1.0f, 1.0f);
	sl[1].Position = m_camera.Pos;
	sl[1].Direction = m_camera.Target;
	sl[1].Attenuation.Linear = 0.1f;
	sl[1].Cutoff = 45.0f;*/
	glUniform1i(m_numSpotLightsLocation, 1);

	for (unsigned int i = 0; i < 2; i++) {
		glUniform3f(m_spotLightsLocation[i].Color, sl[i].Color.x, sl[i].Color.y, sl[i].Color.z);
		glUniform1f(m_spotLightsLocation[i].AmbientIntensity, sl[i].AmbientIntensity);
		glUniform1f(m_spotLightsLocation[i].DiffuseIntensity, sl[i].DiffuseIntensity);
		glUniform3f(m_spotLightsLocation[i].Position, sl[i].Position.x, sl[i].Position.y, sl[i].Position.z);
		glm::fvec3 Direction = sl[i].Direction;
		Direction = glm::normalize(Direction);
		glUniform3f(m_spotLightsLocation[i].Direction, Direction.x, Direction.y, Direction.z);
		glUniform1f(m_spotLightsLocation[i].Cutoff, cosf(ToRadian(sl[i].Cutoff)));
		glUniform1f(m_spotLightsLocation[i].Atten.Constant, sl[i].Attenuation.Constant);
		glUniform1f(m_spotLightsLocation[i].Atten.Linear, sl[i].Attenuation.Linear);
		glUniform1f(m_spotLightsLocation[i].Atten.Exp, sl[i].Attenuation.Exp);
	}

	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), 0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (const GLvoid*)12);
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (const GLvoid*)20);
	pTexture->Bind(GL_TEXTURE0);

	glDrawElements(GL_TRIANGLES, 12, GL_UNSIGNED_INT, 0);

	glDisableVertexAttribArray(0);
	glDisableVertexAttribArray(1);
	glDisableVertexAttribArray(2);

	glutSwapBuffers();
}

static void AddShader(GLuint ShaderProgram, const char* pShaderText, GLenum ShaderType){
	GLuint ShaderObj = glCreateShader(ShaderType);

	if (ShaderObj == 0) {
		fprintf(stderr, "Error creating shader type %d\n", ShaderType);
		exit(0);
	}

	const GLchar* p[1];
	p[0] = pShaderText;
	GLint Lengths[1];
	Lengths[0] = strlen(pShaderText);
	glShaderSource(ShaderObj, 1, p, Lengths);
	glCompileShader(ShaderObj);
	GLint success;
	glGetShaderiv(ShaderObj, GL_COMPILE_STATUS, &success);
	if (!success) {
		GLchar InfoLog[1024];
		glGetShaderInfoLog(ShaderObj, 1024, NULL, InfoLog);
		fprintf(stderr, "Error compiling shader type %d: '%s'\n", ShaderType, InfoLog);
		exit(1);
	}

	glAttachShader(ShaderProgram, ShaderObj);
}

static void CompileShaders(){
	GLuint ShaderProgram = glCreateProgram();

	if (ShaderProgram == 0) {
		fprintf(stderr, "Error creating shader program\n");
		exit(1);
	}

	AddShader(ShaderProgram, pVS, GL_VERTEX_SHADER);
	AddShader(ShaderProgram, pFS, GL_FRAGMENT_SHADER);

	GLint Success = 0;
	GLchar ErrorLog[1024] = { 0 };

	glLinkProgram(ShaderProgram);
	glGetProgramiv(ShaderProgram, GL_LINK_STATUS, &Success);
	if (Success == 0) {
		glGetProgramInfoLog(ShaderProgram, sizeof(ErrorLog), NULL, ErrorLog);
		fprintf(stderr, "Error linking shader program: '%s'\n", ErrorLog);
		exit(1);
	}

	glValidateProgram(ShaderProgram);
	glGetProgramiv(ShaderProgram, GL_VALIDATE_STATUS, &Success);
	if (!Success) {
		glGetProgramInfoLog(ShaderProgram, sizeof(ErrorLog), NULL, ErrorLog);
		fprintf(stderr, "Invalid shader program: '%s'\n", ErrorLog);
		exit(1);
	}

	glUseProgram(ShaderProgram);

	gWVPLocation = glGetUniformLocation(ShaderProgram, "gWVP");
	assert(gWVPLocation != 0xFFFFFFFF);
	m_WorldMatrixLocation = glGetUniformLocation(ShaderProgram, "gWorld");
	assert(m_WorldMatrixLocation != 0xFFFFFFFF);
	gSampler = glGetUniformLocation(ShaderProgram, "gSampler");
	assert(gSampler != 0xFFFFFFFF);
	m_dirLightColorLocation = glGetUniformLocation(ShaderProgram, "gDirectionalLight.Color");
	assert(m_dirLightColorLocation != 0xFFFFFFFF);
	m_dirLightAmbientIntensityLocation = glGetUniformLocation(ShaderProgram, "gDirectionalLight.AmbientIntensity");
	assert(m_dirLightAmbientIntensityLocation != 0xFFFFFFFF);
	m_dirLightLocationDirection = glGetUniformLocation(ShaderProgram, "gDirectionalLight.Direction");
	assert(m_dirLightLocationDirection != 0xFFFFFFFF);
	m_dirLightLocationDiffuseIntensity = glGetUniformLocation(ShaderProgram, "gDirectionalLight.DiffuseIntensity");
	assert(m_dirLightLocationDiffuseIntensity != 0xFFFFFFFF);
	m_eyeWorldPosition = glGetUniformLocation(ShaderProgram, "gEyeWorldPos");
	assert(m_eyeWorldPosition != 0xFFFFFFFF);
	m_matSpecularIntensityLocation = glGetUniformLocation(ShaderProgram, "gMatSpecularIntensity");
	assert(m_matSpecularIntensityLocation != 0xFFFFFFFF);
	m_matSpecularPowerLocation = glGetUniformLocation(ShaderProgram, "gSpecularPower");
	assert(m_matSpecularPowerLocation != 0xFFFFFFFF);
	m_numPointLightsLocation = glGetUniformLocation(ShaderProgram, "gNumPointLights");
	assert(m_numPointLightsLocation != 0xFFFFFFFF);
	for (unsigned int i = 0; i < MAX_POINT_LIGHTS; i++) {
		char Name[128];
		memset(Name, 0, sizeof(Name));
		snprintf(Name, sizeof(Name), "gPointLights[%d].Base.Color", i);
		m_pointLightsLocation[i].Color = glGetUniformLocation(ShaderProgram, Name);
		snprintf(Name, sizeof(Name), "gPointLights[%d].Base.AmbientIntensity", i);
		m_pointLightsLocation[i].AmbientIntensity = glGetUniformLocation(ShaderProgram, Name);
		snprintf(Name, sizeof(Name), "gPointLights[%d].Position", i);
		m_pointLightsLocation[i].Position = glGetUniformLocation(ShaderProgram, Name);
		snprintf(Name, sizeof(Name), "gPointLights[%d].Base.DiffuseIntensity", i);
		m_pointLightsLocation[i].DiffuseIntensity = glGetUniformLocation(ShaderProgram, Name);
		snprintf(Name, sizeof(Name), "gPointLights[%d].Atten.Constant", i);
		m_pointLightsLocation[i].Atten.Constant = glGetUniformLocation(ShaderProgram, Name);
		snprintf(Name, sizeof(Name), "gPointLights[%d].Atten.Linear", i);
		m_pointLightsLocation[i].Atten.Linear = glGetUniformLocation(ShaderProgram, Name);
		snprintf(Name, sizeof(Name), "gPointLights[%d].Atten.Exp", i);
		m_pointLightsLocation[i].Atten.Exp = glGetUniformLocation(ShaderProgram, Name);
	}
	m_numSpotLightsLocation = glGetUniformLocation(ShaderProgram, "gNumSpotLights");
	assert(m_numSpotLightsLocation != 0xFFFFFFFF);
	for (unsigned int i = 0; i < MAX_POINT_LIGHTS; i++) {
		char Name[128];
		memset(Name, 0, sizeof(Name));
		snprintf(Name, sizeof(Name), "gSpotLights[%d].Base.Base.Color", i);
		m_spotLightsLocation[i].Color = glGetUniformLocation(ShaderProgram, Name);
		snprintf(Name, sizeof(Name), "gSpotLights[%d].Base.Base.AmbientIntensity", i);
		m_spotLightsLocation[i].AmbientIntensity = glGetUniformLocation(ShaderProgram, Name);
		snprintf(Name, sizeof(Name), "gSpotLights[%d].Base.Position", i);
		m_spotLightsLocation[i].Position = glGetUniformLocation(ShaderProgram, Name);
		snprintf(Name, sizeof(Name), "gSpotLights[%d].Direction", i);
		m_spotLightsLocation[i].Direction = glGetUniformLocation(ShaderProgram, Name);

		snprintf(Name, sizeof(Name), "gSpotLights[%d].Cutoff", i);
		m_spotLightsLocation[i].Cutoff = glGetUniformLocation(ShaderProgram, Name);
		snprintf(Name, sizeof(Name), "gSpotLights[%d].Base.Base.DiffuseIntensity", i);
		m_spotLightsLocation[i].DiffuseIntensity = glGetUniformLocation(ShaderProgram, Name);
		snprintf(Name, sizeof(Name), "gSpotLights[%d].Base.Atten.Constant", i);
		m_spotLightsLocation[i].Atten.Constant = glGetUniformLocation(ShaderProgram, Name);
		snprintf(Name, sizeof(Name), "gSpotLights[%d].Base.Atten.Linear", i);
		m_spotLightsLocation[i].Atten.Linear = glGetUniformLocation(ShaderProgram, Name);
		snprintf(Name, sizeof(Name), "gSpotLights[%d].Base.Atten.Exp", i);
		m_spotLightsLocation[i].Atten.Exp = glGetUniformLocation(ShaderProgram, Name);
	}
}
int main(int argc, char** argv){
	Magick::InitializeMagick(*argv);
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
	glutInitWindowSize(1024, 768);
	glutInitWindowPosition(100, 100);
	glutCreateWindow("Tutorial 21");
	glutDisplayFunc(RenderSceneCB);
	glutIdleFunc(RenderSceneCB);

	GLenum res = glewInit();
	if(res != GLEW_OK){
		fprintf(stderr, "Error: '%s' \n", glewGetErrorString(res));
		return 1;
	}
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glFrontFace(GL_CW);
	glCullFace(GL_BACK);
	glEnable(GL_CULL_FACE);


	Vertex Vertices[4] = {
		Vertex(glm::fvec3(-1.0f, -1.0f, 0.5773f), glm::fvec2(0.0f, 0.0f)),
		Vertex(glm::fvec3(0.0f, -1.0f, -1.15475), glm::fvec2(0.5f, 0.0f)),
		Vertex(glm::fvec3(1.0f, -1.0f, 0.5773f),  glm::fvec2(1.0f, 0.0f)),
		Vertex(glm::fvec3(0.0f, 1.0f, 0.0f),      glm::fvec2(0.5f, 1.0f))
	};

	unsigned int Indices[] = {0, 3, 1,
							   1, 3, 2,
							   2, 3, 0,
							   1, 2, 0};

	CalcNormals(Indices, 12, Vertices, 4);

	glGenBuffers(1, &VBO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(Vertices), Vertices, GL_STATIC_DRAW);

	glGenBuffers(1, &IBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(Indices), Indices, GL_STATIC_DRAW);


	glm::fvec3 CameraPos(2.0f, 0.0f, 0.0f);
	glm::fvec3 CameraTarget(0.45f, 0.0f, 1.0f);
	glm::fvec3 CameraUp(0.0f, 1.0f, 0.0f);
	SetCamera(CameraPos, CameraTarget, CameraUp);

	CompileShaders();
	glUniform1i(gSampler, 0);

	pTexture = new Texture(GL_TEXTURE_2D, "C:\\test.jpg");

	if (!pTexture->Load()) {
		return 1;
	}
	glutMainLoop();
}