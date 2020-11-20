#include "stdafx.h"
#include "Widget_ToolBar.h"

Widget_ToolBar::Widget_ToolBar(Context * context)
	: IWidget(context)
{
	title = "ToolBar";
	window_flags |=
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoDocking;
}

void Widget_ToolBar::Begin()
{
	auto& ctx = *GImGui;
	ctx.NextWindowData.MenuBarOffsetMinVal = ImVec2
	(
		ctx.Style.DisplaySafeAreaPadding.x,
		ImMax(ctx.Style.DisplaySafeAreaPadding.y - ctx.Style.FramePadding.y, 0.0f)
	);

	ImGui::SetNextWindowPos(ImVec2(ctx.Viewports[0]->Pos.x, ctx.Viewports[0]->Pos.y + 23.9f));
	ImGui::SetNextWindowSize
	(
		ImVec2
		(
			ctx.Viewports[0]->Size.x,
			ctx.NextWindowData.MenuBarOffsetMinVal.y + ctx.FontBaseSize + ctx.Style.FramePadding.y + 20.0f
		)
	);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 5.0f));
	ImGui::Begin(title.c_str(), &is_visible, window_flags);
}

void Widget_ToolBar::Render()
{
	//Play Button
	ImGui::SameLine();
	ImGui::PushStyleColor
	(
		ImGuiCol_Button,
		ImGui::GetStyle().Colors[Engine::IsFlagEnabled(EngineFlags_Game) ? ImGuiCol_ButtonActive : ImGuiCol_Button]
	);

	if (Icon_Provider::Get().ImageButton(IconType::Button_Play, 20.0f))
		Engine::FlagToggle(EngineFlags_Game);

	ImGui::PopStyleColor();

	//Option Button
	ImGui::SameLine();
	ImGui::PushStyleColor
	(
		ImGuiCol_Button,
		ImGui::GetStyle().Colors[is_show_options ? ImGuiCol_ButtonActive : ImGuiCol_Button]
	);

	if (Icon_Provider::Get().ImageButton(IconType::Button_Option, 20.0f))
		is_show_options = true;

	ImGui::PopStyleColor();
	ImGui::PopStyleVar();

	if (is_show_options)
		ShowOptions();
}

void Widget_ToolBar::ShowOptions()
{
	auto renderer = Editor_Helper::Get().renderer;

	ImGui::PushStyleVar(ImGuiStyleVar_Alpha, window_alpha);
	ImGui::Begin("Renderer Options", &is_show_options, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDocking);
	{
		ImGui::TextUnformatted("Opacity");
		ImGui::SameLine();
		ImGui::SliderFloat("##Opacity", &window_alpha, 0.1f, 1.0f, "%.1f");

		if (ImGui::CollapsingHeader("PostProcess", ImGuiTreeNodeFlags_DefaultOpen))
		{
			static std::vector<std::string> tone_mapping_types
			{
				"Off",
				"ACES",
				"Reinhard",
				"Uncharted2",
			};
			std::string tone_mapping_type = tone_mapping_types[static_cast<uint>(renderer->GetToneMappingType())];

			if (ImGui::BeginCombo("ToneMapping", tone_mapping_type.c_str()))
			{
				for (uint i = 0; i < tone_mapping_types.size(); i++)
				{
					const auto is_selected = tone_mapping_type == tone_mapping_types[i];
					if (ImGui::Selectable(tone_mapping_types[i].c_str(), is_selected))
					{
						tone_mapping_type = tone_mapping_types[i];
						renderer->SetToneMappingType(static_cast<ToneMappingType>(i));
					}

					if (is_selected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}

			const auto tool_tip = [](const char* text)
			{
				if (ImGui::IsItemHovered())
				{
					ImGui::BeginTooltip();
					ImGui::TextUnformatted(text);
					ImGui::EndTooltip();
				}
			};

			auto is_aabb = renderer->IsOnFlag(RendererOption_AABB);
			auto is_grid = renderer->IsOnFlag(RendererOption_Grid);
			auto is_ssao = renderer->IsOnFlag(RendererOption_SSAO);
			auto is_fxaa = renderer->IsOnFlag(RendererOption_FXAA);
			auto is_bloom = renderer->IsOnFlag(RendererOption_Bloom);
			auto bloom_intensity = renderer->GetBloomIntensity();
			auto is_motion_blur = renderer->IsOnFlag(RendererOption_MotionBlur);
			auto motion_blur_strength = renderer->GetMotionBlurStrength();
			auto is_sharpening = renderer->IsOnFlag(RendererOption_Sharpening);
			auto sharp_strength = renderer->GetSharpStrength();
			auto sharp_clamp = renderer->GetSharpClamp();

			ImGui::Checkbox("AABB", &is_aabb);                  tool_tip("Option to draw AABB for debugging");
			ImGui::Checkbox("Grid", &is_grid);                  tool_tip("Option to draw Grid for debugging");
			ImGui::Checkbox("SSAO", &is_ssao);                  tool_tip("Screen Space Ambient Occlusion");
			ImGui::Checkbox("FXAA", &is_fxaa);                  tool_tip("Fast Approximate Anti-Aliasing");

			ImGui::Checkbox("Bloom", &is_bloom);
			ImGui::SameLine(); ImGui::DragFloat("Bloom Intensity", &bloom_intensity, 0.01f, 0.001, 5.0f);

			ImGui::Checkbox("MotionBlur", &is_motion_blur);
			ImGui::SameLine(); ImGui::InputFloat("Motion Blur Strength", &motion_blur_strength, 0.01f);

			ImGui::Checkbox("Sharpening", &is_sharpening);
			ImGui::PushItemWidth(100.0f);
			ImGui::SameLine();  ImGui::DragFloat("Sharp Strength", &sharp_strength, 0.01f, 0.1f, 3.0f);
			ImGui::SameLine(); ImGui::DragFloat("Sharp Clamp", &sharp_clamp, 0.01f, 0.0f, 1.0f);
			ImGui::PopItemWidth();

			const auto set_options = [&](const RendererOption& option, const bool& is_enabled)
			{
				is_enabled ? renderer->FlagEnable(option) : renderer->FlagDisable(option);
			};

			set_options(RendererOption_AABB, is_aabb);
			set_options(RendererOption_Grid, is_grid);
			set_options(RendererOption_SSAO, is_ssao);
			set_options(RendererOption_FXAA, is_fxaa);
			set_options(RendererOption_Bloom, is_bloom);
			set_options(RendererOption_MotionBlur, is_motion_blur);
			set_options(RendererOption_Sharpening, is_sharpening);

			renderer->SetBloomIntensity(bloom_intensity);
			renderer->SetMotionBlurStrength(motion_blur_strength);
			renderer->SetSharpStrength(sharp_strength);
			renderer->SetSharpClamp(sharp_clamp);
		}

		if (ImGui::CollapsingHeader("Debug", ImGuiTreeNodeFlags_DefaultOpen))
		{
			static std::vector<std::string> debug_buffer_types
			{
				"None",
				"Albedo",
				"Normal",
				"Material",
				"Velocity",
				"Depth",
				"Diffuse",
				"Specular",
				"SSAO",
				"Bloom",
			};

			static std::string current_debug_buffer_type = debug_buffer_types[0];

			if (ImGui::BeginCombo("Debug Buffers", current_debug_buffer_type.c_str()))
			{
				for (uint i = 0; i < debug_buffer_types.size(); i++)
				{
					const auto is_selected = current_debug_buffer_type == debug_buffer_types[i];
					if (ImGui::Selectable(debug_buffer_types[i].c_str(), is_selected))
					{
						current_debug_buffer_type = debug_buffer_types[i];
						renderer->SetDebugBufferType(static_cast<DebugBufferType>(i));
					}

					if (is_selected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
		}
	}
	ImGui::End();
	ImGui::PopStyleVar();
}
