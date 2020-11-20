#pragma once

template <typename T> class Geometry;

class Geometry_Generator final
{
public:

	static void CreateScreenQuad(Geometry<struct VertexTexture>& geometry, const uint& width, const uint& height);
};