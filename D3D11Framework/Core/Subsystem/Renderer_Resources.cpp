#include "D3D11Framework.h"
#include "Renderer.h"

void Renderer::CreateRenderTextures()
{
	auto width = static_cast<uint>(resolution.x);
	auto height = static_cast<uint>(resolution.y);

	if ((width / 4) == 0 || (height / 4) == 0)
	{
		LOG_WARNING_F("%dx%d is an invalid resolution", width, height);
		return;
	}

	Geometry<VertexTexture> screen_quad;
	Geometry_Generator::CreateScreenQuad(screen_quad, width, height);

	screen_vertex_buffer = std::make_shared<VertexBuffer>(context);
	screen_vertex_buffer->Create(screen_quad.GetVertices());

	screen_index_buffer = std::make_shared<IndexBuffer>(context);
	screen_index_buffer->Create(screen_quad.GetIndices());

	//GBuffer
	render_textures[RenderTextureType::GBuffer_Albedo] = std::make_shared<Texture2D>(context, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 1, TEXTURE_BIND_RTV | TEXTURE_BIND_SRV);
	render_textures[RenderTextureType::GBuffer_Normal] = std::make_shared<Texture2D>(context, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, 1, TEXTURE_BIND_RTV | TEXTURE_BIND_SRV);
	render_textures[RenderTextureType::GBuffer_Material] = std::make_shared<Texture2D>(context, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 1, TEXTURE_BIND_RTV | TEXTURE_BIND_SRV);
	render_textures[RenderTextureType::GBuffer_Velocity] = std::make_shared<Texture2D>(context, width, height, DXGI_FORMAT_R16G16_FLOAT, 1, TEXTURE_BIND_RTV | TEXTURE_BIND_SRV);
	render_textures[RenderTextureType::GBuffer_Depth] = std::make_shared<Texture2D>(context, width, height, DXGI_FORMAT_D32_FLOAT, 1, TEXTURE_BIND_DSV | TEXTURE_BIND_SRV);
	//SSAO
	render_textures[RenderTextureType::SSAO_Raw] = std::make_shared<Texture2D>(context, static_cast<uint>(width * ssao_scale), static_cast<uint>(height * ssao_scale), DXGI_FORMAT_R8_UNORM, 1, TEXTURE_BIND_RTV | TEXTURE_BIND_SRV);
	render_textures[RenderTextureType::SSAO_Blur] = std::make_shared<Texture2D>(context, static_cast<uint>(width * ssao_scale), static_cast<uint>(height * ssao_scale), DXGI_FORMAT_R8_UNORM, 1, TEXTURE_BIND_RTV | TEXTURE_BIND_SRV);
	render_textures[RenderTextureType::SSAO] = std::make_shared<Texture2D>(context, width, height, DXGI_FORMAT_R8_UNORM, 1, TEXTURE_BIND_RTV | TEXTURE_BIND_SRV);

	//Light
	render_textures[RenderTextureType::Light_Diffuse] = std::make_shared<Texture2D>(context, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, 1, TEXTURE_BIND_RTV | TEXTURE_BIND_SRV);
	render_textures[RenderTextureType::Light_Specular] = std::make_shared<Texture2D>(context, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, 1, TEXTURE_BIND_RTV | TEXTURE_BIND_SRV);

	//Composition
	render_textures[RenderTextureType::Composition_HDR] = std::make_shared<Texture2D>(context, width, height, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, TEXTURE_BIND_RTV | TEXTURE_BIND_SRV);
	render_textures[RenderTextureType::Composition_HDR2] = std::make_shared<Texture2D>(context, width, height, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, TEXTURE_BIND_RTV | TEXTURE_BIND_SRV);
	render_textures[RenderTextureType::Composition_HDR_Prev] = std::make_shared<Texture2D>(context, width, height, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, TEXTURE_BIND_RTV | TEXTURE_BIND_SRV);
	render_textures[RenderTextureType::Composition_HDR_Prev2] = std::make_shared<Texture2D>(context, width, height, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, TEXTURE_BIND_RTV | TEXTURE_BIND_SRV);
	render_textures[RenderTextureType::Composition_LDR] = std::make_shared<Texture2D>(context, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, 1, TEXTURE_BIND_RTV | TEXTURE_BIND_SRV);
	render_textures[RenderTextureType::Composition_LDR2] = std::make_shared<Texture2D>(context, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, 1, TEXTURE_BIND_RTV | TEXTURE_BIND_SRV);

	//Bloom
	bloom_textures.clear();
	bloom_textures.shrink_to_fit();
	bloom_textures.emplace_back(std::make_shared<Texture2D>(context, width / 2, height / 2, DXGI_FORMAT_R16G16B16A16_FLOAT, 1, TEXTURE_BIND_RTV | TEXTURE_BIND_SRV));
	while (bloom_textures.back()->GetWidth() > 16 && bloom_textures.back()->GetHeight() > 16)
	{
		bloom_textures.emplace_back
		(
			std::make_shared<Texture2D>
			(
				context,
				bloom_textures.back()->GetWidth() / 2,
				bloom_textures.back()->GetHeight() / 2,
				DXGI_FORMAT_R16G16B16A16_FLOAT,
				1,
				TEXTURE_BIND_RTV | TEXTURE_BIND_SRV
				)
		);
	}
}

void Renderer::CreateShaders()
{
	const auto shader_directory = context->GetSubsystem<ResourceManager>()->GetAssetDirectory(AssetType::Shader);

	//Vertex Shader
	auto vs_gbuffer = std::make_shared<Shader>(context);
	vs_gbuffer->AddShader<VertexShader>(shader_directory + "GBuffer.hlsl");
	shaders[ShaderType::VS_GBUFFER] = vs_gbuffer;

	//Animation Shader
	auto vs_skinned_animation = std::make_shared<Shader>(context);
	vs_skinned_animation->AddDefine("SKINNED_ANIMATION");
	vs_skinned_animation->AddShader<VertexShader>(shader_directory + "GBuffer.hlsl");
	shaders[ShaderType::VS_SKINNED_ANIMATION] = vs_skinned_animation;

	auto vs_post_process = std::make_shared<Shader>(context);
	vs_post_process->AddShader<VertexShader>(shader_directory + "PostProcess.hlsl");
	shaders[ShaderType::VS_POST_PROCESS] = vs_post_process;

	//Pixel Shader
	auto ps_texture = std::make_shared<Shader>(context);
	ps_texture->AddDefine("TEXTURE");
	ps_texture->AddShader<PixelShader>(shader_directory + "PostProcess.hlsl");
	shaders[ShaderType::PS_TEXTURE] = ps_texture;

	auto ps_debug_normal = std::make_shared<Shader>(context);
	ps_debug_normal->AddDefine("DEBUG_NORMAL");
	ps_debug_normal->AddShader<PixelShader>(shader_directory + "PostProcess.hlsl");
	shaders[ShaderType::PS_DEBUG_NORMAL] = ps_debug_normal;

	auto ps_debug_velocity = std::make_shared<Shader>(context);
	ps_debug_velocity->AddDefine("DEBUG_VELOCITY");
	ps_debug_velocity->AddShader<PixelShader>(shader_directory + "PostProcess.hlsl");
	shaders[ShaderType::PS_DEBUG_VELOCITY] = ps_debug_velocity;

	auto ps_debug_depth = std::make_shared<Shader>(context);
	ps_debug_depth->AddDefine("DEBUG_DEPTH");
	ps_debug_depth->AddShader<PixelShader>(shader_directory + "PostProcess.hlsl");
	shaders[ShaderType::PS_DEBUG_DEPTH] = ps_debug_depth;

	auto ps_debug_light = std::make_shared<Shader>(context);
	ps_debug_light->AddDefine("DEBUG_LIGHT");
	ps_debug_light->AddShader<PixelShader>(shader_directory + "PostProcess.hlsl");
	shaders[ShaderType::PS_DEBUG_LIGHT] = ps_debug_light;

	//Light
	//Directional Light
	auto ps_directional_light = std::make_shared<Shader>(context);
	ps_directional_light->AddDefine("DIRECTIONAL");
	ps_directional_light->AddShader<PixelShader>(shader_directory + "Light.hlsl");
	shaders[ShaderType::PS_DIRECTIONAL_LIGHT] = ps_directional_light;

	//Point Light
	auto ps_point_light = std::make_shared<Shader>(context);
	ps_point_light->AddDefine("POINT");
	ps_point_light->AddShader<PixelShader>(shader_directory + "Light.hlsl");
	shaders[ShaderType::PS_POINT_LIGHT] = ps_point_light;

	//Spot Light
	auto ps_spot_light = std::make_shared<Shader>(context);
	ps_spot_light->AddDefine("SPOT");
	ps_spot_light->AddShader<PixelShader>(shader_directory + "Light.hlsl");
	shaders[ShaderType::PS_SPOT_LIGHT] = ps_spot_light;

	auto ps_composition = std::make_shared<Shader>(context);
	ps_composition->AddShader<PixelShader>(shader_directory + "Composition.hlsl");
	shaders[ShaderType::PS_COMPOSITION] = ps_composition;

	auto ps_gamma_correction = std::make_shared<Shader>(context);
	ps_gamma_correction->AddDefine("GAMMA_CORRECTION");
	ps_gamma_correction->AddShader<PixelShader>(shader_directory + "PostProcess.hlsl");
	shaders[ShaderType::PS_GAMMA_CORRECTION] = ps_gamma_correction;

	auto ps_upsample = std::make_shared<Shader>(context);
	ps_upsample->AddDefine("UPSAMPLE");
	ps_upsample->AddShader<PixelShader>(shader_directory + "PostProcess.hlsl");
	shaders[ShaderType::PS_UPSAMPLE] = ps_upsample;

	auto ps_downsample = std::make_shared<Shader>(context);
	ps_downsample->AddDefine("DOWNSAMPLE");
	ps_downsample->AddShader<PixelShader>(shader_directory + "PostProcess.hlsl");
	shaders[ShaderType::PS_DOWNSAMPLE] = ps_downsample;

	auto ps_gaussian_blur = std::make_shared<Shader>(context);
	ps_gaussian_blur->AddDefine("GAUSSIAN_BLUR");
	ps_gaussian_blur->AddShader<PixelShader>(shader_directory + "PostProcess.hlsl");
	shaders[ShaderType::PS_GAUSSIAN_BLUR] = ps_gaussian_blur;

	auto ps_bilateral_gaussian_blur = std::make_shared<Shader>(context);
	ps_bilateral_gaussian_blur->AddDefine("BILATERAL_GAUSSIAN_BLUR");
	ps_bilateral_gaussian_blur->AddShader<PixelShader>(shader_directory + "PostProcess.hlsl");
	shaders[ShaderType::PS_BILATERAL_GAUSSIAN_BLUR] = ps_bilateral_gaussian_blur;

	auto ps_luma = std::make_shared<Shader>(context);
	ps_luma->AddDefine("LUMA");
	ps_luma->AddShader<PixelShader>(shader_directory + "PostProcess.hlsl");
	shaders[ShaderType::PS_LUMA] = ps_luma;

	auto ps_fxaa = std::make_shared<Shader>(context);
	ps_fxaa->AddDefine("FXAA");
	ps_fxaa->AddShader<PixelShader>(shader_directory + "PostProcess.hlsl");
	shaders[ShaderType::PS_FXAA] = ps_fxaa;

	auto ps_bloom_luminance = std::make_shared<Shader>(context);
	ps_bloom_luminance->AddDefine("BLOOM_LUMINANCE");
	ps_bloom_luminance->AddShader<PixelShader>(shader_directory + "PostProcess.hlsl");
	shaders[ShaderType::PS_BLOOM_LUMINANCE] = ps_bloom_luminance;

	auto ps_bloom_downsample = std::make_shared<Shader>(context);
	ps_bloom_downsample->AddDefine("BLOOM_DOWNSAMPLE");
	ps_bloom_downsample->AddShader<PixelShader>(shader_directory + "PostProcess.hlsl");
	shaders[ShaderType::PS_BLOOM_DOWNSAMPLE] = ps_bloom_downsample;

	auto ps_bloom_addtive_blend = std::make_shared<Shader>(context);
	ps_bloom_addtive_blend->AddDefine("BLOOM_ADDTIVE_BLEND");
	ps_bloom_addtive_blend->AddShader<PixelShader>(shader_directory + "PostProcess.hlsl");
	shaders[ShaderType::PS_BLOOM_ADDTIVE_BLEND] = ps_bloom_addtive_blend;

	auto ps_motion_blur = std::make_shared<Shader>(context);
	ps_motion_blur->AddDefine("MOTION_BLUR");
	ps_motion_blur->AddShader<PixelShader>(shader_directory + "PostProcess.hlsl");
	shaders[ShaderType::PS_MOTION_BLUR] = ps_motion_blur;

	auto ps_tone_mapping = std::make_shared<Shader>(context);
	ps_tone_mapping->AddDefine("TONE_MAPPING");
	ps_tone_mapping->AddShader<PixelShader>(shader_directory + "PostProcess.hlsl");
	shaders[ShaderType::PS_TONE_MAPPING] = ps_tone_mapping;

	auto ps_luma_sharpening = std::make_shared<Shader>(context);
	ps_luma_sharpening->AddDefine("LUMA_SHARPENING");
	ps_luma_sharpening->AddShader<PixelShader>(shader_directory + "PostProcess.hlsl");
	shaders[ShaderType::PS_LUMA_SHARPENING] = ps_luma_sharpening;

	auto ps_ssao = std::make_shared<Shader>(context);
	ps_ssao->AddShader<PixelShader>(shader_directory + "SSAO.hlsl");
	shaders[ShaderType::PS_SSAO] = ps_ssao;

	//Vertex Pixel Shader(Line Draw¿¡ »ç¿ë)
	auto vps_color = std::make_shared<Shader>(context);
	vps_color->AddShader<VertexShader>(shader_directory + "LineDraw.hlsl");
	vps_color->AddShader<PixelShader>(shader_directory + "LineDraw.hlsl");
	shaders[ShaderType::VPS_LINEDRAW] = vps_color;
}

void Renderer::CreateConstantBuffers()
{
	global_buffer = std::make_shared<ConstantBuffer>(context);
	global_buffer->Create<GLOBAL_DATA>();

	blur_buffer = std::make_shared<ConstantBuffer>(context);
	blur_buffer->Create<BLUR_DATA>();

	light_buffer = std::make_shared<ConstantBuffer>(context);
	light_buffer->Create<LIGHT_DATA>();
}

void Renderer::CreateSamplerStates()
{
	compare_depth_sampler = std::make_shared<SamplerState>(context);
	compare_depth_sampler->Create
	(
		D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT,
		D3D11_TEXTURE_ADDRESS_CLAMP,
		D3D11_COMPARISON_LESS
	);

	point_clamp_sampler = std::make_shared<SamplerState>(context);
	point_clamp_sampler->Create
	(
		D3D11_FILTER_MIN_MAG_MIP_POINT,
		D3D11_TEXTURE_ADDRESS_CLAMP,
		D3D11_COMPARISON_ALWAYS
	);

	bilinear_clamp_sampler = std::make_shared<SamplerState>(context);
	bilinear_clamp_sampler->Create
	(
		D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT,
		D3D11_TEXTURE_ADDRESS_CLAMP,
		D3D11_COMPARISON_ALWAYS
	);

	bilinear_wrap_sampler = std::make_shared<SamplerState>(context);
	bilinear_wrap_sampler->Create
	(
		D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT,
		D3D11_TEXTURE_ADDRESS_WRAP,
		D3D11_COMPARISON_ALWAYS
	);

	trilinear_clamp_sampler = std::make_shared<SamplerState>(context);
	trilinear_clamp_sampler->Create
	(
		D3D11_FILTER_MIN_MAG_MIP_LINEAR,
		D3D11_TEXTURE_ADDRESS_CLAMP,
		D3D11_COMPARISON_ALWAYS
	);

	anisotropic_wrap_sampler = std::make_shared<SamplerState>(context);
	anisotropic_wrap_sampler->Create
	(
		D3D11_FILTER_ANISOTROPIC,
		D3D11_TEXTURE_ADDRESS_WRAP,
		D3D11_COMPARISON_ALWAYS
	);
}

void Renderer::CreateRasterizerStates()
{
	cull_back_solid_state = std::make_shared<RasterizerState>(context);
	cull_back_solid_state->Create(D3D11_CULL_BACK, D3D11_FILL_SOLID, true, false, false, false);

	cull_front_solid_state = std::make_shared<RasterizerState>(context);
	cull_front_solid_state->Create(D3D11_CULL_FRONT, D3D11_FILL_SOLID, true, false, false, false);

	cull_none_solid_state = std::make_shared<RasterizerState>(context);
	cull_none_solid_state->Create(D3D11_CULL_NONE, D3D11_FILL_SOLID, true, false, false, false);

	cull_back_wireframe_state = std::make_shared<RasterizerState>(context);
	cull_back_wireframe_state->Create(D3D11_CULL_BACK, D3D11_FILL_WIREFRAME, true, false, false, true);

	cull_front_wireframe_state = std::make_shared<RasterizerState>(context);
	cull_front_wireframe_state->Create(D3D11_CULL_FRONT, D3D11_FILL_WIREFRAME, true, false, false, true);

	cull_none_wireframe_state = std::make_shared<RasterizerState>(context);
	cull_none_wireframe_state->Create(D3D11_CULL_NONE, D3D11_FILL_WIREFRAME, true, false, false, true);

}

void Renderer::CreateBlendStates()
{
	blend_enabled_state = std::make_shared<BlendState>(context);
	blend_enabled_state->Create(true);

	blend_disabled_state = std::make_shared<BlendState>(context);
	blend_disabled_state->Create(false);

	blend_color_add_state = std::make_shared<BlendState>(context);
	blend_color_add_state->Create(true, D3D11_BLEND_ONE, D3D11_BLEND_ONE);

	blend_bloom_state = std::make_shared<BlendState>(context);
	blend_bloom_state->Create(true, D3D11_BLEND_ONE, D3D11_BLEND_ONE, D3D11_BLEND_OP_ADD, D3D11_BLEND_ONE, D3D11_BLEND_ONE, D3D11_BLEND_OP_ADD, 0.5f);
}

void Renderer::CreateTextures()
{
	const auto directory = context->GetSubsystem<ResourceManager>()->GetAssetDirectory(AssetType::Texture);

	noise_texture = std::make_shared<Texture2D>(context, false);
	noise_texture->LoadFromFile(directory + "noise.jpg");

	white_texture = std::make_shared<Texture2D>(context, false);
	white_texture->LoadFromFile(directory + "white.png");

	black_texture = std::make_shared<Texture2D>(context, false);
	black_texture->LoadFromFile(directory + "black.png");

	ibl_lut_texture = std::make_shared<Texture2D>(context, false);
	ibl_lut_texture->LoadFromFile(directory + "ibl_brdf_lut.png");
}

void Renderer::CreateDepthStencilStates()
{
	depth_stencil_enabled_state = std::make_shared<DepthStencilState>(context);
	depth_stencil_enabled_state->Create(true, is_reverse_z ? D3D11_COMPARISON_GREATER_EQUAL : D3D11_COMPARISON_LESS_EQUAL);

	depth_stencil_disabled_state = std::make_shared<DepthStencilState>(context);
	depth_stencil_disabled_state->Create(false, is_reverse_z ? D3D11_COMPARISON_GREATER_EQUAL : D3D11_COMPARISON_LESS_EQUAL);
}