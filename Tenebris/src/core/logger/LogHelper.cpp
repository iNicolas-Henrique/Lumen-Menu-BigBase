#include "LogHelper.hpp"
#include "LogSink.hpp"

#include "core/filemgr/FileMgr.hpp"

namespace YimMenu
{
	template<typename TP>
	static std::time_t to_time_t(TP tp)
	{
		using namespace std::chrono;
		auto sctp = time_point_cast<system_clock::duration>(tp - TP::clock::now() + system_clock::now());
		return system_clock::to_time_t(sctp);
	}

	void LogHelper::Destroy()
	{
		GetInstance().DestroyImpl();
	}

	bool LogHelper::Init(const std::string_view consoleName, File file, const bool attachConsole)
	{
		return GetInstance().InitImpl(consoleName, file, attachConsole);
	}

	void LogHelper::ToggleConsole(bool toggle)
	{
		GetInstance().ToggleConsoleImpl(toggle);
	}

	void LogHelper::DestroyImpl()
	{
		Logger::Destroy();
		CloseOutputStreams();

		if (m_DidConsoleExist)
			SetConsoleMode(m_ConsoleHandle, m_OriginalConsoleMode);

		if (!m_DidConsoleExist && m_AttachConsole)
			FreeConsole();
	}

	bool LogHelper::InitImpl(const std::string_view consoleName, File file, const bool attachConsole)
	{
		m_ConsoleTitle  = consoleName;
		m_File          = file;
		m_AttachConsole = attachConsole;

		if (m_AttachConsole)
		{
			if (m_DidConsoleExist = ::AttachConsole(GetCurrentProcessId()); !m_DidConsoleExist)
				AllocConsole();

			if (m_ConsoleHandle = GetStdHandle(STD_OUTPUT_HANDLE); m_ConsoleHandle != nullptr)
			{
				SetConsoleTitleA(m_ConsoleTitle.data());
				SetConsoleOutputCP(CP_UTF8);

				DWORD consoleMode;
				GetConsoleMode(m_ConsoleHandle, &consoleMode);
				m_OriginalConsoleMode = consoleMode;

				consoleMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN;
				consoleMode &= ~(ENABLE_QUICK_EDIT_MODE);

				SetConsoleMode(m_ConsoleHandle, consoleMode);
			}
		}

		AttemptCreateBackup();
		OpenOutputStreams();

		static constexpr auto flushInterval = std::chrono::milliseconds(250);
		Logger::Init();
		Logger::AddSink([this](LogMessagePtr msg) {
			if (!m_AttachConsole || !m_ConsoleOut.is_open())
				return;

			m_ConsoleOut << LogSink::FormatConsole(msg);
			const auto now = std::chrono::steady_clock::now();
			const bool urgent = msg && msg->Level() >= al::WARNING;
			if (urgent || now >= m_NextConsoleFlush)
			{
				m_ConsoleOut.flush();
				m_NextConsoleFlush = now + flushInterval;
			}
		});
		Logger::AddSink([this](LogMessagePtr msg) {
			if (!m_FileOut.is_open())
				return;

			m_FileOut << LogSink::FormatFile(msg);
			const auto now = std::chrono::steady_clock::now();
			const bool urgent = msg && msg->Level() >= al::WARNING;
			if (urgent || now >= m_NextFileFlush)
			{
				m_FileOut.flush();
				m_NextFileFlush = now + flushInterval;
			}
		});

		return true;
	}

	void LogHelper::ToggleConsoleImpl(bool toggle)
	{
		if (m_DidConsoleExist)
			return;

		if (m_AttachConsole)
		{
			if (m_ConsoleOut.is_open())
			{
				m_ConsoleOut.flush();
				m_ConsoleOut.close();
			}
			FreeConsole();
		}
		else
		{
			AllocConsole();
			m_ConsoleOut.clear();
			m_ConsoleOut.open("CONOUT$", std::ios_base::out | std::ios_base::app);
			m_NextConsoleFlush = {};
		}

		m_AttachConsole = !m_AttachConsole;
	}

	void LogHelper::CloseOutputStreams()
	{
		if (m_AttachConsole && m_ConsoleOut.is_open())
		{
			m_ConsoleOut.flush();
			m_ConsoleOut.close();
		}
		if (m_FileOut.is_open())
		{
			m_FileOut.flush();
			m_FileOut.close();
		}
	}

	void LogHelper::OpenOutputStreams()
	{
		if (m_AttachConsole)
			m_ConsoleOut.open("CONOUT$", std::ios_base::out | std::ios_base::app);
		m_FileOut.open(m_File, std::ios::out | std::ios::trunc);
		m_NextConsoleFlush = {};
		m_NextFileFlush = {};
	}

	void LogHelper::AttemptCreateBackup()
	{
		if (m_File.Exists())
		{
			auto file_time  = std::filesystem::last_write_time(m_File.Path());
			auto time_t     = to_time_t(file_time);
			auto local_time = std::localtime(&time_t);

			m_File.Move(std::format("./backup/{:0>2}-{:0>2}-{}-{:0>2}-{:0>2}-{:0>2}_{}",
			    local_time->tm_mon + 1,
			    local_time->tm_mday,
			    local_time->tm_year + 1900,
			    local_time->tm_hour,
			    local_time->tm_min,
			    local_time->tm_sec,
			    m_File.Path().filename().string().c_str()));
		}
	}
}
