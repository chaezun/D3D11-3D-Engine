#pragma once
#include "ISubsystem.h"

enum RendererOption : uint
{
	RendererOption_AABB = 1U << 0,
	RendererOption_Grid = 1U << 1,
	RendererOption_Physics = 1U << 2,
	RendererOption_SSAO = 1U << 3, // Screen Space Ambient Occlusion
	RendererOption_FXAA = 1U << 4, // Fast Approximate Anti-Aliasing
	RendererOption_Bloom = 1U << 5,
	RendererOption_MotionBlur = 1U << 6,
	RendererOption_Sharpening = 1U << 7,
};

enum class ShaderType : uint
{
	VS_GBUFFER,
	VS_SKINNED_ANIMATION,
	VS_POST_PROCESS,
	PS_TEXTURE,
	PS_DEBUG_NORMAL,
	PS_DEBUG_VELOCITY,
	PS_DEBUG_DEPTH,
	PS_DEBUG_LIGHT,
	PS_DIRECTIONAL_LIGHT,
	PS_POINT_LIGHT,
	PS_SPOT_LIGHT,
	PS_COMPOSITION,
	PS_GAMMA_CORRECTION,
	PS_TONE_MAPPING,
	PS_SSAO,
	PS_UPSAMPLE,
	PS_DOWNSAMPLE,
	PS_GAUSSIAN_BLUR,
	PS_BILATERAL_GAUSSIAN_BLUR,
	PS_LUMA,
	PS_FXAA,
	PS_BLOOM_LUMINANCE,
	PS_BLOOM_DOWNSAMPLE,
	PS_BLOOM_ADDTIVE_BLEND,
	PS_MOTION_BLUR,
	PS_LUMA_SHARPENING,
	VPS_LINEDRAW, //Line Draw를 위한 쉐이더
};

enum class RenderableType : uint
{
	Opaque,
	Camera,
	Light,
	Directional_Light,
	Spot_Light,
	Point_Light,
};

enum class RenderTextureType : uint
{
	GBuffer_Albedo,
	GBuffer_Normal,
	GBuffer_Material,
	GBuffer_Velocity,
	GBuffer_Depth,
	Light_Diffuse,
	Light_Specular,
	SSAO_Raw,
	SSAO_Blur,
	SSAO,
	Composition_HDR,
	Composition_HDR2,
	Composition_LDR,
	Composition_LDR2,
	Composition_HDR_Prev,
	Composition_HDR_Prev2,
};

enum class DebugBufferType : uint
{
	None,
	Albedo,
	Normal,
	Material,
	Velocity,
	Depth,
	Diffuse,
	Specular,
	SSAO,
	Bloom,
};

enum class ToneMappingType : uint
{
	Off,
	ACES,
	Reinhard,
	Uncharted2,
};

class Renderer final : public ISubsystem
{
public:
	Renderer(class Context* context);
	~Renderer();

	const bool Initialize() override;

	auto GetCamera() -> class Camera* { return camera.get(); }
	auto GetFrameResource() -> ID3D11ShaderResourceView*;

	auto GetEditorOffset() const -> const Vector2& { return editor_offset; }
	void SetEditorOffset(const Vector2& offset) { this->editor_offset = offset; }

	auto GetResolution() const -> const Vector2& { return resolution; }
	void SetResolution(const uint& width, const uint& height);

	auto GetDebugBufferType() const -> const DebugBufferType& { return debug_buffer_type; }
	void SetDebugBufferType(const DebugBufferType& type) { this->debug_buffer_type = type; }

	auto GetToneMappingType() const -> const ToneMappingType& { return tone_mapping_type; }
	void SetToneMappingType(const ToneMappingType& type) { this->tone_mapping_type = type; }

	auto IsReverseZ() const -> const bool& { return is_reverse_z; }
	void SetReverseZ(const bool& is_reverse_z) { this->is_reverse_z = is_reverse_z; }

	auto GetBloomIntensity() const -> const float& { return bloom_intensity; }
	void SetBloomIntensity(const float& intensity) { this->bloom_intensity = intensity; }

	auto GetMotionBlurStrength() const -> const float& { return motion_blur_strength; }
	void SetMotionBlurStrength(const float& strength) { this->motion_blur_strength = strength; }

	auto GetSharpStrength() const -> const float& { return sharpen_strength; }
	void SetSharpStrength(const float& strength) { this->sharpen_strength = strength; }

	auto GetSharpClamp() const -> const float& { return sharpen_clamp; }
	void SetSharpClamp(const float& clamp) { this->sharpen_clamp = clamp; }

	void AcquireRenderables(class Scene* scene);
	void SortRenderables(std::vector<class Actor*>* actors);

	void DrawLine
	(
		const Vector3& from,
		const Vector3& to,
		const Color4& from_color = Color4(0.41f, 0.86f, 1.0f, 1.0f),
		const Color4& to_color = Color4(0.41f, 0.86f, 1.0f, 1.0f),
		const bool& is_depth = true
	);

	void DrawBox
	(
	   const BoundBox& box, 
	   const Color4& color = Color4(0.41f, 0.86f, 1.0f, 1.0f),
	   const bool& is_depth = true
	);

	void Render();

	void FlagEnable(const RendererOption& flag)  { renderer_options |= flag; }
	void FlagDisable(const RendererOption& flag) { renderer_options &= ~flag; }
	auto IsOnFlag(const RendererOption& flag) -> const bool { return (renderer_options & flag) > 0U; }

private:
	void CreateRenderTextures();
	void CreateShaders();
	void CreateConstantBuffers();
	void CreateSamplerStates();
	void CreateRasterizerStates();
	void CreateBlendStates();
	void CreateTextures();
	void CreateDepthStencilStates();

	void UpdateGlobalBuffer(const uint& width, const uint& height, const Matrix& transform = Matrix::Identity);
	void UpdateLightBuffer(const std::vector<class Actor*>& actors);
	void UpdateBlurBuffer(const Vector2& direction, const float& sigma);
	void UpdateGlobalSamplers();
	
	auto GetClearDepth() const -> const float { return is_reverse_z ? 0.0f : 1.0f; }
	auto GetRasterizerState(const D3D11_CULL_MODE& cull_mode, const D3D11_FILL_MODE& fill_mode = D3D11_FILL_SOLID) -> const std::shared_ptr<class RasterizerState>&;

private:
	void PassMain();
    void PassGBuffer();
	void PassSSAO();
	void PassLight();
	void PassComposition();
	void PassPostProcess();
	void PassLine(const std::shared_ptr<class ITexture>& out);
	void PassDebug(const std::shared_ptr<class ITexture>& out);

	void PassGammaCorrection(std::shared_ptr<class ITexture>& in, std::shared_ptr<class ITexture>& out);
	void PassToneMapping(std::shared_ptr<class ITexture>& in, std::shared_ptr<class ITexture>& out);
	void PassCopy(std::shared_ptr<class ITexture>& in, std::shared_ptr<class ITexture>& out);
	void PassMotionBlur(std::shared_ptr<class ITexture>& in, std::shared_ptr<class ITexture>& out);
	void PassLumaSharpening(std::shared_ptr<class ITexture>& in, std::shared_ptr<class ITexture>& out);
	void PassUpsample(std::shared_ptr<class ITexture>& in, std::shared_ptr<class ITexture>& out);
	void PassDownsample(std::shared_ptr<class ITexture>& in, std::shared_ptr<class ITexture>& out, const ShaderType& pixel_shader_type = ShaderType::PS_DOWNSAMPLE);
	void PassGaussianBlur(std::shared_ptr<class ITexture>& in, std::shared_ptr<class ITexture>& out, const float& sigma, const float& pixel_stride = 1.0f);
	void PassBilateralGaussianBlur(std::shared_ptr<class ITexture>& in, std::shared_ptr<class ITexture>& out, const float& sigma, const float& pixel_stride = 1.0f);
	void PassFXAA(std::shared_ptr<class ITexture>& in, std::shared_ptr<class ITexture>& out);
	void PassBloom(std::shared_ptr<class ITexture>& in, std::shared_ptr<class ITexture>& out);

private:
	uint renderer_options = 0;
	DebugBufferType debug_buffer_type = DebugBufferType::None;
	ToneMappingType tone_mapping_type = ToneMappingType::ACES;
	bool is_reverse_z = true;

	Matrix post_process_view;
	Matrix post_process_proj;
	Matrix post_process_view_proj;
	Matrix camera_view;
	Matrix camera_proj;
	Matrix camera_view_proj;
	Matrix camera_view_proj_inverse;
	float camera_near = 0.0f;
	float camera_far = 0.0f;
	Vector3 camera_position = Vector3::Zero;
	Vector2 editor_offset = Vector2::Zero;
	Vector2 resolution = Vector2(1280, 720);

	float gamma = 2.2f;
	float ssao_scale = 1.0f;
	float fxaa_sub_pixel = 1.25f;
	float fxaa_edge_threshold = 0.125f;
	float fxaa_edge_threshold_min = 0.0312f;
	float bloom_intensity = 0.005f;
	float exposure = 1.0f;
	float motion_blur_strength = 0.01f;
	float sharpen_strength = 1.0f;
	float sharpen_clamp = 0.35f;

	//Camera
	std::shared_ptr<class Camera> camera;

	std::shared_ptr<class CommandList> command_list;

private:
	std::vector<struct VertexColor> depth_enabled_line_vertices;
	std::vector<struct VertexColor> depth_disabled_line_vertices;
	std::shared_ptr<class VertexBuffer> line_vertex_buffer;
	std::shared_ptr<class VertexBuffer> screen_vertex_buffer;
	std::shared_ptr<class IndexBuffer> screen_index_buffer;

	std::shared_ptr<class Grid> grid;

	std::shared_ptr<class SkyBox> skybox;
private:
	//Textures
	std::shared_ptr<class ITexture> noise_texture;
	std::shared_ptr<class ITexture> white_texture;
	std::shared_ptr<class ITexture> black_texture;
	std::shared_ptr<class ITexture> ibl_lut_texture;

	//Shaders
	std::map<ShaderType, std::shared_ptr<class Shader>> shaders;

	//Constant Buffers
	std::shared_ptr<class ConstantBuffer> global_buffer;
	std::shared_ptr<class ConstantBuffer> light_buffer;
	std::shared_ptr<class ConstantBuffer> blur_buffer;

	//Sampler States
	std::shared_ptr<class SamplerState> compare_depth_sampler;
	std::shared_ptr<class SamplerState> point_clamp_sampler;
	std::shared_ptr<class SamplerState> bilinear_clamp_sampler;
	std::shared_ptr<class SamplerState> bilinear_wrap_sampler;
	std::shared_ptr<class SamplerState> trilinear_clamp_sampler;
	std::shared_ptr<class SamplerState> anisotropic_wrap_sampler;

	//Rasterizer States
	std::shared_ptr<class RasterizerState> cull_back_solid_state;
	std::shared_ptr<class RasterizerState> cull_front_solid_state;
	std::shared_ptr<class RasterizerState> cull_none_solid_state;
	std::shared_ptr<class RasterizerState> cull_back_wireframe_state;
	std::shared_ptr<class RasterizerState> cull_front_wireframe_state;
	std::shared_ptr<class RasterizerState> cull_none_wireframe_state;

	//Blend State
	std::shared_ptr<class BlendState> blend_enabled_state;
	std::shared_ptr<class BlendState> blend_disabled_state;
	std::shared_ptr<class BlendState> blend_color_add_state;
	std::shared_ptr<class BlendState> blend_bloom_state;


	//Depth Stencil States
	std::shared_ptr<class DepthStencilState> depth_stencil_enabled_state;
	std::shared_ptr<class DepthStencilState> depth_stencil_disabled_state;

	//Render Textures
	std::map<RenderTextureType, std::shared_ptr<class ITexture>> render_textures;
	std::vector<std::shared_ptr<class ITexture>> bloom_textures;

	//Renderables
	std::unordered_map<RenderableType, std::vector<class Actor*>> renderables;
};

//RTT - Render To Texture
//화면에 그려질 놈을 저장하는 기법 (백버퍼는 그려질 놈을 화면에 띄우는 역할을 하는데 백버퍼와는 의미가 다름)