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
	}

	void AdvancedEditor::Close()
	{
		if (!g_Item || g_Phase == TransitionPhase::Closing)
			return;

		// O editor permanece no lado esquerdo durante a saida. Apenas a opacidade
		// diminui enquanto o menu classico reaparece no mesmo lugar.
		g_Phase = TransitionPhase::Closing;
		g_SlideProgress = 1.0f;
	}

	void AdvancedEditor::CloseImmediate()
	{
		if (!g_Item)
			return;
		FinishClose();
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

		// Enquanto o fade de saida esta acontecendo, consome a entrada para nao
		// acionar acidentalmente uma opcao do menu que esta reaparecendo.
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
		const float gap = std::clamp(display.x * 0.0125f, 8.0f, 14.0f);
		const auto classicLayout = GetResponsiveMenuLayout();

		const float desiredWidth = std::clamp(display.x * 0.32f, 300.0f, 410.0f);
		const float editorWidth = std::min(desiredWidth, display.x - gap * 2.0f);
		const float availableHeight = std::max(180.0f, display.y - gap * 2.0f);
		const float desiredHeight = g_Item->GetPreferredEditorHeight();
		const float editorHeight = std::clamp(desiredHeight, 180.0f, availableHeight);

		const float rightX = origin.x + display.x - editorWidth - gap;
		const float leftX = classicLayout.X;
		const float slide = SmoothStep(g_SlideProgress);
		const float editorX = rightX + (leftX - rightX) * slide;
		const ImVec2 editorPosition(editorX, origin.y + gap);
		const ImVec2 editorSize(editorWidth, editorHeight);
		const float alpha = GetEditorAlpha();

		ImGui::SetNextWindowPos(editorPosition, ImGuiCond_Always);
		ImGui::SetNextWindowSize(editorSize, ImGuiCond_Always);
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 5.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 8.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 4.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(7.0f, 4.0f));
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.025f, 0.04f, 0.02f, 0.96f));
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.20f, 0.30f, 0.055f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.13f, 0.19f, 0.043f, 1.0f));
		if (ImGui::Begin("##TenebrisAdvancedEditor", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings))
		{
			ImGui::SetWindowFontScale(0.92f);

			ImGui::TextUnformatted("EDITOR AVANCADO DO TENEBRIS");
			ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - ImGui::CalcTextSize("X").x - 8.0f);
			if (ImGui::SmallButton("X"))
				open = false;
			ImGui::Separator();
			ImGui::TextDisabled("%s", g_Item->GetMenuLabel().data());
			ImGui::SameLine();
			ImGui::TextDisabled("| BACK: voltar");
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
