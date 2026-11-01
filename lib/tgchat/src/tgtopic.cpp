// tgtopic.cpp
//
// Copyright (c) 2026 Kristofer Berggren
// All rights reserved.
//
// nchat is distributed under the MIT license, see LICENSE for details.

#include "tgtopic.h"

#include <algorithm>
#include <utility>

#include "strutil.h"

std::string TgTopic::TopicIdToHex(const td::td_api::MessageTopic* p_Topic)
{
  if ((p_Topic != nullptr) && (p_Topic->get_id() == td::td_api::messageTopicForum::ID))
  {
    const auto& topicForum = static_cast<const td::td_api::messageTopicForum&>(*p_Topic);
    return StrUtil::NumToHex(topicForum.forum_topic_id_);
  }

  return "";
}

td::td_api::object_ptr<td::td_api::messageTopicForum> TgTopic::TopicIdFromHex(const std::string& p_TopicId)
{
  return td::td_api::make_object<td::td_api::messageTopicForum>(StrUtil::NumFromHex<int32_t>(p_TopicId));
}

ForumTopicData TgTopic::ForumTopicDataFromInfo(const td::td_api::forumTopicInfo& p_Info)
{
  ForumTopicData data;
  data.info.id = StrUtil::NumToHex(p_Info.forum_topic_id_);
  data.info.name = p_Info.name_;
  data.info.isGeneral = p_Info.is_general_;
  data.info.isClosed = p_Info.is_closed_;
  data.info.isHidden = p_Info.is_hidden_;
  data.info.lastMessageTime = ((int64_t)p_Info.creation_date_) * 1000;
  data.creationDate = p_Info.creation_date_;
  return data;
}

ForumTopicData TgTopic::ForumTopicDataFromTd(const td::td_api::forumTopic& p_ForumTopic)
{
  ForumTopicData data;
  if (p_ForumTopic.info_)
  {
    data = ForumTopicDataFromInfo(*p_ForumTopic.info_);
  }

  data.order = p_ForumTopic.order_;
  data.info.unreadCount = p_ForumTopic.unread_count_;
  data.info.unreadMentionCount = p_ForumTopic.unread_mention_count_;
  if (p_ForumTopic.last_message_)
  {
    data.info.lastMessageTime = ((int64_t)p_ForumTopic.last_message_->date_) * 1000;
  }

  return data;
}

std::map<int32_t, ForumTopicData> TgTopic::ForumTopicsFromTd(const td::td_api::forumTopics& p_ForumTopics)
{
  std::map<int32_t, ForumTopicData> forumTopics;
  for (const auto& topic : p_ForumTopics.topics_)
  {
    if (!topic || !topic->info_) continue;

    forumTopics[topic->info_->forum_topic_id_] = ForumTopicDataFromTd(*topic);
  }

  return forumTopics;
}

std::vector<ChatThreadInfo> TgTopic::ThreadInfosFromForumTopics(const std::map<int32_t, ForumTopicData>& p_ForumTopics)
{
  std::vector<std::pair<int32_t, ForumTopicData>> topics(p_ForumTopics.begin(), p_ForumTopics.end());

  // General first, then by tdlib topic order (most recent activity first),
  // with the topic creation date as deterministic tiebreaker
  std::stable_sort(topics.begin(), topics.end(), [](const std::pair<int32_t, ForumTopicData>& p_A,
                                                    const std::pair<int32_t, ForumTopicData>& p_B)
  {
    if (p_A.second.info.isGeneral != p_B.second.info.isGeneral) return p_A.second.info.isGeneral;

    if (p_A.second.order != p_B.second.order) return (p_A.second.order > p_B.second.order);

    return (p_A.second.creationDate > p_B.second.creationDate);
  });

  std::vector<ChatThreadInfo> threadInfos;
  threadInfos.reserve(topics.size());
  for (auto& topic : topics)
  {
    threadInfos.push_back(std::move(topic.second.info));
  }

  return threadInfos;
}
