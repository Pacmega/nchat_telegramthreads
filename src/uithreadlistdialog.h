// uithreadlistdialog.h
//
// Copyright (c) 2026 Kristofer Berggren
// All rights reserved.
//
// nchat is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <string>
#include <vector>

#include "protocol.h"
#include "uilistdialog.h"

struct UiThreadListItem
{
  std::string topicId;
  std::string name;
};

class UiThreadListDialog : public UiListDialog
{
public:
  UiThreadListDialog(const UiDialogParams& p_Params, const std::string& p_ProfileId, const std::string& p_ChatId);
  virtual ~UiThreadListDialog();

  UiThreadListItem GetSelectedThreadItem();

protected:
  virtual void OnSelect();
  virtual void OnBack();
  virtual bool OnTimer();

  void UpdateList();

private:
  std::string m_ProfileId;
  std::string m_ChatId;
  std::vector<UiThreadListItem> m_ThreadListItemVec;
  UiThreadListItem m_SelectedThreadItem;
};
