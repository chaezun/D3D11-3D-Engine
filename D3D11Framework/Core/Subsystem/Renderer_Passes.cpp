#include "D3D11Framework.h"
#include "Renderer.h"
#include "Core/D3D11Graphics/CommandList.h"
#include "Gizmo/Grid.h"
#include "Scene/Scene.h"
#include "Scene/Actor.h"
#include "Scene/Component/Transform.h"
#include "Scene/Component/Renderable.h"
#include "Scene/Component/Camera.h"
#include "Scene/Component/Light.h"
#include "Scene/Component/Terrain.h"
#include "Scene/Component/SkyBox.h"
#include "Scene/Component/Animator.h"

static const float GIZMO_MIN_SIZE = 0.1f;
static const float GIZMO_MAX_SIZE = 5.0f;

void Renderer::PassMain()
{
	command_list->Begin("PassMain");
	{
		UpdateGlobalSamplers();

		PassGBuffer();
		PassSSAO();
		PassLight();
		PassComposition();
		PassPostProcess();
		PassLine(render_textures[RenderTextureType::Composition_LDR]);
		PassDebug(render_textures[RenderTextureType::Composition_LDR]);
	}
	command_list->End();
	command_list->Submit();
}

void Renderer::PassGBuffer()
{
	command_list->Begin("PassMain");
	{
		auto& albedo_texture = render_textures[RenderTextureType::GBuffer_Albedo];
		auto& normal_texture = render_textures[RenderTextureType::GBuffer_Normal];
		auto& material_texture = render_textures[RenderTextureType::GBuffer_Material];
		auto& velocity_texture = render_textures[RenderTextureType::GBuffer_Velocity];
		auto& depth_texture = render_textures[RenderTextureType::GBuffer_Depth];
		const auto& vertex_shader = shaders[ShaderType::VS_GBUFFER];
		if (!vertex_shader)
			return;

		if (renderables[RenderableType::Opaque].empty())
		{
			command_list->ClearRenderTarget(albedo_texture);
			command_list->ClearRenderTarget(normal_texture);
			command_list->ClearRenderTarget(material_texture);
			command_list->ClearRenderTarget(velocity_texture);
			command_list->ClearDepthStencilTarget(depth_texture, D3D11_CLEAR_DEPTH, GetClearDepth());
			command_list->End();
			command_list->Submit();
			return;
		}

		const std::vector<ID3D11RenderTargetView*> render_targets
		{
			albedo_texture->GetRenderTargetView(),
			normal_texture->GetRenderTargetView(),
			material_texture->GetRenderTargetView(),
			velocity_texture->GetRenderTargetView(),
		};

		UpdateGlobalBuffer(static_cast<uint>(resolution.x), static_cast<uint>(resolution.y));

		uint current_mesh_id = 0;     //현재 적용된 매쉬의 ID
		uint current_material_id = 0; //현재 적용된 재질(표면 텍스처)의 ID
		uint current_shader_id = 0;   //현재 적용된 쉐이더의 ID

		const auto draw_actor = [this, &current_material_id, &current_mesh_id, &current_shader_id](Actor* actor)
		{
			const auto& renderable = actor->GetRenderable();
			if (!renderable)
				return;

			if (!camera->IsInViewFrustum(renderable.get()))
				return;

			//Mesh
			const auto& mesh = renderable->GetMesh();
			if (!mesh || !mesh->GetVertexBuffer() || !mesh->GetIndexBuffer())
				return;

			if (current_mesh_id != mesh->GetID())
			{
				command_list->SetVertexBuffer(mesh->GetVertexBuffer());
				command_list->SetIndexBuffer(mesh->GetIndexBuffer());
				current_mesh_id = mesh->GetID();
			}

			//Material
			const auto& material = renderable->GetMaterial();
			if (!material)
				return;

			if (current_material_id != material->GetID())
			{
				command_list->SetRasterizerState(GetRasterizerState(material->GetCullMode()));
				command_list->SetShaderResources(0, ShaderScope::PS, material->GetTextureShaderResources());

				material->UpdateConstantBuffer();
				command_list->SetConstantBuffer(1, ShaderScope::PS, material->GetConstantBuffer());
				current_material_id = material->GetID();
			}

			//Pixel Shader
			const auto& pixel_shader = material->GetShader();
			if (!pixel_shader)
				return;

			if (current_shader_id != pixel_shader->GetID())
			{
				command_list->SetPixelShader(pixel_shader);
				current_shader_id = pixel_shader->GetID();
			}

			const auto& transform = actor->GetTransform();
			transform->UpdateConstantBuffer(camera_view_proj);
			command_list->SetConstantBuffer(2, ShaderScope::VS, transform->GetConstantBuffer());

			if (renderable->GetHasAnimation())
			{
				auto root_transform = transform->GetRoot();
				auto root_actor = root_transform->GetActor();

				if (auto animator = root_actor->GetComponent<Animator>())
				{
					if (animator->GetAnimationCount() != 0)
					{
						animator->UpdateConstantBuffer();
						command_list->SetConstantBuffer(3, ShaderScope::VS, animator->GetConstantBuffer());

						auto& vertex_shader = shaders[ShaderType::VS_SKINNED_ANIMATION];
						command_list->SetVertexShader(vertex_shader);
						command_list->SetInputLayout(vertex_shader->GetInputLayout());
					}
				}
			}

			//if (auto terrain = actor->GetComponent<Terrain>())
			//{
			//    terrain->UpdateConstantBuffer();
			//    command_list->SetConstantBuffer(4, ShaderScope::VS, terrain->GetConstantBuffer());
			//    command_list->SetShaderResources(1, ShaderScope::PS, terrain->GetTextureShaderResources());
			//}

			command_list->DrawIndexed(mesh->GetIndexBuffer()->GetCount(), mesh->GetIndexBuffer()->GetOffset(), mesh->GetVertexBuffer()->GetOffset());
		};

		command_list->SetRasterizerState(cull_back_solid_state);
		command_list->SetBlendState(blend_disabled_state);
		command_list->SetDepthStencilState(depth_stencil_enabled_state);
		command_list->SetRenderTargets(render_targets, depth_texture->GetDepthStencilView());
		command_list->SetViewport(albedo_texture->GetViewport());
		command_list->ClearRenderTargets(render_targets);
		command_list->ClearDepthStencilTarget(depth_texture, D3D11_CLEAR_DEPTH, GetClearDepth());
		command_list->SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		command_list->SetInputLayout(vertex_shader->GetInputLayout());
		command_list->SetVertexShader(vertex_shader);
		command_list->SetConstantBuffer(0, ShaderScope::Global, global_buffer);

		for (const auto& actor : renderables[RenderableType::Opaque])
			draw_actor(actor);
	}
	command_list->End();
	command_list->Submit();
}

void Renderer::PassSSAO()
{
	const auto& vertex_shader = shaders[ShaderType::VS_POST_PROCESS];
	const auto& pixel_shader = shaders[ShaderType::PS_SSAO];
	if (!vertex_shader || !pixel_shader)
		return;

	auto& ssao_raw_texture = render_textures[RenderTextureType::SSAO_Raw];
	auto& ssao_blur_texture = render_textures[RenderTextureType::SSAO_Blur];
	auto& ssao_texture = render_textures[RenderTextureType::SSAO];

	command_list->Begin("PassSSAO");
	command_list->ClearRenderTarget(ssao_raw_texture, Color4::White);
	command_list->ClearRenderTarget(ssao_texture, Color4::White);

	if (renderer_options & RendererOption_SSAO)
	{
		const std::vector<ID3D11ShaderResourceView*> shader_resources
		{
			render_textures[RenderTextureType::GBuffer_Normal]->GetShaderResourceView(),
			render_textures[RenderTextureType::GBuffer_Depth]->GetShaderResourceView(),
			noise_texture->GetShaderResourceView(),
		};

		UpdateGlobalBuffer(ssao_raw_texture->GetWidth(), ssao_raw_texture->GetHeight());

		command_list->ClearShaderResources(ShaderScope::PS);
		command_list->SetRenderTarget(ssao_raw_texture);
		command_list->SetViewport(ssao_raw_texture->GetViewport());
		command_list->SetRasterizerState(cull_back_solid_state);
		command_list->SetBlendState(blend_disabled_state);
		command_list->SetDepthStencilState(depth_stencil_disabled_state);
		command_list->SetVertexBuffer(screen_vertex_buffer);
		command_list->SetIndexBuffer(screen_index_buffer);
		command_list->SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		command_list->SetInputLayout(vertex_shader->GetInputLayout());
		command_list->SetVertexShader(vertex_shader);
		command_list->SetPixelShader(pixel_shader);
		command_list->SetConstantBuffer(0, ShaderScope::Global, global_buffer);
		command_list->SetShaderResources(0, ShaderScope::PS, shader_resources);
		command_list->DrawIndexed(6, 0, 0);
		command_list->Submit();

		PassBilateralGaussianBlur(ssao_raw_texture, ssao_blur_texture, 2.0f, 2.0f);

		if (ssao_scale < 1.0f)
			PassUpsample(ssao_blur_texture, ssao_texture);
		else if (ssao_scale > 1.0f)
			PassDownsample(ssao_blur_texture, ssao_texture);
		else
			ssao_blur_texture.swap(ssao_texture);
	}

	command_list->End();
	command_list->Submit();
}



void Renderer::PassLight()
{
	const auto& vertex_shader = shaders[ShaderType::VS_POST_PROCESS];
	const auto& directional_shader = shaders[ShaderType::PS_DIRECTIONAL_LIGHT];
	const auto& point_shader = shaders[ShaderType::PS_POINT_LIGHT];
	const auto& spot_shader = shaders[ShaderType::PS_SPOT_LIGHT];
	if (!vertex_shader || !directional_shader || !point_shader || !spot_shader)
		return;

	auto& diffuse_texture = render_textures[RenderTextureType::Light_Diffuse];
	auto& specular_texture = render_textures[RenderTextureType::Light_Specular];

	std::vector<ID3D11RenderTargetView*> render_targets
	{
		diffuse_texture->GetRenderTargetView(),
		specular_texture->GetRenderTargetView(),
	};

	command_list->Begin("PassLight");

	UpdateGlobalBuffer(diffuse_texture->GetWidth(), diffuse_texture->GetHeight());

	command_list->ClearRenderTargets(render_targets);
	command_list->SetRenderTargets(render_targets);
	command_list->SetViewport(diffuse_texture->GetViewport());
	command_list->SetRasterizerState(cull_back_solid_state);
	command_list->SetBlendState(blend_color_add_state);
	command_list->SetDepthStencilState(depth_stencil_disabled_state);
	command_list->SetVertexBuffer(screen_vertex_buffer);
	command_list->SetIndexBuffer(screen_index_buffer);
	command_list->SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	command_list->SetInputLayout(vertex_shader->GetInputLayout());
	command_list->SetVertexShader(vertex_shader);
	command_list->SetConstantBuffer(0, ShaderScope::Global, global_buffer);

	auto draw_lights = [this, &directional_shader, &point_shader, &spot_shader](const RenderableType& type)
	{
		const auto& actors = renderables[type];
		if (actors.empty())
			return;

		std::shared_ptr<Shader> pixel_shader;
		if (type == RenderableType::Directional_Light)  pixel_shader = directional_shader;
		else if (type == RenderableType::Point_Light)   pixel_shader = point_shader;
		else if (type == RenderableType::Spot_Light)    pixel_shader = spot_shader;
		else return;

		UpdateLightBuffer(actors);

		std::vector<ID3D11ShaderResourceView*> shader_resources
		{
			render_textures[RenderTextureType::GBuffer_Normal]->GetShaderResourceView(),
			render_textures[RenderTextureType::GBuffer_Material]->GetShaderResourceView(),
			render_textures[RenderTextureType::GBuffer_Depth]->GetShaderResourceView(),
		};

		for (const auto& actor : actors)
		{
			if (auto light_component = actor->GetComponent<Light>())
			{
				command_list->SetPixelShader(pixel_shader);
				command_list->SetShaderResources(0, ShaderScope::PS, shader_resources);
				command_list->SetConstantBuffer(1, ShaderScope::PS, light_buffer);
				command_list->DrawIndexed(6, 0, 0);
				command_list->Submit();
			}
		}
	};

	draw_lights(RenderableType::Directional_Light);
	draw_lights(RenderableType::Point_Light);
	draw_lights(RenderableType::Spot_Light);

	command_list->End();
	command_list->Submit();
}

void Renderer::PassComposition()
{
	const auto& vertex_shader = shaders[ShaderType::VS_POST_PROCESS];
	const auto& pixel_shader = shaders[ShaderType::PS_COMPOSITION];
	if (!vertex_shader || !pixel_shader)
		return;

	auto& out_texture = render_textures[RenderTextureType::Composition_HDR];

	command_list->Begin("PassComposition");

	UpdateGlobalBuffer(out_texture->GetWidth(), out_texture->GetHeight());

	const std::vector<ID3D11ShaderResourceView*> shader_resources
	{
		render_textures[RenderTextureType::GBuffer_Albedo]->GetShaderResourceView(),
		render_textures[RenderTextureType::GBuffer_Normal]->GetShaderResourceView(),
		render_textures[RenderTextureType::GBuffer_Depth]->GetShaderResourceView(),
		render_textures[RenderTextureType::GBuffer_Material]->GetShaderResourceView(),
		render_textures[RenderTextureType::Light_Diffuse]->GetShaderResourceView(),
		render_textures[RenderTextureType::Light_Specular]->GetShaderResourceView(),
		 black_texture->GetShaderResourceView(),
		nullptr,
		skybox ? (skybox->GetTexture() ? skybox->GetTexture()->GetShaderResourceView() : white_texture->GetShaderResourceView()) : white_texture->GetShaderResourceView(),
		ibl_lut_texture->GetShaderResourceView(),
		nullptr,
	};

	command_list->SetRenderTarget(out_texture);
	command_list->SetViewport(out_texture->GetViewport());
	command_list->SetBlendState(blend_disabled_state);
	command_list->SetDepthStencilState(depth_stencil_disabled_state);
	command_list->SetRasterizerState(cull_back_solid_state);
	command_list->SetVertexBuffer(screen_vertex_buffer);
	command_list->SetIndexBuffer(screen_index_buffer);
	command_list->SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	command_list->SetInputLayout(vertex_shader->GetInputLayout());
	command_list->SetVertexShader(vertex_shader);
	command_list->SetPixelShader(pixel_shader);
	command_list->SetConstantBuffer(0, ShaderScope::Global, global_buffer);
	command_list->SetShaderResources(0, ShaderScope::PS, shader_resources);
	command_list->DrawIndexed(6, 0, 0);

	command_list->End();
	command_list->Submit();
}

void Renderer::PassPostProcess()
{
	const auto& vertex_shader = shaders[ShaderType::VS_POST_PROCESS];
	if (!vertex_shader)
		return;

	auto& in_hdr_texture = render_textures[RenderTextureType::Composition_HDR];
	auto& out_hdr_texture = render_textures[RenderTextureType::Composition_HDR2];
	auto& in_ldr_texture = render_textures[RenderTextureType::Composition_LDR];
	auto& out_ldr_texture = render_textures[RenderTextureType::Composition_LDR2];

	const auto swap_hdr_textures = [this, &in_hdr_texture, &out_hdr_texture]()
	{
		command_list->Submit();
		in_hdr_texture.swap(out_hdr_texture);
	};

	const auto swap_ldr_textures = [this, &in_ldr_texture, &out_ldr_texture]()
	{
		command_list->Submit();
		in_ldr_texture.swap(out_ldr_texture);
	};

	command_list->Begin("PassPostProcess");

	command_list->SetBlendState(blend_disabled_state);
	command_list->SetDepthStencilState(depth_stencil_disabled_state);
	command_list->SetRasterizerState(cull_back_solid_state);
	command_list->SetVertexBuffer(screen_vertex_buffer);
	command_list->SetIndexBuffer(screen_index_buffer);
	command_list->SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	command_list->SetInputLayout(vertex_shader->GetInputLayout());
	command_list->SetVertexShader(vertex_shader);

	if (renderer_options & RendererOption_MotionBlur)
	{
		PassMotionBlur(in_hdr_texture, out_hdr_texture);
		swap_hdr_textures();
	}

	if (renderer_options & RendererOption_Bloom)
	{
		PassBloom(in_hdr_texture, out_hdr_texture);
		swap_hdr_textures();
	}

	if (tone_mapping_type != ToneMappingType::Off)
		PassToneMapping(in_hdr_texture, in_ldr_texture);
	else
		PassCopy(in_hdr_texture, in_ldr_texture);

	if (renderer_options & RendererOption_FXAA)
	{
		PassFXAA(in_ldr_texture, out_ldr_texture);
		swap_ldr_textures();
	}

	if (renderer_options & RendererOption_Sharpening)
	{
		PassLumaSharpening(in_ldr_texture, out_ldr_texture);
		swap_ldr_textures();
	}

	PassGammaCorrection(in_ldr_texture, out_ldr_texture);
	swap_ldr_textures();

	command_list->End();
	command_list->Submit();
}

void Renderer::PassLine(const std::shared_ptr<ITexture>& out)
{
	const bool is_draw_aabb = renderer_options & RendererOption_AABB;
	const bool is_draw_grid = renderer_options & RendererOption_Grid;
	const auto& shader = shaders[ShaderType::VPS_LINEDRAW];
	if (!shader)
		return;

	command_list->Begin("PassLine");

	if (is_draw_aabb)
	{
		for (const auto& actor : renderables[RenderableType::Opaque])
		{
			if (auto renderable = actor->GetRenderable())
				DrawBox(renderable->GetBoundBox());
		}
	}

	command_list->SetViewport(out->GetViewport());
	command_list->SetBlendState(blend_disabled_state);
	command_list->SetRasterizerState(cull_back_wireframe_state);
	command_list->SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
	command_list->SetInputLayout(shader->GetInputLayout());
	command_list->SetVertexShader(shader);
	command_list->SetPixelShader(shader);
	command_list->SetSamplerState(0, ShaderScope::PS, point_clamp_sampler);

	//Depth enabled
	command_list->SetDepthStencilState(depth_stencil_enabled_state);
	command_list->SetRenderTarget(out, render_textures[RenderTextureType::GBuffer_Depth]);
	{
		//Grid
		{
			if (is_draw_grid)
			{
				UpdateGlobalBuffer
				(
					static_cast<uint>(resolution.x),
					static_cast<uint>(resolution.y),
					grid->GetComputeWorldMatrix(camera->GetTransform()) *  camera_view_proj
				);

				command_list->SetVertexBuffer(grid->GetVertexBuffer());
				command_list->SetIndexBuffer(grid->GetIndexBuffer());
				command_list->SetConstantBuffer(0, ShaderScope::Global, global_buffer);
				command_list->DrawIndexed(grid->GetIndexCount(), 0, 0);
			}
		}

		//Line
		{
			const auto vertex_count = static_cast<uint>(depth_enabled_line_vertices.size());
			if (vertex_count)
			{
				if (vertex_count > line_vertex_buffer->GetCount())
				{
					line_vertex_buffer->Clear();
					line_vertex_buffer->Create(depth_enabled_line_vertices, D3D11_USAGE_DYNAMIC);
				}

				auto vertex_data = static_cast<VertexColor*>(line_vertex_buffer->Map());
				std::copy(depth_enabled_line_vertices.begin(), depth_enabled_line_vertices.end(), vertex_data);
				line_vertex_buffer->Unmap();

				UpdateGlobalBuffer(out->GetWidth(), out->GetHeight(), camera_view_proj);

				command_list->SetVertexBuffer(line_vertex_buffer);
				command_list->SetConstantBuffer(0, ShaderScope::Global, global_buffer);
				command_list->Draw(vertex_count);

				depth_enabled_line_vertices.clear();
			}
		}
	}

	//Depth disabled
	command_list->SetDepthStencilState(depth_stencil_disabled_state);
	command_list->SetRenderTarget(out);
	{
		//Line
		{
			const auto vertex_count = static_cast<uint>(depth_disabled_line_vertices.size());
			if (vertex_count)
			{
				if (vertex_count > line_vertex_buffer->GetCount())
				{
					line_vertex_buffer->Clear();
					line_vertex_buffer->Create(depth_disabled_line_vertices, D3D11_USAGE_DYNAMIC);
				}

				auto vertex_data = static_cast<VertexColor*>(line_vertex_buffer->Map());
				std::copy(depth_disabled_line_vertices.begin(), depth_disabled_line_vertices.end(), vertex_data);
				line_vertex_buffer->Unmap();

				UpdateGlobalBuffer(out->GetWidth(), out->GetHeight(), camera_view_proj);

				command_list->SetVertexBuffer(line_vertex_buffer);
				command_list->SetConstantBuffer(0, ShaderScope::Global, global_buffer);
				command_list->Draw(vertex_count);

				depth_disabled_line_vertices.clear();
			}
		}
	}

	command_list->End();
	command_list->Submit();
}

void Renderer::PassDebug(const std::shared_ptr<class ITexture>& out)
{
	if (debug_buffer_type == DebugBufferType::None)
		return;

	std::shared_ptr<ITexture> debug_texture;
	ShaderType shader_type = ShaderType::PS_TEXTURE;

	switch (debug_buffer_type)
	{
	case DebugBufferType::Albedo:
	{
		debug_texture = render_textures[RenderTextureType::GBuffer_Albedo];
		shader_type = ShaderType::PS_TEXTURE;
		break;
	}
	case DebugBufferType::Normal:
	{
		debug_texture = render_textures[RenderTextureType::GBuffer_Normal];
		shader_type = ShaderType::PS_DEBUG_NORMAL;
		break;
	}
	case DebugBufferType::Material:
	{
		debug_texture = render_textures[RenderTextureType::GBuffer_Material];
		shader_type = ShaderType::PS_TEXTURE;
		break;
	}
	case DebugBufferType::Velocity:
	{
		debug_texture = render_textures[RenderTextureType::GBuffer_Velocity];
		shader_type = ShaderType::PS_DEBUG_VELOCITY;
		break;
	}
	case DebugBufferType::Depth:
	{
		debug_texture = render_textures[RenderTextureType::GBuffer_Depth];
		shader_type = ShaderType::PS_DEBUG_DEPTH;
		break;
	}
	case DebugBufferType::Diffuse:
	{
		debug_texture = render_textures[RenderTextureType::Light_Diffuse];
		shader_type = ShaderType::PS_DEBUG_LIGHT;
		break;
	}
	case DebugBufferType::Specular:
	{
		debug_texture = render_textures[RenderTextureType::Light_Specular];
		shader_type = ShaderType::PS_DEBUG_LIGHT;
		break;
	}
	case DebugBufferType::SSAO:
	{
		debug_texture = (renderer_options & RendererOption_SSAO) ? render_textures[RenderTextureType::SSAO] : white_texture;
		shader_type = ShaderType::PS_DEBUG_DEPTH;
		break;
	}
	case DebugBufferType::Bloom:
	{
		debug_texture = bloom_textures.front();
		shader_type = ShaderType::PS_DEBUG_LIGHT;
		break;
	}
	}

	const auto& vertex_shader = shaders[ShaderType::VS_POST_PROCESS];
	const auto& pixel_shader = shaders[shader_type];
	if (!vertex_shader || !pixel_shader)
		return;

	command_list->Begin("PassDebug");
	{
		UpdateGlobalBuffer(out->GetWidth(), out->GetHeight(), post_process_view_proj);

		command_list->SetRasterizerState(cull_back_solid_state);
		command_list->SetBlendState(blend_disabled_state);
		command_list->SetDepthStencilState(depth_stencil_disabled_state);
		command_list->SetRenderTarget(out);
		command_list->SetViewport(out->GetViewport());
		command_list->SetVertexBuffer(screen_vertex_buffer);
		command_list->SetIndexBuffer(screen_index_buffer);
		command_list->SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		command_list->SetInputLayout(vertex_shader->GetInputLayout());
		command_list->SetVertexShader(vertex_shader);
		command_list->SetPixelShader(pixel_shader);
		command_list->SetConstantBuffer(0, ShaderScope::Global, global_buffer);
		command_list->SetShaderResource(0, ShaderScope::PS, debug_texture);
		command_list->DrawIndexed(6, 0, 0);
	}
	command_list->End();
	command_list->Submit();
}

void Renderer::PassGammaCorrection(std::shared_ptr<class ITexture>& in, std::shared_ptr<class ITexture>& out)
{
	const auto& pixel_shader = shaders[ShaderType::PS_GAMMA_CORRECTION];
	if (!pixel_shader)
		return;

	command_list->Begin("PassGammaCorrection");

	UpdateGlobalBuffer(out->GetWidth(), out->GetHeight());

	command_list->ClearShaderResources(ShaderScope::PS);
	command_list->SetRenderTarget(out);
	command_list->SetViewport(out->GetViewport());
	command_list->SetPixelShader(pixel_shader);
	command_list->SetConstantBuffer(0, ShaderScope::Global, global_buffer);
	command_list->SetShaderResource(0, ShaderScope::PS, in);
	command_list->DrawIndexed(6, 0, 0);

	command_list->End();
	command_list->Submit();
}

void Renderer::PassToneMapping(std::shared_ptr<class ITexture>& in, std::shared_ptr<class ITexture>& out)
{
	const auto& pixel_shader = shaders[ShaderType::PS_TONE_MAPPING];
	if (!pixel_shader)
		return;

	command_list->Begin("PassToneMapping");

	UpdateGlobalBuffer(out->GetWidth(), out->GetHeight());

	command_list->ClearShaderResources(ShaderScope::PS);
	command_list->SetRenderTarget(out);
	command_list->SetViewport(out->GetViewport());
	command_list->SetRasterizerState(cull_back_solid_state);
	command_list->SetDepthStencilState(depth_stencil_disabled_state);
	command_list->SetBlendState(blend_disabled_state);
	command_list->SetPixelShader(pixel_shader);
	command_list->SetConstantBuffer(0, ShaderScope::Global, global_buffer);
	command_list->SetShaderResource(0, ShaderScope::PS, in);
	command_list->DrawIndexed(6, 0, 0);

	command_list->End();
	command_list->Submit();
}

void Renderer::PassCopy(std::shared_ptr<class ITexture>& in, std::shared_ptr<class ITexture>& out)
{
	const auto& pixel_shader = shaders[ShaderType::PS_TEXTURE];
	if (!pixel_shader)
		return;

	command_list->Begin("PassCopy");

	UpdateGlobalBuffer(out->GetWidth(), out->GetHeight());

	command_list->ClearShaderResources(ShaderScope::PS);
	command_list->SetRenderTarget(out);
	command_list->SetViewport(out->GetViewport());
	command_list->SetBlendState(blend_disabled_state);
	command_list->SetPixelShader(pixel_shader);
	command_list->SetConstantBuffer(0, ShaderScope::Global, global_buffer);
	command_list->SetShaderResource(0, ShaderScope::PS, in);
	command_list->DrawIndexed(6, 0, 0);

	command_list->End();
	command_list->Submit();
}

void Renderer::PassMotionBlur(std::shared_ptr<class ITexture>& in, std::shared_ptr<class ITexture>& out)
{
	const auto& pixel_shader = shaders[ShaderType::PS_MOTION_BLUR];
	if (!pixel_shader)
		return;

	command_list->Begin("PassMotionBlur");

	UpdateGlobalBuffer(out->GetWidth(), out->GetHeight());

	const std::vector<ID3D11ShaderResourceView*> shader_resources
	{
		in->GetShaderResourceView(),
		render_textures[RenderTextureType::GBuffer_Velocity]->GetShaderResourceView(),
		render_textures[RenderTextureType::GBuffer_Depth]->GetShaderResourceView(),
	};

	command_list->ClearShaderResources(ShaderScope::PS);
	command_list->SetRenderTarget(out);
	command_list->SetViewport(out->GetViewport());
	command_list->SetDepthStencilState(depth_stencil_disabled_state);
	command_list->SetRasterizerState(cull_back_solid_state);
	command_list->SetPixelShader(pixel_shader);
	command_list->SetConstantBuffer(0, ShaderScope::Global, global_buffer);
	command_list->SetShaderResources(0, ShaderScope::PS, shader_resources);
	command_list->DrawIndexed(6, 0, 0);

	command_list->End();
	command_list->Submit();
}

void Renderer::PassLumaSharpening(std::shared_ptr<class ITexture>& in, std::shared_ptr<class ITexture>& out)
{
	const auto& pixel_shader = shaders[ShaderType::PS_LUMA_SHARPENING];
	if (!pixel_shader)
		return;

	command_list->Begin("PassLumaSharpening");

	UpdateGlobalBuffer(out->GetWidth(), out->GetHeight());

	command_list->ClearShaderResources(ShaderScope::PS);
	command_list->SetRenderTarget(out);
	command_list->SetViewport(out->GetViewport());
	command_list->SetDepthStencilState(depth_stencil_disabled_state);
	command_list->SetRasterizerState(cull_back_solid_state);
	command_list->SetPixelShader(pixel_shader);
	command_list->SetConstantBuffer(0, ShaderScope::Global, global_buffer);
	command_list->SetShaderResource(0, ShaderScope::PS, in);
	command_list->DrawIndexed(6, 0, 0);

	command_list->End();
	command_list->Submit();
}

void Renderer::PassUpsample(std::shared_ptr<class ITexture>& in, std::shared_ptr<class ITexture>& out)
{
	const auto& pixel_shader = shaders[ShaderType::PS_UPSAMPLE];
	if (!pixel_shader)
		return;

	command_list->Begin("PassUpsample");

	UpdateGlobalBuffer(out->GetWidth(), out->GetHeight());

	command_list->SetRenderTarget(out);
	command_list->SetViewport(out->GetViewport());
	command_list->SetDepthStencilState(depth_stencil_disabled_state);
	command_list->SetRasterizerState(cull_back_solid_state);
	command_list->SetPixelShader(pixel_shader);
	command_list->SetConstantBuffer(0, ShaderScope::Global, global_buffer);
	command_list->SetShaderResource(0, ShaderScope::PS, in);
	command_list->DrawIndexed(6, 0, 0);

	command_list->End();
	command_list->Submit();
}

void Renderer::PassDownsample(std::shared_ptr<class ITexture>& in, std::shared_ptr<class ITexture>& out, const ShaderType & pixel_shader_type)
{
	const auto& pixel_shader = shaders[pixel_shader_type];
	if (!pixel_shader)
		return;

	command_list->Begin("PassDownsample");

	UpdateGlobalBuffer(out->GetWidth(), out->GetHeight());

	command_list->SetRenderTarget(out);
	command_list->SetViewport(out->GetViewport());
	command_list->SetDepthStencilState(depth_stencil_disabled_state);
	command_list->SetRasterizerState(cull_back_solid_state);
	command_list->SetPixelShader(pixel_shader);
	command_list->SetConstantBuffer(0, ShaderScope::Global, global_buffer);
	command_list->SetShaderResource(0, ShaderScope::PS, in);
	command_list->DrawIndexed(6, 0, 0);

	command_list->End();
	command_list->Submit();
}

void Renderer::PassGaussianBlur(std::shared_ptr<class ITexture>& in, std::shared_ptr<class ITexture>& out, const float & sigma, const float & pixel_stride)
{
	if (in->GetWidth() != out->GetWidth() || in->GetHeight() != out->GetHeight() || in->GetFormat() != out->GetFormat())
	{
		LOG_ERROR("Invalid parameter, textures must match because they will get swapped");
		return;
	}

	const auto& pixel_shader = shaders[ShaderType::PS_GAUSSIAN_BLUR];
	if (!pixel_shader)
		return;

	command_list->Begin("PassGaussianBlur");

	UpdateGlobalBuffer(out->GetWidth(), out->GetHeight());

	command_list->SetViewport(out->GetViewport());
	command_list->SetDepthStencilState(depth_stencil_disabled_state);
	command_list->SetRasterizerState(cull_back_solid_state);
	command_list->SetPixelShader(pixel_shader);
	command_list->SetConstantBuffer(0, ShaderScope::Global, global_buffer);

	//Horizontal
	{
		UpdateBlurBuffer(Vector2(pixel_stride, 0.0f), sigma);

		command_list->ClearShaderResources(ShaderScope::PS);
		command_list->SetRenderTarget(out);
		command_list->SetShaderResource(0, ShaderScope::PS, in);
		command_list->SetConstantBuffer(1, ShaderScope::PS, blur_buffer);
		command_list->DrawIndexed(6, 0, 0);
		command_list->Submit();
	}

	//Vertical
	{
		UpdateBlurBuffer(Vector2(0.0f, pixel_stride), sigma);

		command_list->ClearShaderResources(ShaderScope::PS);
		command_list->SetRenderTarget(in);
		command_list->SetShaderResource(0, ShaderScope::PS, out);
		command_list->SetConstantBuffer(1, ShaderScope::PS, blur_buffer);
		command_list->DrawIndexed(6, 0, 0);
		command_list->Submit();
	}

	command_list->End();
	command_list->Submit();

	in.swap(out);
}

void Renderer::PassBilateralGaussianBlur(std::shared_ptr<class ITexture>& in, std::shared_ptr<class ITexture>& out, const float & sigma, const float & pixel_stride)
{
	if (in->GetWidth() != out->GetWidth() || in->GetHeight() != out->GetHeight() || in->GetFormat() != out->GetFormat())
	{
		LOG_ERROR("Invalid parameter, textures must match because they will get swapped");
		return;
	}

	const auto& pixel_shader = shaders[ShaderType::PS_BILATERAL_GAUSSIAN_BLUR];
	if (!pixel_shader)
		return;

	const auto& depth_texture = render_textures[RenderTextureType::GBuffer_Depth];
	const auto& normal_texture = render_textures[RenderTextureType::GBuffer_Normal];

	command_list->Begin("PassBilateralGaussianBlur");

	UpdateGlobalBuffer(out->GetWidth(), out->GetHeight());

	command_list->SetViewport(out->GetViewport());
	command_list->SetDepthStencilState(depth_stencil_disabled_state);
	command_list->SetRasterizerState(cull_back_solid_state);
	command_list->SetPixelShader(pixel_shader);
	command_list->SetConstantBuffer(0, ShaderScope::Global, global_buffer);

	//Horizontal
	{
		const std::vector<ID3D11ShaderResourceView*> shader_resources
		{
			in->GetShaderResourceView(),
			depth_texture->GetShaderResourceView(),
			normal_texture->GetShaderResourceView(),
		};

		UpdateBlurBuffer(Vector2(pixel_stride, 0.0f), sigma);

		command_list->ClearShaderResources(ShaderScope::PS);
		command_list->SetRenderTarget(out);
		command_list->SetShaderResources(0, ShaderScope::PS, shader_resources);
		command_list->SetConstantBuffer(1, ShaderScope::PS, blur_buffer);
		command_list->DrawIndexed(6, 0, 0);
		command_list->Submit();
	}

	//Vertical
	{
		const std::vector<ID3D11ShaderResourceView*> shader_resources
		{
			out->GetShaderResourceView(),
			depth_texture->GetShaderResourceView(),
			normal_texture->GetShaderResourceView(),
		};

		UpdateBlurBuffer(Vector2(0.0f, pixel_stride), sigma);

		command_list->ClearShaderResources(ShaderScope::PS);
		command_list->SetRenderTarget(in);
		command_list->SetShaderResources(0, ShaderScope::PS, shader_resources);
		command_list->SetConstantBuffer(1, ShaderScope::PS, blur_buffer);
		command_list->DrawIndexed(6, 0, 0);
		command_list->Submit();
	}

	command_list->End();
	command_list->Submit();

	in.swap(out);
}

void Renderer::PassFXAA(std::shared_ptr<class ITexture>& in, std::shared_ptr<class ITexture>& out)
{
	const auto& luma_shader = shaders[ShaderType::PS_LUMA];
	const auto& fxaa_shader = shaders[ShaderType::PS_FXAA];

	if (!luma_shader || !fxaa_shader)
		return;

	command_list->Begin("PassFXAA");

	UpdateGlobalBuffer(out->GetWidth(), out->GetHeight());

	command_list->ClearShaderResources(ShaderScope::PS);
	command_list->SetRenderTarget(out);
	command_list->SetViewport(out->GetViewport());
	command_list->SetBlendState(blend_disabled_state);
	command_list->SetDepthStencilState(depth_stencil_disabled_state);

	//Luma
	command_list->SetPixelShader(luma_shader);
	command_list->SetShaderResource(0, ShaderScope::PS, in);
	command_list->DrawIndexed(6, 0, 0);

	//FXAA
	command_list->SetRenderTarget(in);
	command_list->SetViewport(in->GetViewport());
	command_list->SetPixelShader(fxaa_shader);
	command_list->SetShaderResource(0, ShaderScope::PS, out);
	command_list->DrawIndexed(6, 0, 0);

	command_list->End();
	command_list->Submit();

	in.swap(out);
}

void Renderer::PassBloom(std::shared_ptr<class ITexture>& in, std::shared_ptr<class ITexture>& out)
{
	const auto& bloom_luminance_shader = shaders[ShaderType::PS_BLOOM_LUMINANCE];
	const auto& bloom_downsample_shader = shaders[ShaderType::PS_BLOOM_DOWNSAMPLE];
	const auto& bloom_upsample_shader = shaders[ShaderType::PS_UPSAMPLE];
	const auto& bloom_addtive_blend_shader = shaders[ShaderType::PS_BLOOM_ADDTIVE_BLEND];

	if (!bloom_luminance_shader || !bloom_downsample_shader || !bloom_upsample_shader || !bloom_addtive_blend_shader)
		return;

	command_list->Begin("PassBloom");
	command_list->SetRasterizerState(cull_back_solid_state);
	command_list->SetDepthStencilState(depth_stencil_disabled_state);
	{
		//Luminance - Luminance + Downsample
		command_list->Begin("Bloom_Luminance");
		{
			UpdateGlobalBuffer(bloom_textures.front()->GetWidth(), bloom_textures.front()->GetHeight());
			command_list->SetRenderTarget(bloom_textures.front());
			command_list->SetViewport(bloom_textures.front()->GetViewport());
			command_list->SetPixelShader(bloom_luminance_shader);
			command_list->SetConstantBuffer(0, ShaderScope::Global, global_buffer);
			command_list->SetShaderResource(0, ShaderScope::PS, in);
			command_list->DrawIndexed(6, 0, 0);
		}
		command_list->End();
		command_list->Submit();

		//Downsample
		const auto downsample = [this, &bloom_downsample_shader](std::shared_ptr<class ITexture>& in, std::shared_ptr<class ITexture>& out)
		{
			command_list->Begin("Bloom_Downsample");
			{
				UpdateGlobalBuffer(out->GetWidth(), out->GetHeight());
				command_list->SetRenderTarget(out);
				command_list->SetViewport(out->GetViewport());
				command_list->SetPixelShader(bloom_downsample_shader);
				command_list->SetConstantBuffer(0, ShaderScope::Global, global_buffer);
				command_list->SetShaderResource(0, ShaderScope::PS, in);
				command_list->DrawIndexed(6, 0, 0);
			}
			command_list->End();
			command_list->Submit();
		};

		for (int i = 0; i < static_cast<int>(bloom_textures.size() - 1); i++)
			downsample(bloom_textures[i], bloom_textures[i + 1]);

		//Upsample
		const auto upsample = [this, &bloom_upsample_shader](std::shared_ptr<class ITexture>& in, std::shared_ptr<class ITexture>& out)
		{
			command_list->Begin("Bloom_Upsample");
			{
				UpdateGlobalBuffer(out->GetWidth(), out->GetHeight());
				command_list->SetBlendState(blend_bloom_state);
				command_list->SetRenderTarget(out);
				command_list->SetViewport(out->GetViewport());
				command_list->SetPixelShader(bloom_upsample_shader);
				command_list->SetConstantBuffer(0, ShaderScope::Global, global_buffer);
				command_list->SetShaderResource(0, ShaderScope::PS, in);
				command_list->DrawIndexed(6, 0, 0);
			}
			command_list->End();
			command_list->Submit();
		};

		for (int i = static_cast<int>(bloom_textures.size() - 1); i > 0; i--)
			upsample(bloom_textures[i], bloom_textures[i - 1]);

		//Addtive blend
		command_list->Begin("Addtive_Blend");
		{
			std::vector<ID3D11ShaderResourceView*> shader_resources
			{
				in->GetShaderResourceView(),
				bloom_textures.front()->GetShaderResourceView(),
			};

			UpdateGlobalBuffer(out->GetWidth(), out->GetHeight());
			command_list->SetRenderTarget(out);
			command_list->SetViewport(out->GetViewport());
			command_list->SetPixelShader(bloom_addtive_blend_shader);
			command_list->SetConstantBuffer(0, ShaderScope::Global, global_buffer);
			command_list->SetShaderResources(0, ShaderScope::PS, shader_resources);
			command_list->DrawIndexed(6, 0, 0);
		}
		command_list->End();
		command_list->Submit();
	}
	command_list->End();
	command_list->Submit();
}
