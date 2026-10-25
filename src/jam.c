#include "jam.h"
#include "friends.h"
#include "network.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WM_APP_JAM_SYNC (WM_APP + 401)

typedef struct {
    BOOL isActive;
    char jamId[64];
    wchar_t partnerTag[32];
    wchar_t partnerName[64];
    BOOL isHost;

    BOOL hasPendingInvite;
    char pendingJamId[64];
    wchar_t pendingTag[32];
    wchar_t pendingName[64];
    DWORD pendingTick;
} JamSession;

static JamSession g_jam = {0};
static HWND g_hMainWnd = NULL;

void Jam_Init(void) {
    memset(&g_jam, 0, sizeof(g_jam));
}

void Jam_SetHwnd(HWND hWnd) {
    g_hMainWnd = hWnd;
}

BOOL Jam_IsActive(void) {
    return g_jam.isActive;
}

void Jam_GetPartnerName(wchar_t *buf, int maxLen) {
    if (buf && maxLen > 0) wcsncpy(buf, g_jam.partnerName, maxLen - 1);
}

void Jam_GetPartnerTag(wchar_t *buf, int maxLen) {
    if (buf && maxLen > 0) wcsncpy(buf, g_jam.partnerTag, maxLen - 1);
}

BOOL Jam_HasPendingInvite(void) {
    if (!g_jam.hasPendingInvite) return FALSE;
    // Timeout invite after 30 seconds
    if (GetTickCount() - g_jam.pendingTick > 30000) {
        g_jam.hasPendingInvite = FALSE;
        return FALSE;
    }
    return TRUE;
}

void Jam_GetPendingInviteInfo(wchar_t *nameBuf, wchar_t *tagBuf) {
    if (nameBuf) wcsncpy(nameBuf, g_jam.pendingName, 63);
    if (tagBuf) wcsncpy(tagBuf, g_jam.pendingTag, 31);
}

void Jam_InviteFriend(const wchar_t *friendTag, const wchar_t *friendName) {
    if (!friendTag || !friendTag[0]) return;

    wchar_t myTag[32] = {0};
    wchar_t myName[64] = {0};
    Friends_GetMyTag(myTag, 32);
    Friends_GetMyName(myName, 64);

    char uMyTag[128] = {0};
    char uMyName[128] = {0};
    char uFriendTag[128] = {0};
    WideCharToMultiByte(CP_UTF8, 0, myTag, -1, uMyTag, 127, NULL, NULL);
    WideCharToMultiByte(CP_UTF8, 0, myName, -1, uMyName, 127, NULL, NULL);
    WideCharToMultiByte(CP_UTF8, 0, friendTag, -1, uFriendTag, 127, NULL, NULL);

    // Create unique Jam ID: jam_selim_12345
    char jamId[64];
    snprintf(jamId, sizeof(jamId), "jam_%s_%lu", uMyTag, (unsigned long)GetTickCount());
    // Replace '#' with '_'
    for (int i = 0; jamId[i]; i++) {
        if (jamId[i] == '#') jamId[i] = '_';
    }

    g_jam.isActive = TRUE;
    g_jam.isHost = TRUE;
    strncpy(g_jam.jamId, jamId, sizeof(g_jam.jamId) - 1);
    wcsncpy(g_jam.partnerTag, friendTag, 31);
    wcsncpy(g_jam.partnerName, friendName ? friendName : friendTag, 63);

    // Subscribe to Jam topic
    char jamTopic[128];
    snprintf(jamTopic, sizeof(jamTopic), "seloplayer/jam/%s", jamId);
    Network_Subscribe(jamTopic);

    // Send invite to friend's inbox
    char friendKey[64] = {0};
    Network_TagToTopicKeyW(friendTag, friendKey, sizeof(friendKey));
    char friendInbox[128];
    snprintf(friendInbox, sizeof(friendInbox), "seloplayer/u/%s", friendKey);

    char payload[256];
    snprintf(payload, sizeof(payload), "JAM_INV|%s|%s|%s", uMyTag, uMyName, jamId);
    Network_Publish(friendInbox, payload);
}

void Jam_AcceptInvite(void) {
    if (!g_jam.hasPendingInvite) return;

    g_jam.hasPendingInvite = FALSE;
    g_jam.isActive = TRUE;
    g_jam.isHost = FALSE;
    strncpy(g_jam.jamId, g_jam.pendingJamId, sizeof(g_jam.jamId) - 1);
    wcsncpy(g_jam.partnerTag, g_jam.pendingTag, 31);
    wcsncpy(g_jam.partnerName, g_jam.pendingName, 63);

    // Subscribe to Jam topic
    char jamTopic[128];
    snprintf(jamTopic, sizeof(jamTopic), "seloplayer/jam/%s", g_jam.jamId);
    Network_Subscribe(jamTopic);

    // Send join acknowledgement
    wchar_t myTag[32] = {0};
    wchar_t myName[64] = {0};
    Friends_GetMyTag(myTag, 32);
    Friends_GetMyName(myName, 64);
    char uMyTag[128] = {0};
    char uMyName[128] = {0};
    WideCharToMultiByte(CP_UTF8, 0, myTag, -1, uMyTag, 127, NULL, NULL);
    WideCharToMultiByte(CP_UTF8, 0, myName, -1, uMyName, 127, NULL, NULL);

    char payload[256];
    snprintf(payload, sizeof(payload), "JAM_JOIN|%s|%s", uMyTag, uMyName);
    Network_Publish(jamTopic, payload);
}

void Jam_RejectInvite(void) {
    g_jam.hasPendingInvite = FALSE;
}

void Jam_Leave(void) {
    if (!g_jam.isActive) return;

    wchar_t myTag[32] = {0};
    Friends_GetMyTag(myTag, 32);
    char uMyTag[128] = {0};
    WideCharToMultiByte(CP_UTF8, 0, myTag, -1, uMyTag, 127, NULL, NULL);

    char jamTopic[128];
    snprintf(jamTopic, sizeof(jamTopic), "seloplayer/jam/%s", g_jam.jamId);

    char payload[128];
    snprintf(payload, sizeof(payload), "SYNC_LEAVE|%s", uMyTag);
    Network_Publish(jamTopic, payload);

    Network_Unsubscribe(jamTopic);

    g_jam.isActive = FALSE;
    g_jam.jamId[0] = '\0';
    g_jam.partnerTag[0] = L'\0';
    g_jam.partnerName[0] = L'\0';
}

void Jam_BroadcastPlay(const wchar_t *trackTitle, int posSec) {
    if (!g_jam.isActive) return;

    wchar_t myTag[32] = {0};
    Friends_GetMyTag(myTag, 32);
    char uMyTag[128] = {0};
    WideCharToMultiByte(CP_UTF8, 0, myTag, -1, uMyTag, 127, NULL, NULL);

    char uTrack[128] = {0};
    if (trackTitle) {
        WideCharToMultiByte(CP_UTF8, 0, trackTitle, -1, uTrack, 127, NULL, NULL);
        for (int i = 0; uTrack[i]; i++) {
            if (uTrack[i] == '|') uTrack[i] = '-';
        }
    }

    char jamTopic[128];
    snprintf(jamTopic, sizeof(jamTopic), "seloplayer/jam/%s", g_jam.jamId);

    char payload[512];
    snprintf(payload, sizeof(payload), "SYNC_PLAY|%s|%s|%d", uMyTag, uTrack, posSec);
    Network_Publish(jamTopic, payload);
}

void Jam_BroadcastPause(int posSec) {
    if (!g_jam.isActive) return;

    wchar_t myTag[32] = {0};
    Friends_GetMyTag(myTag, 32);
    char uMyTag[128] = {0};
    WideCharToMultiByte(CP_UTF8, 0, myTag, -1, uMyTag, 127, NULL, NULL);

    char jamTopic[128];
    snprintf(jamTopic, sizeof(jamTopic), "seloplayer/jam/%s", g_jam.jamId);

    char payload[256];
    snprintf(payload, sizeof(payload), "SYNC_PAUSE|%s|%d", uMyTag, posSec);
    Network_Publish(jamTopic, payload);
}

void Jam_BroadcastSeek(int posSec) {
    if (!g_jam.isActive) return;

    wchar_t myTag[32] = {0};
    Friends_GetMyTag(myTag, 32);
    char uMyTag[128] = {0};
    WideCharToMultiByte(CP_UTF8, 0, myTag, -1, uMyTag, 127, NULL, NULL);

    char jamTopic[128];
    snprintf(jamTopic, sizeof(jamTopic), "seloplayer/jam/%s", g_jam.jamId);

    char payload[256];
    snprintf(payload, sizeof(payload), "SYNC_SEEK|%s|%d", uMyTag, posSec);
    Network_Publish(jamTopic, payload);
}

void Jam_BroadcastTrackChange(const wchar_t *trackTitle) {
    if (!g_jam.isActive) return;
    Jam_BroadcastPlay(trackTitle, 0);
}

void Jam_HandleNetworkMessage(const char *topic, const char *payload) {
    if (!topic || !payload) return;

    wchar_t myTag[32] = {0};
    Friends_GetMyTag(myTag, 32);
    char uMyTag[128] = {0};
    WideCharToMultiByte(CP_UTF8, 0, myTag, -1, uMyTag, 127, NULL, NULL);

    // 1. Check personal inbox for Jam invites
    char myKey[64] = {0};
    Network_TagToTopicKeyW(myTag, myKey, sizeof(myKey));
    char myInbox[128];
    snprintf(myInbox, sizeof(myInbox), "seloplayer/u/%s", myKey);

    if (strcmp(topic, myInbox) == 0) {
        // JAM_INV|fromTag|fromName|jamId
        if (strncmp(payload, "JAM_INV|", 8) == 0) {
            char fTag[64] = {0};
            char fName[64] = {0};
            char jamId[64] = {0};

            const char *p1 = payload + 8;
            const char *p2 = strchr(p1, '|');
            if (p2) {
                int tLen = (int)(p2 - p1);
                if (tLen > 63) tLen = 63;
                strncpy(fTag, p1, tLen);

                const char *p3 = strchr(p2 + 1, '|');
                if (p3) {
                    int nLen = (int)(p3 - (p2 + 1));
                    if (nLen > 63) nLen = 63;
                    strncpy(fName, p2 + 1, nLen);
                    strncpy(jamId, p3 + 1, 63);

                    g_jam.hasPendingInvite = TRUE;
                    g_jam.pendingTick = GetTickCount();
                    strncpy(g_jam.pendingJamId, jamId, 63);
                    MultiByteToWideChar(CP_UTF8, 0, fTag, -1, g_jam.pendingTag, 31);
                    MultiByteToWideChar(CP_UTF8, 0, fName, -1, g_jam.pendingName, 63);
                }
            }
        }
    }
    // 2. Check Jam channel messages
    else if (g_jam.isActive) {
        char jamTopic[128];
        snprintf(jamTopic, sizeof(jamTopic), "seloplayer/jam/%s", g_jam.jamId);

        if (strcmp(topic, jamTopic) == 0) {
            if (strncmp(payload, "JAM_JOIN|", 9) == 0) {
                // Partner connected
            }
            else if (strncmp(payload, "SYNC_LEAVE|", 11) == 0) {
                const char *sTag = payload + 11;
                if (strcmp(sTag, uMyTag) != 0) {
                    g_jam.isActive = FALSE;
                    g_jam.jamId[0] = '\0';
                }
            }
        }
    }
}
