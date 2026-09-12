#include "Settings.hpp"

#include "IStateSerializer.hpp"
#include "Settings.hpp"


namespace YimMenu
{
	Settings::Settings() :
	    m_SettingsFile(),
	    m_StateSerializers(),
	    m_InitialLoadDone(false)
	{
	}

	void Settings::InitializeImpl(File settingsFile)
	{
		m_SettingsFile = settingsFile;

		if (!settingsFile.Exists())
		{
			Reset();
			return;
		}

		std::ifstream file(m_SettingsFile);

		try
		{
			file >> m_Json;
			file.close();

			if (!m_Json.is_object())
			{
				LOG(WARNING) << "Settings root is not an object, resetting settings...";
				Reset();
				return;
			}
		}
		catch (std::exception&)
		{
			LOG(WARNING) << "Detected corrupt settings, resetting settings...";
			Reset();
			return;
		}

		for (auto& serializer : m_StateSerializers)
			LoadComponentImpl(serializer);

		LOG(VERBOSE) << "All settings loaded";
		m_InitialLoadDone = true;
	}

	void Settings::TickImpl()
	{
		std::string pendingWrite;
		{
			std::lock_guard lock(m_Mutex);
			while (!m_LateLoaders.empty())
			{
				if (auto component = std::move(m_LateLoaders.front()))
				{
					LoadComponentImpl(component);
				}

				m_LateLoaders.pop();
			}

			if (m_InitialLoadDone && ShouldSave())
			{
				for (auto& serializer : m_StateSerializers)
					if (serializer->IsStateDirty())
						SaveComponentImpl(serializer);

				// Build a stable snapshot while protected, then perform disk I/O after
				// releasing the mutex so settings users are never blocked by the drive.
				pendingWrite = m_Json.dump(4);
			}
		}

		if (!pendingWrite.empty())
		{
			std::ofstream file(m_SettingsFile, std::ios::out | std::ios::trunc);
			if (file.is_open())
			{
				file << pendingWrite;
			}
			else
			{
				LOG(WARNING) << "Unable to save settings file";
			}
		}
	}

	void Settings::AddComponentImpl(IStateSerializer* serializer)
	{
		std::lock_guard lock(m_Mutex);
		m_StateSerializers.push_back(serializer);
		if (m_InitialLoadDone)
			m_LateLoaders.push(serializer);
	}

	void Settings::LoadComponentImpl(IStateSerializer* serializer)
	{
		LOG(VERBOSE) << "Loading component: " << serializer->GetSerializerComponentName();

		if (!m_Json.contains(serializer->GetSerializerComponentName()) || !m_Json[serializer->GetSerializerComponentName()].is_object())
			m_Json[serializer->GetSerializerComponentName()] = nlohmann::json::object();

		serializer->LoadState(m_Json[serializer->GetSerializerComponentName()]);
	}

	void Settings::SaveComponentImpl(IStateSerializer* serializer)
	{
		//LOG(VERBOSE) << "Saving component: " << serializer->GetSerializerComponentName();
		serializer->SaveState(m_Json[serializer->GetSerializerComponentName()]);
	}

	void Settings::Reset()
	{
		m_Json = nlohmann::json::object();

		std::ofstream file(m_SettingsFile, std::ios::out | std::ios::trunc);
		if (file.is_open())
			file << m_Json.dump(4) << std::endl;
		else
			LOG(WARNING) << "Unable to reset settings file";

		m_InitialLoadDone = true;
	}

	bool Settings::ShouldSave()
	{
		for (auto& serializer : m_StateSerializers)
			if (serializer->IsStateDirty())
				return true;

		return false;
	}
}