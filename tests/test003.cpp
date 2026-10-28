// test003.cpp - thread support data model and tgchat topic conversions
//
// Covers the message cache topicId schema (roundtrip and v12 migration) and
// the self-contained tgchat forum topic conversions (topic id hex encoding,
// incoming topic id mapping and thread list conversion).
//
// Copyright (c) 2026 Kristofer Berggren
// All rights reserved.
//
// nchat is distributed under the MIT license, see LICENSE for details.

#include <cstdint>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include <sqlite_modern_cpp.h>

#include "appconfig.h"
#include "fileutil.h"
#include "log.h"
#include "messagecache.h"
#include "strutil.h"
#include "tgtopic.h"
#include "timeutil.h"

#include "unittest.h"

static const std::string s_AppDir = "test003-appdir";

// Verifies the cached message within a bounded time, covering the async
// message cache write queue.
static bool WaitForCachedMessage(const std::string& p_ProfileId, const std::string& p_ChatId,
                                 const std::string& p_MsgId, ChatMessage& p_ChatMessage)
{
  for (int i = 0; i < 200; ++i) // up to 2 s
  {
    std::vector<ChatMessage> chatMessages;
    if (MessageCache::GetOneMessage(p_ProfileId, p_ChatId, p_MsgId, chatMessages) && !chatMessages.empty())
    {
      p_ChatMessage = chatMessages.front();
      return true;
    }

    TimeUtil::Sleep(0.010);
  }

  return false;
}

static void TestTopicIdHexRoundtrip()
{
  // forum topic ids are int32, message ids are int53; the hex encoding of the
  // decimal representation must roundtrip exactly for topic ids
  for (const int32_t topicId : { 1, 2, 100, 255, 65536, 2147483647 })
  {
    const std::string hex = StrUtil::NumToHex(topicId);
    unittest::ExpectEqual(int32_t, StrUtil::NumFromHex<int32_t>(hex), topicId);
    unittest::ExpectEqual(int32_t, TgTopic::TopicIdFromHex(hex)->forum_topic_id_, topicId);
    unittest::ExpectEqual(std::string, TgTopic::TopicIdToHex(TgTopic::TopicIdFromHex(hex).get()), hex);
  }

  // pinned formats for the General topic and a sample topic
  unittest::ExpectEqual(std::string, StrUtil::NumToHex(1), "31");
  unittest::ExpectEqual(std::string, StrUtil::NumToHex(100), "313030");
}

static void TestTopicIdConversion()
{
  td::td_api::object_ptr<td::td_api::message> message = td::td_api::make_object<td::td_api::message>();

  // a forum topic id on an incoming message is hex-encoded
  message->topic_id_ = td::td_api::make_object<td::td_api::messageTopicForum>(100);
  unittest::ExpectEqual(std::string, TgTopic::TopicIdToHex(message->topic_id_.get()), "313030");

  // General in a forum supergroup carries the implicit topic id 1
  message->topic_id_ = td::td_api::make_object<td::td_api::messageTopicForum>(1);
  unittest::ExpectEqual(std::string, TgTopic::TopicIdToHex(message->topic_id_.get()), "31");

  // General in threaded bot chats arrives without a topic id
  message->topic_id_ = nullptr;
  unittest::ExpectEqual(std::string, TgTopic::TopicIdToHex(message->topic_id_.get()), "");

  // non-forum topics (e.g. message threads in groups) are not thread ids
  message->topic_id_ = td::td_api::make_object<td::td_api::messageTopicThread>(123);
  unittest::ExpectEqual(std::string, TgTopic::TopicIdToHex(message->topic_id_.get()), "");
}

static void TestChatThreadInfoDefaults()
{
  const ChatThreadInfo threadInfo;
  unittest::ExpectEqual(std::string, threadInfo.id, "");
  unittest::ExpectEqual(std::string, threadInfo.name, "");
  unittest::ExpectEqual(int32_t, threadInfo.unreadCount, 0);
  unittest::ExpectEqual(int32_t, threadInfo.unreadMentionCount, 0);
  unittest::ExpectFalse(threadInfo.isGeneral);
  unittest::ExpectFalse(threadInfo.isClosed);
  unittest::ExpectFalse(threadInfo.isHidden);
  unittest::ExpectEqual(int64_t, threadInfo.lastMessageTime, -1);

  // equality covers all fields
  ChatThreadInfo other;
  unittest::ExpectTrue(threadInfo == other);
  unittest::ExpectFalse(threadInfo != other);

  other.unreadMentionCount = 2;
  unittest::ExpectTrue(threadInfo != other);
  unittest::ExpectFalse(threadInfo == other);

  other.unreadMentionCount = 0;
  other.isClosed = true;
  unittest::ExpectTrue(threadInfo != other);
}

static td::td_api::object_ptr<td::td_api::forumTopic> MakeForumTopic(int32_t p_ForumTopicId,
                                                                     const std::string& p_Name,
                                                                     bool p_IsGeneral, bool p_IsClosed,
                                                                     bool p_IsHidden, int32_t p_CreationDate,
                                                                     int64_t p_Order, int32_t p_UnreadCount,
                                                                     int32_t p_UnreadMentionCount,
                                                                     int32_t p_LastMessageDate)
{
  auto topicInfo = td::td_api::make_object<td::td_api::forumTopicInfo>();
  topicInfo->chat_id_ = -1001234567890;
  topicInfo->forum_topic_id_ = p_ForumTopicId;
  topicInfo->name_ = p_Name;
  topicInfo->is_general_ = p_IsGeneral;
  topicInfo->is_closed_ = p_IsClosed;
  topicInfo->is_hidden_ = p_IsHidden;
  topicInfo->creation_date_ = p_CreationDate;

  auto topic = td::td_api::make_object<td::td_api::forumTopic>();
  topic->info_ = std::move(topicInfo);
  topic->order_ = p_Order;
  topic->unread_count_ = p_UnreadCount;
  topic->unread_mention_count_ = p_UnreadMentionCount;
  if (p_LastMessageDate > 0)
  {
    auto lastMessage = td::td_api::make_object<td::td_api::message>();
    lastMessage->date_ = p_LastMessageDate;
    topic->last_message_ = std::move(lastMessage);
  }

  return topic;
}

static void TestThreadInfosFromForumTopics()
{
  auto forumTopics = td::td_api::make_object<td::td_api::forumTopics>();
  forumTopics->topics_.push_back(MakeForumTopic(200, "Older topic", false, false, false, 1700000100, 200, 3, 1,
                                                1700000300));
  forumTopics->topics_.push_back(MakeForumTopic(1, "General", true, false, false, 1700000000, 500, 0, 0, 1700000400));
  forumTopics->topics_.push_back(MakeForumTopic(300, "Closed topic", false, true, false, 1700000200, 300, 1, 0, 0));

  const std::vector<ChatThreadInfo> threads = TgTopic::ThreadInfosFromForumTopics(
    TgTopic::ForumTopicsFromTd(*forumTopics));

  // General first, remaining topics by tdlib topic order
  unittest::ExpectEqual(size_t, threads.size(), 3);
  unittest::ExpectEqual(std::string, threads.at(0).id, "31");
  unittest::ExpectEqual(std::string, threads.at(0).name, "General");
  unittest::ExpectTrue(threads.at(0).isGeneral);
  unittest::ExpectEqual(int64_t, threads.at(0).lastMessageTime, 1700000400000);

  unittest::ExpectEqual(std::string, threads.at(1).id, "333030"); // "300"
  unittest::ExpectEqual(std::string, threads.at(1).name, "Closed topic");
  unittest::ExpectTrue(threads.at(1).isClosed);
  // without a last message, the creation date is used
  unittest::ExpectEqual(int64_t, threads.at(1).lastMessageTime, 1700000200000);

  unittest::ExpectEqual(std::string, threads.at(2).id, "323030"); // "200"
  unittest::ExpectEqual(std::string, threads.at(2).name, "Older topic");
  unittest::ExpectEqual(int32_t, threads.at(2).unreadCount, 3);
  unittest::ExpectEqual(int32_t, threads.at(2).unreadMentionCount, 1);
  unittest::ExpectEqual(int64_t, threads.at(2).lastMessageTime, 1700000300000);

  // equal topic orders are broken by the creation date
  auto tiedTopics = td::td_api::make_object<td::td_api::forumTopics>();
  tiedTopics->topics_.push_back(MakeForumTopic(400, "Older", false, false, false, 1700000000, 0, 0, 0, 0));
  tiedTopics->topics_.push_back(MakeForumTopic(500, "Newer", false, false, false, 1700000500, 0, 0, 0, 0));

  const std::vector<ChatThreadInfo> tied = TgTopic::ThreadInfosFromForumTopics(TgTopic::ForumTopicsFromTd(
                                                                                 *tiedTopics));
  unittest::ExpectEqual(size_t, tied.size(), 2);
  unittest::ExpectEqual(std::string, tied.at(0).name, "Newer");
  unittest::ExpectEqual(std::string, tied.at(1).name, "Older");

  // an empty listing maps to an empty thread list
  unittest::ExpectEqual(size_t, TgTopic::ThreadInfosFromForumTopics(
                          TgTopic::ForumTopicsFromTd(*td::td_api::make_object<td::td_api::forumTopics>())).size(), 0);
}

static void TestCacheTopicIdRoundtrip()
{
  const std::string profileId = "test003-profile-a";
  const std::string chatId = "cafe";

  MessageCache::AddProfile(profileId, false /*p_CheckSequence*/, 2 /*p_DirVersion*/, false /*p_IsSetup*/,
                           false /*p_AllowReadOnly*/);

  ChatMessage topicMessage;
  topicMessage.id = "msg1";
  topicMessage.senderId = "user1";
  topicMessage.text = "thread message";
  topicMessage.timeSent = 1700000001000;
  topicMessage.isOutgoing = false;
  topicMessage.isRead = false;
  topicMessage.topicId = "313030"; // "100"

  ChatMessage plainMessage;
  plainMessage.id = "msg2";
  plainMessage.senderId = "user1";
  plainMessage.text = "general message";
  plainMessage.timeSent = 1700000002000;
  plainMessage.isOutgoing = false;
  plainMessage.isRead = false;

  std::vector<ChatMessage> chatMessages = { topicMessage, plainMessage };
  MessageCache::AddMessages(profileId, chatId, "" /*p_FromMsgId*/, chatMessages);

  ChatMessage cached;
  unittest::ExpectTrue(WaitForCachedMessage(profileId, chatId, "msg1", cached));
  unittest::ExpectEqual(std::string, cached.text, "thread message");
  unittest::ExpectEqual(std::string, cached.topicId, "313030");

  unittest::ExpectTrue(WaitForCachedMessage(profileId, chatId, "msg2", cached));
  unittest::ExpectEqual(std::string, cached.text, "general message");
  unittest::ExpectEqual(std::string, cached.topicId, "");
}

static void TestCacheMigration()
{
  const std::string profileId = "test003-profile-b";
  const std::string chatId = "cafe";
  const std::string dbDir = FileUtil::GetApplicationDir() + "/history/" + profileId;
  const std::string dbPath = dbDir + "/db.sqlite";

  // pre-create the directory at the current dir version so the handcrafted
  // database is not discarded, then create a schema 12 database with a legacy
  // message row
  FileUtil::MkDir(dbDir);
  FileUtil::InitDirVersion(dbDir, 2);
  {
    sqlite::database db(dbPath);
    db << "CREATE TABLE messages ("
      "chatId TEXT,"
      "id TEXT,"
      "senderId TEXT,"
      "text TEXT,"
      "quotedId TEXT,"
      "quotedText TEXT,"
      "quotedSender TEXT,"
      "fileInfo TEXT,"
      "fileStatus INT,"
      "fileType TEXT,"
      "timeSent INT,"
      "isOutgoing INT,"
      "isRead INT,"
      "UNIQUE(chatId, id) ON CONFLICT REPLACE"
      ");";
    db << "ALTER TABLE messages ADD COLUMN reactions BLOB;";
    db << "ALTER TABLE messages ADD COLUMN isSequence INT DEFAULT 1;";
    db << "ALTER TABLE messages ADD COLUMN isEdited INT DEFAULT 0;";
    db << "ALTER TABLE messages ADD COLUMN isDeleted INT DEFAULT 0;";
    db << "ALTER TABLE messages ADD COLUMN isPinned INT DEFAULT 0;";
    db << "CREATE TABLE version AS "
      "SELECT COUNT(name) AS schema FROM sqlite_master WHERE TYPE='table' AND "
      "(name='contacts2' OR name='chats2' OR name='messages');";
    db << "UPDATE version SET schema = 12;";

    db << "INSERT INTO messages (chatId, id, senderId, text, quotedId, quotedText, quotedSender, fileInfo, "
      "timeSent, isOutgoing, isRead) VALUES "
      "(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);" <<
      chatId << "legacy" << "user1" << "legacy message" << "" << "" << "" << "" << 1700000000000 << 0 << 0;
  }

  MessageCache::AddProfile(profileId, false /*p_CheckSequence*/, 2 /*p_DirVersion*/, false /*p_IsSetup*/,
                           false /*p_AllowReadOnly*/);

  // the schema was migrated, the topicId column exists and old rows load with
  // an empty topicId
  {
    sqlite::database db(dbPath);
    int64_t schemaVersion = 0;
    db << "SELECT schema FROM version;" >> schemaVersion;
    unittest::ExpectEqual(int64_t, schemaVersion, 13);

    int64_t topicIdColumns = 0;
    db << "SELECT COUNT(*) FROM pragma_table_info('messages') WHERE name = 'topicId';" >> topicIdColumns;
    unittest::ExpectEqual(int64_t, topicIdColumns, 1);
  }

  ChatMessage cached;
  unittest::ExpectTrue(WaitForCachedMessage(profileId, chatId, "legacy", cached));
  unittest::ExpectEqual(std::string, cached.text, "legacy message");
  unittest::ExpectEqual(std::string, cached.topicId, "");

  // and newly cached messages keep their thread assignment
  ChatMessage topicMessage;
  topicMessage.id = "topicmsg";
  topicMessage.senderId = "user1";
  topicMessage.text = "thread message";
  topicMessage.timeSent = 1700000005000;
  topicMessage.isOutgoing = false;
  topicMessage.isRead = false;
  topicMessage.topicId = "31"; // General in a forum supergroup

  std::vector<ChatMessage> chatMessages = { topicMessage };
  MessageCache::AddMessages(profileId, chatId, "" /*p_FromMsgId*/, chatMessages);

  unittest::ExpectTrue(WaitForCachedMessage(profileId, chatId, "topicmsg", cached));
  unittest::ExpectEqual(std::string, cached.topicId, "31");
}

int main()
{
  int rv = 0;

  FileUtil::RmDir(s_AppDir);
  FileUtil::MkDir(s_AppDir);
  FileUtil::SetApplicationDir(s_AppDir);
  AppConfig::Init();
  Log::Init(s_AppDir + "/test003.log");
  Log::SetVerboseLevel(Log::DEBUG_LEVEL);
  MessageCache::Init();

  try
  {
    TestTopicIdHexRoundtrip();
    TestTopicIdConversion();
    TestChatThreadInfoDefaults();
    TestThreadInfosFromForumTopics();
    TestCacheTopicIdRoundtrip();
    TestCacheMigration();
  }
  catch (const std::exception& ex)
  {
    std::cout << "exception: " << ex.what() << std::endl;
    rv = 1;
  }

  MessageCache::Cleanup();
  AppConfig::Cleanup();
  Log::Cleanup(false);

  // keep the log for debugging when the test failed
  if (rv == 0)
  {
    FileUtil::RmDir(s_AppDir);
  }

  return rv;
}
