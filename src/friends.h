#ifndef FRIENDS_H
#define FRIENDS_H

#include <winsock2.h>
#include <windows.h>

#define MAX_FRIENDS 32
#define MAX_REQUESTS 16

typedef struct {
    wchar_t tag[32];
    wchar_t name[64];
    BOOL isOnline;
    wchar_t currentTrack[128];
    DWORD lastSeenTick;
} FriendItem;

typedef struct {
    wchar_t tag[32];
    wchar_t name[64];
} FriendRequest;

void Friends_Init(void);
void Friends_Save(void);

void Friends_GetMyTag(wchar_t *buf, int maxLen);
void Friends_GetMyName(wchar_t *buf, int maxLen);
void Friends_SetMyName(const wchar_t *newName);

int Friends_GetFriendCount(void);
const FriendItem* Friends_GetFriend(int index);

int Friends_GetPendingCount(void);
const FriendRequest* Friends_GetPending(int index);

BOOL Friends_SendRequest(const wchar_t *targetTag);
BOOL Friends_AcceptRequest(const wchar_t *targetTag);
BOOL Friends_RejectRequest(const wchar_t *targetTag);
BOOL Friends_Remove(const wchar_t *targetTag);

void Friends_HandleNetworkMessage(const char *topic, const char *payload);
void Friends_PublishMyStatus(BOOL isPlaying, const wchar_t *trackTitle, int posSec);
void Friends_CheckTimeouts(void);

#endif // FRIENDS_H
