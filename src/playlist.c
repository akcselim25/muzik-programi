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
    StrTrimW(str, L" -_\t|\uFF5C");
}

static void NormalizeUnicodeTurkishW(wchar_t *str) {
    if (!str) return;
    int r = 0, w = 0;
    while (str[r]) {
        // Decomposed 'I' + combining dot above (U+0307) -> 'İ' (U+0130)
        if (str[r] == L'I' && str[r + 1] == 0x0307) {
            str[w++] = 0x0130;
            r += 2;
            continue;
        }
        // Decomposed 'i' + combining dot above (U+0307) -> 'i'
        if (str[r] == L'i' && str[r + 1] == 0x0307) {
            str[w++] = L'i';
            r += 2;
            continue;
        }
        // Fullwidth pipe / slash replacements from yt-dlp
        if (str[r] == 0xFF5C) { // ｜ -> |
            str[w++] = L'|';
            r++;
            continue;
        }
        if (str[r] == 0x29F8 || str[r] == 0xFF0F) { // ⧸ or ／ -> /
            str[w++] = L'/';
            r++;
            continue;
        }
        str[w++] = str[r++];
    }
    str[w] = L'\0';
}

static inline wchar_t TurkishToUpperChar(wchar_t ch) {
    if (ch == L'i') return 0x0130; // İ
    if (ch == 0x0131) return L'I'; // ı -> I
    if (ch >= L'a' && ch <= L'z') return (wchar_t)(ch - L'a' + L'A');
    if (ch < 128) return ch;
    switch (ch) {
        case 0x015F: return 0x015E; // ş -> Ş
        case 0x011F: return 0x011E; // ğ -> Ğ
        case 0x00FC: return 0x00DC; // ü -> Ü
        case 0x00F6: return 0x00D6; // ö -> Ö
        case 0x00E7: return 0x00C7; // ç -> Ç
        default: break;
    }
    wchar_t out = ch;
    if (LCMapStringW(0x041F, LCMAP_UPPERCASE | LCMAP_LINGUISTIC_CASING, &ch, 1, &out, 1) > 0) {
        return out;
    }
    CharUpperBuffW(&out, 1);
    return out;
}

static inline wchar_t TurkishToLowerChar(wchar_t ch) {
    if (ch == 0x0130) return L'i';    // İ -> i
    if (ch == L'I') return 0x0131;    // I -> ı
    if (ch >= L'A' && ch <= L'Z') return (wchar_t)(ch - L'A' + L'a');
    if (ch < 128) return ch;
    switch (ch) {
        case 0x015E: return 0x015F; // Ş -> ş
        case 0x011E: return 0x011F; // Ğ -> ğ
        case 0x00DC: return 0x00FC; // Ü -> ü
        case 0x00D6: return 0x00F6; // Ö -> ö
        case 0x00C7: return 0x00E7; // Ç -> ç
        default: break;
    }
    wchar_t out = ch;
    if (LCMapStringW(0x041F, LCMAP_LOWERCASE | LCMAP_LINGUISTIC_CASING, &ch, 1, &out, 1) > 0) {
        return out;
    }
    CharLowerBuffW(&out, 1);
    return out;
}

static inline BOOL IsLetterW(wchar_t ch) {
    if ((ch >= L'A' && ch <= L'Z') || (ch >= L'a' && ch <= L'z')) return TRUE;
    if (ch < 128) return FALSE;
    WORD ctype = 0;
    if (GetStringTypeW(CT_CTYPE1, &ch, 1, &ctype)) {
        return (ctype & C1_ALPHA) != 0;
    }
    return iswalpha(ch) != 0;
}

static inline BOOL IsUpperLetterW(wchar_t ch) {
    if (ch >= L'A' && ch <= L'Z') return TRUE;
    if (ch < 128) return FALSE;
    if (ch == 0x0130 || ch == 0x015E || ch == 0x011E || ch == 0x00DC || ch == 0x00D6 || ch == 0x00C7) return TRUE;
    WORD ctype = 0;
    if (GetStringTypeW(CT_CTYPE1, &ch, 1, &ctype)) {
        return (ctype & C1_UPPER) != 0;
    }
    return iswupper(ch) != 0;
}

static inline BOOL IsLowerLetterW(wchar_t ch) {
    if (ch >= L'a' && ch <= L'z') return TRUE;
    if (ch < 128) return FALSE;
    if (ch == 0x0131 || ch == 0x015F || ch == 0x011F || ch == 0x00FC || ch == 0x00F6 || ch == 0x00E7) return TRUE;
    WORD ctype = 0;
    if (GetStringTypeW(CT_CTYPE1, &ch, 1, &ctype)) {
        return (ctype & C1_LOWER) != 0;
    }
    return iswlower(ch) != 0;
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
            // Parantez içi kenar boşluklarını düzelt: "( ORJİNAL )" -> "(ORJİNAL)"
            if ((str[r] == L')' || str[r] == L']') && w > 0 && str[w - 1] == L' ') {
                w--;
            }
            str[w++] = str[r];
            if (str[r] == L'(' || str[r] == L'[') {
                inSpace = 1; // Açılış parantezinden hemen sonraki boşluğu yut
            } else {
                inSpace = 0;
            }
            // Kapanış parantezinden hemen sonra harf geliyorsa araya boşluk koy: ")Yüreğime" -> ") Yüreğime"
            if ((str[r] == L')' || str[r] == L']') && str[r + 1] && IsLetterW(str[r + 1])) {
                str[w++] = L' ';
                inSpace = 1;
            }
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

static BOOL LooksLikeYouTubeId(const wchar_t *s, int len, wchar_t openCh) {
    // Biçim 1: (_6zej_upJhvE_) -> 13 karakter, başta ve sonda '_'
    if (len == 13 && s[0] == L'_' && s[12] == L'_') {
        for (int i = 1; i < 12; i++) {
            wchar_t c = s[i];
            if (!((c >= L'A' && c <= L'Z') || (c >= L'a' && c <= L'z') ||
                  (c >= L'0' && c <= L'9') || c == L'_' || c == L'-')) {
                return FALSE;
            }
        }
        return TRUE;
    }
    // Biçim 2: [6LM_mYcwTpM] veya [yeUQ9mjVp4k] -> tam 11 karakter ASCII YouTube ID
    if (len == 11) {
        int hasDigitOrSym = 0;
        int hasUpper = 0, hasLower = 0;
        for (int i = 0; i < 11; i++) {
            wchar_t c = s[i];
            if (c >= L'0' && c <= L'9') hasDigitOrSym = 1;
            else if (c == L'_' || c == L'-') hasDigitOrSym = 1;
            else if (c >= L'A' && c <= L'Z') hasUpper = 1;
            else if (c >= L'a' && c <= L'z') hasLower = 1;
            else return FALSE; // Türkçe/boşluk karakteri varsa YouTube ID değildir
        }
        if (openCh == L'[' && (hasDigitOrSym || (hasUpper && hasLower))) return TRUE;
        if (hasDigitOrSym && hasUpper && hasLower) return TRUE;
    }
    return FALSE;
}

static void RemovePatternInBracketsW(wchar_t *str, wchar_t openCh, wchar_t closeCh, const wchar_t **keywords, int kwCount) {
    wchar_t *start = str;
    while (*start) {
        if (*start == openCh) {
            wchar_t *end = wcschr(start + 1, closeCh);
            if (end) {
                int len = (int)(end - start - 1);
                int shouldRemove = 0;

                if (LooksLikeYouTubeId(start + 1, len, openCh)) {
                    shouldRemove = 1;
                }

                // Anahtar kelime kontrolü (Sadece gerçek çöp etiketler; Cover/Remix/Akustik/Live korunur!)
                if (!shouldRemove && keywords && len > 0 && len < 255) {
                    wchar_t inside[256];
                    for (int i = 0; i < len; i++) {
                        inside[i] = TurkishToLowerChar(start[1 + i]);
                    }
                    inside[len] = L'\0';
                    for (int k = 0; k < kwCount; k++) {
                        if (wcsstr(inside, keywords[k]) != NULL) {
                            shouldRemove = 1;
                            break;
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
        _snwprintf(threeWords, 255, L"%s %s %s", tokens[0], tokens[1], tokens[2]);
        threeWords[255] = L'\0';
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

    // Sadece gerçek ses/video uzantısı ise uzantıyı sil (örn: "ÜZÜLECEKSİN 2.0" bozulmasın!)
    const wchar_t *ext = PathFindExtensionW(buf);
    if (ext && (_wcsicmp(ext, L".mp3") == 0 || _wcsicmp(ext, L".m4a") == 0 ||
                _wcsicmp(ext, L".wav") == 0 || _wcsicmp(ext, L".flac") == 0 ||
                _wcsicmp(ext, L".ogg") == 0 || _wcsicmp(ext, L".wma") == 0 ||
                _wcsicmp(ext, L".aac") == 0 || _wcsicmp(ext, L".opus") == 0 ||
                _wcsicmp(ext, L".webm") == 0)) {
        PathRemoveExtensionW(buf);
    }

    NormalizeUnicodeTurkishW(buf);

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

    // 3. Parantez içindeki gereksiz YouTube/kalite etiketlerini temizle (Cover, Akustik, Remix, Live KORUNUR!)
    static const wchar_t *kw[] = {
        L"official", L"video", L"audio", L"lyrics", L"lyric", L"klip",
        L"visualizer", L"4k", L"8k", L"1080p", L"720p",
        L"orjinal", L"orijinal", L"esen m", L"netd", L"dokuz sekiz", L"poll production"
    };
    int kwCount = sizeof(kw) / sizeof(kw[0]);
    RemovePatternInBracketsW(buf, L'(', L')', kw, kwCount);
    RemovePatternInBracketsW(buf, L'[', L']', kw, kwCount);

    // 4. Yalın etiketleri (#hashtag) ve prod.@... kısımlarını temizle
    wchar_t *pProd;
    while ((pProd = StrStrIW(buf, L"prod.@")) != NULL) {
        wchar_t *end = pProd + 6;
        while (*end && *end != L' ' && *end != L'-' && *end != L'(') end++;
        wmemmove(pProd, end, wcslen(end) + 1);
    }

    wchar_t *pHash;
    while ((pHash = wcschr(buf, L'#')) != NULL) {
        wchar_t *end = pHash + 1;
        while (*end && *end != L' ') end++;
        wmemmove(pHash, end, wcslen(end) + 1);
    }

    // " | 8K", " | 4K", " | Official..." gibi dikey çizgi sonrası çöp bilgileri temizle
    wchar_t *pPipe = wcschr(buf, L'|');
    if (pPipe) {
        *pPipe = L'\0';
    }

    // " _ isim" (örnek: " _ nursena yener") temizle
    wchar_t *pUploader = StrStrIW(buf, L" _ ");
    if (pUploader) {
        *pUploader = L'\0';
    }

    // Yalın kanal/video eklerini temizle
    static const wchar_t *bareKw[] = {
        L"Official Music Video", L"Official Video", L"Official Audio",
        L"Official Lyric Video", L"Music Video", L"Video Klip", L"Lyric Video",
        L"- Esen Müzik", L"- Netd Müzik", L"- Kalan Müzik"
    };
    for (int i = 0; i < (int)(sizeof(bareKw)/sizeof(bareKw[0])); i++) {
        RemoveSubstrW(buf, bareKw[i]);
    }

    // Sonda tek başına kalan " Lyrics" ifadesini kaldır (ör: "Gökhan Türkmen - Taş Lyrics")
    CleanTrimW(buf);
    int bLen = (int)wcslen(buf);
    if (bLen > 7 && _wcsicmp(buf + bLen - 7, L" Lyrics") == 0) {
        buf[bLen - 7] = L'\0';
    }

    // 5. Tire ile birleşik isimleri düzenle
    FixHyphenatedNameW(buf, 512);

    // 6. Boşlukları ve kenarları normalize et
    StrCollapseSpacesW(buf);
    CleanTrimW(buf);

    // 7. Tamamı küçük harf olan dosya adlarında Türkçe duyarlı Baş Harf Büyütme (i -> İ, ş -> Ş, ç -> Ç)
    int lowerCount = 0, upperCount = 0;
    for (int i = 0; buf[i]; i++) {
        if (IsUpperLetterW(buf[i])) upperCount++;
        else if (IsLowerLetterW(buf[i])) lowerCount++;
    }
    if (upperCount == 0 && lowerCount > 0) {
        int newWord = 1;
        for (int i = 0; buf[i]; i++) {
            if (iswspace(buf[i]) || buf[i] == L'-' || buf[i] == L'(' || buf[i] == L'[') {
                newWord = 1;
            } else if (newWord && IsLetterW(buf[i])) {
                buf[i] = TurkishToUpperChar(buf[i]);
                newWord = 0;
            } else if (newWord && iswdigit(buf[i])) {
                newWord = 0;
            }
        }
    }

    wcsncpy(outTitle, buf, maxLen - 1);
    outTitle[maxLen - 1] = L'\0';
}

static int Playlist_AddInternal(const wchar_t *filePath, BOOL checkDuplicate) {
    if (!filePath || !filePath[0]) return -1;

    if (checkDuplicate) {
        for (int i = 0; i < g_count; i++) {
            if (_wcsicmp(g_items[i].filePath, filePath) == 0) {
                return i;
            }
        }
    }

    if (g_count >= g_capacity) {
        int newCap = (g_capacity == 0) ? 256 : g_capacity * 2;
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

int Playlist_Add(const wchar_t *filePath) {
    return Playlist_AddInternal(filePath, TRUE);
}

void Playlist_Remove(int index) {
    if (index < 0 || index >= g_count) return;
    if (index < g_count - 1) {
        memmove(&g_items[index], &g_items[index + 1], (g_count - 1 - index) * sizeof(PlaylistItem));
    }
    g_count--;
}

void Playlist_Move(int fromIndex, int toIndex) {
    if (fromIndex < 0 || fromIndex >= g_count || toIndex < 0 || toIndex >= g_count || fromIndex == toIndex) {
        return;
    }
    PlaylistItem temp = g_items[fromIndex];
    if (fromIndex < toIndex) {
        memmove(&g_items[fromIndex], &g_items[fromIndex + 1], (toIndex - fromIndex) * sizeof(PlaylistItem));
    } else {
        memmove(&g_items[toIndex + 1], &g_items[toIndex], (fromIndex - toIndex) * sizeof(PlaylistItem));
    }
    g_items[toIndex] = temp;
}

void Playlist_Clear(void) {
    g_count = 0;
}

int Playlist_FindNext(int currentIndex, int shuffle, int repeat) {
    if (g_count <= 0) return -1;
    if (repeat == 2) { // Tek Şarkı Tekrarla (Repeat One)
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
        return (repeat == 1) ? 0 : -1;
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
    _snwprintf(searchPattern, MAX_PATH - 1, L"%s\\*.*", folderPath);
    searchPattern[MAX_PATH - 1] = L'\0';

    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileExW(searchPattern, FindExInfoBasic, &fd, FindExSearchNameMatch, NULL, FIND_FIRST_EX_LARGE_FETCH);
    if (hFind == INVALID_HANDLE_VALUE) {
        hFind = FindFirstFileW(searchPattern, &fd);
        if (hFind == INVALID_HANDLE_VALUE) return 0;
    }

    // Liste boşsa O(N^2) tekrar kontrolüne gerek yok — anında O(N) yükle!
    BOOL checkDup = (g_count > 0);
    int added = 0;

    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            continue;
        }
        const wchar_t *ext = PathFindExtensionW(fd.cFileName);
        if (IsAudioExtension(ext)) {
            wchar_t fullPath[MAX_PATH];
            _snwprintf(fullPath, MAX_PATH - 1, L"%s\\%s", folderPath, fd.cFileName);
            fullPath[MAX_PATH - 1] = L'\0';
            if (Playlist_AddInternal(fullPath, checkDup) >= 0) {
                added++;
            }
        }
    } while (FindNextFileW(hFind, &fd));

    FindClose(hFind);
    return added;
}

static void ExtractYtIdFromUrlW(const wchar_t *url, wchar_t *outId) {
    outId[0] = L'\0';
    if (!url) return;
    const wchar_t *p = wcsstr(url, L"v=");
    if (p) p += 2;
    else {
        p = wcsstr(url, L"youtu.be/");
        if (p) p += 9;
    }
    if (!p) return;
    for (int i = 0; i < 11; i++) {
        wchar_t c = p[i];
        if (!((c >= L'A' && c <= L'Z') || (c >= L'a' && c <= L'z') ||
              (c >= L'0' && c <= L'9') || c == L'_' || c == L'-')) {
            return;
        }
    }
    wchar_t nextCh = p[11];
    if (nextCh == L'\0' || nextCh == L'&' || nextCh == L'?' || nextCh == L'/' || nextCh == L' ') {
        wcsncpy(outId, p, 11);
        outId[11] = L'\0';
    }
}

int Playlist_ImportSmart(const wchar_t *filePath, MissingPlaylistTrack *outMissing, int maxMissing, int *outMissingCount) {
    if (outMissingCount) *outMissingCount = 0;
    if (!filePath || !filePath[0]) return 0;

    FILE *f = _wfopen(filePath, L"rt, ccs=UTF-8");
    if (!f) {
        f = _wfopen(filePath, L"r");
        if (!f) return 0;
    }

    wchar_t dir[MAX_PATH];
    wcsncpy(dir, filePath, MAX_PATH - 1);
    dir[MAX_PATH - 1] = L'\0';
    PathRemoveFileSpecW(dir);

    wchar_t line[1024];
    wchar_t pendingTitle[256] = {0};
    wchar_t pendingYtId[16] = {0};
    int added = 0;

    while (fgetws(line, 1024, f)) {
        wchar_t *s = line;
        if (s[0] == 0xFEFF) s++; // UTF-8 BOM atla

        int len = (int)wcslen(s);
        while (len > 0 && (s[len - 1] == L'\r' || s[len - 1] == L'\n' || s[len - 1] == L' ' || s[len - 1] == L'\t')) {
            s[--len] = L'\0';
        }
        while (*s == L' ' || *s == L'\t') { s++; len--; }
        if (len <= 0) continue;

        if (s[0] == L'#') {
            if (_wcsnicmp(s, L"#EXTAURNS:YTID=", 15) == 0) {
                const wchar_t *idStr = s + 15;
                if (wcslen(idStr) == 11) {
                    wcsncpy(pendingYtId, idStr, 11);
                    pendingYtId[11] = L'\0';
                }
            } else if (_wcsnicmp(s, L"#EXTINF:", 8) == 0) {
                const wchar_t *comma = wcschr(s + 8, L',');
                if (comma && comma[1]) {
                    wcsncpy(pendingTitle, comma + 1, 255);
                    pendingTitle[255] = L'\0';
                    StrTrimW(pendingTitle, L" \t");
                }
            }
            continue;
        }

        BOOL isUrl = (_wcsnicmp(s, L"http://", 7) == 0 || _wcsnicmp(s, L"https://", 8) == 0);
        BOOL resolvedLocally = FALSE;

        if (!isUrl) {
            wchar_t fullPath[MAX_PATH];
            if (PathIsRelativeW(s)) {
                PathCombineW(fullPath, dir, s);
            } else {
                wcsncpy(fullPath, s, MAX_PATH - 1);
                fullPath[MAX_PATH - 1] = L'\0';
            }

            DWORD attr = GetFileAttributesW(fullPath);
            if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
                if (Playlist_Add(fullPath) >= 0) {
                    added++;
                }
                resolvedLocally = TRUE;
            }
        }

        if (!resolvedLocally) {
            if (!pendingYtId[0]) {
                if (isUrl) ExtractYtIdFromUrlW(s, pendingYtId);
                else Playlist_ExtractYouTubeId(s, pendingYtId, 16);
            }
            if (!pendingTitle[0] && !isUrl) {
                Playlist_CleanTitle(s, pendingTitle, 256);
            }
            if (!pendingTitle[0] && pendingYtId[0]) {
                _snwprintf(pendingTitle, 255, L"YouTube Parçası (%s)", pendingYtId);
                pendingTitle[255] = L'\0';
            }

            // Yerel kitaplikta bu sarki farkli bir dosya yoluyla zaten yuklu mu?
            int existingIdx = Playlist_FindBestMatch(pendingTitle, pendingYtId);
            if (existingIdx >= 0) {
                added++;
            } else if ((pendingTitle[0] || pendingYtId[0]) && outMissing && outMissingCount && *outMissingCount < maxMissing) {
                BOOL dup = FALSE;
                for (int k = 0; k < *outMissingCount; k++) {
                    if (pendingYtId[0] && outMissing[k].ytId[0] && wcscmp(outMissing[k].ytId, pendingYtId) == 0) {
                        dup = TRUE;
                        break;
                    }
                    if (pendingTitle[0] && _wcsicmp(outMissing[k].title, pendingTitle) == 0) {
                        dup = TRUE;
                        break;
                    }
                }
                if (!dup) {
                    int idx = (*outMissingCount)++;
                    wcsncpy(outMissing[idx].title, pendingTitle, 255);
                    outMissing[idx].title[255] = L'\0';
                    wcsncpy(outMissing[idx].ytId, pendingYtId, 15);
                    outMissing[idx].ytId[15] = L'\0';
                }
            }
        }

        pendingTitle[0] = L'\0';
        pendingYtId[0]  = L'\0';
    }

    fclose(f);
    return added;
}

int Playlist_ExportSmart(const wchar_t *filePath) {
    FILE *f = _wfopen(filePath, L"wt, ccs=UTF-8");
    if (!f) return 0;

    fwprintf(f, L"#EXTM3U\n");
    for (int i = 0; i < g_count; i++) {
        wchar_t ytId[16] = {0};
        Playlist_ExtractYouTubeId(g_items[i].filePath, ytId, 16);
        if (ytId[0]) {
            fwprintf(f, L"#EXTAURNS:YTID=%s\n", ytId);
        }
        fwprintf(f, L"#EXTINF:-1,%s\n", g_items[i].title);
        fwprintf(f, L"%s\n", g_items[i].filePath);
    }
    fclose(f);
    return 1;
}

int Playlist_FindByPrefix(wchar_t prefixChar, int startIndex) {
    if (g_count <= 0) return -1;
    wchar_t lowerP = TurkishToLowerChar(prefixChar);

    for (int step = 0; step < g_count; step++) {
        int idx = (startIndex + 1 + step) % g_count;
        if (g_items[idx].title[0] && TurkishToLowerChar(g_items[idx].title[0]) == lowerP) {
            return idx;
        }
    }
    return -1;
}

void Playlist_SplitArtistSong(const wchar_t *fullTitle, wchar_t *outArtist, int artistMax, wchar_t *outSong, int songMax) {
    if (outArtist && artistMax > 0) outArtist[0] = L'\0';
    if (outSong && songMax > 0) outSong[0] = L'\0';
    if (!fullTitle || !fullTitle[0]) return;

    const wchar_t *dash = wcsstr(fullTitle, L" - ");
    if (dash && dash > fullTitle && *(dash + 3) != L'\0') {
        if (outArtist && artistMax > 0) {
            int aLen = (int)(dash - fullTitle);
            if (aLen >= artistMax) aLen = artistMax - 1;
            wcsncpy(outArtist, fullTitle, aLen);
            outArtist[aLen] = L'\0';
            StrTrimW(outArtist, L" \t");
        }
        if (outSong && songMax > 0) {
            wcsncpy(outSong, dash + 3, songMax - 1);
            outSong[songMax - 1] = L'\0';
            StrTrimW(outSong, L" \t");
        }
    } else {
        if (outSong && songMax > 0) {
            wcsncpy(outSong, fullTitle, songMax - 1);
            outSong[songMax - 1] = L'\0';
            StrTrimW(outSong, L" \t");
        }
    }
}

void Playlist_ExtractYouTubeId(const wchar_t *filePath, wchar_t *outId, int maxLen) {
    if (!outId || maxLen <= 0) return;
    outId[0] = L'\0';
    if (!filePath || !filePath[0] || maxLen < 12) return;

    const wchar_t *fn = PathFindFileNameW(filePath);
    const wchar_t *p = fn;
    while (*p) {
        if (*p == L'[' || *p == L'(') {
            wchar_t closeCh = (*p == L'[') ? L']' : L')';
            const wchar_t *end = wcschr(p + 1, closeCh);
            if (end) {
                int len = (int)(end - (p + 1));
                if (LooksLikeYouTubeId(p + 1, len, *p)) {
                    if (len == 13 && p[1] == L'_' && p[13] == L'_') {
                        wcsncpy(outId, p + 2, 11);
                        outId[11] = L'\0';
                        return;
                    } else if (len == 11) {
                        wcsncpy(outId, p + 1, 11);
                        outId[11] = L'\0';
                        return;
                    }
                }
            }
        }
        p++;
    }
}

static wchar_t FoldAsciiCharW(wchar_t ch) {
    switch (ch) {
        case 0x0130: case 0x0131: case L'I': case L'i': case 0x00CE: case 0x00EE: return L'i';
        case 0x015E: case 0x015F: return L's';
        case 0x011E: case 0x011F: return L'g';
        case 0x00DC: case 0x00FC: case 0x00DB: case 0x00FB: return L'u';
        case 0x00D6: case 0x00F6: case 0x00D4: case 0x00F4: return L'o';
        case 0x00C7: case 0x00E7: return L'c';
        case 0x00C2: case 0x00E2: case 0x00C4: case 0x00E4: case 0x00C1: case 0x00E1: case 0x00C0: case 0x00E0: return L'a';
        case 0x00C9: case 0x00E9: case 0x00C8: case 0x00E8: case 0x00CA: case 0x00EA: return L'e';
        default: break;
    }
    if (ch >= L'A' && ch <= L'Z') return (wchar_t)(ch - L'A' + L'a');
    if ((ch >= L'a' && ch <= L'z') || (ch >= L'0' && ch <= L'9')) return ch;
    wchar_t low = TurkishToLowerChar(ch);
    if ((low >= L'a' && low <= L'z') || (low >= L'0' && low <= L'9')) return low;
    return L' ';
}

static void FoldForMatchW(const wchar_t *src, wchar_t *out, int maxLen) {
    if (!out || maxLen <= 0) return;
    out[0] = L'\0';
    if (!src || !src[0]) return;

    int w = 0;
    BOOL inSpace = TRUE;
    for (int r = 0; src[r] && w < maxLen - 1; r++) {
        if (src[r] == 0x0307) continue; // Combining dot above
        wchar_t fc = FoldAsciiCharW(src[r]);
        if (fc == L' ') {
            if (!inSpace) {
                out[w++] = L' ';
                inSpace = TRUE;
            }
        } else {
            out[w++] = fc;
            inSpace = FALSE;
        }
    }
    if (w > 0 && out[w - 1] == L' ') w--;
    out[w] = L'\0';
}

static int SplitFoldedTokens(const wchar_t *folded, wchar_t tokens[][48], int maxTokens, BOOL skipEditionWords) {
    if (!folded || !folded[0]) return 0;
    wchar_t buf[256];
    wcsncpy(buf, folded, 255);
    buf[255] = L'\0';

    int count = 0;
    wchar_t *ctx = NULL;
    wchar_t *tok = wcstok(buf, L" ", &ctx);
    while (tok && count < maxTokens) {
        BOOL skip = FALSE;
        if (skipEditionWords) {
            if (wcscmp(tok, L"feat") == 0 || wcscmp(tok, L"ft") == 0 ||
                wcscmp(tok, L"ile") == 0 || wcscmp(tok, L"ve") == 0) {
                skip = TRUE;
            }
        }
        if (!skip && tok[0]) {
            wcsncpy(tokens[count], tok, 47);
            tokens[count][47] = L'\0';
            count++;
        }
        tok = wcstok(NULL, L" ", &ctx);
    }
    return count;
}

static BOOL SongTokensEquivalent(const wchar_t *songA, const wchar_t *songB) {
    if (!songA[0] || !songB[0]) return FALSE;
    if (wcscmp(songA, songB) == 0) return TRUE;

    wchar_t tokA[16][48], tokB[16][48];
    int nA = SplitFoldedTokens(songA, tokA, 16, TRUE);
    int nB = SplitFoldedTokens(songB, tokB, 16, TRUE);
    if (nA == 0 || nB == 0) return FALSE;

    // Kısa olan tarafın TÜM kelimeleri uzun olan tarafta tam kelime olarak bulunmalı
    // ve kelime sayısı farkı en fazla 2 olmalı (ör: "mavi mavi" vs "mavi mavi akustik")
    if (abs(nA - nB) > 2) return FALSE;

    int minN = (nA < nB) ? nA : nB;
    int matched = 0;
    for (int i = 0; i < nA; i++) {
        for (int j = 0; j < nB; j++) {
            if (wcscmp(tokA[i], tokB[j]) == 0) {
                matched++;
                break;
            }
        }
    }
    return (matched >= minN);
}

static BOOL ArtistTokensShareIdentity(const wchar_t *artA, const wchar_t *artB) {
    if (!artA[0] || !artB[0]) return FALSE;
    if (wcscmp(artA, artB) == 0) return TRUE;

    wchar_t tokA[12][48], tokB[12][48];
    int nA = SplitFoldedTokens(artA, tokA, 12, TRUE);
    int nB = SplitFoldedTokens(artB, tokB, 12, TRUE);
    if (nA == 0 || nB == 0) return FALSE;

    for (int i = 0; i < nA; i++) {
        if (wcslen(tokA[i]) < 3) continue;
        for (int j = 0; j < nB; j++) {
            if (wcscmp(tokA[i], tokB[j]) == 0) return TRUE;
        }
    }
    return FALSE;
}

int Playlist_FindBestMatch(const wchar_t *targetTitle, const wchar_t *targetYtId) {
    if (g_count <= 0) return -1;

    // 1. Katman: YouTube 11 haneli Video ID tam eşleşmesi (%100 kesinlik)
    if (targetYtId && wcslen(targetYtId) == 11) {
        for (int i = 0; i < g_count; i++) {
            wchar_t itemYtId[16] = {0};
            Playlist_ExtractYouTubeId(g_items[i].filePath, itemYtId, 16);
            if (itemYtId[0] && wcscmp(itemYtId, targetYtId) == 0) {
                return i;
            }
        }
    }

    if (!targetTitle || !targetTitle[0]) return -1;

    // 2. Katman: Birebir başlık veya Türkçe/ASCII normalize edilmiş tam başlık eşleşmesi
    wchar_t tFoldFull[256];
    FoldForMatchW(targetTitle, tFoldFull, 256);
    if (!tFoldFull[0]) return -1;

    for (int i = 0; i < g_count; i++) {
        if (_wcsicmp(g_items[i].title, targetTitle) == 0) return i;
    }

    for (int i = 0; i < g_count; i++) {
        wchar_t cFoldFull[256];
        FoldForMatchW(g_items[i].title, cFoldFull, 256);
        if (wcscmp(cFoldFull, tFoldFull) == 0) return i;
    }

    // 3. Katman: Sanatçı + Şarkı yapısal kelime eşleşmesi (Yanlış şarkı açmayı %100 engeller!)
    wchar_t tArtRaw[128], tSongRaw[128];
    wchar_t tArt[128], tSong[128];
    Playlist_SplitArtistSong(targetTitle, tArtRaw, 128, tSongRaw, 128);
    FoldForMatchW(tArtRaw, tArt, 128);
    FoldForMatchW(tSongRaw, tSong, 128);

    int bestIdx = -1;
    int bestScore = 0;

    for (int i = 0; i < g_count; i++) {
        wchar_t cArtRaw[128], cSongRaw[128];
        wchar_t cArt[128], cSong[128];
        Playlist_SplitArtistSong(g_items[i].title, cArtRaw, 128, cSongRaw, 128);
        FoldForMatchW(cArtRaw, cArt, 128);
        FoldForMatchW(cSongRaw, cSong, 128);

        int score = 0;
        if (tArt[0] && cArt[0]) {
            // Normal yön: Sanatçı == Sanatçı VE Şarkı == Şarkı
            if (SongTokensEquivalent(tSong, cSong) && ArtistTokensShareIdentity(tArt, cArt)) {
                score = (wcscmp(tSong, cSong) == 0) ? 920 : 880;
            }
            // Ters yön ("Şarkı - Sanatçı" vs "Sanatçı - Şarkı")
            else if (SongTokensEquivalent(tSong, cArt) && ArtistTokensShareIdentity(tArt, cSong)) {
                score = 870;
            }
        } else if (tArt[0] && !cArt[0]) {
            if (wcslen(cSong) >= 5 && wcscmp(tSong, cSong) == 0) {
                score = 820;
            }
        } else if (!tArt[0] && cArt[0]) {
            if (wcslen(tSong) >= 5 && wcscmp(tSong, cSong) == 0) {
                score = 820;
            }
        }

        if (score > bestScore) {
            bestScore = score;
            bestIdx = i;
        }
    }

    return (bestScore >= 800) ? bestIdx : -1;
}

BOOL Playlist_IsSameTrack(int index, const wchar_t *targetTitle, const wchar_t *targetYtId) {
    if (index < 0 || index >= g_count) return FALSE;
    if (targetYtId && wcslen(targetYtId) == 11) {
        wchar_t itemYtId[16] = {0};
        Playlist_ExtractYouTubeId(g_items[index].filePath, itemYtId, 16);
        if (itemYtId[0] && wcscmp(itemYtId, targetYtId) == 0) return TRUE;
    }
    if (!targetTitle || !targetTitle[0]) return FALSE;
    if (_wcsicmp(g_items[index].title, targetTitle) == 0) return TRUE;

    wchar_t f1[256], f2[256];
    FoldForMatchW(g_items[index].title, f1, 256);
    FoldForMatchW(targetTitle, f2, 256);
    if (f1[0] && wcscmp(f1, f2) == 0) return TRUE;

    return (Playlist_FindBestMatch(targetTitle, targetYtId) == index);
}
