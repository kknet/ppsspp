#pragma once
// Copyright (c) 2012- PPSSPP Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0 or later versions.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official git repository and contact information can be found at
// https://github.com/hrydgard/ppsspp and http://www.ppsspp.org/.

#pragma once

#include <map>

#include "base/basictypes.h"
#include "Globals.h"
#include "GPU/Common/ShaderCommon.h"
#include "GPU/Common/ShaderId.h"
#include "GPU/Vulkan/VertexShaderGeneratorVulkan.h"
#include "GPU/Vulkan/FragmentShaderGeneratorVulkan.h"
#include "GPU/Vulkan/VulkanUtil.h"
#include "math/lin/matrix4x4.h"

void ConvertProjMatrixToVulkan(Matrix4x4 & in);

// Pretty much full. Will need more bits for more fine grained dirty tracking for lights.
enum {
	DIRTY_PROJMATRIX = (1 << 0),
	DIRTY_PROJTHROUGHMATRIX = (1 << 1),
	DIRTY_FOGCOLOR = (1 << 2),
	DIRTY_FOGCOEF = (1 << 3),
	DIRTY_TEXENV = (1 << 4),
	DIRTY_ALPHACOLORREF = (1 << 5),
	DIRTY_STENCILREPLACEVALUE = (1 << 6),

	DIRTY_ALPHACOLORMASK = (1 << 7),
	DIRTY_LIGHT0 = (1 << 8),
	DIRTY_LIGHT1 = (1 << 9),
	DIRTY_LIGHT2 = (1 << 10),
	DIRTY_LIGHT3 = (1 << 11),

	DIRTY_MATDIFFUSE = (1 << 12),
	DIRTY_MATSPECULAR = (1 << 13),
	DIRTY_MATEMISSIVE = (1 << 14),
	DIRTY_AMBIENT = (1 << 15),
	DIRTY_MATAMBIENTALPHA = (1 << 16),
	DIRTY_SHADERBLEND = (1 << 17),  // Used only for in-shader blending.
	DIRTY_UVSCALEOFFSET = (1 << 18),  // this will be dirtied ALL THE TIME... maybe we'll need to do "last value with this shader compares"
	DIRTY_TEXCLAMP = (1 << 19),

	DIRTY_DEPTHRANGE = (1 << 20),

	DIRTY_WORLDMATRIX = (1 << 21),
	DIRTY_VIEWMATRIX = (1 << 22),  // Maybe we'll fold this into projmatrix eventually
	DIRTY_TEXMATRIX = (1 << 23),
	DIRTY_BONEMATRIX0 = (1 << 24),
	DIRTY_BONEMATRIX1 = (1 << 25),
	DIRTY_BONEMATRIX2 = (1 << 26),
	DIRTY_BONEMATRIX3 = (1 << 27),
	DIRTY_BONEMATRIX4 = (1 << 28),
	DIRTY_BONEMATRIX5 = (1 << 29),
	DIRTY_BONEMATRIX6 = (1 << 30),
	DIRTY_BONEMATRIX7 = (1 << 31),

	DIRTY_ALL = 0xFFFFFFFF
};

struct UB_VS_TransformCommon {
	float proj[16];
	float view[16];
	float world[16];
	float tex[16];  // not that common, may want to break out
	float uvScaleOffset[4];
	float depthRange[4];
	float fogCoef[4];
	float matAmbient[4];
};

static const char *ub_baseStr =
R"(matrix4x4 proj;
  matrix4x4 view;
  matrix4x4 world;
  matrix4x4 tex;
	vec4 uvScaleOffset;
  vec4 depthRange;
  vec2 fogCoef;
	vec4 matAmbient;
	// Blend function replacement
	vec3 u_blendFixA;
	vec3 u_blendFixB;

		// Texture clamp emulation
	vec4 u_texclamp;
	vec2 u_texclampoff;

		// Alpha/Color test emulation
	vec4 u_alphacolorref;
	ivec4 u_alphacolormask;

		// Stencil replacement
	float u_stencilReplaceValue;
	vec3 u_texenv;

	vec3 u_fogcolor;
)";

struct UB_VS_Lights {
	float ambientColor[4];
	float materialDiffuse[4];
	float materialSpecular[4];
	float materialEmissive[4];
	float lpos[4][4];
	float ldir[4][4];
	float latt[4][4];
	float lightAngle[4][4];   // TODO: Merge with lightSpotCoef, use .xy
	float lightSpotCoef[4][4];
	float lightAmbient[4][4];
	float lightDiffuse[4][4];
	float lightSpecular[4][4];
};

static const char *ub_vs_lightsStr =
R"(vec3 ambientColor;
	vec3 materialDiffuse;
  vec4 materialSpecular;
  vec3 materialEmissive;
  vec3 lpos[4];
  vec3 ldir[4];
  vec3 latt[4];
	float lightAngle[4];
  float lightSpotCoef[4];
	vec3 lightAmbient[4];
	vec3 lightDiffuse[4];
	vec3 lightSpecular[4];
)";

struct UB_VS_Bones {
	float bones[8][16];
};

static const char *ub_vs_bonesStr =
R"(matrix4x4 m[8];
)";

// Let's not bother splitting this, we'll just upload the lot for every draw call.
struct UB_FS_All {
	float fogColor[4];
	float texEnvColor[4];
	float alphaColorRef[4];
	float colorTestMask[4];
	float stencilReplace[4];  // only first float used
	float blendFixA[4];
	float blendFixB[4];
	float texClamp[4];
	float texClampOffset[4];
};

static const char *ub_fs_allStr =
R"(
  vec3 fogColor;
	vec3 texenv;
	vec4 alphaColorRef;
	vec3 colorTestMask;
	float stencilReplace;
	vec3 blendFixA;
	vec3 blendFixB;
	vec4 texClamp;
	vec2 texClampOffset;
)";

class VulkanFragmentShader {
public:
	VulkanFragmentShader(VulkanContext *vulkan, ShaderID id, const char *code, bool useHWTransform);
	~VulkanFragmentShader();

	const std::string &source() const { return source_; }

	bool Failed() const { return failed_; }
	bool UseHWTransform() const { return useHWTransform_; }

	std::string GetShaderString(DebugShaderStringType type) const;

protected:	
	VkShaderModule module_;

	VulkanContext *vulkan_;
	std::string source_;
	bool failed_;
	bool useHWTransform_;
	ShaderID id_;
};

class VulkanVertexShader {
public:
	VulkanVertexShader(VulkanContext *vulkan, ShaderID id, const char *code, int vertType, bool useHWTransform);
	~VulkanVertexShader();

	const std::string &source() const { return source_; }

	bool Failed() const { return failed_; }
	bool UseHWTransform() const { return useHWTransform_; }

	std::string GetShaderString(DebugShaderStringType type) const;

protected:
	VkShaderModule module_;

	VulkanContext *vulkan_;
	std::string source_;
	bool failed_;
	bool useHWTransform_;
	ShaderID id_;
};

class ShaderManagerVulkan {
public:
	ShaderManagerVulkan(VulkanContext *vulkan);
	~ShaderManagerVulkan();

	void ClearCache(bool deleteThem);  // TODO: deleteThem currently not respected
	void GetShaders(int prim, u32 vertType, VulkanVertexShader **vshader, VulkanFragmentShader **fshader);

	void DirtyShader();
	void DirtyUniform(u32 what) {
		globalDirty_ |= what;
	}
	void DirtyLastShader();

	int GetNumVertexShaders() const { return (int)vsCache_.size(); }
	int GetNumFragmentShaders() const { return (int)fsCache_.size(); }

	std::vector<std::string> DebugGetShaderIDs(DebugShaderType type);
	std::string DebugGetShaderString(std::string id, DebugShaderType type, DebugShaderStringType stringType);

private:
	void PSUpdateUniforms(int dirtyUniforms);
	void VSUpdateUniforms(int dirtyUniforms);

	void Clear();

	VulkanContext *vulkan_;

	u32 globalDirty_;
	char *codeBuffer_;

	// Uniform block scratchpad. These (the relevant ones) are copied to the current pushbuffer at draw time.
	UB_VS_TransformCommon ub_transformCommon;
	UB_VS_Lights ub_lights;
	UB_VS_Bones ub_bones;
	UB_FS_All ub_fragment;

	typedef std::map<ShaderID, VulkanFragmentShader *> FSCache;
	FSCache fsCache_;

	typedef std::map<ShaderID, VulkanVertexShader *> VSCache;
	VSCache vsCache_;

	VulkanFragmentShader *lastFShader_;
	VulkanVertexShader *lastVShader_;

	ShaderID lastFSID_;
	ShaderID lastVSID_;
};
