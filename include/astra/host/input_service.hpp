#pragma once
#include <atomic>
#include <functional>
#include <mutex>
#include <stack>
#include <string>
#include <string_view>
#include <vector>

namespace big
{
	using input_callback_t = std::function<void(std::string const&)>;

	enum eInputType
	{
		NONE,
		INPUT_TEXT,
		INPUT_AUTOCOMPLETE
	};

	// On-screen text prompt shared across game hosts. Drawn each frame by canvas::draw_input() in the menu's theme,
	// holding the mouse and keyboard until the player submits with Enter or cancels with Escape.
	class input_service
	{
		static input_service& get()
		{
			static input_service instance{};
			return instance;
		}

	public:
		input_service() = default;
		~input_service() noexcept = default;

		static void show(std::string_view title)
		{
			get().show_impl(title);
		}

		static void show(std::string_view title, input_callback_t cb)
		{
			get().show_impl(title, std::move(cb));
		}

		template <typename Container, typename Extractor>
		static void show_with_autocomplete(std::string_view title, Container const& data, Extractor extractor, input_callback_t cb)
		{
			std::vector<std::string> suggestions;
			for (const auto& item : data)
				suggestions.emplace_back(extractor(item));
			get().show_autocomplete_impl(title, std::move(suggestions), std::move(cb));
		}

		static void hide()
		{
			get().hide_impl();
		}

		static void toggle()
		{
			get().toggle_impl();
		}

		static void callback(input_callback_t cb)
		{
			get().callback_impl(std::move(cb));
		}

		static void execute()
		{
			get().execute_impl();
		}

		static void cancel()
		{
			get().cancel_impl();
		}

		static bool is_open()
		{
			return get().m_open.load();
		}

		static const char* title()
		{
			return get().m_title.c_str();
		}

		static eInputType type()
		{
			return get().m_type;
		}

		static std::vector<std::string> suggestion()
		{
			std::lock_guard lock(get().m_mutex);
			return get().m_suggestions;
		}

		static void draw()
		{
			get().draw_impl();
		}

	private:
		void show_impl(std::string_view title);
		void show_impl(std::string_view title, input_callback_t cb);
		void show_autocomplete_impl(std::string_view title, std::vector<std::string> suggestions, input_callback_t cb);
		void hide_impl();
		void toggle_impl();
		void callback_impl(input_callback_t cb);
		void execute_impl();
		void cancel_impl();
		void draw_impl();

		std::atomic_bool m_open{false};
		bool m_focus_next = false;
		char m_input[256]{};
		std::string m_title = "Input Here";
		std::stack<input_callback_t> m_callback;
		std::recursive_mutex m_mutex;
		eInputType m_type = NONE;
		std::vector<std::string> m_suggestions;
	};

	inline input_service g_input_service{};
}

