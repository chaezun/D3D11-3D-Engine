#pragma once
#include "IComponent.h"

enum class SkyBoxType : uint
{
	Cubemap,
	Sphere,
};

class SkyBox final : public IComponent
{
public:
	SkyBox
	(
		class Context* context,
		class Actor* actor,
		class Transform* transform
	);
	~SkyBox() = default;

	void OnStart() override;
	void OnUpdate() override;
	void OnStop() override;

	auto GetTexture() -> const std::shared_ptr<class ITexture>& { return texture; }
	void SetTexture(const std::shared_ptr<class ITexture>& texture);

	void SetStandardSkyBox();

private:
	void CreateFromCubemap(const std::vector<std::string>& paths);
	void CreateFromSphere(const std::string& path);

private:
	std::shared_ptr<class ITexture> texture;
	std::vector<std::string> texture_paths;
	SkyBoxType sky_box_type = SkyBoxType::Sphere;
};