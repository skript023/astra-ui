#include <astra/host/input_service.hpp>
#include <astra/host/canvas.hpp>
#include <astra/theme.hpp>
#include "fiber_pool.hpp"
#include <imgui.h>

#include <cstdio>
#include <cstring>

namespace big
{
	void input_service::show_impl(std::string_view title)
	{
		std::lock_guard lock(m_mutex);
		m_title = title;
		std::memset(m_input, 0, sizeof(m_input));
		m_type = INPUT_TEXT;
		m_focus_next = true;
		m_open = true;
		canvas::hold_mouse(true);
	}

	void input_service::show_impl(std::string_view title, input_callback_t cb)
	{
		std::lock_guard lock(m_mutex);
		show_impl(title);
		callback_impl(std::move(cb));
	}

	void input_service::show_autocomplete_impl(std::string_view title, std::vector<std::string> suggestions, input_callback_t cb)
	{
		std::lock_guard lock(m_mutex);
		m_title = title;
		std::memset(m_input, 0, sizeof(m_input));
		m_suggestions = std::move(suggestions);
		m_type = INPUT_AUTOCOMPLETE;
		m_focus_next = true;
		m_open = true;
		canvas::hold_mouse(true);
		callback_impl(std::move(cb));
	}

	void input_service::hide_impl()
	{
		std::lock_guard lock(m_mutex);
		m_title = "Input Here";
		std::memset(m_input, 0, sizeof(m_input));
		m_suggestions.clear();
		m_type = NONE;
		m_focus_next = false;
		m_open = false;
		canvas::hold_mouse(false);
	}

	void input_service::toggle_impl()
	{
		std::lock_guard lock(m_mutex);
		if (m_open.load())
			hide_impl();
		else
			show_impl(m_title);
	}

	void input_service::callback_impl(input_callback_t cb)
	{
		if (!cb)
			return;
		std::lock_guard lock(m_mutex);
		m_callback.push(std::move(cb));
	}

	void input_service::execute_impl()
	{
		input_callback_t job;
		std::string text;
		{
			std::lock_guard lock(m_mutex);
			if (!m_callback.empty())
			{
				job = std::move(m_callback.top());
				m_callback.pop();
			}
			text = m_input;
			hide_impl();
		}

		if (job)
		{
			g_fiber_pool->queue_job([job = std::move(job), text = std::move(text)] {
				std::invoke(job, text);
			});
		}
	}

	void input_service::cancel_impl()
	{
		std::lock_guard lock(m_mutex);
		m_callback = {};
		hide_impl();
	}

	void input_service::draw_impl()
	{
		if (!m_open.load())
			return;

		std::lock_guard lock(m_mutex);
		if (!m_open.load() || m_type == NONE)
			return;

		astra::scoped_theme style(astra::panel_theme(canvas::current_theme()));

		const auto display = ImGui::GetIO().DisplaySize;
		const float screen_x = display.x > 0.f ? display.x : 1920.f;
		const float screen_y = display.y > 0.f ? display.y : 1080.f;

		ImGui::SetNextWindowPos({screen_x * 0.25f, screen_y * 0.2f}, ImGuiCond_Always);
		ImGui::SetNextWindowSize({screen_x * 0.5f, -1.f});

		if (ImGui::Begin("Input###astra_input", nullptr,
		        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav))
		{
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {10.f, 15.f});
			ImGui::TextUnformatted(m_title.c_str());

			if (m_focus_next || !ImGui::IsAnyItemActive())
			{
				ImGui::SetKeyboardFocusHere(0);
				m_focus_next = false;
			}

			ImGui::SetNextItemWidth((screen_x * 0.5f) - 30.f);
			if (m_type == INPUT_AUTOCOMPLETE)
			{
				if (ImGui::InputTextWithHint("##InputBox", "Type something...", m_input, sizeof(m_input), ImGuiInputTextFlags_EnterReturnsTrue))
				{
					execute_impl();
				}
				else if (ImGui::IsKeyPressed(ImGuiKey_Escape))
				{
					cancel_impl();
				}

				if (m_open.load() && m_input[0] != '\0')
				{
					const std::string_view typed(m_input);
					for (const auto& item : m_suggestions)
					{
						if (item.find(typed) != std::string::npos)
						{
							if (ImGui::Selectable(item.c_str()))
							{
								std::snprintf(m_input, sizeof(m_input), "%s", item.c_str());
								execute_impl();
								break;
							}
						}
					}
				}
			}
			else
			{
				if (ImGui::InputText("##Input Here", m_input, sizeof(m_input), ImGuiInputTextFlags_EnterReturnsTrue))
				{
					execute_impl();
				}
				else if (ImGui::IsKeyPressed(ImGuiKey_Escape))
				{
					cancel_impl();
				}

				if (m_open.load())
					ImGui::Text("You entered: %s", m_input);
			}

			ImGui::PopStyleVar();
		}
		ImGui::End();
	}
}

