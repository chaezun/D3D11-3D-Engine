#include "D3D11Framework.h"
#include "Rect.h"

Rect::Rect()
	: x(0.0f)
	, y(0.0f)
	, width(0.0f)
	, height(0.0f)
{
}

Rect::Rect(const float & x, const float & y, const float & width, const float & height)
	: x(x)
	, y(y)
	, width(width)
	, height(height)
{
}

Rect::Rect(const Rect & rhs)
	: x(rhs.x)
	, y(rhs.y)
	, width(rhs.width)
	, height(rhs.height)
{
}

void Rect::CreateBuffers(Renderer * renderer)
{
	if (!renderer)
	{
		LOG_ERROR("Invaild parameter, renderer is nullptr");
		return;
	}

	auto resolution = renderer->GetResolution();
	auto screen_left = -(resolution.x * 0.5f) + x;
	auto screen_right = screen_left + width;
	auto screen_top = (resolution.y * 0.5f) - y;
	auto screen_bottom = screen_top - height;

	std::vector<VertexTexture> vertices
	{
		VertexTexture(Vector3(screen_left,  screen_bottom,  0.0f), Vector2(0.0f, 1.0f)),
		VertexTexture(Vector3(screen_left,  screen_top,     0.0f), Vector2(0.0f, 0.0f)),
		VertexTexture(Vector3(screen_right, screen_bottom,  0.0f), Vector2(1.0f, 1.0f)),
		VertexTexture(Vector3(screen_right, screen_top,     0.0f), Vector2(1.0f, 0.0f)),
	};

	std::vector<uint> indices{ 0, 1, 2, 2, 1, 3 };

	vertex_buffer = std::make_shared<VertexBuffer>(renderer->GetContext());
	vertex_buffer->Create(vertices);

	index_buffer = std::make_shared<IndexBuffer>(renderer->GetContext());
	index_buffer->Create(indices);
}
