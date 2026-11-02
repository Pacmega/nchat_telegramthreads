// duchat.cpp
//
// Copyright (c) 2020-2026 Kristofer Berggren
// All rights reserved.
//
// nchat is distributed under the MIT license, see LICENSE for details.

#include "duchat.h"

#include <algorithm>
#include <iostream>
#include <iterator>
#include <map>

#include <sys/stat.h>

#include "log.h"
#include "status.h"
#include "strutil.h"

static const std::string s_GroupId = "The Office_0";
static const std::string s_ThreadedBotId = "Threadbot_0";
static const std::string s_ForbiddenBotId = "Forbidden_0";
static const std::string s_EmptyBotId = "Emptybot_0";
static const std::string s_PagerBotId = "Pager_0";

// renamed topic names for the threaded bot fixture, applied by
// GetThreadedBotThreadInfos (simulates editForumTopic success)
static std::map<std::string, std::string> s_RenamedTopicNames;

static std::string RenamedTopicName(const std::string& p_TopicId, const std::string& p_Name)
{
  auto it = s_RenamedTopicNames.find(p_TopicId);
  return (it != s_RenamedTopicNames.end()) ? it->second : p_Name;
}

static std::vector<ChatThreadInfo> GetThreadedBotThreadInfos()
{
  std::vector<ChatThreadInfo> threadInfos;
  ChatThreadInfo threadInfo;
  threadInfo.id = StrUtil::NumToHex(6);
  threadInfo.name = RenamedTopicName(threadInfo.id, "Topic six");
  threadInfo.lastMessageTime = 1237989000000;
  threadInfos.push_back(threadInfo);

  threadInfo.id = StrUtil::NumToHex(5);
  threadInfo.name = RenamedTopicName(threadInfo.id, "Topic five");
  threadInfo.unreadCount = 1;
  threadInfo.unreadMentionCount = 0;
  threadInfo.lastMessageTime = 1237992000000;
  threadInfos.push_back(threadInfo);
  return threadInfos;
}

extern "C" DuChat* CreateDuChat()
{
  return new DuChat();
}

DuChat::DuChat()
{
  m_ProfileId = GetName();
}

DuChat::~DuChat()
{
}

std::string DuChat::GetProfileId() const
{
  return m_ProfileId;
}

bool DuChat::HasFeature(ProtocolFeature p_ProtocolFeature) const
{
  static int customFeatures =
    FeatureAutoGetContactsOnLogin |
    FeatureThreads;
  return (p_ProtocolFeature & customFeatures);
}

bool DuChat::IsGroupChat(const std::string& p_ChatId) const
{
  return (p_ChatId == s_GroupId);
}

std::string DuChat::GetSelfId() const
{
  return "Stanley_0";
}

std::string DuChat::GetProfileDisplayName() const
{
  static std::string profileDisplayName = "";
  return profileDisplayName;
}

bool DuChat::SetupProfile(const std::string& p_ProfilesDir, std::string& p_ProfileId)
{
  std::string phoneNumber = StrUtil::GetPhoneNumber();

  m_ProfileId = m_ProfileId + "_" + phoneNumber;
  std::string profileDir = p_ProfilesDir + "/" + m_ProfileId;

  mkdir(profileDir.c_str(), 0777);

  p_ProfileId = m_ProfileId;

  return true;
}

bool DuChat::LoadProfile(const std::string& p_ProfilesDir, const std::string& p_ProfileId)
{
  LOG_INFO("load dummy profile");
  (void)p_ProfilesDir;
  m_ProfileId = p_ProfileId;
  return true;
}

bool DuChat::CloseProfile()
{
  m_ProfileId = "";
  return true;
}

bool DuChat::Login()
{
  Status::Set(m_ProfileId, Status::FlagOnline);

  if (!m_Running)
  {
    m_Running = true;
    m_Thread = std::thread(&DuChat::Process, this);

    std::shared_ptr<ConnectNotify> connectNotify =
      std::make_shared<ConnectNotify>(m_ProfileId);
    connectNotify->success = true;

    std::shared_ptr<DeferNotifyRequest> deferNotifyRequest =
      std::make_shared<DeferNotifyRequest>();
    deferNotifyRequest->serviceMessage = connectNotify;
    SendRequest(deferNotifyRequest);
  }
  return true;
}

bool DuChat::Logout()
{
  Status::Clear(m_ProfileId, Status::FlagOnline);

  if (m_Running)
  {
    std::unique_lock<std::mutex> lock(m_ProcessMutex);
    m_Running = false;
    m_ProcessCondVar.notify_one();
  }

  if (m_Thread.joinable())
  {
    m_Thread.join();
  }
  return true;
}

void DuChat::Process()
{
  while (m_Running)
  {
    std::shared_ptr<RequestMessage> requestMessage;

    {
      std::unique_lock<std::mutex> lock(m_ProcessMutex);
      while (m_RequestsQueue.empty() && m_Running)
      {
        m_ProcessCondVar.wait(lock);
      }

      if (!m_Running)
      {
        break;
      }

      requestMessage = m_RequestsQueue.front();
      m_RequestsQueue.pop_front();
    }

    PerformRequest(requestMessage);
  }
}

void DuChat::SendRequest(std::shared_ptr<RequestMessage> p_RequestMessage)
{
  std::unique_lock<std::mutex> lock(m_ProcessMutex);
  m_RequestsQueue.push_back(p_RequestMessage);
  m_ProcessCondVar.notify_one();
}


void DuChat::SetMessageHandler(const std::function<void(std::shared_ptr<ServiceMessage>)>& p_MessageHandler)
{
  m_MessageHandler = p_MessageHandler;
}

void DuChat::PerformRequest(std::shared_ptr<RequestMessage> p_RequestMessage)
{
  if (!m_MessageHandler) return;

  static std::map<std::string, std::vector<ChatMessage>> s_Messages;

  switch (p_RequestMessage->GetMessageType())
  {
    case GetChatsRequestType:
      {
        std::shared_ptr<GetChatsRequest> getChatsRequest =
          std::static_pointer_cast<GetChatsRequest>(p_RequestMessage);
        std::shared_ptr<NewChatsNotify> newChatsNotify =
          std::make_shared<NewChatsNotify>(m_ProfileId);
        std::shared_ptr<NewContactsNotify> newContactsNotify =
          std::make_shared<NewContactsNotify>(m_ProfileId);
        newChatsNotify->success = true;

        static std::vector<std::pair<std::string, std::string>> messages =
        {
          { "Michael",
            "Would I rather be feared or loved? Easy. Both. I want people to "
            "be afraid of how much they love me." },
          { "Dwight",
            "Whenever I'm about to do something, I think, 'Would an idiot do "
            "that?' And if they would, I do not do that thing." },
          { "Jim",
            "Right now, this is just a job. If I advance any higher in this "
            "company, this would be my career. And, uh, if this were my "
            "career, I’d have to throw myself in front of a train." },
          { "Pam", "There's a lot of beauty in ordinary things. Isn't that "
            "kind of the point?" },
          /* { "Stanley", "" }, */
          { "Phyllis",
            "Andy sings beautifully. And he's really good at dancing. He's "
            "a good speaker. But there's just something there you don't want "
            "to look at." },
          { "Kevin",
            "Mini-cupcakes? As in the mini version of regular cupcakes? Which "
            "is already a mini version of cake? Honestly, where does it end "
            "with you people?" },
          { "Ryan", "I'd rather she be alone than with somebody. Is that love?" },
          { "Angela",
            "Malls are just awful and humiliating. They’re just store after "
            "store of these horrible salespeople making a big fuss out of an "
            "adult shopping in a junior’s section. There are petite adults "
            "who are sort of… smaller who need to wear… maybe a kids’ size 10." },
          { "Oscar",
            "Angela's engaged to a gay man. As a gay man, I'm horrified. As a "
            "friend of Angela's, horrified. As a lover of elegant weddings, "
            "I'm a little excited." },
          { "Kelly", "I have a lot of questions. Number one, how dare you?" },
          { "Meredith",
            "Stop fighting! Just on St. Patrick's Day okay? Just one, perfect "
            "day a year. No hassles. No problems. No kids." },
          { "Creed",
            "I am not offended by homosexuality, in the sixties I made love "
            "to many, many women, often outdoors in the mud & rain. It’s "
            "possible a man could’ve slipped in there. There’d be no way of "
            "knowing." },
          { "Darryl",
            "I've been meaning to join a gym for my health. I used to say I "
            "wanted to live long enough to see a black president. I didn't "
            "realize how easy that would be. So now I want to live long "
            "enough to see a really, really gay president. Or a supermodel "
            "president. I want to see all the different kinds of presidents." },
          { "Toby",
            "Oh, I went zip lining my third day in Costa Rica. I guess the "
            "harness wasn't strapped in exactly right. I broke my neck. And, "
            "I've been in the hospital five weeks now. I still haven't seen "
            "the beach. It's nice to have visitors." },
          { "Erin",
            "Whenever I'm sick, it goes away within a few hours. Except that "
            "once when I was in the hospital from age three to six." },
          { "Gabe",
            "Apparently, I bear a passing resemblance to Abraham Lincoln. "
            "Makes it kind of hard for me to go to places like museums, "
            "historical monuments, elementary schools... I don't see it." },
          { "Andy",
            "I went to Cornell. Ever heard of it? I graduated in four years, "
            "I never studied once, I was drunk the whole time, and I sang in "
            "the acapella group, 'Here Comes Treble'." },
          { "Lorem",
            "Lorem ipsum dolor sit amet, ea vel tale virtute qualisque. Ad "
            "eam viris salutandi, per no lucilius corrumpit. Ea gubergren "
            "inciderint cum. An melsaperet insolens dissentiunt. Diam vide "
            "ne sed, elitr causae ea mea. Quo ut graece latine." },
          { "Mixed",
            "Hello 你好! This is a text with mixed English and 中文 characters. "
            "Hello 你好! This is a text with mixed English and 中文 characters. " },
          { "Chinese",
            "一二三。一二三。一二三。一二三。一二三。一二三。一二三。一二三。一二三。一二三。"
            "一二三。一二三。一二三。一二三。一二三。一二三。一二三。一二三。一二三。一二三。" },
          { "No Space",
            "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz"
            "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz" },
          { "Emoji 🌍",
            "📃📃📃📃📃 📃📃📃📃📃. 📃📃📃📃📃 📃📃📃📃📃. 📃📃📃📃📃 📃📃📃📃📃. "
            "📃📃📃📃📃 📃📃📃📃📃. 📃📃📃📃📃 📃📃📃📃📃." },
          { "Emoji ❤️",
            "👍😊😘😄😅😔😂🎉🙂❤💪🙏🔔🤗👌😮🤔😱👼😍😻😢😬😋🍺😁😛🤞🤣😴😎🍻🖤😇"
            "🤓😝😜😓🪥🎂🥣🎄🥶🌍💚🥪⛈📘🚙💼👏🔐😰🥟😳🍔🎧🗾🍋📝👱🛟😆🐍💬☎📞❌ "
            "💯🥇🥈🥉🎱🧮🛫⏰" },
        };

        static std::vector<std::pair<std::string, std::string>> groupMessages =
        {
          { "Stanley", "Maybe you should go into your office, close the door, "
            "and make some calls about jobs?" },
          { "Michael", "I have a job." },
          { "Andy", "For four more days." },
          { "Pam", "Do you have any leads on a job?" },
          { "Michael",
            "Pam, what you don't understand is that at my level you just don't "
            "look in the want-ads for a job. You are head-hunted." },
          { "Jim", "You've called any headhunters? 🤔" },
          { "Michael", "Any good headhunter knows I am available." },
          { "Dwight",
            "Any really good headhunter would storm your village at sunset with "
            "overwhelming force and cut off your head with a ceremonial knife." },
        };
        std::reverse(groupMessages.begin(), groupMessages.end());

        int64_t t = 1237922000;
        s_Messages.clear();
        newChatsNotify->chatInfos.clear();

        // Individual chats
        for (auto& message : messages)
        {
          std::string name = message.first;
          std::string text = message.second;
          std::string id = name + "_0";
          bool isUnread = false;

          ChatInfo chatInfo;
          chatInfo.id = id;
          chatInfo.lastMessageTime = (t * 1000);
          chatInfo.isUnread = isUnread;
          newChatsNotify->chatInfos.push_back(chatInfo);

          ContactInfo contactInfo;
          contactInfo.id = id;
          contactInfo.name = name;
          newContactsNotify->contactInfos.push_back(contactInfo);

          ChatMessage chatMessage;
          chatMessage.id = id + "_" + std::to_string(t);
          chatMessage.senderId = id;
          chatMessage.text = text;
          chatMessage.timeSent = (t * 1000);
          chatMessage.isOutgoing = false;
          chatMessage.isRead = !isUnread;
          t = t - 100;
          s_Messages[id].push_back(chatMessage);
        }

        // Self
        std::string sname = "Stanley";
        std::string sid = sname + "_0";
        {
          ContactInfo scontactInfo;
          scontactInfo.id = sid;
          scontactInfo.name = sname;
          scontactInfo.isSelf = true;
          newContactsNotify->contactInfos.push_back(scontactInfo);
        }

        // Group chat
        {
          t = 1237962000;
          ChatInfo chatInfo;
          chatInfo.id = s_GroupId;
          chatInfo.lastMessageTime = (t * 1000);
          newChatsNotify->chatInfos.push_back(chatInfo);

          ContactInfo contactInfo;
          contactInfo.id = s_GroupId;
          contactInfo.name = "The Office";
          newContactsNotify->contactInfos.push_back(contactInfo);

          // From others
          bool isFirst = true;
          for (auto& message : groupMessages)
          {
            std::string name = message.first;
            std::string id = name + "_0";

            ChatMessage chatMessage;
            chatMessage.id = id + "_" + std::to_string(t);
            chatMessage.senderId = id;
            chatMessage.text = message.second;
            chatMessage.timeSent = (t * 1000);
            chatMessage.isOutgoing = (id == sid);
            chatMessage.isRead = true;

            if (isFirst)
            {
              isFirst = false;
              chatMessage.reactions.emojiCounts["😨"] = 4;
              chatMessage.reactions.emojiCounts["🤣"] = 2;
            }

            t = t - 100;
            s_Messages[s_GroupId].push_back(chatMessage);
          }
        }

        // Threaded bot chat; messages carry topic ids, the General topic
        // messages have none (as in threaded bot chats on Telegram)
        {
          t = 1237992000;
          ChatInfo chatInfo;
          chatInfo.id = s_ThreadedBotId;
          chatInfo.lastMessageTime = (t * 1000);
          newChatsNotify->chatInfos.push_back(chatInfo);

          ContactInfo contactInfo;
          contactInfo.id = s_ThreadedBotId;
          contactInfo.name = "Threaded Bot";
          newContactsNotify->contactInfos.push_back(contactInfo);

          const std::vector<std::pair<std::string, std::string>> threadMessages =
          {
            { "35", "This message belongs to the first topic." },
            { "35", "Another message in the first topic." },
            { "", "This message belongs to General." },
            { "36", "A message in the second topic." },
          };

          for (auto& message : threadMessages)
          {
            ChatMessage chatMessage;
            chatMessage.id = s_ThreadedBotId + "_" + std::to_string(t);
            chatMessage.senderId = s_ThreadedBotId;
            chatMessage.text = message.second;
            chatMessage.timeSent = (t * 1000);
            chatMessage.isOutgoing = false;
            chatMessage.isRead = true;
            chatMessage.topicId = message.first;

            t = t - 100;
            s_Messages[s_ThreadedBotId].push_back(chatMessage);
          }
        }

        // Threaded bot chat which forbids new thread creation
        {
          t = 1238002000;
          ChatInfo chatInfo;
          chatInfo.id = s_ForbiddenBotId;
          chatInfo.lastMessageTime = (t * 1000);
          newChatsNotify->chatInfos.push_back(chatInfo);

          ContactInfo contactInfo;
          contactInfo.id = s_ForbiddenBotId;
          contactInfo.name = "Forbidden Bot";
          newContactsNotify->contactInfos.push_back(contactInfo);

          ChatMessage chatMessage;
          chatMessage.id = s_ForbiddenBotId + "_" + std::to_string(t);
          chatMessage.senderId = s_ForbiddenBotId;
          chatMessage.text = "This bot does not allow new threads.";
          chatMessage.timeSent = (t * 1000);
          chatMessage.isOutgoing = false;
          chatMessage.isRead = true;
          s_Messages[s_ForbiddenBotId].push_back(chatMessage);
        }

        // Threaded bot chat without any messages, for the start-a-new-thread hint
        {
          ChatInfo chatInfo;
          chatInfo.id = s_EmptyBotId;
          chatInfo.lastMessageTime = 1238012000000;
          newChatsNotify->chatInfos.push_back(chatInfo);

          ContactInfo contactInfo;
          contactInfo.id = s_EmptyBotId;
          contactInfo.name = "Empty Bot";
          newContactsNotify->contactInfos.push_back(contactInfo);
        }

        // Threaded bot chat with more topics than one page holds
        {
          t = 1238022000;
          ChatInfo chatInfo;
          chatInfo.id = s_PagerBotId;
          chatInfo.lastMessageTime = (t * 1000);
          newChatsNotify->chatInfos.push_back(chatInfo);

          ContactInfo contactInfo;
          contactInfo.id = s_PagerBotId;
          contactInfo.name = "Paginated Bot";
          newContactsNotify->contactInfos.push_back(contactInfo);

          for (int i = 0; i < 120; ++i)
          {
            ChatMessage chatMessage;
            chatMessage.id = s_PagerBotId + "_" + std::to_string(t);
            chatMessage.senderId = s_PagerBotId;
            chatMessage.text = "Message in paginated topic " + std::to_string(i + 1) + ".";
            chatMessage.timeSent = (t * 1000);
            chatMessage.isOutgoing = false;
            chatMessage.isRead = true;
            chatMessage.topicId = StrUtil::NumToHex(1000 + i);
            s_Messages[s_PagerBotId].push_back(chatMessage);
            t = t - 100;
          }
        }

        m_MessageHandler(newChatsNotify);
        m_MessageHandler(newContactsNotify);
      }
      break;

    case GetMessagesRequestType:
      {
        std::shared_ptr<GetMessagesRequest> getMessagesRequest =
          std::static_pointer_cast<GetMessagesRequest>(p_RequestMessage);
        std::shared_ptr<NewMessagesNotify> newMessagesNotify =
          std::make_shared<NewMessagesNotify>(m_ProfileId);
        newMessagesNotify->success = true;
        newMessagesNotify->chatId = getMessagesRequest->chatId;
        newMessagesNotify->cached = false;
        newMessagesNotify->sequence = true;

        newMessagesNotify->chatMessages = s_Messages[getMessagesRequest->chatId];

        // topic-scoped history requests return only the topic's messages
        const std::string& topicId = getMessagesRequest->topicId;
        if (!topicId.empty())
        {
          std::vector<ChatMessage> topicMessages;
          std::copy_if(newMessagesNotify->chatMessages.begin(), newMessagesNotify->chatMessages.end(),
                       std::back_inserter(topicMessages),
                       [&](const ChatMessage& chatMessage) { return (chatMessage.topicId == topicId); });
          newMessagesNotify->chatMessages = topicMessages;
        }

        m_MessageHandler(newMessagesNotify);
      }
      break;

    case GetThreadsRequestType:
      {
        std::shared_ptr<GetThreadsRequest> getThreadsRequest =
          std::static_pointer_cast<GetThreadsRequest>(p_RequestMessage);
        std::shared_ptr<NewThreadsNotify> newThreadsNotify =
          std::make_shared<NewThreadsNotify>(m_ProfileId);
        newThreadsNotify->chatId = getThreadsRequest->chatId;

        if (getThreadsRequest->chatId == s_ThreadedBotId)
        {
          for (const ChatThreadInfo& threadInfo : GetThreadedBotThreadInfos())
          {
            newThreadsNotify->threads.push_back(threadInfo);
          }
        }
        else if (getThreadsRequest->chatId == s_ForbiddenBotId)
        {
          ChatThreadInfo threadInfo;
          threadInfo.id = StrUtil::NumToHex(9);
          threadInfo.name = "Only topic";
          threadInfo.unreadMentionCount = 1;
          threadInfo.lastMessageTime = 1238002000000;
          newThreadsNotify->threads.push_back(threadInfo);
        }
        else if (getThreadsRequest->chatId == s_EmptyBotId)
        {
          ChatThreadInfo threadInfo;
          threadInfo.id = StrUtil::NumToHex(1);
          threadInfo.name = "General topic";
          threadInfo.isGeneral = true;
          threadInfo.lastMessageTime = 1238012000000;
          newThreadsNotify->threads.push_back(threadInfo);
        }
        else if (getThreadsRequest->chatId == s_PagerBotId)
        {
          // simulates getForumTopics pagination; refreshes return the
          // accumulated list as-is, only an explicit next page request extends it
          static std::vector<ChatThreadInfo> pagerTopics;
          if ((getThreadsRequest->nextPage || pagerTopics.empty()) && (pagerTopics.size() < 120))
          {
            const int firstTopic = (int)pagerTopics.size();
            for (int i = firstTopic; i < (firstTopic + 10); ++i)
            {
              ChatThreadInfo threadInfo;
              threadInfo.id = StrUtil::NumToHex(1000 + i);
              threadInfo.name = "Paginated topic " + std::to_string(i + 1);
              threadInfo.lastMessageTime = (1238022000 - i * 100) * 1000;
              pagerTopics.push_back(threadInfo);
            }
          }

          for (const ChatThreadInfo& threadInfo : pagerTopics)
          {
            newThreadsNotify->threads.push_back(threadInfo);
          }

          newThreadsNotify->hasMoreThreads = (pagerTopics.size() < 120);
        }

        // other chats are not threaded (empty snapshot)
        m_MessageHandler(newThreadsNotify);
      }
      break;

    case CreateThreadRequestType:
      {
        std::shared_ptr<CreateThreadRequest> createThreadRequest =
          std::static_pointer_cast<CreateThreadRequest>(p_RequestMessage);
        std::shared_ptr<NewThreadNotify> newThreadNotify =
          std::make_shared<NewThreadNotify>(m_ProfileId);
        newThreadNotify->chatId = createThreadRequest->chatId;

        if (createThreadRequest->chatId == s_ThreadedBotId)
        {
          static int nextTopicId = 100;
          ChatThreadInfo threadInfo;
          threadInfo.id = StrUtil::NumToHex(nextTopicId++);
          threadInfo.name = createThreadRequest->name;
          newThreadNotify->thread = threadInfo;
          newThreadNotify->success = true;
        }
        else
        {
          // simulates the server rejecting thread creation (ex.
          // BOT_FORUM_CREATE_FORBIDDEN)
          newThreadNotify->success = false;
        }

        m_MessageHandler(newThreadNotify);
      }
      break;

    case RenameThreadRequestType:
      {
        std::shared_ptr<RenameThreadRequest> renameThreadRequest =
          std::static_pointer_cast<RenameThreadRequest>(p_RequestMessage);
        std::shared_ptr<NewThreadsNotify> newThreadsNotify =
          std::make_shared<NewThreadsNotify>(m_ProfileId);
        newThreadsNotify->chatId = renameThreadRequest->chatId;

        if (renameThreadRequest->chatId == s_ThreadedBotId)
        {
          s_RenamedTopicNames[renameThreadRequest->topicId] = renameThreadRequest->name;
          newThreadsNotify->threads = GetThreadedBotThreadInfos();
        }
        else
        {
          // simulates the server rejecting topic renames (ex. missing topic
          // manage rights); the snapshot is sent unchanged
        }

        m_MessageHandler(newThreadsNotify);
      }
      break;

    case SendMessageRequestType:
      {
        std::shared_ptr<SendMessageRequest> sendMessageRequest =
          std::static_pointer_cast<SendMessageRequest>(p_RequestMessage);
        std::shared_ptr<SendMessageNotify> sendMessageNotify =
          std::make_shared<SendMessageNotify>(m_ProfileId);
        sendMessageNotify->success = true;
        sendMessageNotify->chatId = sendMessageRequest->chatId;
        sendMessageNotify->chatMessage = sendMessageRequest->chatMessage;
        m_MessageHandler(sendMessageNotify);

        // the threaded bot chats reply in the topic the message was sent to,
        // making the send routing observable in the ui and logs
        const std::string& chatId = sendMessageRequest->chatId;
        const bool isThreadedChat = ((chatId == s_ThreadedBotId) || (chatId == s_ForbiddenBotId) ||
                                     (chatId == s_PagerBotId));
        if (isThreadedChat)
        {
          static int replyCount = 0;
          ++replyCount;

          ChatMessage chatMessage;
          chatMessage.id = chatId + "_reply_" + std::to_string(replyCount);
          chatMessage.senderId = chatId;
          chatMessage.text = "Reply " + std::to_string(replyCount) + " to: " + sendMessageRequest->chatMessage.text;
          chatMessage.timeSent = (1238012000 + replyCount) * 1000;
          chatMessage.isOutgoing = false;
          chatMessage.isRead = false;
          chatMessage.topicId = sendMessageRequest->chatMessage.topicId;
          s_Messages[chatId].push_back(chatMessage);

          std::shared_ptr<NewMessagesNotify> newMessagesNotify =
            std::make_shared<NewMessagesNotify>(m_ProfileId);
          newMessagesNotify->success = true;
          newMessagesNotify->chatId = chatId;
          newMessagesNotify->cached = false;
          newMessagesNotify->sequence = true;
          newMessagesNotify->chatMessages.push_back(chatMessage);
          m_MessageHandler(newMessagesNotify);
        }
      }
      break;

    case DeferNotifyRequestType:
      {
        std::shared_ptr<DeferNotifyRequest> deferNotifyRequest =
          std::static_pointer_cast<DeferNotifyRequest>(p_RequestMessage);
        m_MessageHandler(deferNotifyRequest->serviceMessage);
      }
      break;

    default:
      LOG_DEBUG("unknown request message %d", p_RequestMessage->GetMessageType());
      break;
  }
}
