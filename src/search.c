#include <windows.h>
#include <commctrl.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <olectl.h>
#include <dwmapi.h>
#include "search.h"
#include "playlist.h"

// Premium Obsidian & Sunset-Neon Renk Paleti
#define COLOR_ACCENT_ORANGE RGB(255, 146, 92)   // #FF925C Turuncu Vurgu
#define COLOR_ACCENT_PINK   RGB(240, 68, 184)   // #F044B8 Pembe Vurgu
#define COLOR_ACCENT_PURPLE RGB(139, 53, 255)   // #8B35FF Mor Vurgu

#define COLOR_SEARCH_BG     RGB(5, 5, 5)        // #050505 Ana Arka Plan
#define COLOR_SEARCH_CARD   RGB(16, 16, 16)     // #101010 Kart ve Panel Arka Planı
#define COLOR_SEARCH_BOX    RGB(24, 24, 24)     // #181818 İkincil Yüzey
#define COLOR_SEARCH_LIST   RGB(16, 16, 16)     // #101010 Kart ve Panel Arka Planı
#define COLOR_SEARCH_BORDER RGB(38, 38, 38)     // #262626 Kenarlıklar

#define MAX_SEARCH_RESULTS  30

typedef struct {
    wchar_t id[64];
    wchar_t rawTitle[256];
    wchar_t cleanTitle[256];
    wchar_t uploader[128];
    wchar_t url[512];
    wchar_t source[32];
    int duration;
    IPicture *pThumb;
} SearchItem;

static HWND g_hDlg = NULL;
static HWND g_hParent = NULL;
static HWND g_hEditQuery = NULL;
static HWND g_hBtnSearch = NULL;
static HWND g_hLblFolder = NULL;
static HWND g_hBtnFolder = NULL;
static HWND g_hListResults = NULL;
static HWND g_hBtnDownload = NULL;
static HWND g_hLblStatus = NULL;

static wchar_t g_downloadFolder[MAX_PATH] = {0};
static wchar_t g_activeDlTitle[256] = {0};
static SearchItem *g_results = NULL; // Yalnızca pencere açıkken bellekte tutulur (0 KB boşta RAM!)
static int g_resultCount = 0;
static volatile BOOL g_isSearching = FALSE;
static volatile BOOL g_isDownloading = FALSE;
static volatile int  g_downloadPct = -1;
static volatile LONG g_searchGen = 0;

#define IDC_SEARCH_EDIT         2001
#define IDC_SEARCH_BTN          2002
#define IDC_SEARCH_FOLDER_LBL   2003
#define IDC_SEARCH_FOLDER_BTN   2004
#define IDC_SEARCH_LIST         2005
#define IDC_SEARCH_DOWN         2006
#define IDC_SEARCH_STAT         2007

#define WM_APP_SEARCH_DONE       (WM_APP + 201)
#define WM_APP_DOWN_DONE         (WM_APP + 202)
#define WM_APP_THUMB_READY       (WM_APP + 203)
#define WM_APP_DOWN_PROGRESS     (WM_APP + 204)

static int CALLBACK EnumFontCheckProcS(const LOGFONTW *lpelfe, const TEXTMETRICW *lpntme, DWORD FontType, LPARAM lParam) {
    (void)lpelfe; (void)lpntme; (void)FontType;
    *(BOOL*)lParam = TRUE;
    return 0;
}

static void GetCachedSearchFonts(const wchar_t **outTextFont, const wchar_t **outIconFont) {
    static BOOL s_probed = FALSE;
    static BOOL s_hasVarText = FALSE;
    static BOOL s_hasFluent = FALSE;
    if (!s_probed) {
        s_probed = TRUE;
        HDC hdc = GetDC(NULL);
        if (hdc) {
            LOGFONTW lf = {0};
            lf.lfCharSet = DEFAULT_CHARSET;
            wcscpy(lf.lfFaceName, L"Segoe UI Variable Text");
            EnumFontFamiliesExW(hdc, &lf, (FONTENUMPROCW)EnumFontCheckProcS, (LPARAM)&s_hasVarText, 0);
            wcscpy(lf.lfFaceName, L"Segoe Fluent Icons");
            EnumFontFamiliesExW(hdc, &lf, (FONTENUMPROCW)EnumFontCheckProcS, (LPARAM)&s_hasFluent, 0);
            ReleaseDC(NULL, hdc);
        }
    }
    *outTextFont = s_hasVarText ? L"Segoe UI Variable Text" : L"Segoe UI";
    *outIconFont = s_hasFluent  ? L"Segoe Fluent Icons"     : L"Segoe MDL2 Assets";
}

void Search_GetActiveDownloadTitle(wchar_t *outTitle, int maxLen) {
    if (!outTitle || maxLen <= 0) return;
    wcsncpy(outTitle, g_activeDlTitle, maxLen - 1);
    outTitle[maxLen - 1] = L'\0';
}

static int ParseYtDlpPercentLine(const char *line) {
    if (!line) return -1;
    const char *dlTag = strstr(line, "[download]");
    if (!dlTag) return -1;
    const char *pctSign = strchr(dlTag + 10, '%');
    if (!pctSign) return -1;

    const char *p = pctSign - 1;
    while (p > dlTag && ((*p >= '0' && *p <= '9') || *p == '.')) {
        p--;
    }
    p++;
    if (p >= pctSign) return -1;

    double val = atof(p);
    if (val < 0.0 || val > 100.0) return -1;
    return (int)(val + 0.5);
}

BOOL Search_RunYtDlpDownload(const wchar_t *targetSpec,
                             const wchar_t *destFolder,
                             volatile LONG *pCancelFlag,
                             YtDlpProgressFn onProgress,
                             void *userData) {
    if (!targetSpec || !targetSpec[0] || !destFolder || !destFolder[0]) return FALSE;

    wchar_t cmdLine[2048];
    _snwprintf(cmdLine, 2047,
               L"yt-dlp.exe --progress --newline --no-warnings -f \"ba[ext=m4a]/ba\" --write-thumbnail --no-playlist -o \"%s\\%%(title)s [%%(id)s].%%(ext)s\" \"%s\"",
               destFolder, targetSpec);
    cmdLine[2047] = L'\0';

    HANDLE hRead = NULL, hWrite = NULL;
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) {
        return FALSE;
    }
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = hWrite;
    si.hStdError  = hWrite;

    PROCESS_INFORMATION pi = {0};
    if (!CreateProcessW(NULL, cmdLine, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(hRead);
        CloseHandle(hWrite);
        return FALSE;
    }
    CloseHandle(hWrite);

    if (onProgress) onProgress(3, userData);

    char buffer[1024];
    char lineBuf[512];
    int linePos = 0;
    int bestPct = 3;
    DWORD bytesRead = 0;

    while (ReadFile(hRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
        if (pCancelFlag && *pCancelFlag != 0) {
            TerminateProcess(pi.hProcess, 1);
            break;
        }
        for (DWORD i = 0; i < bytesRead; i++) {
            char ch = buffer[i];
            if (ch == '\n' || ch == '\r') {
                if (linePos > 0) {
                    lineBuf[linePos] = '\0';
                    int pct = ParseYtDlpPercentLine(lineBuf);
                    if (pct >= 0) {
                        if (pct > 99) pct = 99;
                        if (pct > bestPct) {
                            bestPct = pct;
                            if (onProgress) onProgress(bestPct, userData);
                        }
                    }
                    linePos = 0;
                }
            } else if (linePos < (int)sizeof(lineBuf) - 1) {
                lineBuf[linePos++] = ch;
            }
        }
    }

    CloseHandle(hRead);
    WaitForSingleObject(pi.hProcess, 15000);
    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    if (pCancelFlag && *pCancelFlag != 0) return FALSE;
    BOOL success = (exitCode == 0);
    if (success && onProgress) {
        onProgress(100, userData);
    }
    return success;
}

static void FreeSearchResults(void) {
    if (g_results) {
        for (int i = 0; i < g_resultCount; i++) {
            if (g_results[i].pThumb) {
                g_results[i].pThumb->lpVtbl->Release(g_results[i].pThumb);
                g_results[i].pThumb = NULL;
            }
        }
    }
    g_resultCount = 0;
}

static DWORD WINAPI SearchThread(LPVOID param) {
    wchar_t *query = (wchar_t*)param;
    g_resultCount = 0;

    wchar_t cmdLine[2048];
    _snwprintf(cmdLine, 2047, L"yt-dlp.exe --encoding utf-8 --no-warnings --flat-playlist --print \"%%(id)s\t%%(title)s\t%%(duration)s\t%%(uploader)s\t%%(webpage_url)s\" \"ytsearch10:%s\"", query);
    cmdLine[2047] = L'\0';
    free(query);

    HANDLE hRead, hWrite;
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) {
        g_isSearching = FALSE;
        if (IsWindow(g_hDlg)) PostMessageW(g_hDlg, WM_APP_SEARCH_DONE, 0, 0);
        return 0;
    }
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = hWrite;
    si.hStdError = NULL;

    PROCESS_INFORMATION pi = {0};
    if (CreateProcessW(NULL, cmdLine, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(hWrite);

        char buffer[4096];
        DWORD bytesRead;
        char lineBuf[2048] = {0};
        int linePos = 0;

        while (ReadFile(hRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
            for (DWORD i = 0; i < bytesRead; i++) {
                char ch = buffer[i];
                if (ch == '\n' || ch == '\r') {
                    if (linePos > 0) {
                        lineBuf[linePos] = '\0';
                        char *tab1 = strchr(lineBuf, '\t');
                        if (tab1 && g_results && g_resultCount < MAX_SEARCH_RESULTS) {
                            *tab1 = '\0';
                            char *tab2 = strchr(tab1 + 1, '\t');
                            if (tab2) {
                                *tab2 = '\0';
                                char *tab3 = strchr(tab2 + 1, '\t');
                                if (tab3) {
                                    *tab3 = '\0';
                                    char *tab4 = strchr(tab3 + 1, '\t');
                                    if (tab4) *tab4 = '\0';

                                    SearchItem *item = &g_results[g_resultCount];
                                    memset(item, 0, sizeof(SearchItem));
                                    MultiByteToWideChar(CP_UTF8, 0, lineBuf, -1, item->id, 63);
                                    MultiByteToWideChar(CP_UTF8, 0, tab1 + 1, -1, item->rawTitle, 255);
                                    item->duration = (int)atof(tab2 + 1);
                                    MultiByteToWideChar(CP_UTF8, 0, tab3 + 1, -1, item->uploader, 127);
                                    if (tab4) {
                                        MultiByteToWideChar(CP_UTF8, 0, tab4 + 1, -1, item->url, 511);
                                    } else {
                                        _snwprintf(item->url, 511, L"https://www.youtube.com/watch?v=%s", item->id);
                                    }
                                    wcscpy(item->source, L"YouTube");

                                    Playlist_CleanTitle(item->rawTitle, item->cleanTitle, 256);
                                    if (!item->cleanTitle[0]) {
                                        wcsncpy(item->cleanTitle, item->rawTitle, 255);
                                    }

                                    g_resultCount++;
                                }
                            }
                        }
                        linePos = 0;
                    }
                } else if (linePos < (int)sizeof(lineBuf) - 1) {
                    lineBuf[linePos++] = ch;
                }
            }
        }
        CloseHandle(hRead);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    } else {
        CloseHandle(hRead);
        CloseHandle(hWrite);
    }

    g_isSearching = FALSE;
    if (IsWindow(g_hDlg)) {
        PostMessageW(g_hDlg, WM_APP_SEARCH_DONE, 0, 0);
    }
    return 0;
}

typedef struct {
    int itemIndex;
    LONG searchGen;
    wchar_t id[64];
} SingleThumbTask;

static DWORD WINAPI SingleThumbThread(LPVOID param) {
    SingleThumbTask *task = (SingleThumbTask*)param;
    OleInitialize(NULL);

    wchar_t thumbUrl[256];
    _snwprintf(thumbUrl, 255, L"https://i.ytimg.com/vi/%s/mqdefault.jpg", task->id);
    thumbUrl[255] = L'\0';

    IPicture *pPic = NULL;
    HRESULT hr = OleLoadPicturePath(thumbUrl, NULL, 0, 0, &IID_IPicture, (void**)&pPic);
    if (FAILED(hr) || !pPic) {
        _snwprintf(thumbUrl, 255, L"https://i.ytimg.com/vi/%s/default.jpg", task->id);
        thumbUrl[255] = L'\0';
        hr = OleLoadPicturePath(thumbUrl, NULL, 0, 0, &IID_IPicture, (void**)&pPic);
    }

    if (SUCCEEDED(hr) && pPic) {
        if (g_results && IsWindow(g_hDlg) && task->searchGen == g_searchGen &&
            task->itemIndex < g_resultCount && wcscmp(g_results[task->itemIndex].id, task->id) == 0) {
            g_results[task->itemIndex].pThumb = pPic;
            PostMessageW(g_hDlg, WM_APP_THUMB_READY, (WPARAM)task->itemIndex, 0);
        } else {
            pPic->lpVtbl->Release(pPic);
        }
    }

    free(task);
    OleUninitialize();
    return 0;
}

typedef struct {
    wchar_t url[512];
    wchar_t folder[MAX_PATH];
} DownloadParam;

static void OnDialogDownloadProgress(int pct, void *userData) {
    (void)userData;
    g_downloadPct = pct;
    if (IsWindow(g_hDlg)) {
        PostMessageW(g_hDlg, WM_APP_DOWN_PROGRESS, (WPARAM)pct, 0);
    }
    if (IsWindow(g_hParent)) {
        PostMessageW(g_hParent, WM_APP_SINGLE_DL_PROGRESS, (WPARAM)pct, 0);
    }
}

static DWORD WINAPI DownloadThread(LPVOID param) {
    DownloadParam *dp = (DownloadParam*)param;
    BOOL success = Search_RunYtDlpDownload(dp->url, dp->folder, NULL, OnDialogDownloadProgress, NULL);
    free(dp);

    g_isDownloading = FALSE;
    if (!success && IsWindow(g_hParent)) {
        PostMessageW(g_hParent, WM_APP_SINGLE_DL_PROGRESS, (WPARAM)-1, 0);
    }
    if (IsWindow(g_hDlg)) {
        PostMessageW(g_hDlg, WM_APP_DOWN_DONE, (WPARAM)success, 0);
    } else if (success && IsWindow(g_hParent)) {
        PostMessageW(g_hParent, WM_APP_FILE_DOWNLOADED, 0, 0);
    }
    return 0;
}

static void ChooseFolderDialog(HWND hWnd) {
    BROWSEINFOW bi = { 0 };
    bi.hwndOwner = hWnd;
    bi.lpszTitle = L"Müziklerin İndirileceği Klasörü Seçin";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
    if (pidl) {
        wchar_t path[MAX_PATH];
        if (SHGetPathFromIDListW(pidl, path)) {
            wcsncpy(g_downloadFolder, path, MAX_PATH - 1);
            g_downloadFolder[MAX_PATH - 1] = L'\0';
            wchar_t lbl[MAX_PATH + 32];
            _snwprintf(lbl, MAX_PATH + 31, L"Klasör: %s", g_downloadFolder);
            lbl[MAX_PATH + 31] = L'\0';
            SetWindowTextW(g_hLblFolder, lbl);
        }
        CoTaskMemFree(pidl);
    }
}

static void DrawFastCard(HDC hdc, const RECT *rc, COLORREF bg, COLORREF border, int radius) {
    HGDIOBJ oldBr = SelectObject(hdc, GetStockObject(DC_BRUSH));
    HGDIOBJ oldPen = SelectObject(hdc, GetStockObject(DC_PEN));
    SetDCBrushColor(hdc, bg);
    SetDCPenColor(hdc, border);
    RoundRect(hdc, rc->left, rc->top, rc->right, rc->bottom, radius, radius);
    SelectObject(hdc, oldBr);
    SelectObject(hdc, oldPen);
}

static void DrawNeonProgressBarGdi(HDC hdc, const RECT *rcTrack, int pct) {
    int tw = rcTrack->right - rcTrack->left;
    int th = rcTrack->bottom - rcTrack->top;
    if (tw <= 0 || th <= 0) return;

    DrawFastCard(hdc, rcTrack, RGB(24, 24, 24), COLOR_SEARCH_BORDER, th);
    if (pct <= 0) return;
    if (pct > 100) pct = 100;

    int fillW = (tw * pct) / 100;
    if (fillW < th && pct > 0) fillW = th;
    if (fillW > tw) fillW = tw;

    int saved = SaveDC(hdc);
    HRGN hRgn = CreateRoundRectRgn(rcTrack->left, rcTrack->top, rcTrack->left + fillW + 1, rcTrack->bottom + 1, th, th);
    SelectClipRgn(hdc, hRgn);

    int halfW = tw / 2;
    if (halfW < 1) halfW = 1;
    TRIVERTEX v1[2] = {
        { rcTrack->left,         rcTrack->top,    0xFF00, 0x9200, 0x5C00, 0xFF00 },
        { rcTrack->left + halfW, rcTrack->bottom, 0xF000, 0x4400, 0xB800, 0xFF00 }
    };
    TRIVERTEX v2[2] = {
        { rcTrack->left + halfW, rcTrack->top,    0xF000, 0x4400, 0xB800, 0xFF00 },
        { rcTrack->right,        rcTrack->bottom, 0x8B00, 0x3500, 0xFF00, 0xFF00 }
    };
    GRADIENT_RECT gRect = { 0, 1 };
    GdiGradientFill(hdc, v1, 2, &gRect, 1, GRADIENT_FILL_RECT_H);
    GdiGradientFill(hdc, v2, 2, &gRect, 1, GRADIENT_FILL_RECT_H);

    RestoreDC(hdc, saved);
    DeleteObject(hRgn);
}

const wchar_t* Search_GetDownloadFolder(void) {
    return g_downloadFolder;
}

static int GetSearchWindowDpi(HWND hWnd) {
    typedef UINT(WINAPI *GetDpiForWindowProc)(HWND);
    GetDpiForWindowProc pGetDpi = (GetDpiForWindowProc)GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow");
    int dpi = pGetDpi ? (int)pGetDpi(hWnd) : 0;
    if (dpi <= 0) {
        HDC hdc = GetDC(hWnd);
        if (hdc) {
            dpi = GetDeviceCaps(hdc, LOGPIXELSY);
            ReleaseDC(hWnd, hdc);
        }
    }
    if (dpi <= 0) dpi = 96;
    return dpi;
}

static void LayoutSearchControls(HWND hWnd) {
    RECT rcClient;
    GetClientRect(hWnd, &rcClient);
    int clientW = rcClient.right - rcClient.left;
    int clientH = rcClient.bottom - rcClient.top;
    if (clientW <= 0 || clientH <= 0) return;

    int dpi = GetSearchWindowDpi(hWnd);
    int pad = MulDiv(16, dpi, 96);
    int gap = MulDiv(14, dpi, 96);

    int cardW = MulDiv(280, dpi, 96);
    if (cardW > clientW / 2) cardW = clientW / 2;
    if (cardW < MulDiv(240, dpi, 96)) cardW = MulDiv(240, dpi, 96);

    int cardRight  = clientW - pad;
    int cardLeft   = cardRight - cardW;
    int cardBottom = clientH - pad;

    int leftX = pad;
    int leftW = cardLeft - gap - leftX;
    if (leftW < MulDiv(260, dpi, 96)) leftW = MulDiv(260, dpi, 96);

    int btnSearchW = MulDiv(92, dpi, 96);
    int searchH    = MulDiv(36, dpi, 96);
    int editW      = leftW - btnSearchW - MulDiv(8, dpi, 96);

    if (g_hEditQuery) MoveWindow(g_hEditQuery, leftX + MulDiv(10, dpi, 96), pad + MulDiv(8, dpi, 96), editW - MulDiv(20, dpi, 96), searchH - MulDiv(14, dpi, 96), TRUE);
    if (g_hBtnSearch) MoveWindow(g_hBtnSearch, leftX + editW + MulDiv(8, dpi, 96), pad, btnSearchW - MulDiv(8, dpi, 96), searchH, TRUE);

    int row2Y      = pad + searchH + MulDiv(8, dpi, 96);
    int btnFolderW = MulDiv(108, dpi, 96);
    int row2H      = MulDiv(28, dpi, 96);
    int folderLblW = leftW - btnFolderW - MulDiv(8, dpi, 96);

    if (g_hLblFolder) MoveWindow(g_hLblFolder, leftX + 2, row2Y + 4, folderLblW - 4, row2H - 4, TRUE);
    if (g_hBtnFolder) MoveWindow(g_hBtnFolder, leftX + folderLblW + MulDiv(4, dpi, 96), row2Y, btnFolderW - MulDiv(4, dpi, 96), row2H, TRUE);

    int statH = MulDiv(22, dpi, 96);
    int statY = clientH - pad - statH;
    if (g_hLblStatus) MoveWindow(g_hLblStatus, leftX + 2, statY, leftW - 4, statH, TRUE);

    int listY = row2Y + row2H + MulDiv(8, dpi, 96);
    int listH = statY - listY - MulDiv(8, dpi, 96);
    if (g_hListResults) MoveWindow(g_hListResults, leftX, listY, leftW, listH, TRUE);

    int cardPad  = MulDiv(14, dpi, 96);
    int btnDownH = MulDiv(44, dpi, 96);
    int btnDownW = cardW - (cardPad * 2);
    int btnDownX = cardLeft + cardPad;
    int btnDownY = cardBottom - cardPad - btnDownH;
    if (g_hBtnDownload) MoveWindow(g_hBtnDownload, btnDownX, btnDownY, btnDownW, btnDownH, TRUE);
}

static void DoPaste(HWND hEdit) {
    if (OpenClipboard(NULL)) {
        HANDLE hData = GetClipboardData(CF_UNICODETEXT);
        if (hData) {
            const wchar_t *pText = (const wchar_t*)GlobalLock(hData);
            if (pText) {
                size_t len = wcslen(pText);
                wchar_t *clean = (wchar_t*)malloc((len + 1) * sizeof(wchar_t));
                if (clean) {
                    for (size_t i = 0; i <= len; i++) {
                        clean[i] = (pText[i] == L'\r' || pText[i] == L'\n') ? L' ' : pText[i];
                    }
                    SendMessageW(hEdit, EM_REPLACESEL, TRUE, (LPARAM)clean);
                    free(clean);
                }
                GlobalUnlock(hData);
            }
        }
        CloseClipboard();
    }
}

static LRESULT CALLBACK SearchEditSubclassProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    (void)uIdSubclass;
    HWND hDlg = (HWND)dwRefData;

    if (msg == WM_KEYDOWN) {
        if (wParam == VK_RETURN) {
            if (hDlg && g_hBtnSearch) {
                SendMessageW(hDlg, WM_COMMAND, MAKEWPARAM(IDC_SEARCH_BTN, BN_CLICKED), (LPARAM)g_hBtnSearch);
            }
            return 0;
        }
        if (wParam == 'V' && (GetKeyState(VK_CONTROL) & 0x8000)) { DoPaste(hWnd); return 0; }
        if (wParam == 'A' && (GetKeyState(VK_CONTROL) & 0x8000)) { SendMessageW(hWnd, EM_SETSEL, 0, -1); return 0; }
        if (wParam == 'C' && (GetKeyState(VK_CONTROL) & 0x8000)) { SendMessageW(hWnd, WM_COPY, 0, 0); return 0; }
        if (wParam == 'X' && (GetKeyState(VK_CONTROL) & 0x8000)) { SendMessageW(hWnd, WM_CUT, 0, 0); return 0; }
    }
    else if (msg == WM_CHAR) {
        if (wParam == 1 || wParam == 22 || wParam == 3 || wParam == 24) return 0;
    }
    else if (msg == WM_CONTEXTMENU) {
        POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
        if (pt.x == -1 && pt.y == -1) {
            RECT rc;
            GetWindowRect(hWnd, &rc);
            pt.x = rc.left;
            pt.y = rc.bottom;
        }
        HMENU hMenu = CreatePopupMenu();
        AppendMenuW(hMenu, MF_STRING, 1, L"Kes\tCtrl+X");
        AppendMenuW(hMenu, MF_STRING, 2, L"Kopyala\tCtrl+C");
        AppendMenuW(hMenu, MF_STRING, 3, L"Yapıştır\tCtrl+V");
        AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
        AppendMenuW(hMenu, MF_STRING, 4, L"Tümünü Seç\tCtrl+A");

        DWORD sel = (DWORD)SendMessageW(hWnd, EM_GETSEL, 0, 0);
        BOOL hasSel = (LOWORD(sel) != HIWORD(sel));
        if (!hasSel) {
            EnableMenuItem(hMenu, 1, MF_BYCOMMAND | MF_GRAYED);
            EnableMenuItem(hMenu, 2, MF_BYCOMMAND | MF_GRAYED);
        }
        if (!IsClipboardFormatAvailable(CF_UNICODETEXT) && !IsClipboardFormatAvailable(CF_TEXT)) {
            EnableMenuItem(hMenu, 3, MF_BYCOMMAND | MF_GRAYED);
        }

        int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hWnd, NULL);
        DestroyMenu(hMenu);

        if (cmd == 1) SendMessageW(hWnd, WM_CUT, 0, 0);
        else if (cmd == 2) SendMessageW(hWnd, WM_COPY, 0, 0);
        else if (cmd == 3) DoPaste(hWnd);
        else if (cmd == 4) SendMessageW(hWnd, EM_SETSEL, 0, -1);
        return 0;
    }

    return DefSubclassProc(hWnd, msg, wParam, lParam);
}

static LRESULT CALLBACK SearchWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HBRUSH hBrBg = NULL;
    static HBRUSH hBrBox = NULL;
    static HBRUSH hBrList = NULL;
    static HFONT hFont = NULL;
    static HFONT hFontBold = NULL;
    static HFONT hFontSmall = NULL;
    static HFONT hFontIcon = NULL;
    static HDC s_memDC = NULL;
    static HBITMAP s_memBmp = NULL;
    static HBITMAP s_oldBmp = NULL;
    static int s_memW = 0, s_memH = 0;

    switch (msg) {
        case WM_CREATE: {
            g_hDlg = hWnd;
            int dpi = GetSearchWindowDpi(hWnd);

            if (!g_results) {
                g_results = (SearchItem*)calloc(MAX_SEARCH_RESULTS, sizeof(SearchItem));
            }
            g_resultCount = 0;
            if (!g_isDownloading) g_downloadPct = -1;

            BOOL useDark = TRUE;
            DwmSetWindowAttribute(hWnd, 20, &useDark, sizeof(useDark));
            DwmSetWindowAttribute(hWnd, 19, &useDark, sizeof(useDark));
            DWORD cornerPref = 2; // DWMWCP_ROUND
            DwmSetWindowAttribute(hWnd, 33, &cornerPref, sizeof(cornerPref));

            hBrBg   = CreateSolidBrush(COLOR_SEARCH_BG);
            hBrBox  = CreateSolidBrush(COLOR_SEARCH_BOX);
            hBrList = CreateSolidBrush(COLOR_SEARCH_LIST);

            int sMain  = -MulDiv(10, dpi, 72);
            int sBold  = -MulDiv(10, dpi, 72);
            int sSmall = -MulDiv(9,  dpi, 72);
            int sIcon  = -MulDiv(11, dpi, 72);

            const wchar_t *fontFamily = NULL;
            const wchar_t *iconFamily = NULL;
            GetCachedSearchFonts(&fontFamily, &iconFamily);

            hFont      = CreateFontW(sMain,  0, 0, 0, FW_NORMAL,   FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, fontFamily);
            hFontBold  = CreateFontW(sBold,  0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, fontFamily);
            hFontSmall = CreateFontW(sSmall, 0, 0, 0, FW_NORMAL,   FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, fontFamily);
            hFontIcon  = CreateFontW(sIcon,  0, 0, 0, FW_NORMAL,   FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, iconFamily);

            g_hEditQuery = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 0, 0, 10, 10, hWnd, (HMENU)IDC_SEARCH_EDIT, GetModuleHandle(NULL), NULL);
            g_hBtnSearch = CreateWindowExW(0, L"BUTTON", L"Ara", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0, 0, 10, 10, hWnd, (HMENU)IDC_SEARCH_BTN, GetModuleHandle(NULL), NULL);

            wchar_t folderTxt[MAX_PATH + 32];
            _snwprintf(folderTxt, MAX_PATH + 31, L"Klasör: %s", g_downloadFolder);
            folderTxt[MAX_PATH + 31] = L'\0';
            g_hLblFolder = CreateWindowExW(0, L"STATIC", folderTxt, WS_CHILD | WS_VISIBLE | SS_LEFT | SS_ENDELLIPSIS, 0, 0, 10, 10, hWnd, (HMENU)IDC_SEARCH_FOLDER_LBL, GetModuleHandle(NULL), NULL);
            g_hBtnFolder = CreateWindowExW(0, L"BUTTON", L"Klasör Seç", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0, 0, 10, 10, hWnd, (HMENU)IDC_SEARCH_FOLDER_BTN, GetModuleHandle(NULL), NULL);

            g_hListResults = CreateWindowExW(0, L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS | LBS_NOINTEGRALHEIGHT, 0, 0, 10, 10, hWnd, (HMENU)IDC_SEARCH_LIST, GetModuleHandle(NULL), NULL);
            SendMessageW(g_hListResults, LB_SETITEMHEIGHT, 0, MulDiv(60, dpi, 96));

            g_hLblStatus = CreateWindowExW(0, L"STATIC", L"Şarkı veya sanatçı adı yazıp Enter'a basın.", WS_CHILD | WS_VISIBLE | SS_LEFT | SS_ENDELLIPSIS, 0, 0, 10, 10, hWnd, (HMENU)IDC_SEARCH_STAT, GetModuleHandle(NULL), NULL);
            g_hBtnDownload = CreateWindowExW(0, L"BUTTON", L"İndir ve Ekle", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0, 0, 10, 10, hWnd, (HMENU)IDC_SEARCH_DOWN, GetModuleHandle(NULL), NULL);

            SendMessageW(g_hEditQuery, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessageW(g_hEditQuery, 0x1501 /* EM_SETCUEBANNER */, TRUE, (LPARAM)L"Şarkı veya sanatçı ara...");
            SendMessageW(g_hLblFolder, WM_SETFONT, (WPARAM)hFontSmall, TRUE);
            SendMessageW(g_hLblStatus, WM_SETFONT, (WPARAM)hFontSmall, TRUE);

            SetWindowSubclass(g_hEditQuery, SearchEditSubclassProc, 1, (DWORD_PTR)hWnd);

            EnableWindow(g_hBtnDownload, FALSE);
            LayoutSearchControls(hWnd);
            SetFocus(g_hEditQuery);
            return 0;
        }

        case WM_SIZE: {
            LayoutSearchControls(hWnd);
            InvalidateRect(hWnd, NULL, FALSE);
            return 0;
        }

        case WM_GETMINMAXINFO: {
            MINMAXINFO *mmi = (MINMAXINFO*)lParam;
            int dpi = GetSearchWindowDpi(hWnd);
            mmi->ptMinTrackSize.x = MulDiv(680, dpi, 96);
            mmi->ptMinTrackSize.y = MulDiv(460, dpi, 96);
            return 0;
        }

        case WM_ERASEBKGND:
            return 1;

        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORDLG: {
            HDC hdc = (HDC)wParam;
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(161, 161, 170));
            return (LRESULT)hBrBg;
        }

        case WM_CTLCOLOREDIT: {
            HDC hdc = (HDC)wParam;
            SetBkMode(hdc, OPAQUE);
            SetBkColor(hdc, COLOR_SEARCH_BOX);
            SetTextColor(hdc, RGB(255, 255, 255));
            return (LRESULT)hBrBox;
        }

        case WM_CTLCOLORLISTBOX: {
            HDC hdc = (HDC)wParam;
            SetBkMode(hdc, OPAQUE);
            SetBkColor(hdc, COLOR_SEARCH_LIST);
            SetTextColor(hdc, RGB(255, 255, 255));
            return (LRESULT)hBrList;
        }

        case WM_DRAWITEM: {
            DRAWITEMSTRUCT *dis = (DRAWITEMSTRUCT*)lParam;
            if (dis->CtlType == ODT_BUTTON) {
                BOOL isPressed = (dis->itemState & ODS_SELECTED) != 0;
                BOOL isDisabled = (dis->itemState & ODS_DISABLED) != 0;

                FillRect(dis->hDC, &dis->rcItem, (dis->CtlID == IDC_SEARCH_DOWN) ? hBrList : hBrBg);
                if (dis->CtlID == IDC_SEARCH_DOWN) {
                    HGDIOBJ oldBr = SelectObject(dis->hDC, GetStockObject(DC_BRUSH));
                    SetDCBrushColor(dis->hDC, COLOR_SEARCH_CARD);
                    PatBlt(dis->hDC, dis->rcItem.left, dis->rcItem.top, dis->rcItem.right - dis->rcItem.left, dis->rcItem.bottom - dis->rcItem.top, PATCOPY);
                    SelectObject(dis->hDC, oldBr);

                    COLORREF bg = isDisabled ? RGB(32, 32, 36) : (isPressed ? COLOR_ACCENT_PURPLE : COLOR_ACCENT_ORANGE);
                    COLORREF txtCol = isDisabled ? RGB(145, 145, 155) : RGB(5, 5, 5);
                    RECT rcBtn = dis->rcItem;
                    if (isPressed) InflateRect(&rcBtn, -1, -1);
                    DrawFastCard(dis->hDC, &rcBtn, bg, bg, 10);
                    SetBkMode(dis->hDC, TRANSPARENT);
                    SelectObject(dis->hDC, hFontBold);
                    SetTextColor(dis->hDC, txtCol);

                    wchar_t btnTxt[64];
                    if (g_isDownloading) {
                        int p = g_downloadPct < 0 ? 0 : g_downloadPct;
                        _snwprintf(btnTxt, 63, L"İndiriliyor • %%%d", p);
                        btnTxt[63] = L'\0';
                    } else {
                        wcscpy(btnTxt, L"İndir ve Listeye Ekle");
                    }
                    DrawTextW(dis->hDC, btnTxt, -1, &rcBtn, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                    return TRUE;
                }
                else if (dis->CtlID == IDC_SEARCH_BTN) {
                    COLORREF bg = isDisabled ? RGB(26, 26, 30) : (isPressed ? COLOR_ACCENT_PURPLE : COLOR_ACCENT_ORANGE);
                    COLORREF txtCol = isDisabled ? RGB(100, 100, 108) : RGB(5, 5, 5);
                    RECT rcBtn = dis->rcItem;
                    if (isPressed) InflateRect(&rcBtn, -1, -1);
                    DrawFastCard(dis->hDC, &rcBtn, bg, bg, 10);
                    SetBkMode(dis->hDC, TRANSPARENT);
                    SelectObject(dis->hDC, hFontBold);
                    SetTextColor(dis->hDC, txtCol);
                    DrawTextW(dis->hDC, g_isSearching ? L"Aranıyor" : L"Ara", -1, &rcBtn, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                    return TRUE;
                }
                else if (dis->CtlID == IDC_SEARCH_FOLDER_BTN) {
                    COLORREF bg = isPressed ? RGB(38, 38, 38) : RGB(24, 24, 24);
                    DrawFastCard(dis->hDC, &dis->rcItem, bg, COLOR_SEARCH_BORDER, 8);
                    SetBkMode(dis->hDC, TRANSPARENT);
                    SelectObject(dis->hDC, hFontSmall);
                    SetTextColor(dis->hDC, RGB(240, 240, 245));
                    DrawTextW(dis->hDC, L"Klasör Seç", -1, &dis->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                    return TRUE;
                }
            }
            else if (dis->CtlType == ODT_LISTBOX && dis->itemID != (UINT)-1) {
                int idx = (int)dis->itemID;
                BOOL isSel = (dis->itemState & ODS_SELECTED) != 0;

                FillRect(dis->hDC, &dis->rcItem, hBrList);

                COLORREF bg = isSel ? RGB(28, 18, 30) : COLOR_SEARCH_LIST;
                RECT rcRow = dis->rcItem;
                InflateRect(&rcRow, -4, -2);
                if (isSel) {
                    DrawFastCard(dis->hDC, &rcRow, bg, COLOR_ACCENT_PINK, 8);
                }

                if (g_results && idx < g_resultCount) {
                    SearchItem *si = &g_results[idx];
                    int dpi = GetSearchWindowDpi(hWnd);
                    int rowH = rcRow.bottom - rcRow.top;

                    if (isSel) {
                        RECT rcPill = { rcRow.left + 3, rcRow.top + MulDiv(10, dpi, 96), rcRow.left + 6, rcRow.bottom - MulDiv(10, dpi, 96) };
                        DrawFastCard(dis->hDC, &rcPill, COLOR_ACCENT_ORANGE, COLOR_ACCENT_ORANGE, 2);
                    }

                    int thumbW = MulDiv(78, dpi, 96);
                    int thumbH = MulDiv(44, dpi, 96);
                    int thumbX = rcRow.left + MulDiv(10, dpi, 96);
                    int thumbY = rcRow.top + (rowH - thumbH) / 2;
                    RECT rcThumb = { thumbX, thumbY, thumbX + thumbW, thumbY + thumbH };

                    DrawFastCard(dis->hDC, &rcThumb, RGB(24, 24, 24), COLOR_SEARCH_BORDER, 6);

                    if (si->pThumb) {
                        OLE_XSIZE_HIMETRIC cx;
                        OLE_YSIZE_HIMETRIC cy;
                        si->pThumb->lpVtbl->get_Width(si->pThumb, &cx);
                        si->pThumb->lpVtbl->get_Height(si->pThumb, &cy);
                        int oldMode = SetStretchBltMode(dis->hDC, HALFTONE);
                        SetBrushOrgEx(dis->hDC, 0, 0, NULL);
                        si->pThumb->lpVtbl->Render(si->pThumb, dis->hDC, rcThumb.left + 1, rcThumb.top + 1, thumbW - 2, thumbH - 2, 0, cy, cx, -cy, NULL);
                        SetStretchBltMode(dis->hDC, oldMode);
                    } else {
                        SetBkMode(dis->hDC, TRANSPARENT);
                        SelectObject(dis->hDC, hFontIcon);
                        SetTextColor(dis->hDC, RGB(113, 113, 122));
                        DrawTextW(dis->hDC, L"\uE768", -1, &rcThumb, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                    }

                    if (si->duration > 0) {
                        wchar_t durBuf[16];
                        _snwprintf(durBuf, 15, L"%02d:%02d", si->duration / 60, si->duration % 60);
                        durBuf[15] = L'\0';
                        int badgeW = MulDiv(38, dpi, 96);
                        int badgeH = MulDiv(15, dpi, 96);
                        RECT rcDurBadge = { rcThumb.right - badgeW - 3, rcThumb.bottom - badgeH - 3, rcThumb.right - 3, rcThumb.bottom - 3 };
                        DrawFastCard(dis->hDC, &rcDurBadge, RGB(16, 16, 16), COLOR_SEARCH_BORDER, 4);
                        SetBkMode(dis->hDC, TRANSPARENT);
                        SelectObject(dis->hDC, hFontSmall);
                        SetTextColor(dis->hDC, RGB(245, 245, 248));
                        DrawTextW(dis->hDC, durBuf, -1, &rcDurBadge, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                    }

                    int textLeft = rcThumb.right + MulDiv(12, dpi, 96);
                    int textRight = rcRow.right - MulDiv(10, dpi, 96);
                    int midY = rcRow.top + rowH / 2;
                    RECT rcTitle = { textLeft, rcRow.top + MulDiv(6, dpi, 96), textRight, midY + MulDiv(1, dpi, 96) };
                    RECT rcSub   = { textLeft, midY + MulDiv(1, dpi, 96), textRight, rcRow.bottom - MulDiv(6, dpi, 96) };

                    SetBkMode(dis->hDC, TRANSPARENT);
                    SelectObject(dis->hDC, hFontBold);
                    SetTextColor(dis->hDC, isSel ? RGB(255, 255, 255) : RGB(238, 238, 242));
                    DrawTextW(dis->hDC, si->cleanTitle, -1, &rcTitle, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

                    SelectObject(dis->hDC, hFontSmall);
                    SetTextColor(dis->hDC, isSel ? COLOR_ACCENT_PINK : RGB(161, 161, 170));
                    DrawTextW(dis->hDC, si->uploader, -1, &rcSub, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                }
                return TRUE;
            }
            break;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC screenDC = BeginPaint(hWnd, &ps);

            RECT rcClient;
            GetClientRect(hWnd, &rcClient);
            int clientW = rcClient.right - rcClient.left;
            int clientH = rcClient.bottom - rcClient.top;
            if (clientW <= 0 || clientH <= 0) {
                EndPaint(hWnd, &ps);
                return 0;
            }

            if (!s_memDC || !s_memBmp || s_memW != clientW || s_memH != clientH) {
                if (s_memDC) {
                    if (s_oldBmp) SelectObject(s_memDC, s_oldBmp);
                    if (s_memBmp) DeleteObject(s_memBmp);
                    DeleteDC(s_memDC);
                }
                s_memDC  = CreateCompatibleDC(screenDC);
                s_memBmp = CreateCompatibleBitmap(screenDC, clientW, clientH);
                s_oldBmp = (HBITMAP)SelectObject(s_memDC, s_memBmp);
                s_memW   = clientW;
                s_memH   = clientH;
            }

            HDC hdc = s_memDC;
            FillRect(hdc, &rcClient, hBrBg);

            int dpi = GetSearchWindowDpi(hWnd);
            int pad = MulDiv(16, dpi, 96);
            int gap = MulDiv(14, dpi, 96);

            int cardW = MulDiv(280, dpi, 96);
            if (cardW > clientW / 2) cardW = clientW / 2;
            if (cardW < MulDiv(240, dpi, 96)) cardW = MulDiv(240, dpi, 96);

            int cardRight  = clientW - pad;
            int cardLeft   = cardRight - cardW;
            int cardTop    = pad;
            int cardBottom = clientH - pad;

            int leftX = pad;
            int leftW = cardLeft - gap - leftX;
            if (leftW < MulDiv(260, dpi, 96)) leftW = MulDiv(260, dpi, 96);

            int btnSearchW = MulDiv(92, dpi, 96);
            int searchH    = MulDiv(36, dpi, 96);
            int editW      = leftW - btnSearchW - MulDiv(8, dpi, 96);

            RECT rcEditBorder = { leftX, pad, leftX + editW, pad + searchH };
            DrawFastCard(hdc, &rcEditBorder, COLOR_SEARCH_BOX, COLOR_SEARCH_BORDER, 10);

            int row2Y = pad + searchH + MulDiv(8, dpi, 96);
            int row2H = MulDiv(28, dpi, 96);
            int statH = MulDiv(22, dpi, 96);
            int statY = clientH - pad - statH;
            int listY = row2Y + row2H + MulDiv(8, dpi, 96);
            int listH = statY - listY - MulDiv(8, dpi, 96);

            RECT rcListBorder = { leftX - 1, listY - 1, leftX + leftW + 1, listY + listH + 1 };
            DrawFastCard(hdc, &rcListBorder, COLOR_SEARCH_LIST, COLOR_SEARCH_BORDER, 10);

            RECT rcCard = { cardLeft, cardTop, cardRight, cardBottom };
            DrawFastCard(hdc, &rcCard, COLOR_SEARCH_CARD, COLOR_SEARCH_BORDER, 12);

            int cardPad = MulDiv(14, dpi, 96);
            int thumbW  = cardW - (cardPad * 2);
            int thumbH  = (thumbW * 9) / 16;
            RECT rcThumb = { cardLeft + cardPad, cardTop + cardPad, cardLeft + cardPad + thumbW, cardTop + cardPad + thumbH };
            DrawFastCard(hdc, &rcThumb, RGB(24, 24, 24), COLOR_SEARCH_BORDER, 8);

            int sel = (int)SendMessageW(g_hListResults, LB_GETCURSEL, 0, 0);
            IPicture *curPic = (g_results && sel >= 0 && sel < g_resultCount) ? g_results[sel].pThumb : NULL;
            if (curPic) {
                OLE_XSIZE_HIMETRIC cx;
                OLE_YSIZE_HIMETRIC cy;
                curPic->lpVtbl->get_Width(curPic, &cx);
                curPic->lpVtbl->get_Height(curPic, &cy);
                int oldMode = SetStretchBltMode(hdc, HALFTONE);
                SetBrushOrgEx(hdc, 0, 0, NULL);
                curPic->lpVtbl->Render(curPic, hdc, rcThumb.left + 1, rcThumb.top + 1, thumbW - 2, thumbH - 2, 0, cy, cx, -cy, NULL);
                SetStretchBltMode(hdc, oldMode);
            } else {
                SetBkMode(hdc, TRANSPARENT);
                SelectObject(hdc, hFontSmall);
                SetTextColor(hdc, RGB(113, 113, 122));
                const wchar_t *holderTxt = (sel >= 0) ? L"Kapak yükleniyor..." : L"Önizleme için şarkı arayın";
                DrawTextW(hdc, holderTxt, -1, &rcThumb, DT_CENTER | DT_VCENTER | DT_WORDBREAK);
            }

            if (g_results && sel >= 0 && sel < g_resultCount) {
                SetBkMode(hdc, TRANSPARENT);
                int metaY  = rcThumb.bottom + MulDiv(14, dpi, 96);
                int titleH = MulDiv(46, dpi, 96);

                RECT rcTitle = { rcThumb.left, metaY, rcThumb.right, metaY + titleH };
                SelectObject(hdc, hFontBold);
                SetTextColor(hdc, RGB(255, 255, 255));
                DrawTextW(hdc, g_results[sel].cleanTitle, -1, &rcTitle, DT_LEFT | DT_TOP | DT_WORDBREAK | DT_END_ELLIPSIS);

                int artistY = metaY + titleH + MulDiv(4, dpi, 96);
                int artistH = MulDiv(22, dpi, 96);
                RECT rcArtist = { rcThumb.left, artistY, rcThumb.right, artistY + artistH };
                SelectObject(hdc, hFont);
                SetTextColor(hdc, RGB(161, 161, 170));
                DrawTextW(hdc, g_results[sel].uploader, -1, &rcArtist, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS);

                int durY = artistY + artistH + MulDiv(4, dpi, 96);
                int durH = MulDiv(20, dpi, 96);
                RECT rcMeta = { rcThumb.left, durY, rcThumb.right, durY + durH };
                SelectObject(hdc, hFontSmall);
                SetTextColor(hdc, COLOR_ACCENT_ORANGE);
                wchar_t metaTxt[128];
                int m = g_results[sel].duration / 60;
                int s = g_results[sel].duration % 60;
                _snwprintf(metaTxt, 127, L"%02d:%02d  •  %s", m, s, g_results[sel].source);
                metaTxt[127] = L'\0';
                DrawTextW(hdc, metaTxt, -1, &rcMeta, DT_LEFT | DT_TOP | DT_SINGLELINE);
            }

            // Canlı İndirme Yüzdesi ve Sunset-Neon İlerleme Çubuğu
            if (g_isDownloading || g_downloadPct >= 0) {
                int btnDownH = MulDiv(44, dpi, 96);
                int btnDownY = cardBottom - cardPad - btnDownH;
                int progBoxH = MulDiv(46, dpi, 96);
                int progBoxY = btnDownY - MulDiv(10, dpi, 96) - progBoxH;

                RECT rcProgBox = { rcThumb.left, progBoxY, rcThumb.right, progBoxY + progBoxH };
                DrawFastCard(hdc, &rcProgBox, RGB(22, 16, 26), g_isDownloading ? COLOR_ACCENT_PINK : COLOR_SEARCH_BORDER, 10);

                int shownPct = g_downloadPct < 0 ? 0 : g_downloadPct;
                RECT rcPLbl = { rcProgBox.left + MulDiv(10, dpi, 96), rcProgBox.top + MulDiv(6, dpi, 96), rcProgBox.right - MulDiv(54, dpi, 96), rcProgBox.top + MulDiv(24, dpi, 96) };
                RECT rcPPct = { rcProgBox.right - MulDiv(54, dpi, 96), rcProgBox.top + MulDiv(6, dpi, 96), rcProgBox.right - MulDiv(10, dpi, 96), rcProgBox.top + MulDiv(24, dpi, 96) };

                SetBkMode(hdc, TRANSPARENT);
                SelectObject(hdc, hFontSmall);
                SetTextColor(hdc, RGB(235, 235, 240));
                DrawTextW(hdc, g_isDownloading ? L"İndirme durumu" : L"✓ İndirme tamamlandı", -1, &rcPLbl, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

                wchar_t pctTxt[16];
                _snwprintf(pctTxt, 15, L"%%%d", shownPct);
                pctTxt[15] = L'\0';
                SelectObject(hdc, hFontBold);
                SetTextColor(hdc, COLOR_ACCENT_ORANGE);
                DrawTextW(hdc, pctTxt, -1, &rcPPct, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

                RECT rcPTrack = { rcProgBox.left + MulDiv(10, dpi, 96), rcProgBox.bottom - MulDiv(14, dpi, 96), rcProgBox.right - MulDiv(10, dpi, 96), rcProgBox.bottom - MulDiv(7, dpi, 96) };
                DrawNeonProgressBarGdi(hdc, &rcPTrack, shownPct);
            }

            BitBlt(screenDC, 0, 0, clientW, clientH, hdc, 0, 0, SRCCOPY);
            EndPaint(hWnd, &ps);
            return 0;
        }

        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            int wmEvent = HIWORD(wParam);

            if (wmId == IDC_SEARCH_FOLDER_BTN) {
                ChooseFolderDialog(hWnd);
            }
            else if (wmId == IDC_SEARCH_BTN) {
                if (g_isSearching || g_isDownloading) break;

                wchar_t query[512] = {0};
                GetWindowTextW(g_hEditQuery, query, 511);
                if (wcslen(query) == 0) break;

                FreeSearchResults();
                InterlockedIncrement(&g_searchGen);

                g_isSearching = TRUE;
                g_downloadPct = -1;
                EnableWindow(g_hBtnSearch, FALSE);
                EnableWindow(g_hBtnDownload, FALSE);
                SendMessageW(g_hListResults, LB_RESETCONTENT, 0, 0);
                SetWindowTextW(g_hLblStatus, L"YouTube taranıyor...");
                InvalidateRect(hWnd, NULL, FALSE);

                wchar_t *qCopy = _wcsdup(query);
                HANDLE hT = CreateThread(NULL, 64 * 1024, SearchThread, qCopy, STACK_SIZE_PARAM_IS_A_RESERVATION, NULL);
                if (hT) CloseHandle(hT);
            }
            else if (wmId == IDC_SEARCH_LIST) {
                if (wmEvent == LBN_SELCHANGE) {
                    int sel = (int)SendMessageW(g_hListResults, LB_GETCURSEL, 0, 0);
                    BOOL hasSel = (sel >= 0 && sel < g_resultCount);
                    if (!g_isDownloading) g_downloadPct = -1;
                    EnableWindow(g_hBtnDownload, hasSel && !g_isDownloading);
                    InvalidateRect(hWnd, NULL, FALSE);
                } else if (wmEvent == LBN_DBLCLK) {
                    SendMessageW(hWnd, WM_COMMAND, MAKEWPARAM(IDC_SEARCH_DOWN, BN_CLICKED), (LPARAM)g_hBtnDownload);
                }
            }
            else if (wmId == IDC_SEARCH_DOWN) {
                int sel = (int)SendMessageW(g_hListResults, LB_GETCURSEL, 0, 0);
                if (!g_results || sel < 0 || sel >= g_resultCount || g_isDownloading) break;

                g_isDownloading = TRUE;
                g_downloadPct = 0;
                wcsncpy(g_activeDlTitle, g_results[sel].cleanTitle, 255);
                g_activeDlTitle[255] = L'\0';

                EnableWindow(g_hBtnDownload, FALSE);
                EnableWindow(g_hBtnSearch, FALSE);
                InvalidateRect(g_hBtnDownload, NULL, FALSE);
                InvalidateRect(hWnd, NULL, FALSE);

                wchar_t statusTxt[256];
                _snwprintf(statusTxt, 255, L"İndiriliyor (%%0): %s", g_results[sel].cleanTitle);
                statusTxt[255] = L'\0';
                SetWindowTextW(g_hLblStatus, statusTxt);

                if (IsWindow(g_hParent)) {
                    PostMessageW(g_hParent, WM_APP_SINGLE_DL_PROGRESS, 0, 0);
                }

                DownloadParam *dp = (DownloadParam*)malloc(sizeof(DownloadParam));
                wcsncpy(dp->url, g_results[sel].url, 511);
                dp->url[511] = L'\0';
                wcsncpy(dp->folder, g_downloadFolder, MAX_PATH - 1);
                dp->folder[MAX_PATH - 1] = L'\0';

                HANDLE hT = CreateThread(NULL, 64 * 1024, DownloadThread, dp, STACK_SIZE_PARAM_IS_A_RESERVATION, NULL);
                if (hT) CloseHandle(hT);
            }
            break;
        }

        case WM_APP_DOWN_PROGRESS: {
            int pct = (int)wParam;
            wchar_t statusTxt[300];
            _snwprintf(statusTxt, 299, L"İndiriliyor (%%%d): %s", pct, g_activeDlTitle);
            statusTxt[299] = L'\0';
            SetWindowTextW(g_hLblStatus, statusTxt);
            InvalidateRect(g_hBtnDownload, NULL, FALSE);
            InvalidateRect(hWnd, NULL, FALSE);
            break;
        }

        case WM_APP_SEARCH_DONE: {
            EnableWindow(g_hBtnSearch, TRUE);
            InvalidateRect(g_hBtnSearch, NULL, FALSE);
            SendMessageW(g_hListResults, LB_RESETCONTENT, 0, 0);

            if (!g_results || g_resultCount == 0) {
                SetWindowTextW(g_hLblStatus, L"Sonuç bulunamadı.");
            } else {
                for (int i = 0; i < g_resultCount; i++) {
                    SendMessageW(g_hListResults, LB_ADDSTRING, 0, (LPARAM)g_results[i].cleanTitle);
                }
                wchar_t stat[128];
                _snwprintf(stat, 127, L"%d sonuç bulundu • Çift tıklayarak hemen indirebilirsiniz", g_resultCount);
                stat[127] = L'\0';
                SetWindowTextW(g_hLblStatus, stat);

                SendMessageW(g_hListResults, LB_SETCURSEL, 0, 0);
                EnableWindow(g_hBtnDownload, !g_isDownloading);

                LONG curGen = g_searchGen;
                for (int i = 0; i < g_resultCount; i++) {
                    SingleThumbTask *task = (SingleThumbTask*)malloc(sizeof(SingleThumbTask));
                    task->itemIndex = i;
                    task->searchGen = curGen;
                    wcsncpy(task->id, g_results[i].id, 63);
                    task->id[63] = L'\0';
                    HANDLE hT = CreateThread(NULL, 64 * 1024, SingleThumbThread, task, STACK_SIZE_PARAM_IS_A_RESERVATION, NULL);
                    if (hT) CloseHandle(hT);
                }
            }
            InvalidateRect(hWnd, NULL, FALSE);
            break;
        }

        case WM_APP_THUMB_READY: {
            int readyIdx = (int)wParam;
            if (IsWindow(g_hListResults)) {
                InvalidateRect(g_hListResults, NULL, FALSE);
            }
            int sel = (int)SendMessageW(g_hListResults, LB_GETCURSEL, 0, 0);
            if (sel == readyIdx) {
                InvalidateRect(hWnd, NULL, FALSE);
            }
            break;
        }

        case WM_APP_DOWN_DONE: {
            EnableWindow(g_hBtnSearch, TRUE);
            int sel = (int)SendMessageW(g_hListResults, LB_GETCURSEL, 0, 0);
            EnableWindow(g_hBtnDownload, (sel >= 0));
            InvalidateRect(g_hBtnDownload, NULL, FALSE);

            BOOL success = (BOOL)wParam;
            if (success) {
                g_downloadPct = 100;
                SetWindowTextW(g_hLblStatus, L"✓ Tamamlandı (%100)! Şarkı listenin en üstüne eklendi.");
                if (IsWindow(g_hParent)) {
                    PostMessageW(g_hParent, WM_APP_FILE_DOWNLOADED, 0, 0);
                }
            } else {
                g_downloadPct = -1;
                SetWindowTextW(g_hLblStatus, L"İndirme başarısız oldu. Tekrar deneyin.");
            }
            InvalidateRect(hWnd, NULL, FALSE);
            break;
        }

        case WM_DESTROY: {
            InterlockedIncrement(&g_searchGen);
            RemoveWindowSubclass(g_hEditQuery, SearchEditSubclassProc, 1);
            FreeSearchResults();
            if (g_results) {
                free(g_results);
                g_results = NULL;
            }
            if (s_memDC) {
                if (s_oldBmp) SelectObject(s_memDC, s_oldBmp);
                if (s_memBmp) DeleteObject(s_memBmp);
                DeleteDC(s_memDC);
                s_memDC = NULL; s_memBmp = NULL; s_oldBmp = NULL;
                s_memW = 0; s_memH = 0;
            }
            if (hBrBg)   { DeleteObject(hBrBg);   hBrBg = NULL; }
            if (hBrBox)  { DeleteObject(hBrBox);  hBrBox = NULL; }
            if (hBrList) { DeleteObject(hBrList); hBrList = NULL; }
            if (hFont)      { DeleteObject(hFont);      hFont = NULL; }
            if (hFontBold)  { DeleteObject(hFontBold);  hFontBold = NULL; }
            if (hFontSmall) { DeleteObject(hFontSmall); hFontSmall = NULL; }
            if (hFontIcon)  { DeleteObject(hFontIcon);  hFontIcon = NULL; }
            g_hDlg = NULL;
            return 0;
        }
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

void Search_ShowDialog(HWND hParentWnd, const wchar_t *downloadFolder) {
    if (g_hDlg && IsWindow(g_hDlg)) {
        if (IsIconic(g_hDlg)) ShowWindow(g_hDlg, SW_RESTORE);
        SetForegroundWindow(g_hDlg);
        return;
    }

    g_hParent = hParentWnd;

    if (downloadFolder && downloadFolder[0]) {
        wcsncpy(g_downloadFolder, downloadFolder, MAX_PATH - 1);
        g_downloadFolder[MAX_PATH - 1] = L'\0';
    } else {
        GetCurrentDirectoryW(MAX_PATH, g_downloadFolder);
    }

    HINSTANCE hInst = GetModuleHandleW(NULL);
    static BOOL s_classRegistered = FALSE;
    static HICON s_hMainIcon = NULL;
    static HICON s_hSmallIcon = NULL;

    if (!s_classRegistered) {
        s_hMainIcon = LoadIconW(hInst, MAKEINTRESOURCEW(1));
        s_hSmallIcon = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(1), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);

        WNDCLASSEXW wc;
        memset(&wc, 0, sizeof(wc));
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = SearchWndProc;
        wc.hInstance = hInst;
        wc.hIcon = s_hMainIcon ? s_hMainIcon : LoadIcon(NULL, IDI_APPLICATION);
        wc.hIconSm = s_hSmallIcon ? s_hSmallIcon : wc.hIcon;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = NULL;
        wc.lpszClassName = L"AurnsSearchWindow";
        RegisterClassExW(&wc);
        s_classRegistered = TRUE;
    }

    RECT rcParent;
    GetWindowRect(hParentWnd, &rcParent);
    int x = rcParent.left + 24;
    int y = rcParent.top + 24;

    int screenDpi = GetSearchWindowDpi(hParentWnd);
    int dlgW = MulDiv(820, screenDpi, 96);
    int dlgH = MulDiv(530, screenDpi, 96);

    g_hDlg = CreateWindowExW(
        0,
        L"AurnsSearchWindow",
        L"\u039BURNS \u2022 M\u00FCzik Ara ve \u0130ndir",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_VISIBLE,
        x, y, dlgW, dlgH,
        hParentWnd, NULL, hInst, NULL
    );

    if (s_hMainIcon) SendMessageW(g_hDlg, WM_SETICON, ICON_BIG, (LPARAM)s_hMainIcon);
    if (s_hSmallIcon) SendMessageW(g_hDlg, WM_SETICON, ICON_SMALL, (LPARAM)s_hSmallIcon);
}

void Search_ShowDialogWithQuery(HWND hParentWnd, const wchar_t *downloadFolder, const wchar_t *initialQuery) {
    Search_ShowDialog(hParentWnd, downloadFolder);
    if (initialQuery && initialQuery[0] && g_hDlg && IsWindow(g_hDlg) && g_hEditQuery) {
        SetWindowTextW(g_hEditQuery, initialQuery);
        PostMessageW(g_hDlg, WM_COMMAND, MAKEWPARAM(IDC_SEARCH_BTN, BN_CLICKED), (LPARAM)g_hBtnSearch);
    }
}
