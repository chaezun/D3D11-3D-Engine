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
#include "Scene/Component/SkyBox.h"

Renderer::Renderer(Context * context)
	:ISubsystem(context)
{
	renderer_options |=
		RendererOption_AABB |
		RendererOption_Grid |
		RendererOption_SSAO |
		RendererOption_FXAA |
		RendererOption_Bloom |
		RendererOption_MotionBlur |
		RendererOption_Sharpening;

	SUBSCRIBE_TO_EVENT(EventType::Render, EVENT_HANDLER(Render));
}

Renderer::~Renderer()
{
}

const bool Renderer::Initialize()
{
	line_vertex_buffer = std::make_shared<VertexBuffer>(context);
	grid = std::make_shared<Grid>(context);
	command_list = std::make_shared<CommandList>(context);

	CreateRenderTextures();
	CreateShaders();
	CreateConstantBuffers();
	CreateSamplerStates();
	CreateRasterizerStates();
	CreateBlendStates();
	CreateDepthStencilStates();
	CreateTextures();

	return true;
}

auto Renderer::GetFrameResource() -> ID3D11ShaderResourceView *
{
	auto render_texture = render_textures[RenderTextureType::Composition_LDR];

	return render_texture ? render_texture->GetShaderResourceView() : nullptr;
}

void Renderer::SetResolution(const uint & width, const uint & height)
{
	if (width == 0 || height == 0)
	{
		LOG_WARNING_F("%dx%d is an invalid resolution", width, height);
		return;
	}

	auto adjust_width = (width % 2 != 0) ? width - 1 : width;
	auto adjust_height = (height % 2 != 0) ? height - 1 : height;

	if (static_cast<uint>(resolution.x) == adjust_width && static_cast<uint>(resolution.y) == adjust_height)
		return;

	resolution.x = static_cast<float>(adjust_width);
	resolution.y = static_cast<float>(adjust_height);

	CreateRenderTextures();

	LOG_INFO_F("Resolution set to %dx%d", width, height);
}

void Renderer::AcquireRenderables(Scene * scene)
{
	renderables.clear();
	camera = nullptr;

	auto actors = scene->GetAllActors();
	for (const auto& actor : actors)
	{
		if (!actor)
			continue;

		//Component
		auto skybox_component = actor->GetComponent<SkyBox>();
		auto renderable_component = actor->GetComponent<Renderable>();
		auto light_component = actor->GetComponent<Light>();
		auto camera_component = actor->GetComponent<Camera>();

		//Mesh
		if (renderable_component)
		{
			renderables[RenderableType::Opaque].emplace_back(actor.get());
		}

		//Light
		if (light_component)
		{
			renderables[RenderableType::Light].emplace_back(actor.get());

			switch (light_component->GetLightType())
			{
			case LightType::Directional: renderables[RenderableType::Directional_Light].emplace_back(actor.get());  break;
			case LightType::Point:       renderables[RenderableType::Point_Light].emplace_back(actor.get());        break;
			case LightType::Spot:        renderables[RenderableType::Spot_Light].emplace_back(actor.get());         break;
			}
		}

		//SkyBox
		if (skybox_component)
			skybox = skybox_component;

		if (camera_component)
		{
			renderables[RenderableType::Camera].emplace_back(actor.get());
			camera = camera_component;
		}
	}
}

void Renderer::SortRenderables(std::vector<class Actor*>* actors)
{
	if (!camera || renderables.size() <= 2ULL)
		return;

	auto render_hash = [this](Actor* actor) -> const float
	{
		auto renderable = actor->GetRenderable();
		if (!renderable)
			return 0.0f;

		auto material = renderable->GetMaterial();
		if (!material)
			return 0.0f;

		const auto num_depth = (renderable->GetBoundBox().GetCenter() - camera->GetTransform()->GetTranslation()).LengthSq();
		const auto num_material = static_cast<float>(material->GetID());

		return stof(std::to_string(num_depth) + "-" + std::to_string(num_material));
	};

	std::sort(actors->begin(), actors->end(), [&render_hash](Actor* lhs, Actor* rhs)
		{
			return render_hash(lhs) < render_hash(rhs);
		});
}

void Renderer::DrawLine(const Vector3 & from, const Vector3 & to, const Color4 & from_color, const Color4 & to_color, const bool & is_depth)
{
	if (is_depth)
	{
		depth_enabled_line_vertices.emplace_back(from, from_color);
		depth_enabled_line_vertices.emplace_back(to, to_color);
	}

	else
	{
		depth_disabled_line_vertices.emplace_back(from, from_color);
		depth_disabled_line_vertices.emplace_back(to, to_color);
	}
}

void Renderer::DrawBox(const BoundBox & box, const Color4 & color, const bool & is_depth)
{
	const auto& min = box.GetMin();
	const auto& max = box.GetMax();

	//직육면체 모서리선을 그림
	DrawLine(Vector3(min.x, min.y, min.z), Vector3(max.x, min.y, min.z), color, color, is_depth);
	DrawLine(Vector3(max.x, min.y, min.z), Vector3(max.x, max.y, min.z), color, color, is_depth);
	DrawLine(Vector3(max.x, max.y, min.z), Vector3(min.x, max.y, min.z), color, color, is_depth);
	DrawLine(Vector3(min.x, max.y, min.z), Vector3(min.x, min.y, min.z), color, color, is_depth);
	DrawLine(Vector3(min.x, min.y, min.z), Vector3(min.x, min.y, max.z), color, color, is_depth);
	DrawLine(Vector3(max.x, min.y, min.z), Vector3(max.x, min.y, max.z), color, color, is_depth);
	DrawLine(Vector3(max.x, max.y, min.z), Vector3(max.x, max.y, max.z), color, color, is_depth);
	DrawLine(Vector3(min.x, max.y, min.z), Vector3(min.x, max.y, max.z), color, color, is_depth);
	DrawLine(Vector3(min.x, min.y, max.z), Vector3(max.x, min.y, max.z), color, color, is_depth);
	DrawLine(Vector3(max.x, min.y, max.z), Vector3(max.x, max.y, max.z), color, color, is_depth);
	DrawLine(Vector3(max.x, max.y, max.z), Vector3(min.x, max.y, max.z), color, color, is_depth);
	DrawLine(Vector3(min.x, max.y, max.z), Vector3(min.x, min.y, max.z), color, color, is_depth);
}

void Renderer::Render()
{
	if (!camera)
	{
		command_list->ClearRenderTarget(render_textures[RenderTextureType::Composition_LDR], Color4::Black);
		return;
	}

	if (renderables.empty())
	{
		command_list->ClearRenderTarget(render_textures[RenderTextureType::Composition_LDR], Color4::Black);
		return;
	}

	camera_near = camera->GetNearPlane();
	camera_far = camera->GetFarPlane();
	camera_position = camera->GetTransform()->GetTranslation();
	camera_view = camera->GetViewMatrix();
	camera_proj = camera->GetProjectionMatrix();
	camera_view_proj = camera_view * camera_proj;
	camera_view_proj_inverse = camera_view_proj.Inverse();

	post_process_view = Matrix::LookAtLH(Vector3(0.0f, 0.0f, -camera_near), Vector3::Forward, Vector3::Up);
	post_process_proj = Matrix::OrthoLH(resolution.x, resolution.y, camera_near, camera_far);
	post_process_view_proj = post_process_view * post_process_proj;

	//Start Renderer_Passes
	PassMain();
}

void Renderer::UpdateGlobalBuffer(const uint & width, const uint & height, const Matrix & transform)
{
	auto gpu_buffer = global_buffer->Map<GLOBAL_DATA>();
	if (!gpu_buffer)
	{
		LOG_ERROR("Failed to map buffer");
		return;
	}

	float directional_light_intensity = 0.0f;
	if (!renderables[RenderableType::Directional_Light].empty())
	{
		auto& actor = renderables[RenderableType::Directional_Light].front();
		if (auto& light_component = actor->GetComponent<Light>())
			directional_light_intensity = light_component->GetIntensity();
	}
	gpu_buffer->world_view_proj = transform;
	gpu_buffer->view = camera_view;
	gpu_buffer->proj = camera_proj;
	gpu_buffer->view_proj = camera_view_proj;
	gpu_buffer->view_proj_inverse = camera_view_proj_inverse;
	gpu_buffer->post_process_proj = post_process_proj;
	gpu_buffer->post_process_view_proj = post_process_view_proj;
	gpu_buffer->camera_near = camera_near;
	gpu_buffer->camera_far = camera_far;
	gpu_buffer->resolution = Vector2(static_cast<float>(width), static_cast<float>(height));
	gpu_buffer->camera_position = camera_position;
	gpu_buffer->directional_intensity = directional_light_intensity;
	gpu_buffer->gamma = gamma;
	gpu_buffer->ssao_scale = ssao_scale;
	gpu_buffer->fxaa_sub_pixel = fxaa_sub_pixel;
	gpu_buffer->fxaa_edge_threshold = fxaa_edge_threshold;
	gpu_buffer->fxaa_edge_threshold_min = fxaa_edge_threshold_min;
	gpu_buffer->bloom_intensity = bloom_intensity;
	gpu_buffer->exposure = exposure;
	gpu_buffer->tone_mapping = static_cast<float>(tone_mapping_type);
	gpu_buffer->delta_time = context->GetSubsystem<Timer>()->GetDeltaTimeSec();
	gpu_buffer->motion_blur_strength = motion_blur_strength;
	gpu_buffer->sharpen_strength = sharpen_strength;
	gpu_buffer->sharpen_clamp = sharpen_clamp;

	global_buffer->Unmap();
}

void Renderer::UpdateLightBuffer(const std::vector<class Actor*>& actors)
{
	if (actors.empty())
		return;

	auto light_data = light_buffer->Map<LIGHT_DATA>();
	if (!light_data)
	{
		LOG_ERROR("Failed to map buffer");
		return;
	}

	uint light_count = 0;
	for (uint i = 0; i < actors.size(); i++)
	{
		if (auto light_component = actors[i]->GetComponent<Light>())
		{
			light_data->intensity_range_angle_bias[i] = Vector4(light_component->GetIntensity(), light_component->GetRange(), light_component->GetAngle(), light_component->GetBias());
			light_data->normal_bias_shadow_volumetric_contact[i] = Vector4(light_component->GetNormalBias(), light_component->IsCastShadow(), true, true);
			light_data->colors[i] = light_component->GetColor();
			light_data->positions[i] = light_component->GetTransform()->GetTranslation();
			light_data->directions[i] = light_component->GetDirection();
			light_count++;
		}
	}
	light_data->light_count = static_cast<float>(light_count);

	light_buffer->Unmap();
}

void Renderer::UpdateBlurBuffer(const Vector2 & direction, const float & sigma)
{
	auto gpu_buffer = blur_buffer->Map<BLUR_DATA>();
	if (!gpu_buffer)
	{
		LOG_ERROR("Failed to map buffer");
		return;
	}

	gpu_buffer->direction = direction;
	gpu_buffer->sigma = sigma;

	blur_buffer->Unmap();
}


void Renderer::UpdateGlobalSamplers()
{
	const std::vector<ID3D11SamplerState*> sampler_states
	{
		compare_depth_sampler->GetResource(),
		point_clamp_sampler->GetResource(),
		bilinear_clamp_sampler->GetResource(),
		bilinear_wrap_sampler->GetResource(),
		trilinear_clamp_sampler->GetResource(),
		anisotropic_wrap_sampler->GetResource(),
	};
	command_list->SetSamplerStates(0, ShaderScope::PS, sampler_states);
}

auto Renderer::GetRasterizerState(const D3D11_CULL_MODE & cull_mode, const D3D11_FILL_MODE & fill_mode) -> const std::shared_ptr<class RasterizerState>&
{
	switch (cull_mode)
	{
	case D3D11_CULL_NONE:
	{
		switch (fill_mode)
		{
		case D3D11_FILL_WIREFRAME: return cull_none_wireframe_state;
		case D3D11_FILL_SOLID: return cull_none_solid_state;
		}
		break;
	}
	case D3D11_CULL_FRONT:
	{
		switch (fill_mode)
		{
		case D3D11_FILL_WIREFRAME: return cull_front_wireframe_state;
		case D3D11_FILL_SOLID: return cull_front_solid_state;
		}
		break;
	}
	case D3D11_CULL_BACK:
	{
		switch (fill_mode)
		{
		case D3D11_FILL_WIREFRAME: return cull_back_wireframe_state;
		case D3D11_FILL_SOLID: return cull_back_solid_state;
		}
		break;
	}
	}

	return cull_back_solid_state;
}
