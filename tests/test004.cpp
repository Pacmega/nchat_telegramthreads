// test004.cpp - thread support ui model helpers
//
// Copyright (c) 2026 Kristofer Berggren
// All rights reserved.
//
// nchat is distributed under the MIT license, see LICENSE for details.

#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "threadutil.h"

#include "unittest.h"

static void TestBuildThreadInfos()
{
  // a threaded bot chat has no received General topic; the menu entry maps to
  // the unfiltered no-thread selection (empty id)
  ChatThreadInfo topicFive;
  topicFive.id = "35";
  topicFive.name = "Topic five";
  topicFive.unreadCount = 2;

  const std::vector<ChatThreadInfo> botThreads = ThreadUtil::BuildThreadInfos({ topicFive });
  unittest::ExpectEqual(size_t, botThreads.size(), 2);
  unittest::ExpectTrue(botThreads.at(0).isGeneral);
  unittest::ExpectEqual(std::string, botThreads.at(0).id, "");
  unittest::ExpectEqual(std::string, botThreads.at(0).name, "General");
  unittest::ExpectEqual(std::string, botThreads.at(1).id, "35");

  // a forum supergroup receives the General topic (implicit topic id 1), which
  // is folded into the menu entry rather than listed twice
  ChatThreadInfo generalTopic;
  generalTopic.id = "31";
  generalTopic.name = "General";
  generalTopic.isGeneral = true;
  generalTopic.unreadCount = 3;

  ChatThreadInfo hiddenTopic;
  hiddenTopic.id = "37";
  hiddenTopic.name = "Hidden topic";
  hiddenTopic.isHidden = true;

  const std::vector<ChatThreadInfo> forumThreads =
    ThreadUtil::BuildThreadInfos({ hiddenTopic, topicFive, generalTopic });
  unittest::ExpectEqual(size_t, forumThreads.size(), 2);
  unittest::ExpectTrue(forumThreads.at(0).isGeneral);
  unittest::ExpectEqual(std::string, forumThreads.at(0).id, "31");
  unittest::ExpectEqual(int32_t, forumThreads.at(0).unreadCount, 3);
  unittest::ExpectEqual(std::string, forumThreads.at(1).id, "35");

  // an empty snapshot still produces the General entry
  const std::vector<ChatThreadInfo> emptyThreads = ThreadUtil::BuildThreadInfos({ });
  unittest::ExpectEqual(size_t, emptyThreads.size(), 1);
  unittest::ExpectTrue(emptyThreads.at(0).isGeneral);
  unittest::ExpectEqual(std::string, emptyThreads.at(0).id, "");
}

static void TestDeriveThreadName()
{
  unittest::ExpectEqual(std::string, ThreadUtil::DeriveThreadName("Hello world from the office"),
                        "Hello world from the");
  unittest::ExpectEqual(std::string, ThreadUtil::DeriveThreadName("  Hello   world  "), "Hello world");
  unittest::ExpectEqual(std::string, ThreadUtil::DeriveThreadName("one"), "one");

  // a first word longer than the limit is clipped
  unittest::ExpectEqual(std::string, ThreadUtil::DeriveThreadName("abcdefghijklmnopqrstuvwxyz abc"),
                        "abcdefghijklmnopqrstuvwx");
  unittest::ExpectEqual(std::string, ThreadUtil::DeriveThreadName("abcdefghijkl mnop"), "abcdefghijkl mnop");

  // an empty message (ex. attachment only) falls back to a default name
  unittest::ExpectEqual(std::string, ThreadUtil::DeriveThreadName(""), "New thread");

  // multi-byte characters are not cut mid-codepoint
  unittest::ExpectEqual(std::string, ThreadUtil::DeriveThreadName("你好世界 你好世界 你好世界 你好世界", 10),
                        "你好世界 你好世界");
}

static void TestGetSendAction()
{
  typedef ThreadUtil::ThreadSendAction SendAction;

  // non-threaded chats send topic-less, as before
  unittest::ExpectEqual(int, ThreadUtil::GetSendAction(false, false, "", false), SendAction::ThreadSendActionTopicless);
  unittest::ExpectEqual(int, ThreadUtil::GetSendAction(false, true, "", false), SendAction::ThreadSendActionTopicless);

  // a selected thread is the send target, also for forum supergroups
  unittest::ExpectEqual(int, ThreadUtil::GetSendAction(true, false, "35", false),
                        SendAction::ThreadSendActionSelected);
  unittest::ExpectEqual(int, ThreadUtil::GetSendAction(true, true, "31", false),
                        SendAction::ThreadSendActionSelected);

  // no thread selected: threaded bot chats create a new thread first, forum
  // supergroups send topic-less (lands in General)
  unittest::ExpectEqual(int, ThreadUtil::GetSendAction(true, false, "", false),
                        SendAction::ThreadSendActionCreateThenSend);
  unittest::ExpectEqual(int, ThreadUtil::GetSendAction(true, true, "", false), SendAction::ThreadSendActionTopicless);

  // once creation is known to be forbidden, bot chats fall back to topic-less
  unittest::ExpectEqual(int, ThreadUtil::GetSendAction(true, false, "", true), SendAction::ThreadSendActionTopicless);
}

static void TestFilterMessageVec()
{
  std::unordered_map<std::string, ChatMessage> messages;
  for (const int i : { 1, 2, 3, 4 })
  {
    ChatMessage chatMessage;
    chatMessage.id = "msg" + std::to_string(i);
    chatMessage.topicId = ((i % 2) == 0) ? "35" : "";
    messages[chatMessage.id] = chatMessage;
  }

  const std::vector<std::string> messageVec = { "msg4", "msg3", "msg2", "msg1", "msg5" };

  // an empty topic filter passes the full vector through
  const std::vector<std::string> unfiltered = ThreadUtil::FilterMessageVec(messageVec, messages, "");
  unittest::ExpectEqual(size_t, unfiltered.size(), 5);

  // the topic filter keeps the message order, dropping unknown ids
  const std::vector<std::string> filtered = ThreadUtil::FilterMessageVec(messageVec, messages, "35");
  unittest::ExpectEqual(size_t, filtered.size(), 2);
  unittest::ExpectEqual(std::string, filtered.at(0), "msg4");
  unittest::ExpectEqual(std::string, filtered.at(1), "msg2");

  const std::vector<std::string> generalFiltered = ThreadUtil::FilterMessageVec(messageVec, messages, "");
  unittest::ExpectEqual(size_t, generalFiltered.size(), 5);
}

int main()
{
  int rv = 0;

  try
  {
    TestBuildThreadInfos();
    TestDeriveThreadName();
    TestGetSendAction();
    TestFilterMessageVec();
  }
  catch (const std::exception& ex)
  {
    std::cout << "exception: " << ex.what() << std::endl;
    rv = 1;
  }

  return rv;
}
