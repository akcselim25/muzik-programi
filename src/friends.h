#ifndef FRIENDS_H
#define FRIENDS_H

#include <winsock2.h>
#include <windows.h>

#define MAX_FRIENDS  32
#define MAX_REQUESTS 16

typedef struct {
    wchar_t tag[32];
    wchar_t name[64];
    BOOL    isOnline;
    BOOL    isPlaying;
    wchar_t currentTrack[128];
    DWORD   lastSeenTick;
} FriendItem;

typedef struct {
    wchar_t tag[32];
    wchar_t name[64];
} FriendRequest;

void Friends_Init(void);
void Friends_Save(void);

void Friends_GetMyTag(wchar_t *buf, int maxLen);
void Friends_GetMyName(wchar_t *buf, int maxLen);

int  Friends_GetFriendCount(void);
const FriendItem* Friends_GetFriend(int index);

int  Friends_GetPendingCount(void);
const FriendRequest* Friends_GetPending(int index);

BOOL Friends_SendRequest(const wchar_t *targetTag);
BOOL Friends_AcceptRequest(const wchar_t *targetTag);
BOOL Friends_RejectRequest(const wchar_t *targetTag);
BOOL Friends_Remove(const wchar_t *targetTag);

// Yerel oynatma bilgisini bellekte tutar (ag paketi gondermez, sifir yuk!)
void Friends_SetLocalTrackState(BOOL isPlaying, const wchar_t *trackTitle);

// Yalnizca Arkadaslar paneli acildiginda veya Jam aktifken cagirilir
void Friends_RequestAllStatuses(void);
void Friends_NotifyOffline(void);
void Friends_CheckTimeouts(void);

void Friends_HandleNetworkMessage(const char *topic, const char *payload);

#endif // FRIENDS_H
