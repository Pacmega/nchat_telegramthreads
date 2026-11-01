// tgtopic.h
//
// Copyright (c) 2026 Kristofer Berggren
// All rights reserved.
//
// nchat is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <map>
#include <string>
#include <vector>

#include <td/telegram/td_api.h>
#include <td/telegram/td_api.hpp>

#include "protocol.h"

// In-memory snapshot of a single forum topic
struct ForumTopicData
{
  ChatThreadInfo info;
  int64_t order = 0;        // tdlib topic order (position)
  int32_t creationDate = 0; // topic creation date, fallback sort key
};

// Self-contained conversions between TDLib forum topics and protocol thread
// infos, shared by tgchat and unit tested in tests/test003.cpp. Note that the
// "General" topic has forum topic id 1, but messages of the General topic
// arrive without a topic id in threaded bot chats, while carrying
// messageTopicForum{1} in forum supergroups.
class TgTopic
{
public:
  // Hex-encoded forum topic id of p_Topic; empty for a null or non-forum topic
  // (e.g. General in threaded bot chats)
  static std::string TopicIdToHex(const td::td_api::MessageTopic* p_Topic);

  // Outgoing message topic for a hex-encoded forum topic id
  static td::td_api::object_ptr<td::td_api::messageTopicForum> TopicIdFromHex(const std::string& p_TopicId);

  // Snapshot data from a tdlib forum topic info (no order or counters)
  static ForumTopicData ForumTopicDataFromInfo(const td::td_api::forumTopicInfo& p_Info);

  // Snapshot data from a tdlib forum topic
  static ForumTopicData ForumTopicDataFromTd(const td::td_api::forumTopic& p_ForumTopic);

  // Topic snapshot from a tdlib forum topic listing, keyed by forum topic id
  static std::map<int32_t, ForumTopicData> ForumTopicsFromTd(const td::td_api::forumTopics& p_ForumTopics);

  // Ordered thread infos (General first, then by topic order) from a topic
  // snapshot
  static std::vector<ChatThreadInfo> ThreadInfosFromForumTopics(const std::map<int32_t, ForumTopicData>& p_ForumTopics);
};
