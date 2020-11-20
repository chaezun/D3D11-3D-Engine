#include "D3D11Framework.h"
#include "Geometry_Generator.h"

void Geometry_Generator::CreateScreenQuad(Geometry<struct VertexTexture>& geometry, const uint & width, const uint & height)
{
	const auto half_width = width * 0.5f;
	const auto half_height = height * 0.5f;

	geometry.AddVertex(VertexTexture({ -half_width, -half_height, 0.0f }, { 0.0f, 1.0f }));
	geometry.AddVertex(VertexTexture({ -half_width, +half_height, 0.0f }, { 0.0f, 0.0f }));
	geometry.AddVertex(VertexTexture({ +half_width, -half_height, 0.0f }, { 1.0f, 1.0f }));
	geometry.AddVertex(VertexTexture({ +half_width, +half_height, 0.0f }, { 1.0f, 0.0f }));

	geometry.AddIndex(0); geometry.AddIndex(1); geometry.AddIndex(2);
	geometry.AddIndex(2); geometry.AddIndex(1); geometry.AddIndex(3);
}
