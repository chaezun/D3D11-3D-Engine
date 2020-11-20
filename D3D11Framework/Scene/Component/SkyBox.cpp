#include "D3D11Framework.h"
#include "SkyBox.h"
//
#include "Renderable.h"
#include "Transform.h"
#include "Scene/Actor.h"

SkyBox::SkyBox(Context * context, Actor * actor, Transform * transform)
	:IComponent(context, actor, transform)
{
	const auto directory = context->GetSubsystem<ResourceManager>()->GetAssetDirectory(AssetType::Cubemap);

	switch (sky_box_type)
	{
	case SkyBoxType::Cubemap:
	{
		texture_paths =
		{
		   directory + "array/X+.tga",
		   directory + "array/X-.tga",
		   directory + "array/Y+.tga",
		   directory + "array/Y-.tga",
		   directory + "array/Z+.tga",
		   directory + "array/Z-.tga",
		};
		break;
	}
	case SkyBoxType::Sphere:
	{
		texture_paths = { directory + "sky_sphere.hdr" };
		break;
	}
	}
}

void SkyBox::OnStart()
{
}

void SkyBox::OnUpdate()
{

}

void SkyBox::OnStop()
{
}

void SkyBox::SetTexture(const std::shared_ptr<class ITexture>& texture)
{
	this->texture = texture;
}

void SkyBox::SetStandardSkyBox()
{
	context->GetSubsystem<Thread>()->AddTask([this]()
		{
			CreateFromSphere(texture_paths.front());
		});
}

void SkyBox::CreateFromCubemap(const std::vector<std::string>& paths)
{
	
}

void SkyBox::CreateFromSphere(const std::string & path)
{
	LOG_INFO("Creating sky sphere...");

	texture = std::make_shared<Texture2D>(context, true);
	texture->LoadFromFile(path);

	LOG_INFO("Sky sphere has been created successfully");
}
