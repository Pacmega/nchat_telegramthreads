// threadutil.cpp
//
// Copyright (c) 2026 Kristofer Berggren
// All rights reserved.
//
// nchat is distributed under the MIT license, see LICENSE for details.

#include "threadutil.h"

#include <sstream>

#include "strutil.h"

std::vector<ChatThreadInfo> ThreadUtil::BuildThreadInfos(const std::vector<ChatThreadInfo>& p_NewThreadInfos)
{
  ChatThreadInfo generalInfo;
  generalInfo.isGeneral = true;

  std::vector<ChatThreadInfo> threadInfos;
  for (const ChatThreadInfo& threadInfo : p_NewThreadInfos)
  {
    if (threadInfo.isHidden)
    {
      continue;
    }

    if (threadInfo.isGeneral)
    {
      // fold the received General topic into the menu's General entry
      generalInfo = threadInfo;
      continue;
    }

    threadInfos.push_back(threadInfo);
  }

  if (generalInfo.name.empty())
  {
    generalInfo.name = "General";
  }

  threadInfos.insert(threadInfos.begin(), generalInfo);
  return threadInfos;
}

std::string ThreadUtil::DeriveThreadName(const std::string& p_Text, size_t p_MaxLen)
{
  const std::wstring defaultName = L"New thread";

  std::wistringstream iss(StrUtil::ToWString(p_Text));
  std::wstring word;
  std::wstring name;
  std::wstring firstWord;
  while (iss >> word)
  {
    if (firstWord.empty())
    {
      firstWord = word;
    }

    const size_t newLen = name.empty() ? word.size() : (name.size() + 1 + word.size());
    if (newLen > p_MaxLen)
    {
      break;
    }

    if (!name.empty())
    {
      name += L" ";
    }

    name += word;
  }

  if (name.empty())
  {
    name = firstWord.substr(0, p_MaxLen);
  }

  if (name.empty())
  {
    name = defaultName;
  }

  return StrUtil::ToString(name);
}

ThreadUtil::ThreadSendAction ThreadUtil::GetSendAction(bool p_IsThreadedChat, bool p_IsGroupChat,
                                                       const std::string& p_CurrentThreadId,
                                                       bool p_CreationForbidden)
{
  if (!p_IsThreadedChat)
  {
    return ThreadSendActionTopicless;
  }

  if (!p_CurrentThreadId.empty())
  {
    return ThreadSendActionSelected;
  }

  // no thread selected: forum supergroups send topic-less (lands in General),
  // threaded bot chats start a new thread, with an adaptive fallback to a
  // topic-less send once thread creation is known to be forbidden
  if (p_IsGroupChat || p_CreationForbidden)
  {
    return ThreadSendActionTopicless;
  }

  return ThreadSendActionCreateThenSend;
}

std::vector<std::string> ThreadUtil::FilterMessageVec(const std::vector<std::string>& p_MessageVec,
                                                      const std::unordered_map<std::string, ChatMessage>& p_Messages,
                                                      const std::string& p_TopicId)
{
  if (p_TopicId.empty())
  {
    return p_MessageVec;
  }

  std::vector<std::string> messageVec;
  for (const std::string& messageId : p_MessageVec)
  {
    std::unordered_map<std::string, ChatMessage>::const_iterator it = p_Messages.find(messageId);
    if ((it != p_Messages.end()) && (it->second.topicId == p_TopicId))
    {
      messageVec.push_back(messageId);
    }
  }

  return messageVec;
}
