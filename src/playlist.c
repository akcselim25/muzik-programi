#include <windows.h>
#include <shlwapi.h>
#include <wchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <wctype.h>
#include "playlist.h"

static PlaylistItem *g_items = NULL;
static int g_count = 0;
static int g_capacity = 0;

void Playlist_Init(void) {
    g_items = NULL;
    g_count = 0;
    g_capacity = 0;
    srand((unsigned int)time(NULL));
}

void Playlist_Free(void) {
    if (g_items) {
        free(g_items);
        g_items = NULL;
    }
    g_count = 0;
    g_capacity = 0;
}

int Playlist_Count(void) {
    return g_count;
}

const PlaylistItem* Playlist_Get(int index) {
    if (index >= 0 && index < g_count) {
        return &g_items[index];
    }
    return NULL;
}

static void CleanTrimW(wchar_t *str) {
    if (!str) return;
    StrTrimW(str, L" -_\t");
}

static void StrCollapseSpacesW(wchar_t *str) {
    if (!str) return;
    int r = 0, w = 0;
    int inSpace = 0;
    while (str[r]) {
        if (str[r] == L' ' || str[r] == L'\t') {
            if (!inSpace) {
                str[w++] = L' ';
                inSpace = 1;
            }
        } else {
            str[w++] = str[r];
            inSpace = 0;
        }
        r++;
    }
    str[w] = L'\0';
}

static void RemoveSubstrW(wchar_t *str, const wchar_t *sub) {
    int subLen = (int)wcslen(sub);
    if (subLen == 0) return;
    wchar_t *p;
    while ((p = StrStrIW(str, sub)) != NULL) {
        int rest = (int)wcslen(p + subLen);
        wmemmove(p, p + subLen, rest + 1);
    }
}

static void RemovePatternInBracketsW(wchar_t *str, wchar_t openCh, wchar_t closeCh, const wchar_t **keywords, int kwCount) {
    wchar_t *start = str;
    while (*start) {
        if (*start == openCh) {
            wchar_t *end = wcschr(start + 1, closeCh);
            if (end) {
                int len = (int)(end - start - 1);
                int shouldRemove = 0;

                // YouTube ID: (_oK1fFvwSDaM_) veya [6LM_mYcwTpM] (10-16 karakter)
                if (len >= 10 && len <= 16) {
                    int allIdChars = 1;
                    for (int i = 1; i <= len; i++) {
                        wchar_t c = start[i];
                        if (!iswalnum(c) && c != L'_' && c != L'-') {
                            allIdChars = 0;
                            break;
                        }
                    }
                    if (allIdChars) shouldRemove = 1;
                }

                // Anahtar kelime kontrolü
                if (!shouldRemove && keywords) {
                    wchar_t inside[256];
                    if (len < 255) {
                        wcsncpy(inside, start + 1, len);
                        inside[len] = L'\0';
                        for (int k = 0; k < kwCount; k++) {
                            if (StrStrIW(inside, keywords[k]) != NULL) {
                                shouldRemove = 1;
                                break;
                            }
                        }
                    }
                }

                if (shouldRemove) {
                    int rest = (int)wcslen(end + 1);
                    wmemmove(start, end + 1, rest + 1);
                    continue;
                }
            }
        }
        start++;
    }
}

static void FixHyphenatedNameW(wchar_t *str, int maxLen) {
    if (wcschr(str, L' ') != NULL || wcschr(str, L'-') == NULL) return;

    wchar_t tokens[16][128];
    int tokenCount = 0;
    wchar_t buf[512];
    wcsncpy(buf, str, 511);
    buf[511] = L'\0';

    wchar_t *context = NULL;
    wchar_t *tok = wcstok(buf, L"-", &context);
    while (tok && tokenCount < 16) {
        if (tok[0]) {
            wcsncpy(tokens[tokenCount], tok, 127);
            tokens[tokenCount][127] = L'\0';
            tokenCount++;
        }
        tok = wcstok(NULL, L"-", &context);
    }

    if (tokenCount < 2) return;

    // Sondaki sayı ID ise kaldır (ör: -69, -52, -14, -49)
    if (tokenCount > 2) {
        int allDigits = 1;
        for (int i = 0; tokens[tokenCount - 1][i]; i++) {
            if (!iswdigit(tokens[tokenCount - 1][i])) {
                allDigits = 0;
                break;
            }
        }
        if (allDigits) {
            tokenCount--;
        }
    }

    if (tokenCount < 2) return;

    static const wchar_t *oneWordArtists[] = {
        L"yalin", L"simge", L"ikilem", L"deeperise", L"tarkan", L"hadise",
        L"edis", L"sefo", L"uzi", L"cakal", L"motive", L"poizi", L"blok3",
        L"ati242", L"sagopa", L"pinhani", L"gripin", L"koliva", L"karmate",
        L"lessio", L"esmeray", L"funda", L"gulsen", L"nilufer", L"semicenk",
        L"sedat", L"keman"
    };
    int oneWordCount = sizeof(oneWordArtists) / sizeof(oneWordArtists[0]);

    static const wchar_t *threeWordArtists[] = {
        L"leyla the band", L"elif buse dogan", L"ipek acar ile"
    };
    int threeWordCount = sizeof(threeWordArtists) / sizeof(threeWordArtists[0]);

    int artistTokens = 2; // Varsayılan 2 kelimeli sanatçı (İbrahim Tatlıses, Sezen Aksu)

    if (tokenCount >= 4) {
        wchar_t threeWords[256];
        wsprintfW(threeWords, L"%s %s %s", tokens[0], tokens[1], tokens[2]);
        for (int i = 0; i < threeWordCount; i++) {
            if (_wcsicmp(threeWords, threeWordArtists[i]) == 0) {
                artistTokens = 3;
                break;
            }
        }
    }

    if (artistTokens == 2) {
        for (int i = 0; i < oneWordCount; i++) {
            if (_wcsicmp(tokens[0], oneWordArtists[i]) == 0) {
                artistTokens = 1;
                break;
            }
        }
    }

    if (tokenCount == 2) {
        artistTokens = 1;
    }

    wchar_t result[512] = {0};
    for (int i = 0; i < artistTokens; i++) {
        if (i > 0) wcscat(result, L" ");
        wcscat(result, tokens[i]);
    }

    wcscat(result, L" - ");

    for (int i = artistTokens; i < tokenCount; i++) {
        if (i > artistTokens) wcscat(result, L" ");
        wcscat(result, tokens[i]);
    }

    wcsncpy(str, result, maxLen - 1);
    str[maxLen - 1] = L'\0';
}

void Playlist_CleanTitle(const wchar_t *path, wchar_t *outTitle, int maxLen) {
    if (!path || !path[0] || !outTitle || maxLen <= 0) return;

    const wchar_t *fileName = PathFindFileNameW(path);
    wchar_t buf[512];
    wcsncpy(buf, fileName, 511);
    buf[511] = L'\0';
    PathRemoveExtensionW(buf);

    // 1. İndirme sitesi öneklerini temizle
    static const wchar_t *prefixes[] = {
        L"mp3indirdur-", L"mp3semti-", L"sarkiindir-", L"muzikindir-",
        L"indir-", L"mp3indir-", L"tubidy-", L"trendmp3-"
    };
    for (int i = 0; i < (int)(sizeof(prefixes)/sizeof(prefixes[0])); i++) {
        int pLen = (int)wcslen(prefixes[i]);
        if (_wcsnicmp(buf, prefixes[i], pLen) == 0) {
            wmemmove(buf, buf + pLen, wcslen(buf + pLen) + 1);
            break;
        }
    }

    // 2. Başlangıç temizliği
    CleanTrimW(buf);

    // 3. Parantez içindeki gereksiz YouTube etiketlerini temizle
    static const wchar_t *kw[] = {
        L"official", L"video", L"audio", L"lyrics", L"klip", L"remix",
        L"cover", L"akustik", L"live", L"edit", L"4k", L"remaster",
        L"esen m", L"netd"
    };
    int kwCount = sizeof(kw) / sizeof(kw[0]);
    RemovePatternInBracketsW(buf, L'(', L')', kw, kwCount);
    RemovePatternInBracketsW(buf, L'[', L']', kw, kwCount);

    // 4. Yalın etiketleri ve prod.@... kısımlarını temizle
    wchar_t *pProd;
    while ((pProd = StrStrIW(buf, L"prod.@")) != NULL) {
        wchar_t *end = pProd + 6;
        while (*end && *end != L' ' && *end != L'-' && *end != L'(') end++;
        wmemmove(pProd, end, wcslen(end) + 1);
    }

    wchar_t *pHash;
    while ((pHash = wcschr(buf, L'#')) != NULL) {
        wchar_t *end = pHash + 1;
        while (*end && *end != L' ' && *end != L'-') end++;
        wmemmove(pHash, end, wcslen(end) + 1);
    }

    // " _ isim" (örnek: " _ nursena yener") temizle
    wchar_t *pUploader = StrStrIW(buf, L" _ ");
    if (pUploader) {
        *pUploader = L'\0';
    }

    // Yalın "Official Video", "Official Audio" temizle
    static const wchar_t *bareKw[] = {
        L"Official Music Video", L"Official Video", L"Official Audio",
        L"Music Video", L"Video Klip", L"Lyric Video"
    };
    for (int i = 0; i < (int)(sizeof(bareKw)/sizeof(bareKw[0])); i++) {
        RemoveSubstrW(buf, bareKw[i]);
    }

    // 5. Tire ile birleşik isimleri düzenle
    FixHyphenatedNameW(buf, 512);

    // 6. Boşlukları ve tireleri normalize et
    StrCollapseSpacesW(buf);
    CleanTrimW(buf);

    // 7. Hepsi küçük harfse her kelimenin ilk harfini büyüt
    int lowerCount = 0, upperCount = 0;
    for (int i = 0; buf[i]; i++) {
        if (iswlower(buf[i])) lowerCount++;
        else if (iswupper(buf[i])) upperCount++;
    }
    if (upperCount == 0 && lowerCount > 0) {
        int newWord = 1;
        for (int i = 0; buf[i]; i++) {
            if (iswspace(buf[i]) || buf[i] == L'-' || buf[i] == L'(') {
                newWord = 1;
            } else if (newWord && iswalpha(buf[i])) {
                buf[i] = towupper(buf[i]);
                newWord = 0;
            }
        }
    }

    wcsncpy(outTitle, buf, maxLen - 1);
    outTitle[maxLen - 1] = L'\0';
}

int Playlist_Add(const wchar_t *filePath) {
    if (!filePath || !filePath[0]) return -1;

    // Tekrar eklemeyi önlemek için kontrol et
    for (int i = 0; i < g_count; i++) {
        if (_wcsicmp(g_items[i].filePath, filePath) == 0) {
            return i;
        }
    }

    if (g_count >= g_capacity) {
        int newCap = (g_capacity == 0) ? 64 : g_capacity * 2;
        PlaylistItem *newItems = (PlaylistItem*)realloc(g_items, newCap * sizeof(PlaylistItem));
        if (!newItems) return -1;
        g_items = newItems;
        g_capacity = newCap;
    }

    int idx = g_count++;
    wcsncpy(g_items[idx].filePath, filePath, MAX_PATH - 1);
    g_items[idx].filePath[MAX_PATH - 1] = L'\0';

    Playlist_CleanTitle(filePath, g_items[idx].title, MAX_PATH);
    return idx;
}

void Playlist_Remove(int index) {
    if (index < 0 || index >= g_count) return;
    for (int i = index; i < g_count - 1; i++) {
        g_items[i] = g_items[i + 1];
    }
    g_count--;
}

void Playlist_Move(int fromIndex, int toIndex) {
    if (fromIndex < 0 || fromIndex >= g_count || toIndex < 0 || toIndex >= g_count || fromIndex == toIndex) {
        return;
    }
    PlaylistItem temp = g_items[fromIndex];
    if (fromIndex < toIndex) {
        for (int i = fromIndex; i < toIndex; i++) {
            g_items[i] = g_items[i + 1];
        }
    } else {
        for (int i = fromIndex; i > toIndex; i--) {
            g_items[i] = g_items[i - 1];
        }
    }
    g_items[toIndex] = temp;
}

void Playlist_Clear(void) {
    g_count = 0;
}

int Playlist_FindNext(int currentIndex, int shuffle, int repeat) {
    if (g_count <= 0) return -1;
    if (repeat == 2) { // Tekrar Çal (Repeat One)
        return currentIndex >= 0 ? currentIndex : 0;
    }
    if (shuffle && g_count > 1) {
        int nextIdx = rand() % g_count;
        if (nextIdx == currentIndex) {
            nextIdx = (nextIdx + 1) % g_count;
        }
        return nextIdx;
    }
    int next = currentIndex + 1;
    if (next >= g_count) {
        if (repeat == 1) { // Tümünü Tekrarla
            return 0;
        } else {
            return -1; // Bitti
        }
    }
    return next;
}

int Playlist_FindPrev(int currentIndex) {
    if (g_count <= 0) return -1;
    int prev = currentIndex - 1;
    if (prev < 0) {
        prev = g_count - 1;
    }
    return prev;
}

static int IsAudioExtension(const wchar_t *ext) {
    if (!ext) return 0;
    return (_wcsicmp(ext, L".mp3") == 0 ||
            _wcsicmp(ext, L".m4a") == 0 ||
            _wcsicmp(ext, L".wav") == 0 ||
            _wcsicmp(ext, L".flac") == 0 ||
            _wcsicmp(ext, L".ogg") == 0 ||
            _wcsicmp(ext, L".wma") == 0 ||
            _wcsicmp(ext, L".aac") == 0 ||
            _wcsicmp(ext, L".opus") == 0 ||
            _wcsicmp(ext, L".webm") == 0);
}

int Playlist_ScanFolder(const wchar_t *folderPath) {
    if (!folderPath || !folderPath[0]) return 0;

    wchar_t searchPattern[MAX_PATH];
    wsprintfW(searchPattern, L"%s\\*.*", folderPath);

    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(searchPattern, &fd);
    if (hFind == INVALID_HANDLE_VALUE) return 0;

    int added = 0;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            continue;
        }
        const wchar_t *ext = PathFindExtensionW(fd.cFileName);
        if (IsAudioExtension(ext)) {
            wchar_t fullPath[MAX_PATH];
            wsprintfW(fullPath, L"%s\\%s", folderPath, fd.cFileName);
            if (Playlist_Add(fullPath) >= 0) {
                added++;
            }
        }
    } while (FindNextFileW(hFind, &fd));

    FindClose(hFind);
    return added;
}

int Playlist_ImportM3U(const wchar_t *filePath) {
    FILE *f = _wfopen(filePath, L"rt, ccs=UTF-8");
    if (!f) {
        f = _wfopen(filePath, L"r"); // ANSI fallback
        if (!f) return 0;
    }

    wchar_t dir[MAX_PATH];
    wcsncpy(dir, filePath, MAX_PATH - 1);
    dir[MAX_PATH - 1] = L'\0';
    PathRemoveFileSpecW(dir);

    wchar_t line[1024];
    int added = 0;
    while (fgetws(line, 1024, f)) {
        // Trim newline and whitespace
        int len = (int)wcslen(line);
        while (len > 0 && (line[len - 1] == L'\r' || line[len - 1] == L'\n' || line[len - 1] == L' ')) {
            line[--len] = L'\0';
        }
        if (len == 0 || line[0] == L'#') continue;

        wchar_t fullPath[MAX_PATH];
        if (PathIsRelativeW(line)) {
            PathCombineW(fullPath, dir, line);
        } else {
            wcsncpy(fullPath, line, MAX_PATH - 1);
            fullPath[MAX_PATH - 1] = L'\0';
        }

        if (GetFileAttributesW(fullPath) != INVALID_FILE_ATTRIBUTES) {
            if (Playlist_Add(fullPath) >= 0) {
                added++;
            }
        }
    }
    fclose(f);
    return added;
}

int Playlist_ExportM3U(const wchar_t *filePath) {
    FILE *f = _wfopen(filePath, L"wt, ccs=UTF-8");
    if (!f) return 0;

    fwprintf(f, L"#EXTM3U\n");
    for (int i = 0; i < g_count; i++) {
        fwprintf(f, L"#EXTINF:-1,%s\n", g_items[i].title);
        fwprintf(f, L"%s\n", g_items[i].filePath);
    }
    fclose(f);
    return 1;
}

int Playlist_FindByPrefix(wchar_t prefixChar, int startIndex) {
    if (g_count <= 0) return -1;
    wchar_t lowerP = towlower(prefixChar);

    for (int step = 0; step < g_count; step++) {
        int idx = (startIndex + 1 + step) % g_count;
        if (g_items[idx].title[0] && towlower(g_items[idx].title[0]) == lowerP) {
            return idx;
        }
    }
    return -1;
}
