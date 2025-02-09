/*
    Copyright (C) 2020 Sebastian J. Wolf and other contributors

    This file is part of Fernschreiber.

    Fernschreiber is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Fernschreiber is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Fernschreiber. If not, see <http://www.gnu.org/licenses/>.
*/

#include "chatlistmodel.h"
#include "fernschreiberutils.h"
#include <QListIterator>

#define DEBUG_MODULE ChatListModel
#include "debuglog.h"

namespace {
    const QString ID("id");
    const QString DATE("date");
    const QString TEXT("text");
    const QString TYPE("type");
    const QString TITLE("title");
    const QString PHOTO("photo");
    const QString SMALL("small");
    const QString ORDER("order");
    const QString CHAT_ID("chat_id");
    const QString CONTENT("content");
    const QString LAST_MESSAGE("last_message");
    const QString DRAFT_MESSAGE("draft_message");
    const QString SENDER_ID("sender_id");
    const QString USER_ID("user_id");
    const QString BASIC_GROUP_ID("basic_group_id");
    const QString SUPERGROUP_ID("supergroup_id");
    const QString UNREAD_COUNT("unread_count");
    const QString NOTIFICATION_SETTINGS("notification_settings");
    const QString LAST_READ_INBOX_MESSAGE_ID("last_read_inbox_message_id");
    const QString LAST_READ_OUTBOX_MESSAGE_ID("last_read_outbox_message_id");
    const QString SENDING_STATE("sending_state");
    const QString IS_CHANNEL("is_channel");
    const QString IS_VERIFIED("is_verified");
    const QString IS_MARKED_AS_UNREAD("is_marked_as_unread");
    const QString IS_PINNED("is_pinned");
    const QString PINNED_MESSAGE_ID("pinned_message_id");
    const QString _TYPE("@type");
    const QString SECRET_CHAT_ID("secret_chat_id");
}

class ChatListModel::ChatData
{
public:

    ChatData(TDLibWrapper *tdLibWrapper, const QVariantMap &data);

    int compareTo(const ChatData *chat) const;
    bool setOrder(const QString &order);
    const QVariant lastMessage(const QString &key) const;
    QString title() const;
    int unreadCount() const;
    QVariant photoSmall() const;
    qlonglong lastReadInboxMessageId() const;
    qlonglong senderUserId() const;
    qlonglong senderChatId() const;
    bool senderIsChat() const;
    qlonglong senderMessageDate() const;
    QString senderMessageText() const;
    QString senderMessageStatus() const;
    qlonglong draftMessageDate() const;
    QString draftMessageText() const;
    bool isChannel() const;
    bool isHidden() const;
    bool isMarkedAsUnread() const;
    bool isPinned() const;
    bool updateUnreadCount(int unreadCount);
    bool updateLastReadInboxMessageId(qlonglong messageId);
    QVector<int> updateLastMessage(const QVariantMap &message);
    QVector<int> updateGroup(const TDLibWrapper::Group *group);
    QVector<int> updateSecretChat(const QVariantMap &secretChatDetails);
    TDLibWrapper *tdLibWrapper;

public:
    QVariantMap chatData;
    qlonglong chatId;
    qlonglong order;
    qlonglong groupId;
    bool verified;
    TDLibWrapper::ChatType chatType;
    TDLibWrapper::ChatMemberStatus memberStatus;
    TDLibWrapper::SecretChatState secretChatState;

};

ChatListModel::ChatData::ChatData(TDLibWrapper *tdLibWrapper, const QVariantMap &data) :
    tdLibWrapper(tdLibWrapper),
    chatData(data),
    chatId(data.value(ID).toLongLong()),
    order(data.value(ORDER).toLongLong()),
    groupId(0),
    verified(false),
    memberStatus(TDLibWrapper::ChatMemberStatusUnknown),
    secretChatState(TDLibWrapper::SecretChatStateUnknown)
{
    const QVariantMap type(data.value(TYPE).toMap());
    switch (chatType = TDLibWrapper::chatTypeFromString(type.value(_TYPE).toString())) {
    case TDLibWrapper::ChatTypeBasicGroup:
        groupId = type.value(BASIC_GROUP_ID).toLongLong();
        break;
    case TDLibWrapper::ChatTypeSupergroup:
        groupId = type.value(SUPERGROUP_ID).toLongLong();
        break;
    case TDLibWrapper::ChatTypeUnknown:
    case TDLibWrapper::ChatTypePrivate:
    case TDLibWrapper::ChatTypeSecret:
        break;
    }
}

int ChatListModel::ChatData::compareTo(const ChatData *other) const
{
    if (order == other->order) {
        return (chatId < other->chatId) ? 1 : -1;
    } else {
        // This puts most recent ones to the top of the list
        return (order < other->order) ? 1 : -1;
    }
}

bool ChatListModel::ChatData::setOrder(const QString &newOrder)
{
    if (!newOrder.isEmpty()) {
        chatData.insert(ORDER, newOrder);
        order = newOrder.toLongLong();
        return true;
    }
    return false;
}

inline const QVariant ChatListModel::ChatData::lastMessage(const QString &key) const
{
    return chatData.value(LAST_MESSAGE).toMap().value(key);
}

QString ChatListModel::ChatData::title() const
{
    return chatData.value(TITLE).toString();
}

int ChatListModel::ChatData::unreadCount() const
{
    return chatData.value(UNREAD_COUNT).toInt();
}

QVariant ChatListModel::ChatData::photoSmall() const
{
    return chatData.value(PHOTO).toMap().value(SMALL);
}

qlonglong ChatListModel::ChatData::lastReadInboxMessageId() const
{
    return chatData.value(LAST_READ_INBOX_MESSAGE_ID).toLongLong();
}

qlonglong ChatListModel::ChatData::senderUserId() const
{
    return lastMessage(SENDER_ID).toMap().value(USER_ID).toLongLong();
}

qlonglong ChatListModel::ChatData::senderChatId() const
{
    return lastMessage(SENDER_ID).toMap().value(CHAT_ID).toLongLong();
}

bool ChatListModel::ChatData::senderIsChat() const
{
    return lastMessage(SENDER_ID).toMap().value(_TYPE).toString() == "messageSenderChat";
}

qlonglong ChatListModel::ChatData::senderMessageDate() const
{
    return lastMessage(DATE).toLongLong();
}

QString ChatListModel::ChatData::senderMessageText() const
{
    qlonglong myUserId = tdLibWrapper->getUserInformation().value(ID).toLongLong();
    return FernschreiberUtils::getMessageShortText(tdLibWrapper, lastMessage(CONTENT).toMap(), isChannel(), myUserId, lastMessage(SENDER_ID).toMap() );
}


QString ChatListModel::ChatData::senderMessageStatus() const
{
    qlonglong myUserId = tdLibWrapper->getUserInformation().value(ID).toLongLong();
    if (isChannel() || myUserId != senderUserId() || myUserId == chatId) {
        return "";
    }
    if (lastMessage(ID) == chatData.value(LAST_READ_OUTBOX_MESSAGE_ID)) {
        return "&nbsp;&nbsp;✅";
    } else {
        QVariantMap lastMessage = chatData.value(LAST_MESSAGE).toMap();
        if (lastMessage.contains(SENDING_STATE)) {
            QVariantMap sendingState = lastMessage.value(SENDING_STATE).toMap();
            if (sendingState.value(_TYPE).toString() == "messageSendingStatePending") {
                return "&nbsp;&nbsp;🕙";
            } else {
                return "&nbsp;&nbsp;❌";
            }
        } else {
            return "&nbsp;&nbsp;☑️";
        }
    }
}
qlonglong ChatListModel::ChatData::draftMessageDate() const
{
    QVariantMap draft = chatData.value(DRAFT_MESSAGE).toMap();
    if(draft.isEmpty()) {
        return qlonglong(0);
    }
    return draft.value(DATE).toLongLong();
}

QString ChatListModel::ChatData::draftMessageText() const
{
    QVariantMap draft = chatData.value(DRAFT_MESSAGE).toMap();
    if(draft.isEmpty()) {
        return QString();
    }
    return draft.value("input_message_text").toMap().value(TEXT).toMap().value(TEXT).toString();
}

bool ChatListModel::ChatData::isChannel() const
{
    return chatData.value(TYPE).toMap().value(IS_CHANNEL).toBool();
}

bool ChatListModel::ChatData::isHidden() const
{
    // Cover all enum values so that compiler warns us when/if enum gets extended
    switch (chatType) {
    case TDLibWrapper::ChatTypeBasicGroup:
    case TDLibWrapper::ChatTypeSupergroup:
        switch (memberStatus) {
        case TDLibWrapper::ChatMemberStatusLeft:
        case TDLibWrapper::ChatMemberStatusUnknown:
            return true;
        case TDLibWrapper::ChatMemberStatusCreator:
        case TDLibWrapper::ChatMemberStatusAdministrator:
        case TDLibWrapper::ChatMemberStatusMember:
        case TDLibWrapper::ChatMemberStatusRestricted:
        case TDLibWrapper::ChatMemberStatusBanned:
            break;
        }
        break;
    case TDLibWrapper::ChatTypeUnknown:
        break;
    case TDLibWrapper::ChatTypePrivate:
        if (chatData.value(LAST_MESSAGE).isNull()
                && chatData.value(LAST_READ_INBOX_MESSAGE_ID).toLongLong() == 0
                && chatData.value(LAST_READ_OUTBOX_MESSAGE_ID).toLongLong() == 0) {
            return true;
        }
        break;
    case TDLibWrapper::ChatTypeSecret:
        if (secretChatState == TDLibWrapper::SecretChatStateClosed) {
            return true;
        }
        break;
    }
    return false;
}

bool ChatListModel::ChatData::isMarkedAsUnread() const
{
    return chatData.value(IS_MARKED_AS_UNREAD).toBool();
}

bool ChatListModel::ChatData::isPinned() const
{
    return chatData.value(IS_PINNED).toBool();
}

bool ChatListModel::ChatData::updateUnreadCount(int count)
{
    const int prevUnreadCount(unreadCount());
    chatData.insert(UNREAD_COUNT, count);
    return prevUnreadCount != unreadCount();
}

bool ChatListModel::ChatData::updateLastReadInboxMessageId(qlonglong messageId)
{
    const qlonglong prevLastReadInboxMessageId(lastReadInboxMessageId());
    chatData.insert(LAST_READ_INBOX_MESSAGE_ID, messageId);
    return prevLastReadInboxMessageId != lastReadInboxMessageId();
}

QVector<int> ChatListModel::ChatData::updateLastMessage(const QVariantMap &message)
{
    const qlonglong prevSenderUserId(senderUserId());
    const qlonglong prevSenderMessageDate(senderMessageDate());
    const QString prevSenderMessageText(senderMessageText());
    const QString prevSenderMessageStatus(senderMessageStatus());


    chatData.insert(LAST_MESSAGE, message);

    QVector<int> changedRoles;
    changedRoles.append(RoleDisplay);
    if (prevSenderUserId != senderUserId()) {
        changedRoles.append(RoleLastMessageSenderId);
    }
    if (prevSenderMessageDate != senderMessageDate()) {
        changedRoles.append(RoleLastMessageDate);
    }
    if (prevSenderMessageText != senderMessageText()) {
        changedRoles.append(RoleFilter);
        changedRoles.append(RoleLastMessageText);
    }
    if (prevSenderMessageStatus != senderMessageStatus()) {
        changedRoles.append(RoleLastMessageStatus);
    }
    return changedRoles;
}

QVector<int> ChatListModel::ChatData::updateGroup(const TDLibWrapper::Group *group)
{
    QVector<int> changedRoles;

    if (group && groupId == group->groupId) {
        const TDLibWrapper::ChatMemberStatus groupMemberStatus = group->chatMemberStatus();
        if (memberStatus != groupMemberStatus) {
            memberStatus = groupMemberStatus;
            changedRoles.append(RoleChatMemberStatus);
        }
        // There's no "is_verified" in "basic_group" but that's ok since
        // it naturally becomes false
        const bool groupIsVerified = group->groupInfo.value(IS_VERIFIED).toBool();
        if (verified != groupIsVerified) {
            verified = groupIsVerified;
            changedRoles.append(RoleIsVerified);
        }
    }
    return changedRoles;
}

QVector<int> ChatListModel::ChatData::updateSecretChat(const QVariantMap &secretChatDetails)
{
    QVector<int> changedRoles;

    TDLibWrapper::SecretChatState newSecretChatState = TDLibWrapper::secretChatStateFromString(secretChatDetails.value("state").toMap().value(_TYPE).toString());
    if (newSecretChatState != secretChatState) {
        secretChatState = newSecretChatState;
        changedRoles.append(RoleSecretChatState);
    }
    return changedRoles;
}

ChatListModel::ChatListModel(TDLibWrapper *tdLibWrapper, AppSettings *appSettings) : showHiddenChats(false)
{
    this->tdLibWrapper = tdLibWrapper;
    this->appSettings = appSettings;
    connect(tdLibWrapper, SIGNAL(newChatDiscovered(QString, QVariantMap)), this, SLOT(handleChatDiscovered(QString, QVariantMap)));
    connect(tdLibWrapper, SIGNAL(chatLastMessageUpdated(QString, QString, QVariantMap)), this, SLOT(handleChatLastMessageUpdated(QString, QString, QVariantMap)));
    connect(tdLibWrapper, SIGNAL(chatOrderUpdated(QString, QString)), this, SLOT(handleChatOrderUpdated(QString, QString)));
    connect(tdLibWrapper, SIGNAL(chatReadInboxUpdated(QString, QString, int)), this, SLOT(handleChatReadInboxUpdated(QString, QString, int)));
    connect(tdLibWrapper, SIGNAL(chatReadOutboxUpdated(QString, QString)), this, SLOT(handleChatReadOutboxUpdated(QString, QString)));
    connect(tdLibWrapper, SIGNAL(chatPhotoUpdated(qlonglong, QVariantMap)), this, SLOT(handleChatPhotoUpdated(qlonglong, QVariantMap)));
    connect(tdLibWrapper, SIGNAL(chatPinnedMessageUpdated(qlonglong, qlonglong)), this, SLOT(handleChatPinnedMessageUpdated(qlonglong, qlonglong)));
    connect(tdLibWrapper, SIGNAL(messageSendSucceeded(qlonglong, qlonglong, QVariantMap)), this, SLOT(handleMessageSendSucceeded(qlonglong, qlonglong, QVariantMap)));
    connect(tdLibWrapper, SIGNAL(chatNotificationSettingsUpdated(QString, QVariantMap)), this, SLOT(handleChatNotificationSettingsUpdated(QString, QVariantMap)));
    connect(tdLibWrapper, SIGNAL(superGroupUpdated(qlonglong)), this, SLOT(handleGroupUpdated(qlonglong)));
    connect(tdLibWrapper, SIGNAL(basicGroupUpdated(qlonglong)), this, SLOT(handleGroupUpdated(qlonglong)));
    connect(tdLibWrapper, SIGNAL(secretChatUpdated(qlonglong, QVariantMap)), this, SLOT(handleSecretChatUpdated(qlonglong, QVariantMap)));
    connect(tdLibWrapper, SIGNAL(secretChatReceived(qlonglong, QVariantMap)), this, SLOT(handleSecretChatUpdated(qlonglong, QVariantMap)));
    connect(tdLibWrapper, SIGNAL(chatTitleUpdated(QString, QString)), this, SLOT(handleChatTitleUpdated(QString, QString)));
    connect(tdLibWrapper, SIGNAL(chatIsMarkedAsUnreadUpdated(qlonglong, bool)), this, SLOT(handleChatIsMarkedAsUnreadUpdated(qlonglong, bool)));
    connect(tdLibWrapper, SIGNAL(chatPinnedUpdated(qlonglong, bool)), this, SLOT(handleChatPinnedUpdated(qlonglong, bool)));
    connect(tdLibWrapper, SIGNAL(chatDraftMessageUpdated(qlonglong, QVariantMap, QString)), this, SLOT(handleChatDraftMessageUpdated(qlonglong, QVariantMap, QString)));

    // Don't start the timer until we have at least one chat
    relativeTimeRefreshTimer = new QTimer(this);
    relativeTimeRefreshTimer->setSingleShot(false);
    relativeTimeRefreshTimer->setInterval(30000);
    connect(relativeTimeRefreshTimer, SIGNAL(timeout()), SLOT(handleRelativeTimeRefreshTimer()));
    connect(this, SIGNAL(rowsInserted(QModelIndex,int,int)), SIGNAL(countChanged()));
    connect(this, SIGNAL(rowsRemoved(QModelIndex,int,int)), SIGNAL(countChanged()));
    connect(this, SIGNAL(modelReset()), SIGNAL(countChanged()));
}

ChatListModel::~ChatListModel()
{
    LOG("Destroying myself...");
    qDeleteAll(chatList);
    qDeleteAll(hiddenChats.values());
}

void ChatListModel::reset()
{
    chatList.clear();
    hiddenChats.clear();
}

QHash<int,QByteArray> ChatListModel::roleNames() const
{
    QHash<int,QByteArray> roles;
    roles.insert(ChatListModel::RoleDisplay, "display");
    roles.insert(ChatListModel::RoleChatId, "chat_id");
    roles.insert(ChatListModel::RoleChatType, "chat_type");
    roles.insert(ChatListModel::RoleGroupId, "group_id");
    roles.insert(ChatListModel::RoleTitle, "title");
    roles.insert(ChatListModel::RolePhotoSmall, "photo_small");
    roles.insert(ChatListModel::RoleUnreadCount, "unread_count");
    roles.insert(ChatListModel::RoleLastReadInboxMessageId, "last_read_inbox_message_id");
    roles.insert(ChatListModel::RoleLastMessageSenderId, "last_message_sender_id");
    roles.insert(ChatListModel::RoleLastMessageDate, "last_message_date");
    roles.insert(ChatListModel::RoleLastMessageText, "last_message_text");
    roles.insert(ChatListModel::RoleLastMessageStatus, "last_message_status");
    roles.insert(ChatListModel::RoleChatMemberStatus, "chat_member_status");
    roles.insert(ChatListModel::RoleSecretChatState, "secret_chat_state");
    roles.insert(ChatListModel::RoleIsVerified, "is_verified");
    roles.insert(ChatListModel::RoleIsChannel, "is_channel");
    roles.insert(ChatListModel::RoleIsMarkedAsUnread, "is_marked_as_unread");
    roles.insert(ChatListModel::RoleIsPinned, "is_pinned");
    roles.insert(ChatListModel::RoleFilter, "filter");
    roles.insert(ChatListModel::RoleDraftMessageDate, "draft_message_date");
    roles.insert(ChatListModel::RoleDraftMessageText, "draft_message_text");
    return roles;
}

int ChatListModel::rowCount(const QModelIndex &) const
{
    return chatList.size();
}

QVariant ChatListModel::data(const QModelIndex &index, int role) const
{
    const int row = index.row();
    if (row >= 0 && row < chatList.size()) {
        const ChatData *data = chatList.at(row);
        switch ((ChatListModel::Role)role) {
        case ChatListModel::RoleDisplay: return data->chatData;
        case ChatListModel::RoleChatId: return data->chatId;
        case ChatListModel::RoleChatType: return data->chatType;
        case ChatListModel::RoleGroupId: return data->groupId;
        case ChatListModel::RoleTitle: return data->title();
        case ChatListModel::RolePhotoSmall: return data->photoSmall();
        case ChatListModel::RoleUnreadCount: return data->unreadCount();
        case ChatListModel::RoleLastReadInboxMessageId: return data->lastReadInboxMessageId();
        case ChatListModel::RoleLastMessageSenderId: return data->senderUserId();
        case ChatListModel::RoleLastMessageText: return data->senderMessageText();
        case ChatListModel::RoleLastMessageDate: return data->senderMessageDate();
        case ChatListModel::RoleLastMessageStatus: return data->senderMessageStatus();
        case ChatListModel::RoleChatMemberStatus: return data->memberStatus;
        case ChatListModel::RoleSecretChatState: return data->secretChatState;
        case ChatListModel::RoleIsVerified: return data->verified;
        case ChatListModel::RoleIsChannel: return data->isChannel();
        case ChatListModel::RoleIsMarkedAsUnread: return data->isMarkedAsUnread();
        case ChatListModel::RoleIsPinned: return data->isPinned();
        case ChatListModel::RoleFilter: return data->title() + " " + data->senderMessageText();
        case ChatListModel::RoleDraftMessageText: return data->draftMessageText();
        case ChatListModel::RoleDraftMessageDate: return data->draftMessageDate();
        }
    }
    return QVariant();
}

void ChatListModel::redrawModel()
{
    LOG("Enforcing UI redraw...");
    layoutChanged();
}

QVariantMap ChatListModel::get(int row)
{

    QHash<int,QByteArray> names = roleNames();
    QHashIterator<int, QByteArray> i(names);
    QVariantMap res;
    QModelIndex idx = index(row, 0);
    while (i.hasNext()) {
        i.next();
        QVariant data = idx.data(i.key());
        res[i.value()] = data;
    }
    return res;
}

QVariantMap ChatListModel::getById(qlonglong chatId)
{
    if (chatIndexMap.contains(chatId)) {
        return chatList.value(chatIndexMap.value(chatId))->chatData;
    }
    return QVariantMap();
}

int ChatListModel::updateChatOrder(int chatIndex)
{
    ChatData *chat = chatList.at(chatIndex);

    const int n = chatList.size();
    int newIndex = chatIndex;
    while (newIndex > 0 && chat->compareTo(chatList.at(newIndex-1)) < 0) {
        newIndex--;
    }
    if (newIndex == chatIndex) {
        while (newIndex < n-1 && chat->compareTo(chatList.at(newIndex+1)) > 0) {
            newIndex++;
        }
    }
    if (newIndex != chatIndex) {
        LOG("Moving chat" << chat->chatId << "from position" << chatIndex << "to" << newIndex);
        beginMoveRows(QModelIndex(), chatIndex, chatIndex, QModelIndex(), (newIndex < chatIndex) ? newIndex : (newIndex+1));
        chatList.move(chatIndex, newIndex);
        chatIndexMap.insert(chat->chatId, newIndex);
        // Update damaged part of the map
        const int last = qMax(chatIndex, newIndex);
        if (newIndex < chatIndex) {
            // First index is already correct
            for (int i = newIndex + 1; i <= last; i++) {
                chatIndexMap.insert(chatList.at(i)->chatId, i);
            }
        } else {
            // Last index is already correct
            for (int i = chatIndex; i < last; i++) {
                chatIndexMap.insert(chatList.at(i)->chatId, i);
            }
        }
        endMoveRows();
    } else {
        LOG("Chat" << chat->chatId << "stays at position" << chatIndex);
    }

    return newIndex;
}

void ChatListModel::calculateUnreadState()
{
    if (this->appSettings->onlineOnlyMode()) {
        LOG("Online-only mode: Calculating unread state on my own...");
        int unreadMessages = 0;
        int unreadChats = 0;
        QListIterator<ChatData*> chatIterator(this->chatList);
        while (chatIterator.hasNext()) {
            ChatData *currentChat = chatIterator.next();
            int unreadCount = currentChat->unreadCount();
            if (unreadCount > 0) {
                unreadChats++;
                unreadMessages += unreadCount;
            }
        }
        LOG("Online-only mode: New unread state:" << unreadMessages << unreadChats);
        emit unreadStateChanged(unreadMessages, unreadChats);
    }
}

void ChatListModel::addVisibleChat(ChatData *chat)
{
    const int n = chatList.size();
    int pos;
    for (pos = 0; pos < n && chat->compareTo(chatList.at(pos)) >= 0; pos++);
    LOG("Adding chat" << chat->chatId << "at" << pos);
    beginInsertRows(QModelIndex(), pos, pos);
    chatList.insert(pos, chat);
    chatIndexMap.insert(chat->chatId, pos);
    // Update damaged part of the map
    for (int i = pos + 1; i <= n; i++) {
        chatIndexMap.insert(chatList.at(i)->chatId, i);
    }
    endInsertRows();
    if (this->tdLibWrapper->getJoinChatRequested()) {
        this->tdLibWrapper->registerJoinChat();
        emit chatJoined(chat->chatId, chat->chatData.value("title").toString());
    }
}

void ChatListModel::updateChatVisibility(const TDLibWrapper::Group *group)
{
    LOG("Updating chat visibility" << (group ? qPrintable(QString::number(group->groupId)) : ""));
    // See if any group has been removed from from view
    for (int i = 0; i < chatList.size(); i++) {
        ChatData *chat = chatList.at(i);
        const QVector<int> changedRoles(chat->updateGroup(group));
        if (chat->isHidden() && !showHiddenChats) {
            LOG("Hiding chat" << chat->chatId << "at" << i);
            beginRemoveRows(QModelIndex(), i, i);
            chatList.removeAt(i);
            // Update damaged part of the map
            const int n = chatList.size();
            for (int pos = i; pos < n; pos++) {
                chatIndexMap.insert(chatList.at(pos)->chatId, pos);
            }
            i--;
            hiddenChats.insert(chat->chatId, chat);
            endRemoveRows();
        } else if (!changedRoles.isEmpty()) {
            const QModelIndex modelIndex(index(i));
            emit dataChanged(modelIndex, modelIndex, changedRoles);
        }
    }

    // And see if any group been added to the view
    const QList<ChatData*> hiddenChatList = hiddenChats.values();
    const int n = hiddenChatList.size();
    for (int j = 0; j < n; j++) {
        ChatData *chat = hiddenChatList.at(j);
        chat->updateGroup(group);
        if (!chat->isHidden() || showHiddenChats) {
            hiddenChats.remove(chat->chatId);
            addVisibleChat(chat);
        }
    }
}

void ChatListModel::updateSecretChatVisibility(const QVariantMap secretChatDetails)
{
    LOG("Updating secret chat visibility" << secretChatDetails.value(ID).toString());
    // See if any secret chat has been closed
    for (int i = 0; i < chatList.size(); i++) {
        ChatData *chat = chatList.at(i);
        if (chat->chatType != TDLibWrapper::ChatTypeSecret) {
            continue;
        }
        if (chat->chatData.value(TYPE).toMap().value(SECRET_CHAT_ID).toLongLong() != secretChatDetails.value(ID).toLongLong()) {
            continue;
        }
        const QVector<int> changedRoles(chat->updateSecretChat(secretChatDetails));
        if (chat->isHidden() && !showHiddenChats) {
            LOG("Hiding chat" << chat->chatId << "at" << i);
            beginRemoveRows(QModelIndex(), i, i);
            chatList.removeAt(i);
            // Update damaged part of the map
            const int n = chatList.size();
            for (int pos = i; pos < n; pos++) {
                chatIndexMap.insert(chatList.at(pos)->chatId, pos);
            }
            i--;
            hiddenChats.insert(chat->chatId, chat);
            endRemoveRows();
        } else if (!changedRoles.isEmpty()) {
            const QModelIndex modelIndex(index(i));
            emit dataChanged(modelIndex, modelIndex, changedRoles);
        }
    }
}

bool ChatListModel::showAllChats() const
{
    return showHiddenChats;
}

void ChatListModel::setShowAllChats(bool showAll)
{
    if (showHiddenChats != showAll) {
        showHiddenChats = showAll;
        updateChatVisibility(Q_NULLPTR);
        emit showAllChatsChanged();
    }
}

void ChatListModel::handleChatDiscovered(const QString &, const QVariantMap &chatToBeAdded)
{
    ChatData *chat = new ChatData(tdLibWrapper, chatToBeAdded);

    const TDLibWrapper::Group *group = tdLibWrapper->getGroup(chat->groupId);
    if (group) {
        chat->updateGroup(group);
    }

    if (chat->chatType == TDLibWrapper::ChatTypeSecret) {
        QVariantMap secretChatDetails = tdLibWrapper->getSecretChatFromCache(chatToBeAdded.value(TYPE).toMap().value(SECRET_CHAT_ID).toLongLong());
        if (!secretChatDetails.isEmpty()) {
            chat->updateSecretChat(secretChatDetails);
        }
    }

    if (chat->isHidden() && !showHiddenChats) {
        LOG("Hidden chat" << chat->chatId);
        hiddenChats.insert(chat->chatId, chat);
    } else {
        addVisibleChat(chat);

        // Start timestamp refresh timer when the first visible chat is discovered
        if (!relativeTimeRefreshTimer->isActive()) {
            relativeTimeRefreshTimer->start();
        }
    }
}

void ChatListModel::handleChatLastMessageUpdated(const QString &id, const QString &order, const QVariantMap &lastMessage)
{
    bool ok;
    const qlonglong chatId = id.toLongLong(&ok);
    if (ok) {
        if (chatIndexMap.contains(chatId)) {
            int chatIndex = chatIndexMap.value(chatId);
            LOG("Updating last message for chat" << chatId <<" at index" << chatIndex << "new order" << order);
            ChatData *chat = chatList.at(chatIndex);
            if (chat->setOrder(order)) {
                chatIndex = updateChatOrder(chatIndex);
            }
            const QModelIndex modelIndex(index(chatIndex));
            emit dataChanged(modelIndex, modelIndex, chat->updateLastMessage(lastMessage));
            emit chatChanged(chatId);
        } else {
            ChatData *chat = hiddenChats.value(chatId);
            if (chat) {
                LOG("Updating last message for hidden chat" << chatId << "new order" << order);
                chat->setOrder(order);
                chat->chatData.insert(LAST_MESSAGE, lastMessage);
                // A chat can become visible (e.g. when a known contact joins Telegram)
                // When the private chat is discovered it doesn't have any messages, now it could be there...
                if (!chat->isHidden() || showHiddenChats) {
                    hiddenChats.remove(chatId);
                    addVisibleChat(chat);
                }
            }
        }
    }
}

void ChatListModel::handleChatOrderUpdated(const QString &id, const QString &order)
{
    bool ok;
    const qlonglong chatId = id.toLongLong(&ok);
    if (ok) {
        if (chatIndexMap.contains(chatId)) {
            LOG("Updating chat order of" << chatId << "to" << order);
            int chatIndex = chatIndexMap.value(chatId);
            if (chatList.at(chatIndex)->setOrder(order)) {
                updateChatOrder(chatIndex);
            }
        } else {
            ChatData *chat = hiddenChats.value(chatId);
            if (chat) {
                LOG("Updating order of hidden chat" << chatId << "to" << order);
                chat->setOrder(order);
            }
        }
    }
}

void ChatListModel::handleChatReadInboxUpdated(const QString &id, const QString &lastReadInboxMessageId, int unreadCount)
{
    bool ok;
    const qlonglong chatId = id.toLongLong(&ok);
    if (ok) {
        const qlonglong messageId = lastReadInboxMessageId.toLongLong();
        if (chatIndexMap.contains(chatId)) {
            LOG("Updating chat unread count for" << chatId << "unread messages" << unreadCount << ", last read message ID: " << lastReadInboxMessageId);
            const int chatIndex = chatIndexMap.value(chatId);
            ChatData *chat = chatList.at(chatIndex);
            QVector<int> changedRoles;
            changedRoles.append(ChatListModel::RoleDisplay);
            if (chat->updateUnreadCount(unreadCount)) {
                changedRoles.append(ChatListModel::RoleUnreadCount);
            }
            if (chat->updateLastReadInboxMessageId(messageId)) {
                changedRoles.append(ChatListModel::RoleLastReadInboxMessageId);
            }
            const QModelIndex modelIndex(index(chatIndex));
            emit dataChanged(modelIndex, modelIndex, changedRoles);
            this->calculateUnreadState();
        } else {
            ChatData *chat = hiddenChats.value(chatId);
            if (chat) {
                LOG("Updating unread count for hidden chat" << chatId << "unread messages" << unreadCount << ", last read message ID: " << lastReadInboxMessageId);
                chat->updateUnreadCount(unreadCount);
                chat->updateLastReadInboxMessageId(messageId);
            }
        }
    }
}

void ChatListModel::handleChatReadOutboxUpdated(const QString &id, const QString &lastReadOutboxMessageId)
{
    bool ok;
    const qlonglong chatId = id.toLongLong(&ok);
    if (ok) {
        if (chatIndexMap.contains(chatId)) {
            LOG("Updating last read message for" << chatId << "last ID" << lastReadOutboxMessageId);
            const int chatIndex = chatIndexMap.value(chatId);
            ChatData *chat = chatList.at(chatIndex);
            chat->chatData.insert(LAST_READ_OUTBOX_MESSAGE_ID, lastReadOutboxMessageId);
            const QModelIndex modelIndex(index(chatIndex));
            emit dataChanged(modelIndex, modelIndex);
        } else {
            ChatData *chat = hiddenChats.value(chatId);
            if (chat) {
                chat->chatData.insert(LAST_READ_OUTBOX_MESSAGE_ID, lastReadOutboxMessageId);
            }
        }
    }
}

void ChatListModel::handleChatPhotoUpdated(qlonglong chatId, const QVariantMap &photo)
{
    if (chatIndexMap.contains(chatId)) {
        LOG("Updating chat photo for" << chatId);
        const int chatIndex = chatIndexMap.value(chatId);
        ChatData *chat = chatList.at(chatIndex);
        chat->chatData.insert(PHOTO, photo);
        QVector<int> changedRoles;
        changedRoles.append(ChatListModel::RolePhotoSmall);
        const QModelIndex modelIndex(index(chatIndex));
        emit dataChanged(modelIndex, modelIndex, changedRoles);
    } else {
        ChatData *chat = hiddenChats.value(chatId);
        if (chat) {
            LOG("Updating photo for hidden chat" << chatId);
            chat->chatData.insert(PHOTO, photo);
        }
    }
}

void ChatListModel::handleChatPinnedMessageUpdated(qlonglong chatId, qlonglong pinnedMessageId)
{
    if (chatIndexMap.contains(chatId)) {
        LOG("Updating pinned message for" << chatId);
        const int chatIndex = chatIndexMap.value(chatId);
        ChatData *chat = chatList.at(chatIndex);
        chat->chatData.insert(PINNED_MESSAGE_ID, pinnedMessageId);
    } else {
        ChatData *chat = hiddenChats.value(chatId);
        if (chat) {
            LOG("Updating pinned message for hidden chat" << chatId);
            chat->chatData.insert(PINNED_MESSAGE_ID, pinnedMessageId);
        }
    }
}

void ChatListModel::handleMessageSendSucceeded(qlonglong messageId, qlonglong oldMessageId, const QVariantMap &message)
{
    bool ok;
    const qlonglong chatId(message.value(CHAT_ID).toLongLong(&ok));
    if (ok) {
        if (chatIndexMap.contains(chatId)) {
            const int chatIndex = chatIndexMap.value(chatId);
            LOG("Updating last message for chat" << chatId << "at index" << chatIndex << ", as message was sent, old ID:" << oldMessageId << ", new ID:" << messageId);
            const QModelIndex modelIndex(index(chatIndex));
            emit dataChanged(modelIndex, modelIndex, chatList.at(chatIndex)->updateLastMessage(message));
        } else {
            ChatData *chat = hiddenChats.value(chatId);
            if (chat) {
                LOG("Updating last message for hidden chat" << chatId << ", as message was sent, old ID:" << oldMessageId << ", new ID:" << messageId);
                chat->chatData.insert(LAST_MESSAGE, message);
            }
        }
    }
}

void ChatListModel::handleChatNotificationSettingsUpdated(const QString &id, const QVariantMap &chatNotificationSettings)
{
    bool ok;
    const qlonglong chatId = id.toLongLong(&ok);
    if (ok) {
        if (chatIndexMap.contains(chatId)) {
            const int chatIndex = chatIndexMap.value(chatId);
            LOG("Updating notification settings for chat" << chatId << "at index" << chatIndex);
            ChatData *chat = chatList.at(chatIndex);
            chat->chatData.insert(NOTIFICATION_SETTINGS, chatNotificationSettings);
            const QModelIndex modelIndex(index(chatIndex));
            emit dataChanged(modelIndex, modelIndex);
        } else {
            ChatData *chat = hiddenChats.value(chatId);
            if (chat) {
                chat->chatData.insert(NOTIFICATION_SETTINGS, chatNotificationSettings);
            }
        }
    }
}

void ChatListModel::handleGroupUpdated(qlonglong groupId)
{
    updateChatVisibility(tdLibWrapper->getGroup(groupId));
}

void ChatListModel::handleSecretChatUpdated(qlonglong secretChatId, const QVariantMap &secretChat)
{
    LOG("Updating visibility of secret chat " << secretChatId);
    updateSecretChatVisibility(secretChat);
}

void ChatListModel::handleChatTitleUpdated(const QString &chatId, const QString &title)
{
    qlonglong chatIdLongLong = chatId.toLongLong();
    if (chatIndexMap.contains(chatIdLongLong)) {
        LOG("Updating title for" << chatId);
        const int chatIndex = chatIndexMap.value(chatIdLongLong);
        ChatData *chat = chatList.at(chatIndex);
        chat->chatData.insert(TITLE, title);
        QVector<int> changedRoles;
        changedRoles.append(ChatListModel::RoleTitle);
        changedRoles.append(ChatListModel::RoleFilter);
        const QModelIndex modelIndex(index(chatIndex));
        emit dataChanged(modelIndex, modelIndex, changedRoles);
    } else {
        ChatData *chat = hiddenChats.value(chatId.toLongLong());
        if (chat) {
            LOG("Updating title for hidden chat" << chatId);
            chat->chatData.insert(TITLE, title);
        }
    }
}

void ChatListModel::handleChatPinnedUpdated(qlonglong chatId, bool chatIsPinned)
{
    if (chatIndexMap.contains(chatId)) {
        LOG("Updating chat is pinned for" << chatId << chatIsPinned);
        const int chatIndex = chatIndexMap.value(chatId);
        ChatData *chat = chatList.at(chatIndex);
        chat->chatData.insert(IS_PINNED, chatIsPinned);
        QVector<int> changedRoles;
        changedRoles.append(ChatListModel::RoleIsPinned);
        const QModelIndex modelIndex(index(chatIndex));
        emit dataChanged(modelIndex, modelIndex, changedRoles);
    } else {
        ChatData *chat = hiddenChats.value(chatId);
        if (chat) {
            LOG("Updating chat is pinned for hidden chat" << chatId);
            chat->chatData.insert(IS_PINNED, chatIsPinned);
        }
    }
}

void ChatListModel::handleChatIsMarkedAsUnreadUpdated(qlonglong chatId, bool chatIsMarkedAsUnread)
{
    if (chatIndexMap.contains(chatId)) {
        LOG("Updating chat is marked as unread for" << chatId << chatIsMarkedAsUnread);
        const int chatIndex = chatIndexMap.value(chatId);
        ChatData *chat = chatList.at(chatIndex);
        chat->chatData.insert(IS_MARKED_AS_UNREAD, chatIsMarkedAsUnread);
        QVector<int> changedRoles;
        changedRoles.append(ChatListModel::RoleIsMarkedAsUnread);
        const QModelIndex modelIndex(index(chatIndex));
        emit dataChanged(modelIndex, modelIndex, changedRoles);
    } else {
        ChatData *chat = hiddenChats.value(chatId);
        if (chat) {
            LOG("Updating chat is marked as unread for hidden chat" << chatId);
            chat->chatData.insert(IS_MARKED_AS_UNREAD, chatIsMarkedAsUnread);
        }
    }
}

void ChatListModel::handleChatDraftMessageUpdated(qlonglong chatId, const QVariantMap &draftMessage, const QString &order)
{
    LOG("Updating draft message for" << chatId);
    if (chatIndexMap.contains(chatId)) {
        const int chatIndex = chatIndexMap.value(chatId);
        ChatData *chat = chatList.at(chatIndex);
        chat->chatData.insert(DRAFT_MESSAGE, draftMessage);

        QVector<int> changedRoles;
        changedRoles.append(ChatListModel::RoleDraftMessageDate);
        changedRoles.append(ChatListModel::RoleDraftMessageText);
        const QModelIndex modelIndex(index(chatIndex));
        emit dataChanged(modelIndex, modelIndex, changedRoles);
        if (chat->setOrder(order)) {
            updateChatOrder(chatIndex);
        }
    }
}

void ChatListModel::handleRelativeTimeRefreshTimer()
{
    LOG("Refreshing timestamps");
    QVector<int> roles;
    roles.append(ChatListModel::RoleLastMessageDate);
    emit dataChanged(index(0), index(chatList.size() - 1), roles);
}
