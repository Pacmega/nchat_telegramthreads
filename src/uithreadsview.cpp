// uithreadsview.cpp
//
// Copyright (c) 2026 Kristofer Berggren
// All rights reserved.
//
// nchat is distributed under the MIT license, see LICENSE for details.

#include "uithreadsview.h"

#include "strutil.h"
#include "uicolorconfig.h"
#include "uiconfig.h"
#include "uimodel.h"

UiThreadsView::UiThreadsView(const UiViewParams& p_Params)
  : UiViewBase(p_Params)
{
  if (m_Enabled)
  {
    int paddedY = m_Y + 1;
    int paddedX = m_X + 1;
    m_PaddedH = m_H - 2;
    m_PaddedW = m_W - 2;
    m_PaddedWin = newwin(m_PaddedH, m_PaddedW, paddedY, paddedX);

    static int colorPair = UiColorConfig::GetColorPair("threads_color");
    static int attribute = UiColorConfig::GetAttribute("threads_attr");

    werase(m_Win);
    wbkgd(m_Win, attribute | colorPair | ' ');
    wrefresh(m_Win);
  }
}

UiThreadsView::~UiThreadsView()
{
  if (m_PaddedWin != nullptr)
  {
    delwin(m_PaddedWin);
    m_PaddedWin = nullptr;
  }
}

void UiThreadsView::Draw()
{
  if (!m_Enabled || !m_Dirty) return;
  m_Dirty = false;

  curs_set(0);

  static int colorPair = UiColorConfig::GetColorPair("threads_color");
  static int attribute = UiColorConfig::GetAttribute("threads_attr");
  static int attributeSelected = UiColorConfig::GetAttribute("threads_attr_selected");
  static int colorPairUnread = UiColorConfig::GetColorPair("threads_color_unread");

  const std::pair<std::string, std::string>& currentChat = m_Model->GetCurrentChatLocked();
  const bool emojiEnabled = m_Model->GetEmojiEnabledLocked();
  const bool threadsFocus = m_Model->GetThreadsFocusLocked();
  const int index = std::max(0, m_Model->GetCurrentThreadIndexLocked(currentChat.first, currentChat.second));
  const std::vector<ChatThreadInfo>& threadVec = m_Model->GetThreadVecLocked(currentChat.first, currentChat.second);

  std::vector<std::string> names;
  std::vector<bool> unreads;
  for (auto& threadInfo : threadVec)
  {
    std::string name = threadInfo.name;
    if (name.empty())
    {
      name = threadInfo.id;
    }

    if (threadInfo.isClosed)
    {
      name += " (closed)";
    }

    names.push_back(name);
    unreads.push_back((threadInfo.unreadCount > 0) || (threadInfo.unreadMentionCount > 0));
  }

  werase(m_PaddedWin);
  wbkgd(m_PaddedWin, attribute | colorPair | ' ');
  wattron(m_PaddedWin, attribute | colorPair);

  if (!names.empty())
  {
    int height = m_PaddedH;
    int count = names.size();
    int offset = std::min(std::max(0, index - ((height - 1) / 2)), std::max(0, count - height));
    int last = std::min((height + offset), count);
    for (int i = offset; i < last; ++i)
    {
      if (threadsFocus && (i == index))
      {
        wattroff(m_PaddedWin, attribute);
        wattron(m_PaddedWin, attributeSelected);
      }

      int y = i - offset;
      std::string name = names[i];
      if (!emojiEnabled)
      {
        name = StrUtil::Textize(name);
      }

      std::wstring wname = StrUtil::ToWString(name).substr(0, m_PaddedW);
      wname = StrUtil::TrimPadWString(wname, m_PaddedW);

      if (unreads[i])
      {
        wattron(m_PaddedWin, colorPairUnread);
      }

      mvwaddnwstr(m_PaddedWin, y, 0, wname.c_str(), wname.size());

      if (unreads[i])
      {
        static const std::string unreadIndicator = " " + UiConfig::GetStr("unread_indicator");
        static const std::wstring wunread = StrUtil::ToWString(unreadIndicator);
        mvwaddnwstr(m_PaddedWin, y, (m_PaddedW - StrUtil::WStringWidth(wunread)), wunread.c_str(), wunread.size());

        wattron(m_PaddedWin, colorPair);
      }

      if (threadsFocus && (i == index))
      {
        wattroff(m_PaddedWin, attributeSelected);
        wattron(m_PaddedWin, attribute);
      }
    }
  }

  wattroff(m_PaddedWin, attribute | colorPair);
  wrefresh(m_PaddedWin);
}
