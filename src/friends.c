#include "friends.h"
#include "network.h"
#include <shlwapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define FRIENDS_FILE L"friends.json"

static wchar_t g_myTag[32] = {0};
static wchar_t g_myName[64] = {0};

static FriendItem g_friends[MAX_FRIENDS];
static int g_friendCount = 0;

static FriendRequest g_pending[MAX_REQUESTS];
static int g_pendingCount = 0;

static BOOL    g_localIsPlaying = FALSE;
static wchar_t g_localTrack[128] = {0};

static void GetFriendsFilePath(wchar_t *outPath, int maxLen) {
    GetModuleFileNameW(NULL, outPath, maxLen);
    PathRemoveFileSpecW(outPath);
    PathAppendW(outPath, FRIENDS_FILE);
}

static void SanitizePipeUtf8(char *s) {
    if (!s) return;
    for (int i = 0; s[i]; i++) {
        if (s[i] == '|' || s[i] == '\r' || s[i] == '\n') s[i] = '-';
    }
}

static void GenerateRandomTag(void) {
    wchar_t user[64] = {0};
    DWORD uLen = 64;
    if (!GetUserNameW(user, &uLen) || !user[0]) {
        wcscpy(user, L"Kullanici");
    }
    wchar_t clean[64] = {0};
    int cIdx = 0;
    for (int i = 0; user[i] && cIdx < 24; i++) {
        if (user[i] > 32 && user[i] != L'#' && user[i] != L'\"' && user[i] != L'|') {
            clean[cIdx++] = user[i];
        }
    }
    clean[cIdx] = L'\0';
    if (!clean[0]) wcscpy(clean, L"Kullanici");

    srand((unsigned int)time(NULL) ^ (unsigned int)GetTickCount());
    int num = 1000 + (rand() % 9000);
    _snwprintf(g_myTag, 31, L"%s#%04d", clean, num);
    g_myTag[31] = L'\0';
    wcsncpy(g_myName, clean, 63);
    g_myName[63] = L'\0';
}

void Friends_Init(void) {
    g_friendCount = 0;
    g_pendingCount = 0;
    memset(g_friends, 0, sizeof(g_friends));
    memset(g_pending, 0, sizeof(g_pending));

    wchar_t filePath[MAX_PATH];
    GetFriendsFilePath(filePath, MAX_PATH);

    FILE *f = _wfopen(filePath, L"rb");
    if (!f) f = _wfopen(FRIENDS_FILE, L"rb");
    if (!f) {
        GenerateRandomTag();
        Friends_Save();
        return;
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (sz <= 0 || sz > 65536) {
        fclose(f);
        GenerateRandomTag();
        Friends_Save();
        return;
    }

    char *buf = (char*)malloc(sz + 1);
    if (!buf) {
        fclose(f);
        GenerateRandomTag();
        return;
    }
    size_t got = fread(buf, 1, sz, f);
    buf[got] = '\0';
    fclose(f);

    char *pTag = strstr(buf, "\"my_tag\"");
    if (pTag) {
        char *col = strchr(pTag, ':');
        char *q1 = col ? strchr(col, '\"') : NULL;
        char *q2 = q1 ? strchr(q1 + 1, '\"') : NULL;
        if (q2) {
            *q2 = '\0';
            MultiByteToWideChar(CP_UTF8, 0, q1 + 1, -1, g_myTag, 31);
            *q2 = '\"';
        }
    }

    char *pName = strstr(buf, "\"my_name\"");
    if (pName) {
        char *col = strchr(pName, ':');
        char *q1 = col ? strchr(col, '\"') : NULL;
        char *q2 = q1 ? strchr(q1 + 1, '\"') : NULL;
        if (q2) {
            *q2 = '\0';
            MultiByteToWideChar(CP_UTF8, 0, q1 + 1, -1, g_myName, 63);
            *q2 = '\"';
        }
    }

    if (!g_myTag[0]) GenerateRandomTag();
    if (!g_myName[0]) wcsncpy(g_myName, g_myTag, 63);

    char *pFriends = strstr(buf, "\"friends\"");
    if (pFriends) {
        char *openB = strchr(pFriends, '[');
        char *closeB = openB ? strchr(openB, ']') : NULL;
        if (openB && closeB) {
            char *cur = openB;
            while (cur < closeB && g_friendCount < MAX_FRIENDS) {
                char *obj = strchr(cur, '{');
                if (!obj || obj > closeB) break;
                char *objEnd = strchr(obj, '}');
                if (!objEnd || objEnd > closeB) break;

                char saved = *objEnd;
                *objEnd = '\0';

                wchar_t fTag[32] = {0};
                wchar_t fName[64] = {0};

                char *kTag = strstr(obj, "\"tag\"");
                if (kTag) {
                    char *c = strchr(kTag, ':');
                    char *q1 = c ? strchr(c, '\"') : NULL;
                    char *q2 = q1 ? strchr(q1 + 1, '\"') : NULL;
                    if (q2) {
                        *q2 = '\0';
                        MultiByteToWideChar(CP_UTF8, 0, q1 + 1, -1, fTag, 31);
                        *q2 = '\"';
                    }
                }

                char *kName = strstr(obj, "\"name\"");
                if (kName) {
                    char *c = strchr(kName, ':');
                    char *q1 = c ? strchr(c, '\"') : NULL;
                    char *q2 = q1 ? strchr(q1 + 1, '\"') : NULL;
                    if (q2) {
                        *q2 = '\0';
                        MultiByteToWideChar(CP_UTF8, 0, q1 + 1, -1, fName, 63);
                        *q2 = '\"';
                    }
                }

                *objEnd = saved;
                if (fTag[0]) {
                    wcsncpy(g_friends[g_friendCount].tag, fTag, 31);
                    wcsncpy(g_friends[g_friendCount].name, fName[0] ? fName : fTag, 63);
                    g_friends[g_friendCount].isOnline = FALSE;
                    g_friends[g_friendCount].isPlaying = FALSE;
                    wcscpy(g_friends[g_friendCount].currentTrack, L"Çevrimdışı");
                    g_friendCount++;
                }
                cur = objEnd + 1;
            }
        }
    }

    char *pPending = strstr(buf, "\"pending\"");
    if (pPending) {
        char *openB = strchr(pPending, '[');
        char *closeB = openB ? strchr(openB, ']') : NULL;
        if (openB && closeB) {
            char *cur = openB;
            while (cur < closeB && g_pendingCount < MAX_REQUESTS) {
                char *obj = strchr(cur, '{');
                if (!obj || obj > closeB) break;
                char *objEnd = strchr(obj, '}');
                if (!objEnd || objEnd > closeB) break;

                char saved = *objEnd;
                *objEnd = '\0';

                wchar_t rTag[32] = {0};
                wchar_t rName[64] = {0};

                char *kTag = strstr(obj, "\"tag\"");
                if (kTag) {
                    char *c = strchr(kTag, ':');
                    char *q1 = c ? strchr(c, '\"') : NULL;
                    char *q2 = q1 ? strchr(q1 + 1, '\"') : NULL;
                    if (q2) {
                        *q2 = '\0';
                        MultiByteToWideChar(CP_UTF8, 0, q1 + 1, -1, rTag, 31);
                        *q2 = '\"';
                    }
                }

                char *kName = strstr(obj, "\"name\"");
                if (kName) {
                    char *c = strchr(kName, ':');
                    char *q1 = c ? strchr(c, '\"') : NULL;
                    char *q2 = q1 ? strchr(q1 + 1, '\"') : NULL;
                    if (q2) {
                        *q2 = '\0';
                        MultiByteToWideChar(CP_UTF8, 0, q1 + 1, -1, rName, 63);
                        *q2 = '\"';
                    }
                }

                *objEnd = saved;
                if (rTag[0]) {
                    wcsncpy(g_pending[g_pendingCount].tag, rTag, 31);
                    wcsncpy(g_pending[g_pendingCount].name, rName[0] ? rName : rTag, 63);
                    g_pendingCount++;
                }
                cur = objEnd + 1;
            }
        }
    }

    free(buf);
}

void Friends_Save(void) {
    wchar_t filePath[MAX_PATH];
    GetFriendsFilePath(filePath, MAX_PATH);

    FILE *f = _wfopen(filePath, L"wb");
    if (!f) f = _wfopen(FRIENDS_FILE, L"wb");
    if (!f) return;

    char uTag[128] = {0};
    char uName[128] = {0};
    WideCharToMultiByte(CP_UTF8, 0, g_myTag, -1, uTag, 127, NULL, NULL);
    WideCharToMultiByte(CP_UTF8, 0, g_myName, -1, uName, 127, NULL, NULL);

    fprintf(f, "{\n  \"my_tag\": \"%s\",\n  \"my_name\": \"%s\",\n  \"friends\": [\n", uTag, uName);
    for (int i = 0; i < g_friendCount; i++) {
        char fTag[128] = {0};
        char fName[128] = {0};
        WideCharToMultiByte(CP_UTF8, 0, g_friends[i].tag, -1, fTag, 127, NULL, NULL);
        WideCharToMultiByte(CP_UTF8, 0, g_friends[i].name, -1, fName, 127, NULL, NULL);
        fprintf(f, "    { \"tag\": \"%s\", \"name\": \"%s\" }%s\n", fTag, fName, (i == g_friendCount - 1) ? "" : ",");
    }
    fprintf(f, "  ],\n  \"pending\": [\n");
    for (int i = 0; i < g_pendingCount; i++) {
        char rTag[128] = {0};
        char rName[128] = {0};
        WideCharToMultiByte(CP_UTF8, 0, g_pending[i].tag, -1, rTag, 127, NULL, NULL);
        WideCharToMultiByte(CP_UTF8, 0, g_pending[i].name, -1, rName, 127, NULL, NULL);
        fprintf(f, "    { \"tag\": \"%s\", \"name\": \"%s\" }%s\n", rTag, rName, (i == g_pendingCount - 1) ? "" : ",");
    }
    fprintf(f, "  ]\n}\n");

    fclose(f);
}

void Friends_GetMyTag(wchar_t *buf, int maxLen) {
    if (buf && maxLen > 0) {
        wcsncpy(buf, g_myTag, maxLen - 1);
        buf[maxLen - 1] = L'\0';
    }
}

void Friends_GetMyName(wchar_t *buf, int maxLen) {
    if (buf && maxLen > 0) {
        wcsncpy(buf, g_myName, maxLen - 1);
        buf[maxLen - 1] = L'\0';
    }
}

int Friends_GetFriendCount(void) {
    return g_friendCount;
}

const FriendItem* Friends_GetFriend(int index) {
    if (index >= 0 && index < g_friendCount) return &g_friends[index];
    return NULL;
}

int Friends_GetPendingCount(void) {
    return g_pendingCount;
}

const FriendRequest* Friends_GetPending(int index) {
    if (index >= 0 && index < g_pendingCount) return &g_pending[index];
    return NULL;
}

void Friends_SetLocalTrackState(BOOL isPlaying, const wchar_t *trackTitle) {
    g_localIsPlaying = isPlaying;
    if (trackTitle && trackTitle[0]) {
        wcsncpy(g_localTrack, trackTitle, 127);
        g_localTrack[127] = L'\0';
    } else if (!isPlaying) {
        g_localTrack[0] = L'\0';
    }
}

static void SendPongToTag(const wchar_t *targetTag) {
    if (!targetTag || !targetTag[0]) return;

    char targetKey[64] = {0};
    Network_TagToTopicKeyW(targetTag, targetKey, sizeof(targetKey));

    char uMyTag[128] = {0};
    char uMyName[128] = {0};
    char uTrack[192] = {0};
    WideCharToMultiByte(CP_UTF8, 0, g_myTag, -1, uMyTag, 127, NULL, NULL);
    WideCharToMultiByte(CP_UTF8, 0, g_myName, -1, uMyName, 127, NULL, NULL);
    if (g_localIsPlaying && g_localTrack[0]) {
        WideCharToMultiByte(CP_UTF8, 0, g_localTrack, -1, uTrack, 191, NULL, NULL);
        SanitizePipeUtf8(uTrack);
    }

    char topic[256];
    snprintf(topic, sizeof(topic), "seloplayer/u/%s", targetKey);

    char payload[512];
    snprintf(payload, sizeof(payload), "PONG|%s|%s|%d|%s", uMyTag, uMyName, g_localIsPlaying ? 1 : 0, uTrack);
    Network_Publish(topic, payload);
}

static void SendPingToTag(const wchar_t *targetTag) {
    if (!targetTag || !targetTag[0]) return;

    char targetKey[64] = {0};
    Network_TagToTopicKeyW(targetTag, targetKey, sizeof(targetKey));

    char uMyTag[128] = {0};
    char uMyName[128] = {0};
    WideCharToMultiByte(CP_UTF8, 0, g_myTag, -1, uMyTag, 127, NULL, NULL);
    WideCharToMultiByte(CP_UTF8, 0, g_myName, -1, uMyName, 127, NULL, NULL);

    char topic[256];
    snprintf(topic, sizeof(topic), "seloplayer/u/%s", targetKey);

    char payload[256];
    snprintf(payload, sizeof(payload), "PING|%s|%s", uMyTag, uMyName);
    Network_Publish(topic, payload);
}

void Friends_RequestAllStatuses(void) {
    for (int i = 0; i < g_friendCount; i++) {
        SendPingToTag(g_friends[i].tag);
    }
}

void Friends_NotifyOffline(void) {
    if (g_friendCount <= 0) return;

    char uMyTag[128] = {0};
    WideCharToMultiByte(CP_UTF8, 0, g_myTag, -1, uMyTag, 127, NULL, NULL);

    char payload[160];
    snprintf(payload, sizeof(payload), "BYE|%s", uMyTag);

    for (int i = 0; i < g_friendCount; i++) {
        char targetKey[64] = {0};
        Network_TagToTopicKeyW(g_friends[i].tag, targetKey, sizeof(targetKey));
        char topic[256];
        snprintf(topic, sizeof(topic), "seloplayer/u/%s", targetKey);
        Network_Publish(topic, payload);
    }
}

BOOL Friends_SendRequest(const wchar_t *targetTag) {
    if (!targetTag || !targetTag[0]) return FALSE;

    wchar_t cleanTarget[32] = {0};
    wcsncpy(cleanTarget, targetTag, 31);
    StrTrimW(cleanTarget, L" \t\r\n");
    if (!cleanTarget[0] || !wcschr(cleanTarget, L'#')) return FALSE;

    if (_wcsicmp(cleanTarget, g_myTag) == 0) return FALSE;

    for (int i = 0; i < g_friendCount; i++) {
        if (_wcsicmp(g_friends[i].tag, cleanTarget) == 0) return FALSE;
    }

    char targetKey[64] = {0};
    Network_TagToTopicKeyW(cleanTarget, targetKey, sizeof(targetKey));

    char uMyTag[128] = {0};
    char uMyName[128] = {0};
    WideCharToMultiByte(CP_UTF8, 0, g_myTag, -1, uMyTag, 127, NULL, NULL);
    WideCharToMultiByte(CP_UTF8, 0, g_myName, -1, uMyName, 127, NULL, NULL);

    char topic[256];
    snprintf(topic, sizeof(topic), "seloplayer/u/%s", targetKey);

    char payload[512];
    snprintf(payload, sizeof(payload), "REQ|%s|%s", uMyTag, uMyName);

    return Network_Publish(topic, payload);
}

BOOL Friends_AcceptRequest(const wchar_t *targetTag) {
    int foundIdx = -1;
    for (int i = 0; i < g_pendingCount; i++) {
        if (_wcsicmp(g_pending[i].tag, targetTag) == 0) {
            foundIdx = i;
            break;
        }
    }
    if (foundIdx < 0) return FALSE;

    wchar_t acceptedTag[32] = {0};
    wcsncpy(acceptedTag, g_pending[foundIdx].tag, 31);

    if (g_friendCount < MAX_FRIENDS) {
        wcsncpy(g_friends[g_friendCount].tag, g_pending[foundIdx].tag, 31);
        wcsncpy(g_friends[g_friendCount].name, g_pending[foundIdx].name, 63);
        g_friends[g_friendCount].isOnline = TRUE;
        g_friends[g_friendCount].isPlaying = FALSE;
        wcscpy(g_friends[g_friendCount].currentTrack, L"Çevrimiçi");
        g_friends[g_friendCount].lastSeenTick = GetTickCount();
        g_friendCount++;
    }

    char targetKey[64] = {0};
    Network_TagToTopicKeyW(acceptedTag, targetKey, sizeof(targetKey));
    char uMyTag[128] = {0};
    char uMyName[128] = {0};
    WideCharToMultiByte(CP_UTF8, 0, g_myTag, -1, uMyTag, 127, NULL, NULL);
    WideCharToMultiByte(CP_UTF8, 0, g_myName, -1, uMyName, 127, NULL, NULL);

    char topic[256];
    snprintf(topic, sizeof(topic), "seloplayer/u/%s", targetKey);

    char payload[512];
    snprintf(payload, sizeof(payload), "ACC|%s|%s", uMyTag, uMyName);
    Network_Publish(topic, payload);

    // Anında karşılıklı durum senkronizasyonu!
    SendPongToTag(acceptedTag);
    SendPingToTag(acceptedTag);

    for (int i = foundIdx; i < g_pendingCount - 1; i++) {
        g_pending[i] = g_pending[i + 1];
    }
    g_pendingCount--;

    Friends_Save();
    return TRUE;
}

BOOL Friends_RejectRequest(const wchar_t *targetTag) {
    int foundIdx = -1;
    for (int i = 0; i < g_pendingCount; i++) {
        if (_wcsicmp(g_pending[i].tag, targetTag) == 0) {
            foundIdx = i;
            break;
        }
    }
    if (foundIdx < 0) return FALSE;

    for (int i = foundIdx; i < g_pendingCount - 1; i++) {
        g_pending[i] = g_pending[i + 1];
    }
    g_pendingCount--;

    Friends_Save();
    return TRUE;
}

BOOL Friends_Remove(const wchar_t *targetTag) {
    int foundIdx = -1;
    for (int i = 0; i < g_friendCount; i++) {
        if (_wcsicmp(g_friends[i].tag, targetTag) == 0) {
            foundIdx = i;
            break;
        }
    }
    if (foundIdx < 0) return FALSE;

    for (int i = foundIdx; i < g_friendCount - 1; i++) {
        g_friends[i] = g_friends[i + 1];
    }
    g_friendCount--;

    Friends_Save();
    return TRUE;
}

static int FindFriendIndexByTagW(const wchar_t *wTag) {
    for (int i = 0; i < g_friendCount; i++) {
        if (_wcsicmp(g_friends[i].tag, wTag) == 0) return i;
    }
    return -1;
}

void Friends_HandleNetworkMessage(const char *topic, const char *payload) {
    if (!topic || !payload) return;

    char myKey[64] = {0};
    Network_TagToTopicKeyW(g_myTag, myKey, sizeof(myKey));
    char myInbox[256];
    snprintf(myInbox, sizeof(myInbox), "seloplayer/u/%s", myKey);

    if (_stricmp(topic, myInbox) != 0) return;

    // 1. REQ|fromTag|fromName
    if (strncmp(payload, "REQ|", 4) == 0) {
        char tag[64] = {0};
        char name[64] = {0};
        const char *p1 = payload + 4;
        const char *p2 = strchr(p1, '|');
        if (p2) {
            int tLen = (int)(p2 - p1);
            if (tLen > 63) tLen = 63;
            strncpy(tag, p1, tLen);
            strncpy(name, p2 + 1, 63);

            wchar_t wTag[32] = {0};
            wchar_t wName[64] = {0};
            MultiByteToWideChar(CP_UTF8, 0, tag, -1, wTag, 31);
            MultiByteToWideChar(CP_UTF8, 0, name, -1, wName, 63);

            if (FindFriendIndexByTagW(wTag) >= 0) return;

            for (int i = 0; i < g_pendingCount; i++) {
                if (_wcsicmp(g_pending[i].tag, wTag) == 0) return;
            }

            if (g_pendingCount < MAX_REQUESTS && wTag[0]) {
                wcsncpy(g_pending[g_pendingCount].tag, wTag, 31);
                wcsncpy(g_pending[g_pendingCount].name, wName[0] ? wName : wTag, 63);
                g_pendingCount++;
                Friends_Save();
            }
        }
    }
    // 2. ACC|fromTag|fromName
    else if (strncmp(payload, "ACC|", 4) == 0) {
        char tag[64] = {0};
        char name[64] = {0};
        const char *p1 = payload + 4;
        const char *p2 = strchr(p1, '|');
        if (p2) {
            int tLen = (int)(p2 - p1);
            if (tLen > 63) tLen = 63;
            strncpy(tag, p1, tLen);
            strncpy(name, p2 + 1, 63);

            wchar_t wTag[32] = {0};
            wchar_t wName[64] = {0};
            MultiByteToWideChar(CP_UTF8, 0, tag, -1, wTag, 31);
            MultiByteToWideChar(CP_UTF8, 0, name, -1, wName, 63);

            int idx = FindFriendIndexByTagW(wTag);
            if (idx < 0 && g_friendCount < MAX_FRIENDS && wTag[0]) {
                idx = g_friendCount++;
                wcsncpy(g_friends[idx].tag, wTag, 31);
                wcsncpy(g_friends[idx].name, wName[0] ? wName : wTag, 63);
                Friends_Save();
            }
            if (idx >= 0) {
                g_friends[idx].isOnline = TRUE;
                g_friends[idx].lastSeenTick = GetTickCount();
                if (!g_friends[idx].currentTrack[0] || wcscmp(g_friends[idx].currentTrack, L"Çevrimdışı") == 0) {
                    wcscpy(g_friends[idx].currentTrack, L"Çevrimiçi");
                }
                SendPongToTag(wTag);
            }
        }
    }
    // 3. PING|fromTag|fromName -> Anında PONG ile yanıtla!
    else if (strncmp(payload, "PING|", 5) == 0) {
        char tag[64] = {0};
        char name[64] = {0};
        const char *p1 = payload + 5;
        const char *p2 = strchr(p1, '|');
        if (p2) {
            int tLen = (int)(p2 - p1);
            if (tLen > 63) tLen = 63;
            strncpy(tag, p1, tLen);
            strncpy(name, p2 + 1, 63);
        } else {
            strncpy(tag, p1, 63);
        }

        wchar_t wTag[32] = {0};
        wchar_t wName[64] = {0};
        MultiByteToWideChar(CP_UTF8, 0, tag, -1, wTag, 31);
        MultiByteToWideChar(CP_UTF8, 0, name, -1, wName, 63);

        int idx = FindFriendIndexByTagW(wTag);
        if (idx >= 0) {
            g_friends[idx].isOnline = TRUE;
            g_friends[idx].lastSeenTick = GetTickCount();
            if (wName[0] && wcscmp(g_friends[idx].name, wName) != 0) {
                wcsncpy(g_friends[idx].name, wName, 63);
            }
            SendPongToTag(g_friends[idx].tag);
        }
    }
    // 4. PONG|fromTag|fromName|isPlaying|trackTitle
    else if (strncmp(payload, "PONG|", 5) == 0) {
        const char *p1 = payload + 5;
        const char *p2 = strchr(p1, '|');
        if (!p2) return;

        char tag[64] = {0};
        int tLen = (int)(p2 - p1);
        if (tLen > 63) tLen = 63;
        strncpy(tag, p1, tLen);

        const char *p3 = strchr(p2 + 1, '|');
        if (!p3) return;
        char name[64] = {0};
        int nLen = (int)(p3 - (p2 + 1));
        if (nLen > 63) nLen = 63;
        strncpy(name, p2 + 1, nLen);

        const char *p4 = strchr(p3 + 1, '|');
        int isPlay = atoi(p3 + 1);
        const char *uTrack = p4 ? (p4 + 1) : "";

        wchar_t wTag[32] = {0};
        wchar_t wName[64] = {0};
        MultiByteToWideChar(CP_UTF8, 0, tag, -1, wTag, 31);
        MultiByteToWideChar(CP_UTF8, 0, name, -1, wName, 63);

        int idx = FindFriendIndexByTagW(wTag);
        if (idx >= 0) {
            // Karşı taraf büyük/küçük harf farklı yazdıysa gerçek etiketiyle düzelt
            if (wTag[0] && wcscmp(g_friends[idx].tag, wTag) != 0) {
                wcsncpy(g_friends[idx].tag, wTag, 31);
            }
            if (wName[0] && wcscmp(g_friends[idx].name, wName) != 0) {
                wcsncpy(g_friends[idx].name, wName, 63);
            }
            g_friends[idx].isOnline = TRUE;
            g_friends[idx].isPlaying = (isPlay != 0);
            g_friends[idx].lastSeenTick = GetTickCount();

            if (isPlay && uTrack[0]) {
                MultiByteToWideChar(CP_UTF8, 0, uTrack, -1, g_friends[idx].currentTrack, 127);
                g_friends[idx].currentTrack[127] = L'\0';
            } else {
                wcscpy(g_friends[idx].currentTrack, L"Çevrimiçi • Boşta");
            }
        }
    }
    // 5. BYE|fromTag -> Uygulamayı kapattığı anda anında Çevrimdışı göster
    else if (strncmp(payload, "BYE|", 4) == 0) {
        wchar_t wTag[32] = {0};
        MultiByteToWideChar(CP_UTF8, 0, payload + 4, -1, wTag, 31);
        int idx = FindFriendIndexByTagW(wTag);
        if (idx >= 0) {
            g_friends[idx].isOnline = FALSE;
            g_friends[idx].isPlaying = FALSE;
            wcscpy(g_friends[idx].currentTrack, L"Çevrimdışı");
        }
    }
}

void Friends_CheckTimeouts(void) {
    DWORD now = GetTickCount();
    for (int i = 0; i < g_friendCount; i++) {
        if (g_friends[i].isOnline) {
            if (now - g_friends[i].lastSeenTick > 11000) {
                g_friends[i].isOnline = FALSE;
                g_friends[i].isPlaying = FALSE;
                wcscpy(g_friends[i].currentTrack, L"Çevrimdışı");
            }
        }
    }
}
