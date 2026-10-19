#pragma once
#include "file_manager/file.hpp"

#include <atomic>
#include <filesystem>
#include <mutex>
#include <nlohmann/json.hpp>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>

namespace big
{
	class state_serializer;

	// settings.json in the mod folder: one object per state_serializer. The mod saves a component whenever it marks
	// itself dirty, and picks up edits another program makes to the file while the game runs, such as the Astra
	// loader switching a feature or setting its hotkey.
	class settings
	{
	private:
		std::filesystem::path m_settings_file;
		std::vector<state_serializer*> m_state_serializers;
		std::queue<state_serializer*> m_late_loaders;
		bool m_initial_load_done;
		nlohmann::json m_json;
		std::mutex m_mutex;
		// The file as the mod last read or wrote it, so an outside edit shows up as a diff.
		nlohmann::json m_disk_json;
		std::filesystem::file_time_type m_disk_time{};
		// Outside edits per component (key to new value, null when removed) waiting for the game thread.
		std::unordered_map<std::string, nlohmann::json> m_external;
		std::atomic_bool m_has_external{false};

	public:
		settings();

		static void initialize(file settingsFile)
		{
			get_instance().initialize_impl(settingsFile);
		}

		// Saves dirty components and reads outside edits. Any thread; the games call it once a second.
		static void tick()
		{
			get_instance().tick_impl();
		}

		// Hands outside edits to their components. Call it from the thread where features are switched on and off,
		// such as the game or script thread, once a frame.
		static void apply_external()
		{
			get_instance().apply_external_impl();
		}

		static void add_component(state_serializer* serializer)
		{
			get_instance().add_component_impl(serializer);
		}

		// TODO: this is broken
		static void load_component(state_serializer* serializer)
		{
			get_instance().load_component_impl(serializer);
		}

		static bool initial_load_done()
		{
			return get_instance().m_initial_load_done;
		}

		static std::string get_file_path()
		{
			return get_instance().m_settings_file.string();
		}

	private:
		static settings& get_instance()
		{
			static settings Instance;
			return Instance;
		}

		void initialize_impl(file settingsFile);
		void tick_impl();
		void add_component_impl(state_serializer* serializer);
		void load_component_impl(state_serializer* serializer);
		void save_component_impl(state_serializer* serializer);
		void reset();
		bool should_save();
		void read_external_changes();
		void apply_external_impl();
		void write_file();
	};
}
