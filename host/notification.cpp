#include <astra/host/notification.hpp>
#include "fonts/icon_list.hpp"

namespace big
{
	void notification::info(std::string title, std::string message)
	{
		queue().push(astra::notice::info, std::move(title), std::move(message), ICON_FA_INFO_CIRCLE);
	}

	void notification::success(std::string title, std::string message)
	{
		queue().push(astra::notice::success, std::move(title), std::move(message), ICON_FA_CHECK_CIRCLE);
	}

	void notification::warning(std::string title, std::string message)
	{
		queue().push(astra::notice::warning, std::move(title), std::move(message), ICON_FA_EXCLAMATION_TRIANGLE);
	}

	void notification::error(std::string title, std::string message)
	{
		queue().push(astra::notice::error, std::move(title), std::move(message), ICON_FA_TIMES_CIRCLE);
	}

	void notification::protection(std::string message)
	{
		queue().push(astra::notice::protection, "Illegal Event Protection", std::move(message), ICON_FA_SHIELD_ALT);
	}

	void notification::report(std::string message)
	{
		queue().push(astra::notice::protection, "Report Protection", std::move(message), ICON_FA_USER_SHIELD);
	}

	astra::notification_queue& notification::queue()
	{
		static astra::notification_queue instance;
		return instance;
	}
}
