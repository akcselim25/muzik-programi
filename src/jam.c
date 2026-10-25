#include "jam.h"
#include "friends.h"
#include "network.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    JamState state;
    char     jamId[64];
    wchar_t  partnerTag[32];
    wchar_t  partnerName[64];
    DWORD    inviteSentTick;

    wchar_t  syncStatus[128];
    BOOL     isDownloading;
    wchar_t  missingTrack[256];

    BOOL     hasPendingInvite;
    char     pendingJamId[64];
    wchar_t  pendingTag[32];
    wchar_t  pendingName[64];
    DWORD    pendingTick;

    JamRemoteSync pendingRemoteSync;
    BOOL          needSendCurrentState;
} JamSession;

static JamSession g_jam;

static void SanitizeFieldUtf8(char *s) {
    if (!s) return;
    for (int i = 0; s[i]; i++) {
        if (s[i] == '|' || s[i] == '\r' || s[i] == '\n') s[i] = '-';
    }
}

static void PublishToPartner(const wchar_t *partnerTag, const char *payload) {
    if (!partnerTag || !partnerTag[0] || !payload) return;
    char partnerKey[64] = {0};
    Network_TagToTopicKeyW(partnerTag, partnerKey, sizeof(partnerKey));
    char inbox[160];
    snprintf(inbox, sizeof(inbox), "seloplayer/u/%s", partnerKey);
    Network_Publish(inbox, payload);
}

void Jam_Init(void) {
    memset(&g_jam, 0, sizeof(g_jam));
}

JamState Jam_GetState(void) {
    Jam_CheckTimeouts();
    return g_jam.state;
}

BOOL Jam_IsActive(void) {
    return (Jam_GetState() == JAM_STATE_ACTIVE);
}

void Jam_CheckTimeouts(void) {
    DWORD now = GetTickCount();
    if (g_jam.state == JAM_STATE_INVITING) {
        if (now - g_jam.inviteSentTick > 35000) {
            g_jam.state = JAM_STATE_NONE;
            g_jam.jamId[0] = '\0';
            g_jam.syncStatus[0] = L'\0';
        }
    }
    if (g_jam.hasPendingInvite) {
        if (now - g_jam.pendingTick > 35000) {
            g_jam.hasPendingInvite = FALSE;
        }
    }
}

void Jam_GetPartnerName(wchar_t *buf, int maxLen) {
    if (buf && maxLen > 0) {
        wcsncpy(buf, g_jam.partnerName, maxLen - 1);
        buf[maxLen - 1] = L'\0';
    }
}

void Jam_GetPartnerTag(wchar_t *buf, int maxLen) {
    if (buf && maxLen > 0) {
        wcsncpy(buf, g_jam.partnerTag, maxLen - 1);
        buf[maxLen - 1] = L'\0';
    }
}

void Jam_SetSyncStatus(const wchar_t *statusText, BOOL isDownloading, const wchar_t *missingTrack) {
    if (statusText) {
        wcsncpy(g_jam.syncStatus, statusText, 127);
        g_jam.syncStatus[127] = L'\0';
    } else {
        g_jam.syncStatus[0] = L'\0';
    }
    g_jam.isDownloading = isDownloading;
    if (missingTrack && missingTrack[0]) {
        wcsncpy(g_jam.missingTrack, missingTrack, 255);
        g_jam.missingTrack[255] = L'\0';
    } else {
        g_jam.missingTrack[0] = L'\0';
    }
}

void Jam_GetSyncStatus(wchar_t *buf, int maxLen) {
    if (!buf || maxLen <= 0) return;
    if (g_jam.state == JAM_STATE_INVITING) {
        wcsncpy(buf, L"Davet gönderildi • Kabul etmesi bekleniyor...", maxLen - 1);
    } else if (g_jam.syncStatus[0]) {
        wcsncpy(buf, g_jam.syncStatus, maxLen - 1);
    } else {
        wcsncpy(buf, L"Canlı senkronizasyon aktif", maxLen - 1);
    }
    buf[maxLen - 1] = L'\0';
}

BOOL Jam_IsDownloadingTrack(void) {
    return g_jam.isDownloading;
}

void Jam_GetMissingTrack(wchar_t *buf, int maxLen) {
    if (buf && maxLen > 0) {
        wcsncpy(buf, g_jam.missingTrack, maxLen - 1);
        buf[maxLen - 1] = L'\0';
    }
}

BOOL Jam_HasPendingInvite(void) {
    Jam_CheckTimeouts();
    return g_jam.hasPendingInvite;
}

void Jam_GetPendingInviteInfo(wchar_t *nameBuf, wchar_t *tagBuf) {
    if (nameBuf) {
        wcsncpy(nameBuf, g_jam.pendingName, 63);
        nameBuf[63] = L'\0';
    }
    if (tagBuf) {
        wcsncpy(tagBuf, g_jam.pendingTag, 31);
        tagBuf[31] = L'\0';
    }
}

void Jam_InviteFriend(const wchar_t *friendTag, const wchar_t *friendName) {
    if (!friendTag || !friendTag[0]) return;

    if (g_jam.state != JAM_STATE_NONE) {
        Jam_Leave();
    }

    wchar_t myTag[32] = {0};
    wchar_t myName[64] = {0};
    Friends_GetMyTag(myTag, 32);
    Friends_GetMyName(myName, 64);

    char uMyTag[128] = {0};
    char uMyName[128] = {0};
    char myKey[64] = {0};
    WideCharToMultiByte(CP_UTF8, 0, myTag, -1, uMyTag, 127, NULL, NULL);
    WideCharToMultiByte(CP_UTF8, 0, myName, -1, uMyName, 127, NULL, NULL);
    Network_TagToTopicKeyW(myTag, myKey, sizeof(myKey));
    SanitizeFieldUtf8(uMyTag);
    SanitizeFieldUtf8(uMyName);

    char jamId[64];
    snprintf(jamId, sizeof(jamId), "jam_%s_%lu", myKey, (unsigned long)GetTickCount());

    g_jam.state = JAM_STATE_INVITING;
    g_jam.inviteSentTick = GetTickCount();
    strncpy(g_jam.jamId, jamId, sizeof(g_jam.jamId) - 1);
    wcsncpy(g_jam.partnerTag, friendTag, 31);
    wcsncpy(g_jam.partnerName, (friendName && friendName[0]) ? friendName : friendTag, 63);
    Jam_SetSyncStatus(L"Davet gönderildi • Kabul etmesi bekleniyor...", FALSE, NULL);

    char payload[320];
    snprintf(payload, sizeof(payload), "JAM_INV|%s|%s|%s", uMyTag, uMyName, jamId);
    PublishToPartner(friendTag, payload);
}

void Jam_AcceptInvite(void) {
    if (!g_jam.hasPendingInvite) return;

    if (g_jam.state != JAM_STATE_NONE) {
        Jam_Leave();
    }

    g_jam.hasPendingInvite = FALSE;
    g_jam.state = JAM_STATE_ACTIVE;
    strncpy(g_jam.jamId, g_jam.pendingJamId, sizeof(g_jam.jamId) - 1);
    wcsncpy(g_jam.partnerTag, g_jam.pendingTag, 31);
    wcsncpy(g_jam.partnerName, g_jam.pendingName[0] ? g_jam.pendingName : g_jam.pendingTag, 63);
    Jam_SetSyncStatus(L"Canlı bağlantı kuruldu • Birlikte dinlemeye hazır", FALSE, NULL);

    wchar_t myTag[32] = {0};
    wchar_t myName[64] = {0};
    Friends_GetMyTag(myTag, 32);
    Friends_GetMyName(myName, 64);
    char uMyTag[128] = {0};
    char uMyName[128] = {0};
    WideCharToMultiByte(CP_UTF8, 0, myTag, -1, uMyTag, 127, NULL, NULL);
    WideCharToMultiByte(CP_UTF8, 0, myName, -1, uMyName, 127, NULL, NULL);
    SanitizeFieldUtf8(uMyTag);
    SanitizeFieldUtf8(uMyName);

    char payload[320];
    snprintf(payload, sizeof(payload), "JAM_JOIN|%s|%s|%s", uMyTag, uMyName, g_jam.jamId);
    PublishToPartner(g_jam.partnerTag, payload);
}

void Jam_RejectInvite(void) {
    if (!g_jam.hasPendingInvite) return;

    wchar_t myTag[32] = {0};
    Friends_GetMyTag(myTag, 32);
    char uMyTag[128] = {0};
    WideCharToMultiByte(CP_UTF8, 0, myTag, -1, uMyTag, 127, NULL, NULL);
    SanitizeFieldUtf8(uMyTag);

    char payload[256];
    snprintf(payload, sizeof(payload), "JAM_REJ|%s|%s", uMyTag, g_jam.pendingJamId);
    PublishToPartner(g_jam.pendingTag, payload);

    g_jam.hasPendingInvite = FALSE;
}

void Jam_Leave(void) {
    if (g_jam.state == JAM_STATE_NONE) return;

    wchar_t myTag[32] = {0};
    Friends_GetMyTag(myTag, 32);
    char uMyTag[128] = {0};
    WideCharToMultiByte(CP_UTF8, 0, myTag, -1, uMyTag, 127, NULL, NULL);
    SanitizeFieldUtf8(uMyTag);

    char payload[256];
    snprintf(payload, sizeof(payload), "JAM_LEAVE|%s|%s", uMyTag, g_jam.jamId);
    PublishToPartner(g_jam.partnerTag, payload);

    g_jam.state = JAM_STATE_NONE;
    g_jam.jamId[0] = '\0';
    g_jam.partnerTag[0] = L'\0';
    g_jam.partnerName[0] = L'\0';
    g_jam.syncStatus[0] = L'\0';
    g_jam.isDownloading = FALSE;
    g_jam.missingTrack[0] = L'\0';
    g_jam.pendingRemoteSync.valid = FALSE;
    g_jam.needSendCurrentState = FALSE;
}

void Jam_BroadcastState(BOOL isPlaying, const wchar_t *trackTitle, const wchar_t *ytId, int posSec) {
    if (g_jam.state != JAM_STATE_ACTIVE || !g_jam.partnerTag[0]) return;

    wchar_t myTag[32] = {0};
    Friends_GetMyTag(myTag, 32);
    char uMyTag[128] = {0};
    WideCharToMultiByte(CP_UTF8, 0, myTag, -1, uMyTag, 127, NULL, NULL);
    SanitizeFieldUtf8(uMyTag);

    char uYtId[32] = {0};
    if (ytId && ytId[0]) {
        WideCharToMultiByte(CP_UTF8, 0, ytId, -1, uYtId, 31, NULL, NULL);
        SanitizeFieldUtf8(uYtId);
    }

    char uTrack[220] = {0};
    if (trackTitle && trackTitle[0]) {
        WideCharToMultiByte(CP_UTF8, 0, trackTitle, -1, uTrack, 219, NULL, NULL);
        SanitizeFieldUtf8(uTrack);
    }

    char payload[512];
    snprintf(payload, sizeof(payload), "JAM_STATE|%s|%s|%d|%d|%s|%s",
             uMyTag, g_jam.jamId, isPlaying ? 1 : 0, posSec < 0 ? 0 : posSec, uYtId, uTrack);
    PublishToPartner(g_jam.partnerTag, payload);
}

void Jam_BroadcastPause(int posSec) {
    if (g_jam.state != JAM_STATE_ACTIVE || !g_jam.partnerTag[0]) return;

    wchar_t myTag[32] = {0};
    Friends_GetMyTag(myTag, 32);
    char uMyTag[128] = {0};
    WideCharToMultiByte(CP_UTF8, 0, myTag, -1, uMyTag, 127, NULL, NULL);
    SanitizeFieldUtf8(uMyTag);

    char payload[256];
    snprintf(payload, sizeof(payload), "JAM_POS|%s|%s|0|%d", uMyTag, g_jam.jamId, posSec < 0 ? 0 : posSec);
    PublishToPartner(g_jam.partnerTag, payload);
}

void Jam_BroadcastSeek(int posSec) {
    if (g_jam.state != JAM_STATE_ACTIVE || !g_jam.partnerTag[0]) return;

    wchar_t myTag[32] = {0};
    Friends_GetMyTag(myTag, 32);
    char uMyTag[128] = {0};
    WideCharToMultiByte(CP_UTF8, 0, myTag, -1, uMyTag, 127, NULL, NULL);
    SanitizeFieldUtf8(uMyTag);

    char payload[256];
    snprintf(payload, sizeof(payload), "JAM_POS|%s|%s|-1|%d", uMyTag, g_jam.jamId, posSec < 0 ? 0 : posSec);
    PublishToPartner(g_jam.partnerTag, payload);
}

void Jam_RequestPartnerStateNow(void) {
    if (g_jam.state != JAM_STATE_ACTIVE || !g_jam.partnerTag[0]) return;

    wchar_t myTag[32] = {0};
    Friends_GetMyTag(myTag, 32);
    char uMyTag[128] = {0};
    WideCharToMultiByte(CP_UTF8, 0, myTag, -1, uMyTag, 127, NULL, NULL);
    SanitizeFieldUtf8(uMyTag);

    char payload[256];
    snprintf(payload, sizeof(payload), "JAM_ASK|%s|%s", uMyTag, g_jam.jamId);
    PublishToPartner(g_jam.partnerTag, payload);
}

BOOL Jam_PollRemoteSync(JamRemoteSync *outSync) {
    if (!g_jam.pendingRemoteSync.valid || !outSync) return FALSE;
    *outSync = g_jam.pendingRemoteSync;
    g_jam.pendingRemoteSync.valid = FALSE;
    return TRUE;
}

BOOL Jam_PollNeedSendCurrentState(void) {
    if (!g_jam.needSendCurrentState) return FALSE;
    g_jam.needSendCurrentState = FALSE;
    return TRUE;
}

static BOOL NextPipeToken(const char **cursor, char *out, int maxOut) {
    if (!cursor || !*cursor || !out || maxOut <= 0) return FALSE;
    const char *start = *cursor;
    const char *bar = strchr(start, '|');
    if (bar) {
        int len = (int)(bar - start);
        if (len >= maxOut) len = maxOut - 1;
        memcpy(out, start, len);
        out[len] = '\0';
        *cursor = bar + 1;
    } else {
        strncpy(out, start, maxOut - 1);
        out[maxOut - 1] = '\0';
        *cursor = NULL;
    }
    return TRUE;
}

void Jam_HandleNetworkMessage(const char *topic, const char *payload) {
    if (!topic || !payload) return;

    wchar_t myTag[32] = {0};
    Friends_GetMyTag(myTag, 32);
    char myKey[64] = {0};
    Network_TagToTopicKeyW(myTag, myKey, sizeof(myKey));
    char myInbox[160];
    snprintf(myInbox, sizeof(myInbox), "seloplayer/u/%s", myKey);

    if (_stricmp(topic, myInbox) != 0) return;

    // 1. JAM_INV|fromTag|fromName|jamId
    if (strncmp(payload, "JAM_INV|", 8) == 0) {
        const char *cur = payload + 8;
        char fTag[64] = {0}, fName[64] = {0}, jamId[64] = {0};
        if (NextPipeToken(&cur, fTag, sizeof(fTag)) &&
            NextPipeToken(&cur, fName, sizeof(fName)) &&
            cur != NULL) {
            strncpy(jamId, cur, sizeof(jamId) - 1);
            if (fTag[0] && jamId[0]) {
                g_jam.hasPendingInvite = TRUE;
                g_jam.pendingTick = GetTickCount();
                strncpy(g_jam.pendingJamId, jamId, 63);
                MultiByteToWideChar(CP_UTF8, 0, fTag, -1, g_jam.pendingTag, 31);
                MultiByteToWideChar(CP_UTF8, 0, fName, -1, g_jam.pendingName, 63);
            }
        }
    }
    // 2. JAM_JOIN|fromTag|fromName|jamId
    else if (strncmp(payload, "JAM_JOIN|", 9) == 0) {
        const char *cur = payload + 9;
        char fTag[64] = {0}, fName[64] = {0}, jamId[64] = {0};
        if (NextPipeToken(&cur, fTag, sizeof(fTag)) &&
            NextPipeToken(&cur, fName, sizeof(fName)) &&
            cur != NULL) {
            strncpy(jamId, cur, sizeof(jamId) - 1);
            if (g_jam.state != JAM_STATE_NONE && strcmp(g_jam.jamId, jamId) == 0) {
                g_jam.state = JAM_STATE_ACTIVE;
                if (fName[0]) MultiByteToWideChar(CP_UTF8, 0, fName, -1, g_jam.partnerName, 63);
                Jam_SetSyncStatus(L"Canlı senkronizasyon aktif", FALSE, NULL);
                g_jam.needSendCurrentState = TRUE;
            }
        }
    }
    // 3. JAM_REJ|fromTag|jamId
    else if (strncmp(payload, "JAM_REJ|", 8) == 0) {
        const char *cur = payload + 8;
        char fTag[64] = {0};
        if (NextPipeToken(&cur, fTag, sizeof(fTag)) && cur != NULL) {
            if (g_jam.state == JAM_STATE_INVITING && strcmp(g_jam.jamId, cur) == 0) {
                g_jam.state = JAM_STATE_NONE;
                g_jam.jamId[0] = '\0';
            }
        }
    }
    // 4. JAM_LEAVE|fromTag|jamId
    else if (strncmp(payload, "JAM_LEAVE|", 10) == 0) {
        const char *cur = payload + 10;
        char fTag[64] = {0};
        if (NextPipeToken(&cur, fTag, sizeof(fTag)) && cur != NULL) {
            if (g_jam.state != JAM_STATE_NONE && strcmp(g_jam.jamId, cur) == 0) {
                g_jam.state = JAM_STATE_NONE;
                g_jam.jamId[0] = '\0';
                g_jam.isDownloading = FALSE;
                g_jam.missingTrack[0] = L'\0';
            }
        }
    }
    // 5. JAM_ASK|fromTag|jamId
    else if (strncmp(payload, "JAM_ASK|", 8) == 0) {
        const char *cur = payload + 8;
        char fTag[64] = {0};
        if (NextPipeToken(&cur, fTag, sizeof(fTag)) && cur != NULL) {
            if (g_jam.state == JAM_STATE_ACTIVE && strcmp(g_jam.jamId, cur) == 0) {
                g_jam.needSendCurrentState = TRUE;
            }
        }
    }
    // 6. JAM_STATE|fromTag|jamId|isPlaying|posSec|ytId|trackTitle
    else if (strncmp(payload, "JAM_STATE|", 10) == 0) {
        const char *cur = payload + 10;
        char fTag[64] = {0}, jamId[64] = {0}, sPlay[16] = {0}, sPos[16] = {0}, sYt[32] = {0};
        if (NextPipeToken(&cur, fTag, sizeof(fTag)) &&
            NextPipeToken(&cur, jamId, sizeof(jamId)) &&
            NextPipeToken(&cur, sPlay, sizeof(sPlay)) &&
            NextPipeToken(&cur, sPos, sizeof(sPos)) &&
            NextPipeToken(&cur, sYt, sizeof(sYt))) {
            const char *uTrack = cur ? cur : "";
            if (g_jam.state == JAM_STATE_ACTIVE && strcmp(g_jam.jamId, jamId) == 0) {
                JamRemoteSync rs = {0};
                rs.valid = TRUE;
                rs.posOnly = FALSE;
                rs.isPlaying = (atoi(sPlay) != 0);
                rs.posSec = atoi(sPos);
                MultiByteToWideChar(CP_UTF8, 0, sYt, -1, rs.ytId, 15);
                MultiByteToWideChar(CP_UTF8, 0, uTrack, -1, rs.trackTitle, 255);
                g_jam.pendingRemoteSync = rs;
            }
        }
    }
    // 7. JAM_POS|fromTag|jamId|playFlag(-1=seekOnly,0=pause,1=play)|posSec
    else if (strncmp(payload, "JAM_POS|", 8) == 0) {
        const char *cur = payload + 8;
        char fTag[64] = {0}, jamId[64] = {0}, sFlag[16] = {0};
        if (NextPipeToken(&cur, fTag, sizeof(fTag)) &&
            NextPipeToken(&cur, jamId, sizeof(jamId)) &&
            NextPipeToken(&cur, sFlag, sizeof(sFlag)) &&
            cur != NULL) {
            if (g_jam.state == JAM_STATE_ACTIVE && strcmp(g_jam.jamId, jamId) == 0) {
                int flag = atoi(sFlag);
                JamRemoteSync rs = {0};
                rs.valid = TRUE;
                rs.posOnly = TRUE;
                rs.isPlaying = (flag == 1);
                rs.posSec = atoi(cur);
                rs.ytId[0] = (flag < 0) ? L'S' : L'\0'; // 'S' = yalnizca sarma (seek)
                g_jam.pendingRemoteSync = rs;
            }
        }
    }
}
