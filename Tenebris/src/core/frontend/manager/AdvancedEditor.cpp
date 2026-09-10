#include "AdvancedEditor.hpp"

#include "ResponsiveLayout.hpp"
#include "UIItem.hpp"

#include <Windows.h>
#include <algorithm>

namespace YimMenu
{
	namespace
	{
		enum class TransitionPhase
		{
			Closed,
			Opening,
			Open,
			Closing
		};

		UIItem* g_Item{};
		TransitionPhase g_Phase = TransitionPhase::Closed;
		float g_EditorAlpha{};
		float g_SlideProgress{};
		ImVec2 g_EditorSize{};
		bool g_HasEditorSize{};
		bool g_ApplyEditorSize{true};

		float SmoothStep(float value)
		{
			value = std::clamp(value, 0.0f, 1.0f);
			return value * value * (3.0f - 2.0f * value);
		}

		void FinishClose()
		{
			if (g_Item)
				g_Item->OnEditorClosed();
			g_Item = nullptr;
			g_Phase = TransitionPhase::Closed;
			g_EditorAlpha = 0.0f;
			g_SlideProgress = 0.0f;
		}
	}

	void AdvancedEditor::Open(UIItem* item)
	{
		if (!item)
			return;

		if (g_Item == item)
		{
			if (g_Phase == TransitionPhase::Closing)
				g_Phase = TransitionPhase::Opening;
			return;
		}

		if (g_Item)
			g_Item->OnEditorClosed();

		g_Item = item;
		g_Item->OnEditorOpened();
		g_Phase = TransitionPhase::Opening;
		g_EditorAlpha = 0.0f;
		g_SlideProgress = 0.0f;
		g_ApplyEditorSize = true;
	}

	void AdvancedEditor::Close()
	{
		if (!g_Item || g_Phase == TransitionPhase::Closing)
			return;
		g_Phase = TransitionPhase::Closing;
		g_SlideProgress = 1.0f;
	}

	void AdvancedEditor::CloseImmediate()
	{
		if (g_Item)
			FinishClose();
		g_EditorSize = {};
		g_HasEditorSize = false;
		g_ApplyEditorSize = true;
	}

	void AdvancedEditor::Tick()
	{
		if (!g_Item)
			return;

		float delta = ImGui::GetIO().DeltaTime;
		if (delta <= 0.0f)
			delta = 1.0f / 60.0f;
		delta = std::clamp(delta, 0.0f, 0.05f);

		switch (g_Phase)
		{
		case TransitionPhase::Opening:
			g_EditorAlpha = std::min(1.0f, g_EditorAlpha + delta / 0.20f);
			g_SlideProgress = std::min(1.0f, g_SlideProgress + delta / 0.36f);
			if (g_EditorAlpha >= 1.0f && g_SlideProgress >= 1.0f)
				g_Phase = TransitionPhase::Open;
			break;
		case TransitionPhase::Closing:
			g_EditorAlpha = std::max(0.0f, g_EditorAlpha - delta / 0.18f);
			g_SlideProgress = 1.0f;
			if (g_EditorAlpha <= 0.0f)
				FinishClose();
			break;
		case TransitionPhase::Open:
			g_EditorAlpha = 1.0f;
			g_SlideProgress = 1.0f;
			break;
		case TransitionPhase::Closed:
			break;
		}
	}

	bool AdvancedEditor::HandleKey(int key)
	{
		if (!g_Item)
			return false;
		if (g_Phase == TransitionPhase::Closing)
			return true;
		if (key == VK_BACK || key == VK_ESCAPE)
		{
			Close();
			return true;
		}
		return g_Item->HandleEditorKey(key);
	}

	void AdvancedEditor::Draw()
	{
		if (!g_Item || g_EditorAlpha <= 0.001f)
			return;

		bool open = true;
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		if (!viewport || viewport->WorkSize.x < 320.0f || viewport->WorkSize.y < 240.0f)
			return;

		const ImVec2 display = viewport->WorkSize;
		const ImVec2 origin = viewport->WorkPos;
		const float gap = std::clamp(display.x * 0.018f, 10.0f, 22.0f);
		const auto classicLayout = GetResponsiveMenuLayout();

		// Editors intentionally open large by default. At 1280x720 this is roughly
		// the same visual footprint as the reference animation/music window.
		const float defaultWidth = std::clamp(display.x * 0.84f, 650.0f, display.x - gap * 2.0f);
		const float maxHeight = std::max(280.0f, display.y - gap * 2.0f);
		const float requestedHeight = std::max(g_Item->GetPreferredEditorHeight(), display.y * 0.78f);
		const float defaultHeight = std::clamp(requestedHeight, 420.0f, maxHeight);
		const float centeredX = origin.x + (display.x - defaultWidth) * 0.5f;
		const float offscreenX = origin.x + display.x + gap;
		const float slide = SmoothStep(g_SlideProgress);
		const float editorX = offscreenX + (centeredX - offscreenX) * slide;
		const ImVec2 defaultSize(defaultWidth, defaultHeight);
		const ImVec2 editorPosition(editorX, origin.y + (display.y - defaultHeight) * 0.5f);
		const float alpha = GetEditorAlpha();

		if (g_Phase != TransitionPhase::Open)
			ImGui::SetNextWindowPos(editorPosition, ImGuiCond_Always);
		if (g_ApplyEditorSize)
			ImGui::SetNextWindowSize(g_HasEditorSize ? g_EditorSize : defaultSize, ImGuiCond_Always);
		ImGui::SetNextWindowSizeConstraints(ImVec2(620.0f, 400.0f), ImVec2(display.x - gap * 2.0f, maxHeight));
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 12.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 7.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(9.0f, 6.0f));
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.025f, 0.04f, 0.02f, 0.97f));
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.20f, 0.30f, 0.055f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.13f, 0.19f, 0.043f, 1.0f));
		if (ImGui::Begin("##TenebrisAdvancedEditor", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings))
		{
			g_EditorSize = ImGui::GetWindowSize();
			g_HasEditorSize = true;
			g_ApplyEditorSize = false;
			ImGui::SetWindowFontScale(1.0f);

			const char* brand = "TENEBRIS";
			const float brandWidth = ImGui::CalcTextSize(brand).x;
			ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), (ImGui::GetWindowWidth() - brandWidth) * 0.5f));
			ImGui::TextUnformatted(brand);

			ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - ImGui::CalcTextSize("X").x - 8.0f);
			if (ImGui::SmallButton("X"))
				open = false;

			const std::string subtitle = std::string(g_Item->GetMenuLabel()) + " | BACK: Voltar";
			const float subtitleWidth = ImGui::CalcTextSize(subtitle.c_str()).x;
			ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), (ImGui::GetWindowWidth() - subtitleWidth) * 0.5f));
			ImGui::TextDisabled("%s", subtitle.c_str());
			ImGui::Separator();
			ImGui::Spacing();

			if (ImGui::BeginChild("##TenebrisAdvancedContent", ImVec2(0.0f, 0.0f), false))
			{
				ImGui::PushItemWidth(-FLT_MIN);
				g_Item->Draw();
				ImGui::PopItemWidth();
			}
			ImGui::EndChild();
		}
		ImGui::End();
		ImGui::PopStyleColor(3);
		ImGui::PopStyleVar(6);

		if (!open)
			Close();
	}

	bool AdvancedEditor::IsOpen()
	{
		return g_Item != nullptr;
	}

	float AdvancedEditor::GetEditorAlpha()
	{
		return SmoothStep(g_EditorAlpha);
	}

	float AdvancedEditor::GetClassicMenuAlpha()
	{
		return 1.0f - GetEditorAlpha();
	}
}
