#include "Notifications.hpp"

#include "core/logger/LogHelper.hpp"
#include "game/backend/FiberPool.hpp" // TODO: game import in core
#include "util/Joaat.hpp"

#include <algorithm>
#include <mutex>

namespace YimMenu
{

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

		auto message_id = Joaat(title + message + std::to_string(static_cast<int>(placement)));
		std::lock_guard<std::mutex> lock(m_mutex);

		auto exists = std::find_if(m_Notifications.begin(), m_Notifications.end(), [&](auto& notification) {
			return notification.second.m_Identifier == message_id;
		});

		if (exists != m_Notifications.end())
		{
			exists->second.m_CreatedOn = std::chrono::system_clock::now();
			return {};
		}

		Notification notification{};
		notification.m_Title = title;
		notification.m_Message = message;
		notification.m_Type = type;
		notification.m_Placement = placement;
		notification.m_CreatedOn = std::chrono::system_clock::now();
		notification.m_Duration = duration;
		notification.m_Identifier = message_id;
		notification.m_AnimationOffset = placement == NotificationPlacement::TopCenter ? -26.0f : -m_CardSizeX;

		if (context_function)
		{
			notification.m_ContextFunc = context_function;
			notification.m_ContextFuncName = context_function_name.empty() ? "Context Function" : context_function_name;
		}

		auto result = m_Notifications.insert(std::make_pair(title + message + std::to_string(static_cast<int>(placement)), notification));
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
		if (!viewport)
			return;

		const ImVec2 workPos = viewport->WorkPos;
		const ImVec2 workSize = viewport->WorkSize;
		const ImVec2 cardSize(m_CardSizeX, m_CardSizeY);
		float xPos = workPos.x + 10.0f;
		float yPos = workPos.y + 10.0f + stackPosition * m_CardSizeY;

		switch (notification.m_Placement)
		{
		case NotificationPlacement::TopCenter:
			xPos = workPos.x + (workSize.x - cardSize.x) * 0.5f;
			yPos = workPos.y + 18.0f + stackPosition * (m_CardSizeY + 6.0f) + notification.m_AnimationOffset;
			break;
		case NotificationPlacement::Right:
			xPos = workPos.x + workSize.x - cardSize.x - 10.0f - notification.m_AnimationOffset;
			yPos = workPos.y + 10.0f + stackPosition * m_CardSizeY;
			break;
		case NotificationPlacement::Left:
		default:
			xPos = workPos.x + 10.0f + notification.m_AnimationOffset;
			break;
		}

		ImGui::SetNextWindowSize(cardSize, ImGuiCond_Always);
		ImGui::SetNextWindowPos(ImVec2(xPos, yPos), ImGuiCond_Always);

		std::string windowTitle = std::format("##TenebrisNotification_{}", notification.m_Identifier);
		ImGui::Begin(windowTitle.c_str(), nullptr,
		    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
		        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
		        ImGuiWindowFlags_NoFocusOnAppearing);

		auto timeElapsed =
		    (float)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - notification.m_CreatedOn).count();
		auto depletionProgress = std::clamp(1.0f - (timeElapsed / (float)notification.m_Duration), 0.0f, 1.0f);
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
	}

	void Notifications::DrawImpl()
	{
		std::vector<std::string> keysToErase;
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			int leftPosition = 0;
			int centerPosition = 0;
			int rightPosition = 0;

			for (auto& [id, notification] : m_Notifications)
			{
				int* position = &leftPosition;
				if (notification.m_Placement == NotificationPlacement::TopCenter)
					position = &centerPosition;
				else if (notification.m_Placement == NotificationPlacement::Right)
					position = &rightPosition;

				DrawNotification(notification, (*position)++);

				const float targetOffset = 0.0f;
				if (!notification.m_Erasing)
				{
					if (notification.m_AnimationOffset < targetOffset)
					{
						notification.m_AnimationOffset += notification.m_Placement == NotificationPlacement::TopCenter ? 5.0f : m_CardAnimationSpeed;
						if (notification.m_AnimationOffset > targetOffset)
							notification.m_AnimationOffset = targetOffset;
					}
				}
				else
				{
					notification.m_AnimationOffset -= notification.m_Placement == NotificationPlacement::TopCenter ? 5.0f : m_CardAnimationSpeed;
					const float eraseLimit = notification.m_Placement == NotificationPlacement::TopCenter ? -30.0f : -m_CardSizeX;
					if (notification.m_AnimationOffset <= eraseLimit)
						keysToErase.push_back(id);
				}

				if ((float)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - notification.m_CreatedOn).count() >= notification.m_Duration)
					keysToErase.push_back(id);
			}
		}

		std::lock_guard<std::mutex> lock(m_mutex);
		for (const auto& key : keysToErase)
			m_Notifications.erase(key);
	}
}
