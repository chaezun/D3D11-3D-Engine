#include "D3D11Framework.h"
#include "RasterizerState.h"

RasterizerState::RasterizerState(Context * context)
{
   device=context->GetSubsystem<Graphics>()->GetDevice();
}

RasterizerState::~RasterizerState()
{
   Clear();
}

void RasterizerState::Create(const D3D11_CULL_MODE & cull_mode, const D3D11_FILL_MODE & fill_mode, const bool & is_depth_clip_enabled, const bool & is_scissor_enabled, const bool & is_multi_sample_enabled, const bool & is_antialised_line_enabled)
{
	D3D11_RASTERIZER_DESC desc;
	ZeroMemory(&desc, sizeof(D3D11_RASTERIZER_DESC));
	desc.CullMode = cull_mode;
	desc.FillMode = fill_mode;
	desc.FrontCounterClockwise = false;
	desc.DepthBias = 0;
	desc.DepthBiasClamp = 0.0f;
	desc.SlopeScaledDepthBias = 0.0f;
	desc.DepthClipEnable = is_depth_clip_enabled;
	desc.MultisampleEnable = is_multi_sample_enabled;
	desc.AntialiasedLineEnable = is_antialised_line_enabled;
	desc.ScissorEnable = is_scissor_enabled;

	auto result = SUCCEEDED(device->CreateRasterizerState(&desc, &state));
	if (!result)
		LOG_ERROR("Failed to create rasterizer state");
}

void RasterizerState::Clear()
{
   SAFE_RELEASE(state);
}
