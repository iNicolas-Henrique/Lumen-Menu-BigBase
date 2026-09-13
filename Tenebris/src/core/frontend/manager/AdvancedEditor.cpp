#include "AdvancedEditor.hpp"

#include "ResponsiveLayout.hpp"
#include "UIItem.hpp"
#include "core/frontend/PerformanceOptions.hpp"

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
		bool g_DetachedMode{};
		bool g_DetachedPlacementPending{true};

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
			g_DetachedMode = false;
			g_DetachedPlacementPending = true;
		}
	}

	void AdvancedEditor::Open(UIItem* item)
	{
		if (!item)
			return;

		if (g_Item == item)
		{
			if (g_Phase == TransitionPhase::Closing)
			{
				if (PerformanceOptions::EditorAnimations.GetState())
					g_Phase = TransitionPhase::Opening;
				else
				{
					g_Phase = TransitionPhase::Open;
					g_EditorAlpha = 1.0f;
					g_SlideProgress = 1.0f;
				}
			}
			return;
		}

		if (g_Item)
			g_Item->OnEditorClosed();

		g_Item = item;
		g_Item->OnEditorOpened();
		g_ApplyEditorSize = true;

		if (PerformanceOptions::EditorAnimations.GetState())
		{
			g_Phase = TransitionPhase::Opening;
			g_EditorAlpha = 0.0f;
			g_SlideProgress = 0.0f;
		}
		else
		{
			g_Phase = TransitionPhase::Open;
			g_EditorAlpha = 1.0f;
			g_SlideProgress = 1.0f;
		}
	}

	void AdvancedEditor::Close()
	{
		if (!g_Item || g_Phase == TransitionPhase::Closing)
			return;

		if (!PerformanceOptions::EditorAnimations.GetState())
		{
			FinishClose();
			return;
		}

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
		g_DetachedMode = false;
		g_DetachedPlacementPending = true;
	}

	void AdvancedEditor::SetDetachedMode(bool detached)
	{
		if (g_DetachedMode == detached)
			return;
		g_DetachedMode = detached;
		g_ApplyEditorSize = true;
		g_DetachedPlacementPending = detached;
		if (detached)
		{
			g_Phase = TransitionPhase::Open;
			g_EditorAlpha = 1.0f;
			g_SlideProgress = 1.0f;
		}
	}

	bool AdvancedEditor::IsDetachedMode()
	{
		return g_DetachedMode;
	}

	bool AdvancedEditor::ShouldRenderWhenMenuClosed()
	{
		return g_DetachedMode && g_Item != nullptr;
	}

	void AdvancedEditor::Tick()
	{
		if (!g_Item)
			return;

		if (g_DetachedMode)
		{
			g_Phase = TransitionPhase::Open;
			g_EditorAlpha = 1.0f;
			g_SlideProgress = 1.0f;
			return;
		}

		if (!PerformanceOptions::EditorAnimations.GetState())
		{
			if (g_Phase == TransitionPhase::Closing)
			{
				FinishClose();
				return;
			}
			g_Phase = TransitionPhase::Open;
			g_EditorAlpha = 1.0f;
			g_SlideProgress = 1.0f;
			return;
		}

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
		if (!g_DetachedMode && (key == VK_BACK || key == VK_ESCAPE))
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
		(void)classicLayout;

		float defaultWidth{};
		float defaultHeight{};
		ImVec2 editorPosition{};
		if (g_DetachedMode)
		{
			// Freecam mode: compact right-side workspace. At 1280x720 this is
			// roughly 430-460 px wide, leaving most of the world visible.
			defaultWidth = std::clamp(display.x * 0.355f, 360.0f, 470.0f);
			defaultHeight = std::max(360.0f, display.y - gap * 2.0f);
			editorPosition = ImVec2(origin.x + display.x - defaultWidth - gap, origin.y + gap);
		}
		else
		{
			defaultWidth = std::clamp(display.x * 0.84f, 650.0f, display.x - gap * 2.0f);
			const float maxHeight = std::max(280.0f, display.y - gap * 2.0f);
			const float requestedHeight = std::max(g_Item->GetPreferredEditorHeight(), display.y * 0.78f);
			defaultHeight = std::clamp(requestedHeight, 420.0f, maxHeight);
			const float centeredX = origin.x + (display.x - defaultWidth) * 0.5f;
			const float offscreenX = origin.x + display.x + gap;
			const float slide = PerformanceOptions::EditorAnimations.GetState() ? SmoothStep(g_SlideProgress) : 1.0f;
			const float editorX = offscreenX + (centeredX - offscreenX) * slide;
			editorPosition = ImVec2(editorX, origin.y + (display.y - defaultHeight) * 0.5f);
		}

		const ImVec2 defaultSize(defaultWidth, defaultHeight);
		const float alpha = GetEditorAlpha();
		if ((g_DetachedMode && g_DetachedPlacementPending) || (!g_DetachedMode && g_Phase != TransitionPhase::Open))
			ImGui::SetNextWindowPos(editorPosition, ImGuiCond_Always);
		if (g_DetachedMode || g_ApplyEditorSize)
			ImGui::SetNextWindowSize(g_DetachedMode ? defaultSize : (g_HasEditorSize ? g_EditorSize : defaultSize), ImGuiCond_Always);

		const float maxHeight = std::max(280.0f, display.y - gap * 2.0f);
		ImGui::SetNextWindowSizeConstraints(
			g_DetachedMode ? ImVec2(340.0f, 340.0f) : ImVec2(620.0f, 400.0f),
			ImVec2(display.x - gap * 2.0f, maxHeight));
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, g_DetachedMode ? ImVec2(12.0f, 10.0f) : ImVec2(16.0f, 12.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 7.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(9.0f, 6.0f));
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.025f, 0.04f, 0.02f, 0.97f));
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.20f, 0.30f, 0.055f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.13f, 0.19f, 0.043f, 1.0f));
		ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;
		if (!g_DetachedMode)
			windowFlags |= ImGuiWindowFlags_NoMove;
		if (ImGui::Begin("##TenebrisAdvancedEditor", nullptr, windowFlags))
		{
			g_EditorSize = ImGui::GetWindowSize();
			g_HasEditorSize = true;
			g_ApplyEditorSize = false;

			if (g_DetachedMode)
			{
				// Freecam panel is movable, but never allowed to disappear outside
				// the active viewport. Initial placement remains docked at the right.
				const ImVec2 currentPos = ImGui::GetWindowPos();
				const ImVec2 currentSize = ImGui::GetWindowSize();
				const float minX = origin.x;
				const float minY = origin.y;
				const float maxX = std::max(minX, origin.x + display.x - currentSize.x);
				const float maxY = std::max(minY, origin.y + display.y - currentSize.y);
				const ImVec2 clamped(std::clamp(currentPos.x, minX, maxX), std::clamp(currentPos.y, minY, maxY));
				if (clamped.x != currentPos.x || clamped.y != currentPos.y)
					ImGui::SetWindowPos(clamped, ImGuiCond_Always);
				g_DetachedPlacementPending = false;
			}

			ImGui::SetWindowFontScale(1.0f);

			const char* brand = "TENEBRIS";
			if (g_DetachedMode)
			{
				// Keep all labels and controls anchored to the left edge in the
				// narrow freecam panel so nothing is pushed off-screen.
				ImGui::TextUnformatted(brand);
				ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - ImGui::CalcTextSize("FREECAM").x - 8.0f);
				ImGui::TextDisabled("FREECAM");
			}
			else
			{
				const float brandWidth = ImGui::CalcTextSize(brand).x;
				ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), (ImGui::GetWindowWidth() - brandWidth) * 0.5f));
				ImGui::TextUnformatted(brand);
				ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - ImGui::CalcTextSize("X").x - 8.0f);
				if (ImGui::SmallButton("X"))
					open = false;
			}

			const std::string subtitle = std::string(g_Item->GetMenuLabel()) + (g_DetachedMode ? " | Arraste o painel | Mouse: editar | BACK: sair" : " | BACK: Voltar");
			if (g_DetachedMode)
				ImGui::TextDisabled("%s", subtitle.c_str());
			else
			{
				const float subtitleWidth = ImGui::CalcTextSize(subtitle.c_str()).x;
				ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), (ImGui::GetWindowWidth() - subtitleWidth) * 0.5f));
				ImGui::TextDisabled("%s", subtitle.c_str());
			}
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

		if (!open && !g_DetachedMode)
			Close();
	}

	bool AdvancedEditor::IsOpen()
	{
		return g_Item != nullptr;
	}

	float AdvancedEditor::GetEditorAlpha()
	{
		if (g_DetachedMode)
			return 1.0f;
		return PerformanceOptions::EditorAnimations.GetState() ? SmoothStep(g_EditorAlpha) : g_EditorAlpha;
	}

	float AdvancedEditor::GetClassicMenuAlpha()
	{
		return g_DetachedMode ? 0.0f : 1.0f - GetEditorAlpha();
	}
}
