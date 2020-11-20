#include "D3D11Framework.h"
#include "Actor.h"
#include "Scene.h"
#include "Component/Transform.h"
#include "Component/Camera.h"
#include "Component/AudioListener.h"
#include "Component/AudioSource.h"
//#include "Component/Collider.h"
#include "Component/Light.h"
#include "Component/Renderable.h"
//#include "Component/RigidBody.h"
#include "Component/Script.h"
#include "Component/SkyBox.h"
#include "Component/Terrain.h"

Actor::Actor(Context * context)
	:context(context)
{

}

Actor::~Actor()
{
	components.clear();
	components.shrink_to_fit();
}

void Actor::Initialize(const std::shared_ptr<class Transform>& transform)
{
	this->transform = transform;
}

void Actor::Start()
{
	if (!is_active)
		return;

	for (const auto& component : components)
	{
		if (!component->IsEnabled())
		{
			continue;
		}

		component->OnStart();
	}
}

void Actor::Update()
{
	if (!is_active)
		return;

	for (const auto& component : components)
	{
		if (!component->IsEnabled())
		{
			continue;
		}

		component->OnUpdate();
	}
}

void Actor::Stop()
{
	if (!is_active)
		return;

	for (const auto& component : components)
	{
		if (!component->IsEnabled())
		{
			continue;
		}

		component->OnStop();
	}
}

auto Actor::AddComponent(const ComponentType & type) -> std::shared_ptr<IComponent>
{
	std::shared_ptr<IComponent> component;

	switch (type)
	{
	case ComponentType::Camera:         component = AddComponent<Camera>();         break;
	case ComponentType::SkyBox:         component = AddComponent<SkyBox>();         break;
	case ComponentType::Transform:      component = AddComponent<Transform>();      break;
	case ComponentType::Renderable:     component = AddComponent<Renderable>();     break;
	case ComponentType::Script:         component = AddComponent<Script>();         break;
	case ComponentType::Terrain:        component = AddComponent<Terrain>();        break;
	case ComponentType::Light:          component = AddComponent<Light>();          break;
	//case ComponentType::Collider:       component = AddComponent<Collider>();       break;
	case ComponentType::AudioSource:    component = AddComponent<AudioSource>();    break;
	case ComponentType::AudioListener:  component = AddComponent<AudioListener>();  break;
	//case ComponentType::RigidBody:      component = AddComponent<RigidBody>();      break;
	}

	return component;
}

auto Actor::HasComponent(const ComponentType & type) -> const bool
{
	for (const auto& component : components)
	{
		if (component->GetComponentType() == type)
			return true;
	}
	return false;
}
