#include "friends.h"
#include "network.h"
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

static void GenerateRandomTag(void) {
    wchar_t user[64] = {0};
    DWORD uLen = 64;
    if (!GetUserNameW(user, &uLen) || !user[0]) {
        wcscpy(user, L"Kullanici");
    }
    // Remove spaces
    wchar_t clean[64] = {0};
    int cIdx = 0;
    for (int i = 0; user[i] && cIdx < 30; i++) {
        if (user[i] > 32 && user[i] != L'#' && user[i] != L'\"' && user[i] != L'|') {
            clean[cIdx++] = user[i];
        }
    }
    clean[cIdx] = L'\0';
    if (!clean[0]) wcscpy(clean, L"Kullanici");

    srand((unsigned int)time(NULL) ^ (unsigned int)GetTickCount());
    int num = 1000 + (rand() % 9000);
    wsprintfW(g_myTag, L"%s#%04d", clean, num);
    wcsncpy(g_myName, clean, 63);
}

void Friends_Init(void) {
    g_friendCount = 0;
    g_pendingCount = 0;
    memset(g_friends, 0, sizeof(g_friends));
    memset(g_pending, 0, sizeof(g_pending));

    FILE *f = _wfopen(FRIENDS_FILE, L"rb");
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
    fread(buf, 1, sz, f);
    buf[sz] = '\0';
    fclose(f);

    // Parse tag: "my_tag": "..."
    char *pTag = strstr(buf, "\"my_tag\"");
    if (pTag) {
        char *col = strchr(pTag, ':');
        if (col) {
            char *q1 = strchr(col, '\"');
            if (q1) {
                char *q2 = strchr(q1 + 1, '\"');
                if (q2) {
                    *q2 = '\0';
                    MultiByteToWideChar(CP_UTF8, 0, q1 + 1, -1, g_myTag, 31);
                    *q2 = '\"';
                }
            }
        }
    }

    // Parse name: "my_name": "..."
    char *pName = strstr(buf, "\"my_name\"");
    if (pName) {
        char *col = strchr(pName, ':');
        if (col) {
            char *q1 = strchr(col, '\"');
            if (q1) {
                char *q2 = strchr(q1 + 1, '\"');
                if (q2) {
                    *q2 = '\0';
                    MultiByteToWideChar(CP_UTF8, 0, q1 + 1, -1, g_myName, 63);
                    *q2 = '\"';
                }
            }
        }
    }

    if (!g_myTag[0]) {
        GenerateRandomTag();
    }
    if (!g_myName[0]) {
        wcsncpy(g_myName, g_myTag, 63);
    }

    // Parse friends array
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
                    g_friendCount++;
                }
                cur = objEnd + 1;
            }
        }
    }

    // Parse pending requests
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
    FILE *f = _wfopen(FRIENDS_FILE, L"wb");
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
    if (buf && maxLen > 0) wcsncpy(buf, g_myTag, maxLen - 1);
}

void Friends_GetMyName(wchar_t *buf, int maxLen) {
    if (buf && maxLen > 0) wcsncpy(buf, g_myName, maxLen - 1);
}

void Friends_SetMyName(const wchar_t *newName) {
    if (!newName || !newName[0]) return;
    wcsncpy(g_myName, newName, 63);
    Friends_Save();
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

BOOL Friends_SendRequest(const wchar_t *targetTag) {
    if (!targetTag || !targetTag[0]) return FALSE;
    if (_wcsicmp(targetTag, g_myTag) == 0) return FALSE; // Cannot add self

    // Check if already friend
    for (int i = 0; i < g_friendCount; i++) {
        if (_wcsicmp(g_friends[i].tag, targetTag) == 0) return FALSE;
    }

    char targetKey[64] = {0};
    Network_TagToTopicKeyW(targetTag, targetKey, sizeof(targetKey));

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

    // Add to friends
    if (g_friendCount < MAX_FRIENDS) {
        wcsncpy(g_friends[g_friendCount].tag, g_pending[foundIdx].tag, 31);
        wcsncpy(g_friends[g_friendCount].name, g_pending[foundIdx].name, 63);
        g_friends[g_friendCount].isOnline = TRUE;
        g_friends[g_friendCount].lastSeenTick = GetTickCount();
        g_friendCount++;

        // Subscribe to friend's status topic
        char friendKey[64] = {0};
        Network_TagToTopicKeyW(targetTag, friendKey, sizeof(friendKey));
        char sTopic[256];
        snprintf(sTopic, sizeof(sTopic), "seloplayer/status/%s", friendKey);
        Network_Subscribe(sTopic);
    }

    // Send ACC notification
    char targetKey[64] = {0};
    Network_TagToTopicKeyW(targetTag, targetKey, sizeof(targetKey));
    char uMyTag[128] = {0};
    char uMyName[128] = {0};
    WideCharToMultiByte(CP_UTF8, 0, g_myTag, -1, uMyTag, 127, NULL, NULL);
    WideCharToMultiByte(CP_UTF8, 0, g_myName, -1, uMyName, 127, NULL, NULL);

    char topic[256];
    snprintf(topic, sizeof(topic), "seloplayer/u/%s", targetKey);

    char payload[512];
    snprintf(payload, sizeof(payload), "ACC|%s|%s", uMyTag, uMyName);
    Network_Publish(topic, payload);

    // Remove from pending
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

    char friendKey[64] = {0};
    Network_TagToTopicKeyW(targetTag, friendKey, sizeof(friendKey));
    char sTopic[256];
    snprintf(sTopic, sizeof(sTopic), "seloplayer/status/%s", friendKey);
    Network_Unsubscribe(sTopic);

    for (int i = foundIdx; i < g_friendCount - 1; i++) {
        g_friends[i] = g_friends[i + 1];
    }
    g_friendCount--;

    Friends_Save();
    return TRUE;
}

void Friends_HandleNetworkMessage(const char *topic, const char *payload) {
    if (!topic || !payload) return;

    // Check if message is for my personal user inbox
    char myKey[64] = {0};
    Network_TagToTopicKeyW(g_myTag, myKey, sizeof(myKey));
    char myInbox[256];
    snprintf(myInbox, sizeof(myInbox), "seloplayer/u/%s", myKey);

    if (strcmp(topic, myInbox) == 0) {
        // REQ|fromTag|fromName
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

                // Check if already friends
                BOOL alreadyFriend = FALSE;
                for (int i = 0; i < g_friendCount; i++) {
                    if (_wcsicmp(g_friends[i].tag, wTag) == 0) {
                        alreadyFriend = TRUE;
                        break;
                    }
                }

                // Check if already in pending
                BOOL alreadyPending = FALSE;
                for (int i = 0; i < g_pendingCount; i++) {
                    if (_wcsicmp(g_pending[i].tag, wTag) == 0) {
                        alreadyPending = TRUE;
                        break;
                    }
                }

                if (!alreadyFriend && !alreadyPending && g_pendingCount < MAX_REQUESTS) {
                    wcsncpy(g_pending[g_pendingCount].tag, wTag, 31);
                    wcsncpy(g_pending[g_pendingCount].name, wName[0] ? wName : wTag, 63);
                    g_pendingCount++;
                    Friends_Save();
                }
            }
        }
        // ACC|fromTag|fromName
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

                BOOL found = FALSE;
                for (int i = 0; i < g_friendCount; i++) {
                    if (_wcsicmp(g_friends[i].tag, wTag) == 0) {
                        found = TRUE;
                        break;
                    }
                }
                if (!found && g_friendCount < MAX_FRIENDS) {
                    wcsncpy(g_friends[g_friendCount].tag, wTag, 31);
                    wcsncpy(g_friends[g_friendCount].name, wName[0] ? wName : wTag, 63);
                    g_friends[g_friendCount].isOnline = TRUE;
                    g_friends[g_friendCount].lastSeenTick = GetTickCount();
                    g_friendCount++;
                    Friends_Save();

                    char friendKey[64] = {0};
                    Network_TagToTopicKeyW(wTag, friendKey, sizeof(friendKey));
                    char sTopic[256];
                    snprintf(sTopic, sizeof(sTopic), "seloplayer/status/%s", friendKey);
                    Network_Subscribe(sTopic);
                }
            }
        }
    }
    // Check if status update on "seloplayer/status/..."
    else if (strncmp(topic, "seloplayer/status/", 18) == 0) {
        const char *tKey = topic + 18;

        for (int i = 0; i < g_friendCount; i++) {
            char fKey[64] = {0};
            Network_TagToTopicKeyW(g_friends[i].tag, fKey, sizeof(fKey));
            if (strcmp(fKey, tKey) == 0) {
                g_friends[i].isOnline = TRUE;
                g_friends[i].lastSeenTick = GetTickCount();

                // Payload format: STS|isPlaying|trackTitle|posSec
                if (strncmp(payload, "STS|", 4) == 0) {
                    const char *p1 = payload + 4;
                    const char *p2 = strchr(p1, '|');
                    if (p2) {
                        int isPlay = atoi(p1);
                        const char *p3 = strchr(p2 + 1, '|');
                        char track[128] = {0};
                        if (p3) {
                            int trkLen = (int)(p3 - (p2 + 1));
                            if (trkLen > 127) trkLen = 127;
                            strncpy(track, p2 + 1, trkLen);
                        } else {
                            strncpy(track, p2 + 1, 127);
                        }

                        if (track[0] && isPlay) {
                            MultiByteToWideChar(CP_UTF8, 0, track, -1, g_friends[i].currentTrack, 127);
                        } else {
                            wcscpy(g_friends[i].currentTrack, isPlay ? L"Müzik dinliyor" : L"Boşta");
                        }
                    }
                }
                break;
            }
        }
    }
}

void Friends_PublishMyStatus(BOOL isPlaying, const wchar_t *trackTitle, int posSec) {
    char myKey[64] = {0};
    Network_TagToTopicKeyW(g_myTag, myKey, sizeof(myKey));

    char uTrack[128] = {0};
    if (trackTitle && trackTitle[0]) {
        WideCharToMultiByte(CP_UTF8, 0, trackTitle, -1, uTrack, 127, NULL, NULL);
        // Replace pipe characters if any
        for (int i = 0; uTrack[i]; i++) {
            if (uTrack[i] == '|') uTrack[i] = '-';
        }
    }

    char topic[256];
    snprintf(topic, sizeof(topic), "seloplayer/status/%s", myKey);

    char payload[512];
    snprintf(payload, sizeof(payload), "STS|%d|%s|%d", isPlaying ? 1 : 0, uTrack, posSec);

    Network_Publish(topic, payload);
}

void Friends_CheckTimeouts(void) {
    DWORD now = GetTickCount();
    for (int i = 0; i < g_friendCount; i++) {
        if (g_friends[i].isOnline) {
            if (now - g_friends[i].lastSeenTick > 35000) { // 35s without update
                g_friends[i].isOnline = FALSE;
                wcscpy(g_friends[i].currentTrack, L"Çevrimdışı");
            }
        }
    }
}
