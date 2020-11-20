#include "D3D11Framework.h"
#include "Plane.h"

auto Plane::Normalize(const Plane & plane) -> const Plane
{
	auto new_plane = plane;
	new_plane.Normalize();

	return new_plane;
}

auto Plane::DotCoordinate(const Plane & plane, const Vector3 & v) -> const float
{
	auto new_plane = plane;
	return new_plane.DotCoordinate(v);
}

Plane::Plane(const Vector3 & normal, const float & d)
	: normal(normal)
	, d(d)
{
}

Plane::Plane(const Vector3 & a, const Vector3 & b, const Vector3 & c)
{
	auto ab = b - a;
	auto ac = c - a;

	normal = Vector3::Normalize(Vector3::Cross(ab, ac));
	d = -Vector3::Dot(normal, a);
}

void Plane::Normalize()
{
	Plane result;

	result.normal = Vector3::Normalize(normal);
	auto numerator = sqrtf(powf(result.normal.x, 2) + powf(result.normal.y, 2) + powf(result.normal.z, 2));
	auto denominator = sqrtf(powf(normal.x, 2) + powf(normal.y, 2) + powf(normal.z, 2));
	auto f = numerator / denominator;
	result.d = d * f;
	normal = result.normal;
	d = result.d;
}

auto Plane::DotCoordinate(const Vector3 & v) -> const float
{
	return (normal.x * v.x) + (normal.y * v.y) + (normal.z * v.z) + d;
}
