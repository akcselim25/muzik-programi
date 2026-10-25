#ifndef JAM_H
#define JAM_H

#include <winsock2.h>
#include <windows.h>

typedef enum {
    JAM_STATE_NONE = 0,
    JAM_STATE_INVITING = 1,
    JAM_STATE_ACTIVE = 2
} JamState;

typedef struct {
    BOOL    valid;
    BOOL    isPlaying;
    BOOL    posOnly;      // TRUE ise yalnizca duraklatma/sarma guncellemesi (sarki degismedi)
    int     posSec;
    wchar_t ytId[16];
    wchar_t trackTitle[256];
} JamRemoteSync;

void     Jam_Init(void);
JamState Jam_GetState(void);
BOOL     Jam_IsActive(void);
void     Jam_CheckTimeouts(void);

void     Jam_GetPartnerName(wchar_t *buf, int maxLen);
void     Jam_GetPartnerTag(wchar_t *buf, int maxLen);

void     Jam_SetSyncStatus(const wchar_t *statusText, BOOL isDownloading, const wchar_t *missingTrack);
void     Jam_GetSyncStatus(wchar_t *buf, int maxLen);
BOOL     Jam_IsDownloadingTrack(void);
void     Jam_GetMissingTrack(wchar_t *buf, int maxLen);

BOOL     Jam_HasPendingInvite(void);
void     Jam_GetPendingInviteInfo(wchar_t *nameBuf, wchar_t *tagBuf);

void     Jam_InviteFriend(const wchar_t *friendTag, const wchar_t *friendName);
void     Jam_AcceptInvite(void);
void     Jam_RejectInvite(void);
void     Jam_Leave(void);

// Karsidan gelen senkronizasyon olaylarini tipli olarak okur
BOOL     Jam_PollRemoteSync(JamRemoteSync *outSync);
BOOL     Jam_PollNeedSendCurrentState(void);

// Ag mesajlarini isler
void     Jam_HandleNetworkMessage(const char *topic, const char *payload);

// Karsi tarafa anlik durum gonderir
void     Jam_BroadcastState(BOOL isPlaying, const wchar_t *trackTitle, const wchar_t *ytId, int posSec);
void     Jam_BroadcastPause(int posSec);
void     Jam_BroadcastSeek(int posSec);
void     Jam_RequestPartnerStateNow(void);

#endif // JAM_H
