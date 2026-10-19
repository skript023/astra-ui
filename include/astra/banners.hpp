#pragma once
#include <span>
#include <string_view>

namespace astra
{
	struct banner
	{
		std::string_view name;
		// An encoded PNG or JPEG; the host decodes and uploads it with its own renderer.
		std::span<const unsigned char> image;
	};

	// Hosts save the chosen banner as an index into this list, so new banners go at the end.
	std::span<const banner> banners();
}
