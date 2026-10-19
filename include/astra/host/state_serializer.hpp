#pragma once
#include <nlohmann/json.hpp>
#include <string>

namespace big
{
	// One top-level object of settings.json, such as a game's feature states or its hotkeys. Constructing it registers
	// it with settings, so a static instance loads with everything else at start.
	class state_serializer
	{
		std::string m_ser_component_name;
		bool m_is_dirty;

	public:
		state_serializer(const std::string& name);
		virtual void save_state_impl(nlohmann::json& state) = 0;
		virtual void load_state_impl(nlohmann::json& state) = 0;

		inline void save_state(nlohmann::json& state)
		{
			save_state_impl(state);
			m_is_dirty = false;
		}

		inline void load_state(nlohmann::json& state)
		{
			load_state_impl(state);
			m_is_dirty = false;
		}

		// Keys another program changed in the file while the game runs: `changes` holds each new value,
		// or null for a removed key, and `state` the whole component.
		virtual void apply_external_impl(nlohmann::json& state, const nlohmann::json& changes)
		{
			load_state_impl(state);
		}

		inline void apply_external(nlohmann::json& state, const nlohmann::json& changes)
		{
			apply_external_impl(state, changes);
		}

		inline bool is_state_dirty()
		{
			return m_is_dirty;
		}

		inline void mark_state_dirty()
		{
			m_is_dirty = true;
		}

		inline const std::string& get_serializer_component_name()
		{
			return m_ser_component_name;
		}
	};
}
