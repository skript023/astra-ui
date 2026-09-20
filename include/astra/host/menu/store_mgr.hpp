#pragma once
#include "common.hpp"

namespace big
{
	class store
	{
		std::unordered_map<std::string, bool> m_bool_store;
		std::unordered_map<std::string, int> m_int_store;
		std::unordered_map<std::string, float> m_float_store;
		static store& instance()
		{
			static store i{};
			return i;
		}

		void set_bool_impl(std::string key, bool value)
		{
			m_bool_store[key] = value;
		}
		void set_int_impl(std::string key, int value)
		{
			m_int_store[key] = value;
		}
		void set_float_impl(std::string key, float value)
		{
			m_float_store[key] = value;
		}
		bool get_bool_impl(const std::string& key, bool default_value = false)
		{
			if (auto it = m_bool_store.find(key); it != m_bool_store.end())
				return it->second;

			return default_value;
		}
		int get_int_impl(const std::string& key, int default_value = 0)
		{
			if (auto it = m_int_store.find(key); it != m_int_store.end())
				return it->second;

			return default_value;
		}
		float get_float_impl(const std::string& key, float default_value = 0.0f)
		{
			if (auto it = m_float_store.find(key); it != m_float_store.end())
				return it->second;

			return default_value;
		}
	public:
		static void set_bool(std::string key, bool value)
		{
			instance().set_bool_impl(std::move(key), value);
		}
		static void set_int(std::string key, int value)
		{
			instance().set_int_impl(std::move(key), value);
		}
		static void set_float(std::string key, float value)
		{
			instance().set_float_impl(std::move(key), value);
		}
		static bool get_bool(const std::string& key, bool default_value = false)
		{
			return instance().get_bool_impl(key, default_value);
		}
		static int get_int(const std::string& key, int default_value = 0)
		{
			return instance().get_int_impl(key, default_value);
		}
		static float get_float(const std::string& key, float default_value = 0.0f)
		{
			return instance().get_float_impl(key, default_value);
		}
	};
}
