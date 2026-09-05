#include "Notifications.hpp"

#include "core/logger/LogHelper.hpp"
#include "game/backend/FiberPool.hpp"
#include "util/Joaat.hpp"

#include <algorithm>
#include <mutex>

namespace YimMenu
{
	namespace
	{
		NotificationPlacement ResolvePlacement(const std::string& title, NotificationPlacement requested)
		{
			if (requested != NotificationPlacement::Left)
				return requested;

			if (title == "Protections" || title == "Protection" || title == "Proteções" || title == "Proteção")
				return NotificationPlacement::Right;

			if (title == "Teleport" || title == "Teleporte" || title == "Waypoint" || title == "Camp" ||
			    title == "Moonshine Shack" || title == "Madam Nazar" || title == "Guarma")
				return NotificationPlacement::TopCenter;

			return requested;
		}
	}

	Notification Notifications::ShowImpl(std::string title,
	    std::string message,
	    NotificationType type,
	    int duration,
	    std::function<void()> context_function,
	    std::string context_function_name,
	    NotificationPlacement placement)
	{
		if (title.empty() || message.empty())
			return {};

		placement = ResolvePlacement(title, placement);
		auto message_id = Joaat(title + message + std::to_string(static_cast<int>(placement)));
		std::lock_guard<std::mutex> lock(m_mutex);
		auto exists = std::find_if(m_Notifications.begin(), m_Notifications.end(), [&](auto& notification) {
			return notification.second.m_Identifier == message_id;
		});

		if (exists != m_Notifications.end())
		{
			exists->second.m_CreatedOn = std::chrono::system_clock::now();
			exists->second.m_Erasing = false;
			return exists->second;
		}

		Notification notification{};
		notification.m_Title = title;
		notification.m_Message = message;
		notification.m_Type = type;
		notification.m_Placement = placement;
		notification.m_CreatedOn = std::chrono::system_clock::now();
		notification.m_Duration = duration;
		notification.m_Identifier = message_id;
		notification.m_AnimationOffset = placement == NotificationPlacement::TopCenter ? -22.0f : -m_CardSizeX;
		notification.m_Alpha = 0.0f;

		if (context_function)
		{
			notification.m_ContextFunc = context_function;
			notification.m_ContextFuncName = context_function_name.empty() ? "Context Function" : context_function_name;
		}

		m_Notifications.insert(std::make_pair(title + message + std::to_string(static_cast<int>(placement)), notification));
		return notification;
	}

	bool Notifications::EraseImpl(Notification notification)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		for (auto& [id, n] : m_Notifications)
		{
			if (n.m_Identifier == notification.m_Identifier)
			{
				n.m_Erasing = true;
				return true;
			}
		}
		return false;
	}

	static void DrawNotification(Notification& notification, int stackPosition)
	{
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		if (!viewport || notification.m_Alpha <= 0.002f)
			return;

		const ImVec2 workPos = viewport->WorkPos;
		const ImVec2 workSize = viewport->WorkSize;
		const ImVec2 cardSize(m_CardSizeX, m_CardSizeY);
		float xPos = workPos.x + 10.0f;
		float yPos = workPos.y + 10.0f + stackPosition * (m_CardSizeY + 6.0f);

		switch (notification.m_Placement)
		{
		case NotificationPlacement::TopCenter:
			xPos = workPos.x + (workSize.x - cardSize.x) * 0.5f;
			yPos = workPos.y + 18.0f + stackPosition * (m_CardSizeY + 6.0f) + notification.m_AnimationOffset;
			break;
		case NotificationPlacement::Right:
			xPos = workPos.x + workSize.x - cardSize.x - 10.0f - notification.m_AnimationOffset;
			break;
		case NotificationPlacement::Left:
		default:
			xPos = workPos.x + 10.0f + notification.m_AnimationOffset;
			break;
		}

		ImGui::SetNextWindowSize(cardSize, ImGuiCond_Always);
		ImGui::SetNextWindowPos(ImVec2(xPos, yPos), ImGuiCond_Always);
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, std::clamp(notification.m_Alpha, 0.0f, 1.0f));

		std::string windowTitle = std::format("##TenebrisNotification_{}", notification.m_Identifier);
		ImGui::Begin(windowTitle.c_str(), nullptr,
		    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
		        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
		        ImGuiWindowFlags_NoFocusOnAppearing);

		const float timeElapsed = static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(
		    std::chrono::system_clock::now() - notification.m_CreatedOn).count());
		const float depletionProgress = std::clamp(1.0f - (timeElapsed / static_cast<float>(std::max(notification.m_Duration, 1))), 0.0f, 1.0f);
		ImGui::ProgressBar(depletionProgress, ImVec2(-1, 3.5f), "");

		if (notification.m_Type == NotificationType::Info)
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
		else if (notification.m_Type == NotificationType::Success)
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
		else if (notification.m_Type == NotificationType::Warning)
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.0f, 1.0f));
		else
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));

		ImGui::TextUnformatted(notification.m_Title.c_str());
		ImGui::PopStyleColor();
		ImGui::Separator();
		ImGui::TextWrapped("%s", notification.m_Message.c_str());

		if (notification.m_ContextFunc)
		{
			ImGui::Spacing();
			if (ImGui::Selectable(notification.m_ContextFuncName.c_str()))
				FiberPool::Push([notification] { notification.m_ContextFunc(); });
		}
		ImGui::End();
		ImGui::PopStyleVar();
	}

	void Notifications::DrawImpl()
	{
		std::vector<std::string> keysToErase;
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			int leftPosition = 0;
			int centerPosition = 0;
			int rightPosition = 0;
			float delta = ImGui::GetIO().DeltaTime;
			if (delta <= 0.0f)
				delta = 1.0f / 60.0f;
			delta = std::clamp(delta, 0.0f, 0.05f);

			for (auto& [id, notification] : m_Notifications)
			{
				int* position = &leftPosition;
				if (notification.m_Placement == NotificationPlacement::TopCenter)
					position = &centerPosition;
				else if (notification.m_Placement == NotificationPlacement::Right)
					position = &rightPosition;

				const auto elapsed = static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(
				    std::chrono::system_clock::now() - notification.m_CreatedOn).count());
				if (elapsed >= notification.m_Duration)
					notification.m_Erasing = true;

				const float entryDistance = notification.m_Placement == NotificationPlacement::TopCenter ? 22.0f : m_CardSizeX;
				if (!notification.m_Erasing)
				{
					notification.m_Alpha = std::min(1.0f, notification.m_Alpha + delta / 0.20f);
					const float speed = notification.m_Placement == NotificationPlacement::TopCenter ? 150.0f : 1200.0f;
					notification.m_AnimationOffset = std::min(0.0f, notification.m_AnimationOffset + speed * delta);
				}
				else
				{
					notification.m_Alpha = std::max(0.0f, notification.m_Alpha - delta / 0.24f);
					const float speed = notification.m_Placement == NotificationPlacement::TopCenter ? 100.0f : 900.0f;
					notification.m_AnimationOffset = std::max(-entryDistance, notification.m_AnimationOffset - speed * delta);
					if (notification.m_Alpha <= 0.001f)
						keysToErase.push_back(id);
				}

				DrawNotification(notification, (*position)++);
			}
		}

		std::lock_guard<std::mutex> lock(m_mutex);
		for (const auto& key : keysToErase)
			m_Notifications.erase(key);
	}
}
