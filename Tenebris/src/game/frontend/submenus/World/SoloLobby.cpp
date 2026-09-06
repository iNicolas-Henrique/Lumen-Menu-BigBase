#include "SoloLobby.hpp"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <string_view>

namespace YimMenu::Submenus
{
	namespace
	{
		constexpr std::string_view kTenebrisMarker = "<!-- TENEBRIS_SOLO_LOBBY -->";

		enum class LobbyFileState
		{
			Public,
			Tenebris,
			External,
			Unavailable,
		};

		struct LobbyPaths
		{
			std::filesystem::path DataDir;
			std::filesystem::path StartupMeta;
			std::filesystem::path Backup;
		};

		LobbyPaths GetLobbyPaths()
		{
			std::array<wchar_t, 32768> modulePath{};
			const DWORD length = GetModuleFileNameW(nullptr, modulePath.data(), static_cast<DWORD>(modulePath.size()));
			if (length == 0 || length >= modulePath.size())
				return {};

			const std::filesystem::path exePath(std::wstring(modulePath.data(), length));
			LobbyPaths paths;
			paths.DataDir = exePath.parent_path() / L"x64" / L"data";
			paths.StartupMeta = paths.DataDir / L"startup.meta";
			paths.Backup = paths.DataDir / L"startup.meta.tenebris.bak";
			return paths;
		}

		bool ReadTextFile(const std::filesystem::path& path, std::string& out)
		{
			std::ifstream stream(path, std::ios::binary);
			if (!stream)
				return false;

			out.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
			return stream.good() || stream.eof();
		}

		bool IsTenebrisFile(const std::filesystem::path& path)
		{
			std::string contents;
			return ReadTextFile(path, contents) && contents.find(kTenebrisMarker) != std::string::npos;
		}

		LobbyFileState DetectLobbyState(const LobbyPaths& paths)
		{
			if (paths.DataDir.empty())
				return LobbyFileState::Unavailable;

			std::error_code ec;
			if (!std::filesystem::is_directory(paths.DataDir, ec) || ec)
				return LobbyFileState::Unavailable;
			if (!std::filesystem::exists(paths.StartupMeta, ec))
				return ec ? LobbyFileState::Unavailable : LobbyFileState::Public;
			if (ec)
				return LobbyFileState::Unavailable;

			return IsTenebrisFile(paths.StartupMeta) ? LobbyFileState::Tenebris : LobbyFileState::External;
		}

		const char* StateText(LobbyFileState state)
		{
			switch (state)
			{
				case LobbyFileState::Public: return "PUBLICO / sem startup.meta";
				case LobbyFileState::Tenebris: return "SOLO/PRIVADO configurado pelo Tenebris";
				case LobbyFileState::External: return "startup.meta externo detectado";
				case LobbyFileState::Unavailable: return "caminho do RDR2 indisponivel";
			}
			return "desconhecido";
		}

		std::string SanitizeLobbyCode(std::string_view value)
		{
			std::string result;
			result.reserve(std::min<std::size_t>(value.size(), 48));
			for (unsigned char ch : value)
			{
				if (std::isalnum(ch) || ch == '-' || ch == '_')
				{
					result.push_back(static_cast<char>(ch));
					if (result.size() == 48)
						break;
				}
			}
			return result;
		}

		std::string GenerateLobbyCode()
		{
			static constexpr std::string_view alphabet = "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789";
			std::random_device rd;
			std::mt19937 rng(rd());
			std::uniform_int_distribution<std::size_t> pick(0, alphabet.size() - 1);
			std::string result = "TENEBRIS-";
			for (int i = 0; i < 20; ++i)
				result.push_back(alphabet[pick(rng)]);
			return result;
		}

		std::string BuildStartupMeta(std::string_view code)
		{
			std::string xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<CDataFileMgr__ContentsOfDataFileXml>
 <disabledFiles />
 <includedXmlFiles itemType="CDataFileMgr__DataFileArray" />
 <includedDataFiles />
 <dataFiles itemType="CDataFileMgr__DataFile">
  <Item>
   <filename>platform:/data/cdimages/scaleform_platform_pc.rpf</filename>
   <fileType>RPF_FILE</fileType>
  </Item>
  <Item>
   <filename>platform:/data/ui/value_conversion.rpf</filename>
   <fileType>RPF_FILE</fileType>
  </Item>
  <Item>
   <filename>platform:/data/ui/widgets.rpf</filename>
   <fileType>RPF_FILE</fileType>
  </Item>
  <Item>
   <filename>platform:/textures/ui/ui_photo_stickers.rpf</filename>
   <fileType>RPF_FILE</fileType>
  </Item>
  <Item>
   <filename>platform:/textures/ui/ui_platform.rpf</filename>
   <fileType>RPF_FILE</fileType>
  </Item>
  <Item>
   <filename>platform:/data/ui/stylesCatalog</filename>
   <fileType>aWeaponizeDisputants</fileType>
  </Item>
  <Item>
   <filename>platform:/data/cdimages/scaleform_frontend.rpf</filename>
   <fileType>RPF_FILE_PRE_INSTALL</fileType>
  </Item>
  <Item>
   <filename>platform:/textures/ui/ui_startup_textures.rpf</filename>
   <fileType>RPF_FILE</fileType>
  </Item>
  <Item>
   <filename>platform:/data/ui/startup_data.rpf</filename>
   <fileType>RPF_FILE</fileType>
  </Item>
 </dataFiles>
 <contentChangeSets itemType="CDataFileMgr__ContentChangeSet" />
 <patchFiles />
 <!-- TENEBRIS_SOLO_LOBBY -->
</CDataFileMgr__ContentsOfDataFileXml>===)";
			xml.append(code);
			xml.append("===\r\n");
			return xml;
		}

		bool WriteAtomically(const std::filesystem::path& destination, std::string_view contents, std::string& error)
		{
			const auto temp = destination.wstring() + L".tenebris.tmp";
			{
				std::ofstream stream(std::filesystem::path(temp), std::ios::binary | std::ios::trunc);
				if (!stream)
				{
					error = "Nao foi possivel criar o arquivo temporario. Verifique permissao de escrita na pasta do RDR2.";
					return false;
				}
				stream.write(contents.data(), static_cast<std::streamsize>(contents.size()));
				stream.flush();
				if (!stream)
				{
					error = "Falha ao gravar o startup.meta temporario.";
					return false;
				}
			}

			if (!MoveFileExW(temp.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
			{
				const DWORD winError = GetLastError();
				DeleteFileW(temp.c_str());
				error = "Windows recusou a substituicao do startup.meta. Codigo: " + std::to_string(winError);
				return false;
			}
			return true;
		}

		bool ApplyPrivateLobby(const LobbyPaths& paths, std::string_view requestedCode, std::string& status)
		{
			const std::string code = SanitizeLobbyCode(requestedCode);
			if (code.size() < 4)
			{
				status = "Use um codigo com pelo menos 4 caracteres alfanumericos.";
				return false;
			}

			std::error_code ec;
			if (!std::filesystem::is_directory(paths.DataDir, ec) || ec)
			{
				status = "Nao encontrei a pasta x64\\data do RDR2 a partir do executavel atual.";
				return false;
			}

			const bool metaExists = std::filesystem::exists(paths.StartupMeta, ec) && !ec;
			const bool backupExists = std::filesystem::exists(paths.Backup, ec) && !ec;
			if (metaExists && !IsTenebrisFile(paths.StartupMeta))
			{
				if (backupExists)
				{
					status = "Ja existe um startup.meta externo e um backup Tenebris. Nenhum arquivo foi alterado.";
					return false;
				}
				if (!CopyFileW(paths.StartupMeta.c_str(), paths.Backup.c_str(), TRUE))
				{
					status = "Falha ao fazer backup do startup.meta existente. Codigo Windows: " + std::to_string(GetLastError());
					return false;
				}
			}

			std::string error;
			if (!WriteAtomically(paths.StartupMeta, BuildStartupMeta(code), error))
			{
				status = error + " O arquivo anterior/backup foi preservado.";
				return false;
			}

			status = "Lobby privado configurado com codigo '" + code + "'. Reinicie o RDR2 para a mudanca valer.";
			return true;
		}

		bool RestorePublicLobby(const LobbyPaths& paths, std::string& status)
		{
			std::error_code ec;
			const bool metaExists = std::filesystem::exists(paths.StartupMeta, ec) && !ec;
			const bool backupExists = std::filesystem::exists(paths.Backup, ec) && !ec;

			if (metaExists && !IsTenebrisFile(paths.StartupMeta))
			{
				status = "O startup.meta atual nao pertence ao Tenebris. Por seguranca ele nao foi removido.";
				return false;
			}

			if (backupExists)
			{
				if (!MoveFileExW(paths.Backup.c_str(), paths.StartupMeta.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
				{
					status = "Falha ao restaurar o backup original. Codigo Windows: " + std::to_string(GetLastError());
					return false;
				}
				status = "startup.meta anterior restaurado. Reinicie o RDR2 para aplicar.";
				return true;
			}

			if (metaExists && !DeleteFileW(paths.StartupMeta.c_str()))
			{
				status = "Falha ao remover o startup.meta do Tenebris. Codigo Windows: " + std::to_string(GetLastError());
				return false;
			}

			status = metaExists ? "Modo privado removido. Reinicie o RDR2 para voltar ao matchmaking normal." : "Nenhum startup.meta do Tenebris esta ativo.";
			return true;
		}
	}

	void RenderSoloLobbyMenu()
	{
		static std::array<char, 64> lobbyCode{};
		static bool initialized = false;
		static std::string status = "Pronto.";
		static LobbyFileState cachedState = LobbyFileState::Unavailable;
		static auto nextStateRefresh = std::chrono::steady_clock::time_point{};

		if (!initialized)
		{
			const std::string generated = GenerateLobbyCode();
			std::copy_n(generated.c_str(), std::min(generated.size(), lobbyCode.size() - 1), lobbyCode.data());
			initialized = true;
		}

		const LobbyPaths paths = GetLobbyPaths();
		const auto now = std::chrono::steady_clock::now();
		if (now >= nextStateRefresh)
		{
			cachedState = DetectLobbyState(paths);
			nextStateRefresh = now + std::chrono::seconds(1);
		}

		ImGui::TextWrapped("Metodo startup.meta: cria um lobby isolado no proximo inicio do RDR2. O jogo precisa ser reiniciado depois de ativar, trocar o codigo ou restaurar o lobby publico.");
		ImGui::Separator();
		ImGui::Text("Estado do arquivo: %s", StateText(cachedState));
		if (!paths.StartupMeta.empty())
			ImGui::TextWrapped("Destino: %s", paths.StartupMeta.string().c_str());

		ImGui::InputText("Codigo privado", lobbyCode.data(), lobbyCode.size());
		if (ImGui::Button("Gerar codigo aleatorio"))
		{
			lobbyCode.fill('\0');
			const std::string generated = GenerateLobbyCode();
			std::copy_n(generated.c_str(), std::min(generated.size(), lobbyCode.size() - 1), lobbyCode.data());
		}
		ImGui::SameLine();
		if (ImGui::Button("Atualizar estado"))
		{
			cachedState = DetectLobbyState(paths);
			nextStateRefresh = now + std::chrono::seconds(1);
		}

		if (ImGui::Button("Ativar solo/privado no proximo inicio"))
		{
			ApplyPrivateLobby(paths, lobbyCode.data(), status);
			cachedState = DetectLobbyState(paths);
			nextStateRefresh = now + std::chrono::seconds(1);
		}
		ImGui::SameLine();
		if (ImGui::Button("Restaurar lobby publico"))
		{
			RestorePublicLobby(paths, status);
			cachedState = DetectLobbyState(paths);
			nextStateRefresh = now + std::chrono::seconds(1);
		}

		ImGui::Separator();
		ImGui::TextWrapped("Status: %s", status.c_str());
		ImGui::TextWrapped("Para jogar com amigos em lobby privado, todos precisam usar o mesmo codigo e a mesma versao deste gerador antes de iniciar o jogo.");
		ImGui::TextWrapped("Protecao de arquivos: se ja existir um startup.meta que nao foi criado pelo Tenebris, ele e salvo como startup.meta.tenebris.bak antes da substituicao. Na restauracao, o Tenebris nao apaga arquivos externos que nao tenham sua marca.");
	}
}
