#pragma once
#include <astra/hud.hpp>
#include <string>

namespace big
{
	// The notifications every game shows the same way: cards down the top-right corner, drawn each frame by
	// canvas::draw_notifications. Callable from any thread, such as a game or script thread.
	class notification final
	{
	public:
		static void info(std::string title, std::string message);
		static void success(std::string title, std::string message);
		static void warning(std::string title, std::string message);
		static void error(std::string title, std::string message);
		// A network event the mod blocked, under the title "Illegal Event Protection".
		static void protection(std::string message);
		// A report against the player that the mod blocked, under the title "Report Protection".
		static void report(std::string message);

		static astra::notification_queue& queue();
	};
}
