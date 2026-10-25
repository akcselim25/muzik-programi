#ifndef JAM_H
#define JAM_H

#include <winsock2.h>
#include <windows.h>

void Jam_Init(void);
BOOL Jam_IsActive(void);
void Jam_GetPartnerName(wchar_t *buf, int maxLen);
void Jam_GetPartnerTag(wchar_t *buf, int maxLen);

BOOL Jam_HasPendingInvite(void);
void Jam_GetPendingInviteInfo(wchar_t *nameBuf, wchar_t *tagBuf);

void Jam_InviteFriend(const wchar_t *friendTag, const wchar_t *friendName);
void Jam_AcceptInvite(void);
void Jam_RejectInvite(void);
void Jam_Leave(void);

// Processes incoming messages on Jam topics
void Jam_HandleNetworkMessage(const char *topic, const char *payload);

// Broadcast playback changes to partner
void Jam_BroadcastPlay(const wchar_t *trackTitle, int posSec);
void Jam_BroadcastPause(int posSec);
void Jam_BroadcastSeek(int posSec);
void Jam_BroadcastTrackChange(const wchar_t *trackTitle);

#endif // JAM_H
