#pragma once
#include <astra/theme.hpp>
#include <chrono>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace astra
{
	struct toast
	{
		std::string title, message;
		ImVec4 color;      // Marks the kind of notification: info, success, warning, and so on.
		float alpha = 1.f; // Fades the whole card out as the notification expires.
		std::string icon;  // Drawn before the title in the kind colour, such as a Font Awesome glyph; without one a dot marks the kind.
	};
	// Stacks notification cards down the top-right corner of the display, in the given order. Each card looks like
	// the overlay window: the icon and title in a title bar of the theme's panel colour, the message below on the
	// window colour, the theme's rounding, and a faint border. The kind colour is shifted toward the text colour as
	// far as it takes to read at WCAG AA on the title bar.
	void draw_notifications(std::span<const toast> toasts, const theme& colors);
	// The WCAG contrast ratio between two colours, from 1 to 21; alpha is ignored.
	float contrast_ratio(ImVec4 a, ImVec4 b);

	enum class notice
	{
		info,
		success,
		warning,
		error,
		protection, // Something the mod blocked, such as an illegal network event or a report.
	};
	// The colour that marks each kind of notification on its card.
	ImVec4 notice_color(notice kind);

	// Notifications waiting to be drawn. Any thread may push while the render thread takes the cards.
	class notification_queue
	{
	public:
		using clock = std::chrono::steady_clock;
		static constexpr float fade_seconds = .6f;

		// Shows for seconds, then fades out over fade_seconds. A title that is still on screen keeps its card: the
		// card takes the new kind, message, and icon in its place and starts its time again, so a feature toggled
		// on and off shows one card with its latest state.
		void push(notice kind, std::string title, std::string message, std::string icon = {}, float seconds = 3.f,
			clock::time_point now = clock::now());
		// The cards to draw, oldest first, with their fade applied; cards that have faded out are dropped.
		std::vector<toast> cards(clock::time_point now = clock::now());
		void clear();

	private:
		struct entry
		{
			notice kind;
			std::string title, message, icon;
			clock::time_point shown_at;
			float seconds;
		};
		std::mutex m_mutex;
		std::vector<entry> m_entries;
	};

	struct overlay_row
	{
		std::string label, value;
	};
	// A read-only window of label and value rows in the same style as a game's other panels. While interactive it
	// has a title bar and can be dragged; otherwise it is a bare read-out that takes no input. position and pivot
	// place it only the first time; after that ImGui keeps where it was left, saved in the host's ImGui ini file.
	void draw_overlay(std::string_view title, std::span<const overlay_row> rows, const theme& colors, ImVec2 position,
		ImVec2 pivot = {0.f, 0.f}, bool interactive = true);

	// The details window every game shows for a selected thing, such as an online player.
	struct info_window
	{
		std::string heading;           // Above a separator, such as the player's name.
		std::vector<overlay_row> rows; // "Label: value" lines. An empty value reads "Not available"; an empty label shows the value alone.
		std::string note;              // Below a closing separator, such as how often the data refreshes.
		std::string message;           // Shown on its own instead of the rest, such as while nothing is selected.
	};
	// A titled, resizable window. position and size apply only the first time, like any ImGui window.
	void draw_info_window(const char* title, const info_window& content, const theme& colors, ImVec2 position, ImVec2 size);

	// The card for what the player is looking at, such as an animal, a horse, or another player.
	struct inspector
	{
		std::string name;
		std::string tag;                  // Muted, after the name, such as "2 stars" or "Lvl 12".
		float bar = -1.f;                 // 0 to 1: a thin bar under the name, such as health. Below 0 leaves it out.
		std::vector<overlay_row> rows;    // An empty value reads "Not available".
		std::string hint;                 // One accent line under the rows, such as the hotkey that acts on the target.
		std::vector<std::string> actions; // Buttons along the bottom, shown only while interactive.
	};
	// A fixed-width card whose height follows its rows. While interactive it has a title bar, can be dragged, and
	// shows the action buttons; otherwise it is a bare card that takes no input. position and pivot place it only
	// the first time. Returns the index of the action the player clicked, or -1.
	int draw_inspector(const char* title, const inspector& content, const theme& colors, ImVec2 position,
		ImVec2 pivot = {0.f, 0.f}, bool interactive = true, float width = 300.f);

	struct panel_options
	{
		ImVec2 size{480.f, 420.f}; // When the panel first opens.
		ImVec2 min_size{0.f, 0.f}; // The smallest the player can resize it to. 0 on an axis keeps size as the floor.
		bool collapsible = true;   // The arrow in the title bar folds the panel down to its title bar.
		bool fit_content = false;  // Sizes the panel to its content instead, for a small panel with fixed content.
	};
	// The window a game panel opens in, such as a spawner, a browser, or a console. The player can resize it, but
	// not below min_size, so the content is never squeezed until it is cut off. position and size apply only the
	// first time; after that ImGui keeps what the player chose. A null open leaves out the close button. Returns true
	// when the contents should be drawn; call end_panel whatever it returns.
	bool begin_panel(const char* title, bool* open, ImVec2 position, const panel_options& options = {});
	void end_panel();
}
