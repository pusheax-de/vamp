#pragma once
// UIDropdown.h - Dropdown with search textbox for selecting from a list
//
// Displays a button showing the current selection. When clicked, opens a
// dropdown panel with a search textbox and a scrollable list of items.
// Typing in the textbox filters the list. Clicking an item selects it and
// closes the dropdown. Pressing Escape closes without selecting.

#include "UIElement.h"
#include "UIRenderer.h"
#include "BitmapFont.h"
#include <string>
#include <vector>
#include <functional>
#include <algorithm>
#include <cctype>
#include <cstdint>

namespace ui
{

// ---------------------------------------------------------------------------
// DropdownItem - a single selectable entry
// ---------------------------------------------------------------------------
struct DropdownItem
{
    uint64_t    id   = 0;       // Application-defined identifier
    std::string label;          // Display text
};

// ---------------------------------------------------------------------------
// UIDropdown - searchable dropdown selector
// ---------------------------------------------------------------------------
class UIDropdown : public UIElement
{
public:
    // Set the font used for all text rendering
    void SetFont(const BitmapFont* font) { m_font = font; }

    // Populate the list of items
    void SetItems(const std::vector<DropdownItem>& items) { m_items = items; }

    // Set the selection callback (fires when an item is clicked)
    void SetOnSelect(std::function<void(uint64_t id, const std::string& label)> cb)
    {
        m_onSelect = cb;
    }

    // Get/set the display text shown on the closed button
    void SetPlaceholder(const std::string& text) { m_placeholder = text; }
    const std::string& GetPlaceholder() const { return m_placeholder; }

    // Open / close the dropdown
    bool IsOpen() const { return m_open; }
    void Open()
    {
        m_open = true;
        m_searchText.clear();
        m_scrollOffset = 0;
        RebuildFiltered();
    }
    void Close()    { m_open = false; }
    void Toggle()   { if (m_open) Close(); else Open(); }

    // Feed a character from WM_CHAR for the search box (when open)
    void HandleChar(char ch)
    {
        if (!m_open)
            return;

        if (ch == 8) // Backspace
        {
            if (!m_searchText.empty())
                m_searchText.pop_back();
        }
        else if (ch == 27) // Escape
        {
            Close();
            return;
        }
        else if (ch >= 32 && ch < 127)
        {
            m_searchText += ch;
        }
        m_scrollOffset = 0;
        RebuildFiltered();
    }

    // Handle a mouse click at screen coordinates. Returns true if consumed.
    bool HandleClick(float px, float py)
    {
        const Rect& r = GetScreenRect();

        // Click on the button header toggles open/close
        Rect buttonRect = { r.x, r.y, r.w, m_rowHeight };
        if (buttonRect.Contains(px, py))
        {
            Toggle();
            return true;
        }

        if (!m_open)
            return false;

        // Click in the dropdown area
        Rect dropRect = GetDropdownRect();
        if (!dropRect.Contains(px, py))
        {
            Close();
            return false;
        }

        // Which row was clicked?
        float searchBoxH = m_rowHeight;
        float listTop = dropRect.y + searchBoxH;
        if (py < listTop)
            return true; // Click in search box area, just consume

        int rowIndex = static_cast<int>((py - listTop) / m_rowHeight);
        rowIndex += m_scrollOffset;

        if (rowIndex >= 0 && rowIndex < static_cast<int>(m_filtered.size()))
        {
            const auto& item = m_filtered[rowIndex];
            m_placeholder = item.label;
            if (m_onSelect)
                m_onSelect(item.id, item.label);
            Close();
        }

        return true;
    }

    // Handle scroll wheel over the dropdown. Returns true if consumed.
    bool HandleScroll(float px, float py, float delta)
    {
        if (!m_open)
            return false;

        Rect dropRect = GetDropdownRect();
        if (!dropRect.Contains(px, py))
            return false;

        m_scrollOffset -= static_cast<int>(delta);
        int maxScroll = static_cast<int>(m_filtered.size()) - m_maxVisibleRows;
        if (maxScroll < 0) maxScroll = 0;
        if (m_scrollOffset < 0) m_scrollOffset = 0;
        if (m_scrollOffset > maxScroll) m_scrollOffset = maxScroll;
        return true;
    }

    // Style
    void SetRowHeight(float h)          { m_rowHeight = h; }
    void SetMaxVisibleRows(int n)       { m_maxVisibleRows = n; }
    void SetButtonColor(const Color& c) { m_buttonColor = c; }
    void SetDropBgColor(const Color& c) { m_dropBgColor = c; }
    void SetHoverColor(const Color& c)  { m_hoverColor = c; }
    void SetTextColor(const Color& c)   { m_textColor = c; }
    void SetSearchBgColor(const Color& c) { m_searchBgColor = c; }

    // Set the current mouse position for hover highlighting
    void SetMousePos(float px, float py) { m_mouseX = px; m_mouseY = py; }

    uint64_t GetHoveredItemId() const
    {
        if (!m_open)
            return 0;

        Rect dropRect = GetDropdownRect();
        if (!dropRect.Contains(m_mouseX, m_mouseY))
            return 0;

        float listTop = dropRect.y + m_rowHeight;
        if (m_mouseY < listTop)
            return 0;

        int rowIndex = static_cast<int>((m_mouseY - listTop) / m_rowHeight);
        rowIndex += m_scrollOffset;
        if (rowIndex < 0 || rowIndex >= static_cast<int>(m_filtered.size()))
            return 0;

        return m_filtered[rowIndex].id;
    }

protected:
    void OnRender(UIRenderer& renderer) override
    {
        if (!m_font)
            return;

        const Rect& r = GetScreenRect();
        float pad = 4.0f;

        // Draw the closed button
        Rect buttonRect = { r.x, r.y, r.w, m_rowHeight };
        renderer.DrawRect(buttonRect, m_buttonColor);

        // Button text
        std::string btnText = m_placeholder;
        if (m_open)
            btnText += "  ^";
        else
            btnText += "  v";

        float textY = buttonRect.y + (m_rowHeight - m_font->GetLineHeight()) * 0.5f;
        renderer.DrawText(*m_font, btnText.c_str(),
                          buttonRect.x + pad, textY, m_textColor);

        if (!m_open)
            return;

        // Draw the dropdown panel
        Rect dropRect = GetDropdownRect();

        // Background
        renderer.DrawRect(dropRect, m_dropBgColor);

        // Border
        Rect borderLeft   = { dropRect.x - 1.0f, dropRect.y, 1.0f, dropRect.h };
        Rect borderRight  = { dropRect.x + dropRect.w, dropRect.y, 1.0f, dropRect.h };
        Rect borderTop    = { dropRect.x - 1.0f, dropRect.y - 1.0f, dropRect.w + 2.0f, 1.0f };
        Rect borderBottom = { dropRect.x - 1.0f, dropRect.y + dropRect.h, dropRect.w + 2.0f, 1.0f };
        Color borderColor = { 0.5f, 0.5f, 0.5f, 0.8f };
        renderer.DrawRect(borderLeft,   borderColor);
        renderer.DrawRect(borderRight,  borderColor);
        renderer.DrawRect(borderTop,    borderColor);
        renderer.DrawRect(borderBottom, borderColor);

        // Search box
        Rect searchRect = { dropRect.x, dropRect.y, dropRect.w, m_rowHeight };
        renderer.DrawRect(searchRect, m_searchBgColor);

        std::string searchDisplay = m_searchText + "_";
        float searchTextY = searchRect.y + (m_rowHeight - m_font->GetLineHeight()) * 0.5f;
        renderer.DrawText(*m_font, searchDisplay.c_str(),
                          searchRect.x + pad, searchTextY, m_textColor);

        // List items
        float listTop = dropRect.y + m_rowHeight;
        int visibleCount = (std::min)(m_maxVisibleRows,
                                       static_cast<int>(m_filtered.size()) - m_scrollOffset);
        for (int i = 0; i < visibleCount; ++i)
        {
            int idx = m_scrollOffset + i;
            if (idx < 0 || idx >= static_cast<int>(m_filtered.size()))
                break;

            float rowY = listTop + i * m_rowHeight;
            Rect rowRect = { dropRect.x, rowY, dropRect.w, m_rowHeight };

            // Hover highlight
            if (rowRect.Contains(m_mouseX, m_mouseY))
                renderer.DrawRect(rowRect, m_hoverColor);

            float itemTextY = rowY + (m_rowHeight - m_font->GetLineHeight()) * 0.5f;
            renderer.DrawText(*m_font, m_filtered[idx].label.c_str(),
                              rowRect.x + pad, itemTextY, m_textColor);
        }

        // Scroll indicator
        if (static_cast<int>(m_filtered.size()) > m_maxVisibleRows)
        {
            float totalRows = static_cast<float>(m_filtered.size());
            float scrollFrac = static_cast<float>(m_scrollOffset) / (totalRows - m_maxVisibleRows);
            float listH = m_maxVisibleRows * m_rowHeight;
            float thumbH = (std::max)(10.0f, listH * m_maxVisibleRows / totalRows);
            float thumbY = listTop + scrollFrac * (listH - thumbH);

            Rect scrollTrack = { dropRect.x + dropRect.w - 4.0f, listTop, 4.0f, listH };
            renderer.DrawRect(scrollTrack, { 0.1f, 0.1f, 0.1f, 0.5f });

            Rect scrollThumb = { dropRect.x + dropRect.w - 4.0f, thumbY, 4.0f, thumbH };
            renderer.DrawRect(scrollThumb, { 0.5f, 0.5f, 0.5f, 0.8f });
        }
    }

private:
    Rect GetDropdownRect() const
    {
        const Rect& r = GetScreenRect();
        int visRows = (std::min)(m_maxVisibleRows, static_cast<int>(m_filtered.size()));
        float dropH = m_rowHeight + visRows * m_rowHeight; // search box + rows
        return { r.x, r.y + m_rowHeight, r.w, dropH };
    }

    void RebuildFiltered()
    {
        m_filtered.clear();
        if (m_searchText.empty())
        {
            m_filtered = m_items;
            return;
        }

        // Case-insensitive substring match
        std::string lowerSearch = m_searchText;
        for (auto& c : lowerSearch) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

        for (const auto& item : m_items)
        {
            std::string lowerLabel = item.label;
            for (auto& c : lowerLabel) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

            if (lowerLabel.find(lowerSearch) != std::string::npos)
                m_filtered.push_back(item);
        }
    }

    const BitmapFont*   m_font          = nullptr;
    std::vector<DropdownItem> m_items;
    std::vector<DropdownItem> m_filtered;
    std::function<void(uint64_t, const std::string&)> m_onSelect;
    std::string         m_placeholder   = "Select...";
    std::string         m_searchText;
    bool                m_open          = false;
    int                 m_scrollOffset  = 0;
    int                 m_maxVisibleRows = 8;
    float               m_rowHeight     = 20.0f;
    float               m_mouseX        = 0.0f;
    float               m_mouseY        = 0.0f;

    // Colors
    Color   m_buttonColor   = { 0.18f, 0.16f, 0.16f, 0.9f };
    Color   m_dropBgColor   = { 0.12f, 0.10f, 0.10f, 0.95f };
    Color   m_hoverColor    = { 0.3f, 0.25f, 0.15f, 0.7f };
    Color   m_textColor     = Color::White();
    Color   m_searchBgColor = { 0.08f, 0.08f, 0.08f, 0.95f };
};

} // namespace ui
