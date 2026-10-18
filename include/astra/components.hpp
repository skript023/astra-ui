#pragma once
#include <astra/hud.hpp>
#include <imgui.h>
#include <cstddef>
#include <functional>
#include <span>
#include <string>
#include <string_view>

namespace astra
{
	// Controls for a game's own panels, so every panel in every game looks the same.
	// They use the current ImGui style; game hosts set it to the menu theme each frame (canvas::apply_panel_theme).
	// Labels may carry an ImGui "##id" suffix, which is not shown.

	// A framed slider with the value and suffix inside the frame and the label on its right; Ctrl+click types a value.
	// The value stays within the bounds. SetNextItemWidth before the call sets the frame width.
	bool slider(const char* label, int* value, int minimum, int maximum, const char* suffix = "");
	bool slider(const char* label, float* value, float minimum, float maximum, int precision = 2, const char* suffix = "");
	bool slider(const char* label, double* value, double minimum, double maximum, int precision = 2, const char* suffix = "");
	// A typed number. The int field has - and + buttons that move by step.
	bool input_number(const char* label, int* value, int step = 1);
	bool input_number(const char* label, float* value, int precision = 2, const char* suffix = "");
	// Three numbers side by side, such as a position or a rotation.
	bool input_vector3(const char* label, float* xyz, int precision = 2, const char* suffix = "");

	// The label above a full-width dropdown; returns true when the player picks an item.
	// A label that is only an "##id" leaves out the line above.
	bool choice(const char* label, int* current, std::span<const char* const> items);
	bool choice(const char* label, int* current, std::span<const std::string> items);
	bool checkbox(const char* label, bool* value);
	// A colour swatch beside its label; clicking the swatch opens a picker.
	bool color_picker(const char* label, ImVec4* color);

	bool button(const char* label, ImVec2 size = {0.f, 0.f});
	// A compact button for an action inside a list row, such as Remove or Teleport.
	bool small_button(const char* label);

	// hint is the grey text shown while the field is empty. flags are ImGuiInputTextFlags, such as
	// ImGuiInputTextFlags_EnterReturnsTrue to return true only when Enter is pressed.
	bool input_text(const char* label, char* buffer, std::size_t size, const char* hint = nullptr, ImGuiInputTextFlags flags = 0);
	// The std::string field grows as the player types.
	bool input_text(const char* label, std::string* text, const char* hint = nullptr, ImGuiInputTextFlags flags = 0);
	// Multi-line text, such as a script editor. A size of 0 on either axis takes the full width or eight lines.
	// ImGuiInputTextFlags_ReadOnly shows text that can be selected and copied; AllowTabInput types tabs.
	bool text_editor(const char* label, std::string* text, ImVec2 size = {-FLT_MIN, 0.f}, ImGuiInputTextFlags flags = 0);

	// A full-width search field, for the query that matches_search filters by.
	bool search_field(const char* id, char* buffer, std::size_t size, const char* hint = "Search");
	// True when text contains query, ignoring letter case. An empty query matches everything.
	bool matches_search(std::string_view text, std::string_view query);

	struct list_item
	{
		std::string label;
		bool selected = false;
		ImTextureID icon{};  // Drawn when list_options::icon_size is set; rows without one keep the empty square.
		std::string tooltip; // Shown while the row is hovered.
	};
	struct list_options
	{
		float height    = 0.f; // Of the bordered list. 0 fills the rest of the window; a negative value leaves that much room below.
		float icon_size = 0.f; // Leaves a square at the start of each row for its icon and makes rows that tall.
		const char* count_label = "Matches";   // Shown above the list with the row count; nullptr leaves it out.
		const char* empty_text  = "No matches."; // Shown inside the list when it has no rows.
	};
	// A bordered, scrolling list of selectable rows, such as search results. item(i) describes row i and is called
	// only for rows on screen, so a list of thousands stays cheap. Returns the row the player clicked, or -1.
	int match_list(const char* id, int count, const std::function<list_item(int)>& item, const list_options& options = {});

	struct action_row
	{
		std::string label;
		std::string detail; // Muted, on a line under the label, such as a prefab ID or coordinates.
	};
	struct row_action
	{
		int row    = -1;
		int action = -1; // Index into the actions passed to action_list.
	};
	// A bordered, scrolling list where every row ends in the same small buttons, such as Teleport and Delete. row(i)
	// describes row i and is called only for rows on screen. Returns the row and button the player clicked, or -1 for both.
	row_action action_list(const char* id, int count, const std::function<action_row(int)>& row,
		std::span<const char* const> actions, const list_options& options = {});

	enum class log_level
	{
		info,
		warning,
		error,
		input, // Code the player ran, such as a REPL line.
		output // What that code returned.
	};
	struct log_entry
	{
		std::string time;
		log_level level = log_level::info;
		std::string message;
	};
	// Which entries a log_view shows. Keep one per log, such as a static, so the player's choices last between frames.
	struct log_filter
	{
		bool info = true, warning = true, error = true, input_output = true;
		bool auto_scroll = true; // Follows new entries while the list is scrolled to the bottom.
		ImGuiTextFilter text;
	};
	// A console log: level checkboxes and a text filter, Copy and Clear, and a scrolling list with each entry's time
	// and level. Copy puts the entries on show on the clipboard. Returns true when the player presses Clear; the game
	// empties its own log. height 0 fills the rest of the window. font, when given, draws the entries, such as a
	// monospace font.
	bool log_view(const char* id, std::span<const log_entry> entries, log_filter& filter, float height = 0.f, ImFont* font = nullptr);

	// A header that folds away the controls under it; returns true while it is open.
	bool section(const char* label, bool open_by_default = false);
	// A bar filled to fraction (0 to 1), such as health or taming progress. With text, the bar is a full control
	// height and shows the text; without, it is a thin line. A height above 0 overrides both.
	void progress_bar(float fraction, const char* text = nullptr, float height = 0.f);
	// Text in the theme's accent, for the one line that matters most, such as the selected item.
	void text_accent(const char* format, ...) IM_FMTARGS(1);
	// Label and value rows in two columns, labels muted; an empty value reads "Not available".
	void details(const char* id, std::span<const overlay_row> rows);
	// Shows text in a tooltip while the control drawn just before is hovered.
	void tooltip(const char* text);
}
