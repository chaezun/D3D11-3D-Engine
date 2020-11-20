#pragma once

class Plane final
{
public:
	static auto Normalize(const Plane& plane) -> const Plane;
	static auto DotCoordinate(const Plane& plane, const Vector3& v) -> const float;

public:
	Plane() = default;
	Plane(const Vector3& normal, const float& d);
	Plane(const Vector3& a, const Vector3& b, const Vector3& c);
	~Plane() = default;

	void Normalize();
	auto DotCoordinate(const Vector3& v) -> const float;

public:
	Vector3 normal = Vector3::Zero;
	float d = 0;
};