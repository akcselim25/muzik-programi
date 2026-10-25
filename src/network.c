#include "network.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PRIMARY_BROKER   "broker.hivemq.com"
#define FALLBACK_BROKER  "broker.emqx.io"
#define BROKER_PORT      "1883"
#define MAX_TOPICS       16

static HWND g_hNotifyWnd = NULL;
static char g_clientTag[64] = {0};
static SOCKET g_socket = INVALID_SOCKET;
static HANDLE g_hThread = NULL;
static volatile BOOL g_running = FALSE;
static volatile BOOL g_connected = FALSE;
static CRITICAL_SECTION g_cs;

static char g_topics[MAX_TOPICS][128];
static int  g_topicCount = 0;

static int SendAll(SOCKET s, const char *buf, int len) {
    if (s == INVALID_SOCKET) return -1;
    int total = 0;
    while (total < len) {
        int sent = send(s, buf + total, len - total, 0);
        if (sent <= 0) return -1;
        total += sent;
    }
    return total;
}

static BOOL RawConnect(SOCKET s, const char *clientId) {
    int cidLen = (int)strlen(clientId);
    unsigned char pkt[128];
    int idx = 0;
    pkt[idx++] = 0x10; // CONNECT
    int remLen = 10 + 2 + cidLen;
    pkt[idx++] = (unsigned char)remLen;
    // Protocol Name (4) + Level (1) + Flags (1) + KeepAlive (2)
    pkt[idx++] = 0x00; pkt[idx++] = 0x04;
    pkt[idx++] = 'M';  pkt[idx++] = 'Q'; pkt[idx++] = 'T'; pkt[idx++] = 'T';
    pkt[idx++] = 0x04; // 3.1.1
    pkt[idx++] = 0x02; // Clean Session
    pkt[idx++] = 0x00; pkt[idx++] = 0x1E; // 30s KeepAlive
    // Client ID
    pkt[idx++] = (unsigned char)(cidLen >> 8);
    pkt[idx++] = (unsigned char)(cidLen & 0xFF);
    memcpy(&pkt[idx], clientId, cidLen);
    idx += cidLen;

    if (SendAll(s, (char*)pkt, idx) <= 0) return FALSE;

    unsigned char connack[4];
    int r = recv(s, (char*)connack, 4, 0);
    if (r == 4 && connack[0] == 0x20 && connack[3] == 0x00) {
        return TRUE;
    }
    return FALSE;
}

static BOOL RawSendSubscribe(SOCKET s, const char *topic) {
    int topLen = (int)strlen(topic);
    if (topLen <= 0 || topLen > 200) return FALSE;
    unsigned char pkt[256];
    int idx = 0;
    pkt[idx++] = 0x82; // SUBSCRIBE
    int remLen = 2 + 2 + topLen + 1;
    pkt[idx++] = (unsigned char)remLen;
    pkt[idx++] = 0x00; pkt[idx++] = 0x01; // Packet ID
    pkt[idx++] = (unsigned char)(topLen >> 8);
    pkt[idx++] = (unsigned char)(topLen & 0xFF);
    memcpy(&pkt[idx], topic, topLen);
    idx += topLen;
    pkt[idx++] = 0x00; // QoS 0

    return (SendAll(s, (char*)pkt, idx) > 0);
}

static BOOL RawPublish(SOCKET s, const char *topic, const char *payload) {
    int topLen = (int)strlen(topic);
    int payLen = (int)strlen(payload);
    int remLen = 2 + topLen + payLen;
    if (remLen > 4000) return FALSE;

    unsigned char pkt[4200];
    int idx = 0;
    pkt[idx++] = 0x30; // PUBLISH QoS 0

    // Encode remaining length (up to 2 bytes)
    if (remLen < 128) {
        pkt[idx++] = (unsigned char)remLen;
    } else {
        pkt[idx++] = (unsigned char)((remLen & 0x7F) | 0x80);
        pkt[idx++] = (unsigned char)(remLen >> 7);
    }

    pkt[idx++] = (unsigned char)(topLen >> 8);
    pkt[idx++] = (unsigned char)(topLen & 0xFF);
    memcpy(&pkt[idx], topic, topLen);
    idx += topLen;
    memcpy(&pkt[idx], payload, payLen);
    idx += payLen;

    return (SendAll(s, (char*)pkt, idx) > 0);
}

static void NotifyEvent(NetEventType type, const char *topic, const char *payload) {
    if (!g_hNotifyWnd) return;
    NetEvent *evt = (NetEvent*)calloc(1, sizeof(NetEvent));
    if (!evt) return;
    evt->type = type;
    if (topic) strncpy(evt->topic, topic, sizeof(evt->topic) - 1);
    if (payload) strncpy(evt->payload, payload, sizeof(evt->payload) - 1);

    if (!PostMessageW(g_hNotifyWnd, WM_APP_NETWORK_EVENT, (WPARAM)evt, 0)) {
        free(evt);
    }
}

static SOCKET ConnectToHost(const char *host, const char *port) {
    struct addrinfo hints = {0}, *res = NULL;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    if (getaddrinfo(host, port, &hints, &res) != 0) return INVALID_SOCKET;

    SOCKET s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (s == INVALID_SOCKET) {
        freeaddrinfo(res);
        return INVALID_SOCKET;
    }

    // Set 5s timeout on connect
    u_long mode = 1;
    ioctlsocket(s, FIONBIO, &mode);
    connect(s, res->ai_addr, (int)res->ai_addrlen);
    freeaddrinfo(res);

    fd_set writeSet;
    FD_ZERO(&writeSet);
    FD_SET(s, &writeSet);
    struct timeval tv = { 5, 0 };
    if (select(0, NULL, &writeSet, NULL, &tv) <= 0) {
        closesocket(s);
        return INVALID_SOCKET;
    }

    // Reset to blocking mode
    mode = 0;
    ioctlsocket(s, FIONBIO, &mode);

    return s;
}

static DWORD WINAPI NetworkThreadProc(LPVOID lpParam) {
    (void)lpParam;
    char clientCid[128];
    snprintf(clientCid, sizeof(clientCid), "selo_%s_%lu", g_clientTag, (unsigned long)GetTickCount());
    for (int i = 0; clientCid[i]; i++) {
        if (clientCid[i] == '#' || clientCid[i] == '+') clientCid[i] = '_';
    }

    while (g_running) {
        SOCKET s = ConnectToHost(PRIMARY_BROKER, BROKER_PORT);
        if (s == INVALID_SOCKET) {
            s = ConnectToHost(FALLBACK_BROKER, BROKER_PORT);
        }

        if (s == INVALID_SOCKET) {
            Sleep(3000);
            continue;
        }

        if (!RawConnect(s, clientCid)) {
            closesocket(s);
            Sleep(3000);
            continue;
        }

        EnterCriticalSection(&g_cs);
        g_socket = s;
        g_connected = TRUE;

        // Auto resubscribe to all active topics
        for (int i = 0; i < g_topicCount; i++) {
            RawSendSubscribe(s, g_topics[i]);
        }
        LeaveCriticalSection(&g_cs);

        NotifyEvent(NET_EVENT_CONNECTED, NULL, NULL);

        DWORD lastPing = GetTickCount();

        // Loop reading incoming packets
        while (g_running) {
            fd_set readSet;
            FD_ZERO(&readSet);
            FD_SET(s, &readSet);
            struct timeval tv = { 0, 500000 }; // 500ms

            int sel = select(0, &readSet, NULL, NULL, &tv);
            if (sel == SOCKET_ERROR) break;

            DWORD now = GetTickCount();
            if (now - lastPing >= 20000) {
                // Send PINGREQ
                EnterCriticalSection(&g_cs);
                const char ping[2] = { (char)0xC0, 0x00 };
                int sent = SendAll(s, ping, 2);
                LeaveCriticalSection(&g_cs);
                if (sent <= 0) break;
                lastPing = now;
            }

            if (sel > 0 && FD_ISSET(s, &readSet)) {
                unsigned char header = 0;
                int r = recv(s, (char*)&header, 1, 0);
                if (r <= 0) break;

                // Read remaining length
                int remLen = 0;
                int multiplier = 1;
                unsigned char digit = 0;
                do {
                    if (recv(s, (char*)&digit, 1, 0) <= 0) break;
                    remLen += (digit & 127) * multiplier;
                    multiplier *= 128;
                } while ((digit & 128) != 0);

                if ((header & 0xF0) == 0x30) { // PUBLISH
                    if (remLen >= 2 && remLen < 4000) {
                        unsigned char *buf = (unsigned char*)malloc(remLen + 1);
                        if (buf) {
                            int readTotal = 0;
                            while (readTotal < remLen) {
                                int got = recv(s, (char*)buf + readTotal, remLen - readTotal, 0);
                                if (got <= 0) break;
                                readTotal += got;
                            }

                            if (readTotal == remLen) {
                                int tLen = (buf[0] << 8) | buf[1];
                                if (tLen < remLen && tLen < 127) {
                                    char topic[128] = {0};
                                    memcpy(topic, &buf[2], tLen);
                                    topic[tLen] = '\0';

                                    int pLen = remLen - 2 - tLen;
                                    char payload[512] = {0};
                                    if (pLen > 0) {
                                        if (pLen > 511) pLen = 511;
                                        memcpy(payload, &buf[2 + tLen], pLen);
                                        payload[pLen] = '\0';
                                    }

                                    NotifyEvent(NET_EVENT_MESSAGE, topic, payload);
                                }
                            }
                            free(buf);
                        }
                    }
                } else if (remLen > 0) {
                    // Drain payload of non-PUBLISH packets (e.g. SUBACK 0x90) so TCP framing stays aligned
                    char drain[64];
                    int left = remLen;
                    while (left > 0) {
                        int chunk = (left > (int)sizeof(drain)) ? (int)sizeof(drain) : left;
                        int got = recv(s, drain, chunk, 0);
                        if (got <= 0) { left = -1; break; }
                        left -= got;
                    }
                    if (left < 0) break;
                }
            }
        }

        EnterCriticalSection(&g_cs);
        g_connected = FALSE;
        closesocket(g_socket);
        g_socket = INVALID_SOCKET;
        LeaveCriticalSection(&g_cs);

        NotifyEvent(NET_EVENT_DISCONNECTED, NULL, NULL);

        if (g_running) {
            Sleep(2000); // Wait 2 seconds before auto-reconnect
        }
    }

    return 0;
}

BOOL Network_Init(HWND hWnd, const char *clientTag) {
    if (g_running) return TRUE;

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return FALSE;

    InitializeCriticalSection(&g_cs);

    g_hNotifyWnd = hWnd;
    strncpy(g_clientTag, clientTag, sizeof(g_clientTag) - 1);
    g_running = TRUE;
    g_connected = FALSE;
    g_socket = INVALID_SOCKET;
    g_topicCount = 0;

    g_hThread = CreateThread(NULL, 0, NetworkThreadProc, NULL, 0, NULL);
    return (g_hThread != NULL);
}

void Network_TagToTopicKeyA(const char *tag, char *out, int maxLen) {
    if (!tag || !out || maxLen <= 0) return;
    int i = 0;
    for (; tag[i] && i < maxLen - 1; i++) {
        char c = tag[i];
        if (c == '#' || c == '+' || c == '/' || c == '$' || (unsigned char)c <= 32) {
            out[i] = '_';
        } else {
            out[i] = c;
        }
    }
    out[i] = '\0';
}

void Network_TagToTopicKeyW(const wchar_t *tag, char *out, int maxLen) {
    if (!tag || !out || maxLen <= 0) return;
    char temp[128] = {0};
    WideCharToMultiByte(CP_UTF8, 0, tag, -1, temp, sizeof(temp) - 1, NULL, NULL);
    Network_TagToTopicKeyA(temp, out, maxLen);
}

static void SanitizeTopic(const char *in, char *out, int maxLen) {
    if (!in || !out || maxLen <= 0) return;
    int i = 0;
    for (; in[i] && i < maxLen - 1; i++) {
        char c = in[i];
        if (c == '#' || c == '+') {
            out[i] = '_';
        } else {
            out[i] = c;
        }
    }
    out[i] = '\0';
}

BOOL Network_Subscribe(const char *topic) {
    if (!topic || !topic[0]) return FALSE;

    char safeTopic[128] = {0};
    SanitizeTopic(topic, safeTopic, sizeof(safeTopic));

    EnterCriticalSection(&g_cs);
    // Add to list if not present
    BOOL found = FALSE;
    for (int i = 0; i < g_topicCount; i++) {
        if (strcmp(g_topics[i], safeTopic) == 0) {
            found = TRUE;
            break;
        }
    }
    if (!found && g_topicCount < MAX_TOPICS) {
        strncpy(g_topics[g_topicCount++], safeTopic, 127);
    }

    BOOL ok = FALSE;
    if (g_connected && g_socket != INVALID_SOCKET) {
        ok = RawSendSubscribe(g_socket, safeTopic);
    }
    LeaveCriticalSection(&g_cs);

    return ok;
}

BOOL Network_Unsubscribe(const char *topic) {
    if (!topic || !topic[0]) return FALSE;

    char safeTopic[128] = {0};
    SanitizeTopic(topic, safeTopic, sizeof(safeTopic));

    EnterCriticalSection(&g_cs);
    for (int i = 0; i < g_topicCount; i++) {
        if (strcmp(g_topics[i], safeTopic) == 0) {
            for (int j = i; j < g_topicCount - 1; j++) {
                strcpy(g_topics[j], g_topics[j + 1]);
            }
            g_topicCount--;
            break;
        }
    }
    LeaveCriticalSection(&g_cs);
    return TRUE;
}

BOOL Network_Publish(const char *topic, const char *payload) {
    if (!topic || !payload) return FALSE;

    char safeTopic[128] = {0};
    SanitizeTopic(topic, safeTopic, sizeof(safeTopic));

    EnterCriticalSection(&g_cs);
    BOOL ok = FALSE;
    if (g_connected && g_socket != INVALID_SOCKET) {
        ok = RawPublish(g_socket, safeTopic, payload);
    }
    LeaveCriticalSection(&g_cs);

    return ok;
}

BOOL Network_IsConnected(void) {
    return g_connected;
}

void Network_Close(void) {
    g_running = FALSE;
    EnterCriticalSection(&g_cs);
    if (g_socket != INVALID_SOCKET) {
        closesocket(g_socket);
        g_socket = INVALID_SOCKET;
    }
    LeaveCriticalSection(&g_cs);

    if (g_hThread) {
        WaitForSingleObject(g_hThread, 1500);
        CloseHandle(g_hThread);
        g_hThread = NULL;
    }

    DeleteCriticalSection(&g_cs);
    WSACleanup();
}
