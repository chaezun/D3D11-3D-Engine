#include "D3D11Framework.h"
#include "Frustum.h"

Frustum::Frustum(const Matrix & view, const Matrix & projection, const float & screen_depth)
{
	//Frustum 안에있는 최소 z 거리를 계산
	auto z_minimum = -projection._43 / projection._33;
	auto r = screen_depth / (screen_depth - z_minimum);
	auto proj_update = projection;
	proj_update._33 = r;
	proj_update._43 = -r * z_minimum;

	//view와 업데이트된 proj_update 에서 절두체의 행렬을 만듬
	auto view_proj = view * proj_update;

	//calc near plane of frustum
	planes[0].normal.x = view_proj._14 + view_proj._13;
	planes[0].normal.y = view_proj._24 + view_proj._23;
	planes[0].normal.z = view_proj._34 + view_proj._33;
	planes[0].d = view_proj._44 + view_proj._43;
	planes[0].Normalize();

	//clac far plane of frustum
	planes[1].normal.x = view_proj._14 - view_proj._13;
	planes[1].normal.y = view_proj._24 - view_proj._23;
	planes[1].normal.z = view_proj._34 - view_proj._33;
	planes[1].d = view_proj._44 - view_proj._43;
	planes[1].Normalize();

	//clac left plane of frustum
	planes[2].normal.x = view_proj._14 + view_proj._11;
	planes[2].normal.y = view_proj._24 + view_proj._21;
	planes[2].normal.z = view_proj._34 + view_proj._31;
	planes[2].d = view_proj._44 + view_proj._41;
	planes[2].Normalize();

	//clac right plane of frustum
	planes[3].normal.x = view_proj._14 - view_proj._11;
	planes[3].normal.y = view_proj._24 - view_proj._21;
	planes[3].normal.z = view_proj._34 - view_proj._31;
	planes[3].d = view_proj._44 - view_proj._41;
	planes[3].Normalize();

	//clac top plane of frustum
	planes[4].normal.x = view_proj._14 - view_proj._12;
	planes[4].normal.y = view_proj._24 - view_proj._22;
	planes[4].normal.z = view_proj._34 - view_proj._32;
	planes[4].d = view_proj._44 - view_proj._42;
	planes[4].Normalize();

	//clac bottom plane of frustum
	planes[5].normal.x = view_proj._14 + view_proj._12;
	planes[5].normal.y = view_proj._24 + view_proj._22;
	planes[5].normal.z = view_proj._34 + view_proj._32;
	planes[5].d = view_proj._44 + view_proj._42;
	planes[5].Normalize();
}

auto Frustum::IsVisible(const Vector3 & center, const Vector3 & extent, const bool & is_ignore_near_plane) -> const bool
{
	float radius = 0.0f;

	if (is_ignore_near_plane)
	{
		constexpr float z = std::numeric_limits<float>::infinity();
		radius = Math::Max(extent.x, Math::Max(extent.y, z));
	}
	else
		radius = Math::Max(extent.x, Math::Max(extent.y, extent.z));

	if (CheckSphere(center, radius) != Intersection::Outside)
		return true;

	if (CheckCube(center, radius) != Intersection::Outside)
		return true;

	return false;
}

auto Frustum::CheckCube(const Vector3 & center, const Vector3 & extent) -> const Intersection
{
	auto result = Intersection::Inside;

	for (const auto& plane : planes)
	{
		auto absolute_plane = Plane(plane.normal.Absolute(), plane.d);

		float d = center.x * plane.normal.x + center.y * plane.normal.y + center.z * plane.normal.z;
		float r = extent.x * absolute_plane.normal.x + extent.y * absolute_plane.normal.y + extent.z * absolute_plane.normal.z;

		float d_p_r = d + r;
		float d_m_r = d - r;

		if (d_p_r < -plane.d)
		{
			result = Intersection::Outside;
			break;
		}

		if (d_m_r < -plane.d)
			result = Intersection::Intersect;
	}

	return result;
}

auto Frustum::CheckSphere(const Vector3 & center, const float & radius) -> const Intersection
{
	//각 평면에 대한 거리를 계산
	for (const auto& plane : planes)
	{
		float distance = Vector3::Dot(plane.normal, center) + plane.d;

		if (distance < -radius)
			return Intersection::Outside;

		if (static_cast<float>(Math::Abs(distance)) < radius)
			return Intersection::Intersect;
	}

	return Intersection::Inside;
}
