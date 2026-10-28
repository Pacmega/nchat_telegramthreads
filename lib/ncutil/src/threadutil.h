// threadutil.h
//
// Copyright (c) 2026 Kristofer Berggren
// All rights reserved.
//
// nchat is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "protocol.h"

// Self-contained helpers for thread (forum topic) support in the ui model,
// unit tested in tests/test004.cpp. Note that the "General" topic has forum
// topic id 1, but messages of the General topic arrive without a topic id in
// threaded bot chats, while carrying messageTopicForum{1} in forum supergroups.
class ThreadUtil
{
public:
  enum ThreadSendAction
  {
    ThreadSendActionTopicless = 0, // send without topic id (General or non-threaded chat)
    ThreadSendActionSelected,      // send into the selected thread
    ThreadSendActionCreateThenSend // create a new thread, then send into it
  };

  // Menu thread infos from a notified thread snapshot. The General topic is a
  // regular entry placed first; threaded bot chats have no received General
  // topic (it maps to the unfiltered no-thread selection), while forum
  // supergroups receive it with the implicit topic id 1, which is folded into
  // the entry. Hidden topics are not part of the menu.
  static std::vector<ChatThreadInfo> BuildThreadInfos(const std::vector<ChatThreadInfo>& p_NewThreadInfos);

  // Thread name derived from the first words of the message text, as tdlib
  // requires a non-empty name (1-128 chars) even with an implicit name
  static std::string DeriveThreadName(const std::string& p_Text, size_t p_MaxLen = 24);

  // The send action for a chat based on its threaded state, chat type, the
  // selected thread and whether thread creation is known to be forbidden
  static ThreadSendAction GetSendAction(bool p_IsThreadedChat, bool p_IsGroupChat,
                                        const std::string& p_CurrentThreadId, bool p_CreationForbidden);

  // Message ids of p_MessageVec belonging to the topic p_TopicId, in order;
  // an empty p_TopicId passes the full vector through
  static std::vector<std::string> FilterMessageVec(const std::vector<std::string>& p_MessageVec,
                                                   const std::unordered_map<std::string, ChatMessage>& p_Messages,
                                                   const std::string& p_TopicId);
};
