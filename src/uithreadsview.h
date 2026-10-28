// uithreadsview.h
//
// Copyright (c) 2026 Kristofer Berggren
// All rights reserved.
//
// nchat is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <ncurses.h>

#include "uiviewbase.h"

class UiThreadsView : public UiViewBase
{
public:
  UiThreadsView(const UiViewParams& p_Params);
  virtual ~UiThreadsView();

  virtual void Draw();

private:
  WINDOW* m_PaddedWin = nullptr;
  int m_PaddedH = 0;
  int m_PaddedW = 0;
};
