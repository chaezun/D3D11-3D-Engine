#pragma once

class Frustum final
{
public:
	Frustum() = default;
	Frustum(const Matrix& view, const Matrix& projection, const float& screen_depth);
	~Frustum() = default;

	auto IsVisible(const Vector3& center, const Vector3& extent, const bool& is_ignore_near_plane = false) -> const bool;

private:
	auto CheckCube(const Vector3& center, const Vector3& extent) -> const Intersection;
	auto CheckSphere(const Vector3& center, const float& radius) -> const Intersection;

private:
	Plane planes[6];
};