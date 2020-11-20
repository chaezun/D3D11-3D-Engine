#include "D3D11Framework.h"
#include "Light.h"
#include "Transform.h"
#include "Camera.h"

Light::Light(Context * context, Actor * actor, Transform * transform)
  :IComponent(context, actor, transform)
{
}

void Light::OnStart()
{
}

void Light::OnUpdate()
{
}

void Light::OnStop()
{
}

auto Light::GetDirection() const -> Vector3
{
	return transform->GetForward();
}

void Light::SetLightType(const LightType & type)
{
   light_type=type;
   is_update=true;
   //Directional일 경우 무조건 그림자를 그림
   is_cast_shadow = light_type == LightType::Directional;
}

void Light::SetCastShadow(const bool & is_cast_shadow)
{
}