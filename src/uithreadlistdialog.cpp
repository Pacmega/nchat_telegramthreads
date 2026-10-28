// uithreadlistdialog.cpp
//
// Copyright (c) 2026 Kristofer Berggren
// All rights reserved.
//
// nchat is distributed under the MIT license, see LICENSE for details.

#include "uithreadlistdialog.h"

#include "strutil.h"
#include "uiconfig.h"
#include "uimodel.h"

UiThreadListDialog::UiThreadListDialog(const UiDialogParams& p_Params, const std::string& p_ProfileId,
                                       const std::string& p_ChatId)
  : UiListDialog(p_Params, false /*p_ShadeHidden*/)
  , m_ProfileId(p_ProfileId)
  , m_ChatId(p_ChatId)
{
  UpdateList();
}

UiThreadListDialog::~UiThreadListDialog()
{
}

UiThreadListItem UiThreadListDialog::GetSelectedThreadItem()
{
  return m_SelectedThreadItem;
}

void UiThreadListDialog::OnSelect()
{
  if (m_ThreadListItemVec.empty()) return;

  m_SelectedThreadItem = m_ThreadListItemVec[m_Index];
  m_Result = true;
  m_Running = false;
}

void UiThreadListDialog::OnBack()
{
}

bool UiThreadListDialog::OnTimer()
{
  return false; // no update
}

void UiThreadListDialog::UpdateList()
{
  const bool emojiEnabled = m_Model->GetEmojiEnabled();

  m_Index = 0;
  m_Items.clear();
  m_ThreadListItemVec.clear();

  const std::vector<ChatThreadInfo> threadVec = m_Model->GetThreadVec(m_ProfileId, m_ChatId);
  for (const ChatThreadInfo& threadInfo : threadVec)
  {
    std::string name = threadInfo.name;
    if (name.empty())
    {
      name = threadInfo.id;
    }

    if (m_FilterStr.empty() ||
        (StrUtil::ToFold(name).find(StrUtil::ToFold(StrUtil::ToString(m_FilterStr))) != std::string::npos))
    {
      std::string displayName = name;

      if (!emojiEnabled)
      {
        displayName = StrUtil::Textize(displayName);
      }

      if (threadInfo.isClosed)
      {
        displayName += " (closed)";
      }

      UiThreadListItem threadListItem;
      threadListItem.topicId = threadInfo.id;
      threadListItem.name = displayName;

      m_Items.push_back(StrUtil::TrimPadWString(StrUtil::ToWString(displayName), m_W));
      m_ThreadListItemVec.push_back(threadListItem);
    }
  }
}
