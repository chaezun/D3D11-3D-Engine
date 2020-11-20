#pragma once
#include "D3D11Framework.h"

//Config의 사용용도 : 렌더링에 필요한 데이터를 미리 정의하고 사용.

static const std::string NOT_ASSIGNED_STR			= "N/A";
static const std::string MODEL_BIN_EXTENSION		= ".model";
static const std::string MESH_BIN_EXTENSION			= ".mesh";
static const std::string TEXTURE_BIN_EXTENSION		= ".texture";
static const std::string MATERIAL_BIN_EXTENSION		= ".material";

enum class ShaderScope : uint
{
    Unknown, 
	Global, //Constant Buffer  
	VS,     //Vertex Shader
	PS      //Pixel Shader
};

//ConstantBuffer에 넘겨줄 구조체 변수들
//=================================================================================================================

//World Conversion
struct GLOBAL_DATA final
{
	Matrix world_view_proj;
	Matrix view;
	Matrix proj;
	Matrix view_proj;
	Matrix view_proj_inverse;
	Matrix post_process_proj;
	Matrix post_process_view_proj;
	float camera_near;
	float camera_far;
	Vector2 resolution;
	Vector3 camera_position;
	float directional_intensity;
	float gamma;
	float ssao_scale;
	float fxaa_sub_pixel;
	float fxaa_edge_threshold;
	float fxaa_edge_threshold_min;
	float bloom_intensity;
	float tone_mapping;
	float exposure;
	float delta_time;
	float motion_blur_strength;
	float sharpen_strength;
	float sharpen_clamp;
};

//Transform
struct TRANSFORM_DATA final
{
    Matrix world;
	Matrix wvp_current;
	Matrix wvp_previous;
};

//Material
struct MATERIAL_DATA final
{
	Color4 albedo_color;
	Vector2 uv_tiling;
	Vector2 uv_offset;
	float roughness_coef;
	float metallic_coef;
	float normal_coef;
	float height_coef;
	float shading_mode;
	float padding[3];
};

//BLUR
struct BLUR_DATA final
{
	Vector2 direction;
	float sigma;
	float padding;
};

//Light
#define CASCADE__COUNT 4
#define MAX_LIGHT_COUNT 100
struct LIGHT_DATA final
{
	Matrix light_view_projection[MAX_LIGHT_COUNT][CASCADE__COUNT];
	Vector4 intensity_range_angle_bias[MAX_LIGHT_COUNT];
	Vector4 normal_bias_shadow_volumetric_contact[MAX_LIGHT_COUNT];
	Color4 colors[MAX_LIGHT_COUNT];
	Vector4 positions[MAX_LIGHT_COUNT];
	Vector4 directions[MAX_LIGHT_COUNT];
	float light_count;
	float padding[3];
};

//Animation
#define MAX_BONE_COUNT 100
struct ANIMATION_DATA final
{
	Matrix skinned_transforms[MAX_BONE_COUNT];
};
//=================================================================================================================