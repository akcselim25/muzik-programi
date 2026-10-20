#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"

void Config_Load(AppConfig *cfg, const wchar_t *configPath) {
    if (!cfg) return;
    cfg->lastFolder[0] = L'\0';
    cfg->volume = 80;
    cfg->shuffle = 0;
    cfg->repeat = 0;

    FILE *f = _wfopen(configPath, L"rb");
    if (!f) return;

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (sz <= 0 || sz > 65536) {
        fclose(f);
        return;
    }

    char *buf = (char*)malloc(sz + 1);
    if (!buf) {
        fclose(f);
        return;
    }

    fread(buf, 1, sz, f);
    buf[sz] = '\0';
    fclose(f);

    // "last_folder": "..."
    char *key = strstr(buf, "\"last_folder\"");
    if (key) {
        char *colon = strchr(key, ':');
        if (colon) {
            char *quote1 = strchr(colon, '\"');
            if (quote1) {
                char *quote2 = strchr(quote1 + 1, '\"');
                if (quote2) {
                    *quote2 = '\0';
                    char rawPath[MAX_PATH * 2] = {0};
                    char *srcPtr = quote1 + 1;
                    char *dstPtr = rawPath;
                    while (*srcPtr && (dstPtr - rawPath < (int)sizeof(rawPath) - 1)) {
                        if (*srcPtr == '\\' && *(srcPtr + 1) == '\\') {
                            *dstPtr++ = '\\';
                            srcPtr += 2;
                        } else {
                            *dstPtr++ = *srcPtr++;
                        }
                    }
                    *dstPtr = '\0';
                    MultiByteToWideChar(CP_UTF8, 0, rawPath, -1, cfg->lastFolder, MAX_PATH);
                }
            }
        }
    }

    // "volume": 80
    char *volKey = strstr(buf, "\"volume\"");
    if (volKey) {
        char *colon = strchr(volKey, ':');
        if (colon) {
            int v = atoi(colon + 1);
            if (v >= 0 && v <= 100) cfg->volume = v;
        }
    }

    // "shuffle": 0 or 1
    char *shufKey = strstr(buf, "\"shuffle\"");
    if (shufKey) {
        char *colon = strchr(shufKey, ':');
        if (colon) {
            cfg->shuffle = atoi(colon + 1);
        }
    }

    // "repeat": 0, 1, 2
    char *repKey = strstr(buf, "\"repeat\"");
    if (repKey) {
        char *colon = strchr(repKey, ':');
        if (colon) {
            cfg->repeat = atoi(colon + 1);
        }
    }

    free(buf);
}

void Config_Save(const AppConfig *cfg, const wchar_t *configPath) {
    if (!cfg) return;

    FILE *f = _wfopen(configPath, L"wb");
    if (!f) return;

    char utf8Folder[MAX_PATH * 3] = {0};
    WideCharToMultiByte(CP_UTF8, 0, cfg->lastFolder, -1, utf8Folder, sizeof(utf8Folder), NULL, NULL);

    // Escape backslashes for JSON
    char escapedFolder[MAX_PATH * 4] = {0};
    char *src = utf8Folder;
    char *dst = escapedFolder;
    while (*src && (dst - escapedFolder < (int)sizeof(escapedFolder) - 2)) {
        if (*src == '\\') {
            *dst++ = '\\';
            *dst++ = '\\';
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';

    fprintf(f, "{\n");
    fprintf(f, "  \"last_folder\": \"%s\",\n", escapedFolder);
    fprintf(f, "  \"volume\": %d,\n", cfg->volume);
    fprintf(f, "  \"shuffle\": %d,\n", cfg->shuffle);
    fprintf(f, "  \"repeat\": %d\n", cfg->repeat);
    fprintf(f, "}\n");

    fclose(f);
}
