#pragma once
#include "Category.hpp"
#include "Submenu.hpp"

namespace YimMenu
{
	class UIManager
	{
	public:
		static void AddSubmenu(const std::shared_ptr<Submenu>&& submenu)
		{
			GetInstance().AddSubmenuImpl(std::move(submenu));
		}

		static void SetActiveSubmenu(const std::shared_ptr<Submenu> submenu)
		{
			GetInstance().SetActiveSubmenuImpl(submenu);
		}

		static void Render()
		{
			GetInstance().RenderImpl();
		}

		static void HandleKey(WPARAM key)
		{
			GetInstance().HandleKeyImpl(key);
		}

		static std::shared_ptr<Submenu> GetActiveSubmenu()
		{
			return GetInstance().GetActiveSubmenuImpl();
		}

		static std::shared_ptr<Category> GetActiveCategory()
		{
			return GetInstance().GetActiveCategoryImpl();
		}

		static void SetOptionsFont(ImFont* font)
		{
			GetInstance().m_OptionsFont = font;
		}

	private:
		static inline UIManager& GetInstance()
		{
			static UIManager instance;
			return instance;
		}

		void AddSubmenuImpl(const std::shared_ptr<Submenu>&& submenu);
		void SetActiveSubmenuImpl(const std::shared_ptr<Submenu> submenu);
		void RenderImpl();
		void HandleKeyImpl(WPARAM key);
		std::vector<UIItem*> GetCurrentItems() const;
		std::size_t GetEntryCount() const;
		std::shared_ptr<Submenu> GetActiveSubmenuImpl();
		std::shared_ptr<Category> GetActiveCategoryImpl();

		std::shared_ptr<Submenu> m_ActiveSubmenu;
		std::vector<std::shared_ptr<Submenu>> m_Submenus;
		ImFont* m_OptionsFont = nullptr;
		enum class Level
		{
			Root,
			Categories,
			Options
		};
		Level m_Level = Level::Root;
		std::size_t m_Selected = 0;
	};
}
