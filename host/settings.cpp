#include <astra/host/settings.hpp>
#include <astra/host/state_serializer.hpp>

namespace big
{
	state_serializer::state_serializer(const std::string& name) :
	    m_ser_component_name(name),
	    m_is_dirty(false)
	{
		settings::add_component(this);
	}

	settings::settings() :
	    m_settings_file(),
	    m_state_serializers(),
	    m_initial_load_done(false)
	{
	}

	void settings::initialize_impl(file settingsFile)
	{
		m_settings_file = settingsFile.get_path();

		// A missing or reset file still loads every component, or nothing is saved until the next launch.
		if (!settingsFile.exists())
		{
			reset();
		}
		else
		{
			std::ifstream file(m_settings_file);

			try
			{
				file >> m_json;
				file.close();
			}
			catch (std::exception&)
			{
				LOG(WARNING) << "Detected corrupt settings, resetting settings...";
				reset();
			}

			if (!m_json.is_object())
				reset();
		}

		for (auto& serializer : m_state_serializers)
			load_component_impl(serializer);

		// Tells the Astra loader that this process picks up settings.json edits while the game runs.
		m_json["astra"] = {{"live_reload", 1}, {"pid", GetCurrentProcessId()}};
		write_file();

		LOG(VERBOSE) << "All settings loaded";
		m_initial_load_done = true;
	}

	void settings::tick_impl()
	{
		std::lock_guard lock(m_mutex);
		while (!m_late_loaders.empty())
		{
			if (auto component = std::move(m_late_loaders.front()))
			{
				load_component(component);
			}

			m_late_loaders.pop();
		}

		if (!m_initial_load_done)
			return;

		// Read outside edits before saving, so a save never writes over them.
		read_external_changes();

		if (should_save())
		{
			for (auto& serializer : m_state_serializers)
				// A component with outside edits still waiting for the game thread saves once they apply.
				if (serializer->is_state_dirty() && !m_external.contains(serializer->get_serializer_component_name()))
					save_component_impl(serializer);

			write_file();
		}
	}

	void settings::read_external_changes()
	{
		std::error_code error;
		const auto time = std::filesystem::last_write_time(m_settings_file, error);
		if (error || time == m_disk_time)
			return;

		nlohmann::json disk;
		try
		{
			std::ifstream file(m_settings_file);
			file >> disk;
		}
		catch (std::exception&)
		{
			// The time is not recorded, so the next tick reads the file again.
			return;
		}
		if (!disk.is_object())
			return;
		m_disk_time = time;

		// Only keys that differ from what the mod last read or wrote count as outside edits, so an
		// in-game change that is not saved yet survives.
		const auto empty = nlohmann::json::object();
		for (auto& serializer : m_state_serializers)
		{
			const auto& name = serializer->get_serializer_component_name();
			const auto& now = disk.contains(name) && disk.at(name).is_object() ? disk.at(name) : empty;
			const auto& before = m_disk_json.contains(name) && m_disk_json.at(name).is_object() ? m_disk_json.at(name) : empty;
			auto& component = m_json[name];
			if (!component.is_object())
				component = nlohmann::json::object();

			for (const auto& [key, value] : now.items())
			{
				if (!before.contains(key) || before.at(key) != value)
				{
					component[key] = value;
					m_external[name][key] = value;
				}
			}
			for (const auto& [key, value] : before.items())
			{
				if (!now.contains(key))
				{
					component.erase(key);
					m_external[name][key] = nullptr;
				}
			}
		}

		m_disk_json = std::move(disk);
		if (!m_external.empty())
			m_has_external = true;
	}

	void settings::apply_external_impl()
	{
		if (!m_has_external)
			return;

		struct pending
		{
			state_serializer* serializer;
			nlohmann::json state;
			nlohmann::json changes;
		};
		std::vector<pending> work;
		{
			std::lock_guard lock(m_mutex);
			for (auto& serializer : m_state_serializers)
				if (auto it = m_external.find(serializer->get_serializer_component_name()); it != m_external.end())
					work.push_back({serializer, m_json[it->first], it->second});
			m_external.clear();
			m_has_external = false;
		}

		for (auto& item : work)
			item.serializer->apply_external(item.state, item.changes);
	}

	// Writes a temporary file and renames it, so a reader never sees half a file.
	void settings::write_file()
	{
		auto temp = m_settings_file;
		temp += ".tmp";
		{
			std::ofstream file(temp, std::ios::out | std::ios::trunc);
			file << m_json.dump(4);
		}

		// The rename keeps the temporary file's time, which is how the next tick knows this write was ours.
		std::error_code error;
		const auto time = std::filesystem::last_write_time(temp, error);
		std::filesystem::rename(temp, m_settings_file, error);
		if (error)
		{
			std::ofstream file(m_settings_file, std::ios::out | std::ios::trunc);
			file << m_json.dump(4);
			file.close();
			m_disk_time = std::filesystem::last_write_time(m_settings_file, error);
		}
		else
		{
			m_disk_time = time;
		}
		m_disk_json = m_json;
	}

	void settings::add_component_impl(state_serializer* serializer)
	{
		std::lock_guard lock(m_mutex);
		m_state_serializers.push_back(serializer);
		if (m_initial_load_done)
			m_late_loaders.push(serializer);
	}

	void settings::load_component_impl(state_serializer* serializer)
	{
		LOG(VERBOSE) << "Loading component: " << serializer->get_serializer_component_name();

		if (!m_json.contains(serializer->get_serializer_component_name()) || !m_json[serializer->get_serializer_component_name()].is_object())
			m_json[serializer->get_serializer_component_name()] = nlohmann::json::object();

		serializer->load_state(m_json[serializer->get_serializer_component_name()]);
	}

	void settings::save_component_impl(state_serializer* serializer)
	{
		serializer->save_state(m_json[serializer->get_serializer_component_name()]);
	}

	void settings::reset()
	{
		std::ofstream file(m_settings_file, std::ios::out | std::ios::trunc);
		file << "{}" << std::endl;
		file.close();
		m_json = nlohmann::json::object();
	}

	bool settings::should_save()
	{
		for (auto& serializer : m_state_serializers)
			if (serializer->is_state_dirty())
				return true;

		return false;
	}
}
