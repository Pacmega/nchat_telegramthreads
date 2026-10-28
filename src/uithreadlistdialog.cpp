// uithreadlistdialog.cpp
//
// Copyright (c) 2026 Kristofer Berggren
// All rights reserved.
//
// nchat is distributed under the MIT license, see LICENSE for details.

#include "uithreadlistdialog.h"

#include "apputil.h"
#include "log.h"
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
  // thread snapshots can arrive while the dialog is open (topic updates,
  // renames, creations); keep the list in sync with the model
  if (m_ThreadInfosUpdateTime != m_Model->GetThreadInfosUpdateTime())
  {
    UpdateList();
    return true;
  }

  return false;
}

void UiThreadListDialog::UpdateList()
{
  const bool emojiEnabled = m_Model->GetEmojiEnabled();

  if (m_ThreadInfosUpdateTime != m_Model->GetThreadInfosUpdateTime())
  {
    m_ThreadInfosUpdateTime = m_Model->GetThreadInfosUpdateTime();
    m_ThreadVec = m_Model->GetThreadVec(m_ProfileId, m_ChatId);
  }

  // preserve the cursor position across list updates
  UiThreadListItem currentItem;
  if (m_Index < (int)m_ThreadListItemVec.size())
  {
    currentItem = m_ThreadListItemVec[m_Index];
  }

  m_Index = 0;
  m_Items.clear();
  m_ThreadListItemVec.clear();

  for (const ChatThreadInfo& threadInfo : m_ThreadVec)
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

      // topics may share names (ex. the bot's own "New Chat" topics), so show
      // the id in developer mode to tell them apart
      static const bool developerMode = AppUtil::GetDeveloperMode();
      if (developerMode)
      {
        displayName += " [" + threadInfo.id + "]";
      }

      UiThreadListItem threadListItem;
      threadListItem.topicId = threadInfo.id;
      threadListItem.name = displayName;

      m_Items.push_back(StrUtil::TrimPadWString(StrUtil::ToWString(displayName), m_W));
      m_ThreadListItemVec.push_back(threadListItem);
    }
  }

  // restore the cursor on the previously highlighted item
  if (!currentItem.topicId.empty() || !currentItem.name.empty())
  {
    for (size_t i = 0; i < m_ThreadListItemVec.size(); ++i)
    {
      if (m_ThreadListItemVec.at(i).topicId == currentItem.topicId)
      {
        m_Index = (int)i;
        break;
      }
    }
  }
}
