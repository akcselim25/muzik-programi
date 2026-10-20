#include <windows.h>
#include <commctrl.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <stdio.h>
#include <stdlib.h>
#include <olectl.h>
#include <dwmapi.h>
#include "search.h"
#include "playlist.h"

// Spotify Green & Apple Minimalist Dark Mode Renk Paleti
#define SPOTIFY_GREEN       RGB(29, 185, 84)    // #1DB954
#define SPOTIFY_GREEN_HOV   RGB(30, 215, 96)    // #1ED760
#define SPOTIFY_GREEN_DARK  RGB(22, 156, 70)    // #169C46

#define COLOR_SEARCH_BG     RGB(18, 18, 18)     // #121212 (Spotify Dark Base)
#define COLOR_SEARCH_CARD   RGB(24, 24, 26)     // #18181A (Apple Rounded Hero Card)
#define COLOR_SEARCH_BOX    RGB(30, 30, 34)     // #1E1E22 (Search Box Background)
#define COLOR_SEARCH_LIST   RGB(20, 20, 22)     // #141416 (ListBox Background)
#define COLOR_SEARCH_BORDER RGB(44, 44, 50)     // #2C2C32 (Hairline Separators)

typedef struct {
    wchar_t id[64];
    wchar_t rawTitle[256];
    wchar_t cleanTitle[256];
    wchar_t uploader[128];
    wchar_t url[512];
    wchar_t source[32]; // "YouTube"
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
static SearchItem g_results[50];
static int g_resultCount = 0;
static volatile BOOL g_isSearching = FALSE;
static volatile BOOL g_isDownloading = FALSE;
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

static void FreeSearchResults(void) {
    for (int i = 0; i < g_resultCount; i++) {
        if (g_results[i].pThumb) {
            g_results[i].pThumb->lpVtbl->Release(g_results[i].pThumb);
            g_results[i].pThumb = NULL;
        }
    }
    g_resultCount = 0;
}

static DWORD WINAPI SearchThread(LPVOID param) {
    wchar_t *query = (wchar_t*)param;
    g_resultCount = 0;

    // Şimşek hızında arama: 10 sonuç, hafif ve doğrudan YouTube video bilgisi
    wchar_t cmdLine[2048];
    wsprintfW(cmdLine, L"yt-dlp.exe --encoding utf-8 --no-warnings --flat-playlist --print \"%%(id)s\t%%(title)s\t%%(duration)s\t%%(uploader)s\t%%(webpage_url)s\" \"ytsearch10:%s\"", query);
    free(query);

    HANDLE hRead, hWrite;
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) {
        g_isSearching = FALSE;
        PostMessageW(g_hDlg, WM_APP_SEARCH_DONE, 0, 0);
        return 0;
    }
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si = { sizeof(si) };
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
                        if (tab1 && g_resultCount < 50) {
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
                                    item->pThumb = NULL;
                                    MultiByteToWideChar(CP_UTF8, 0, lineBuf, -1, item->id, 64);
                                    MultiByteToWideChar(CP_UTF8, 0, tab1 + 1, -1, item->rawTitle, 256);
                                    item->duration = (int)atof(tab2 + 1);
                                    MultiByteToWideChar(CP_UTF8, 0, tab3 + 1, -1, item->uploader, 128);
                                    if (tab4) {
                                        MultiByteToWideChar(CP_UTF8, 0, tab4 + 1, -1, item->url, 512);
                                    } else {
                                        wsprintfW(item->url, L"https://www.youtube.com/watch?v=%s", item->id);
                                    }
                                    wcscpy(item->source, L"YouTube");

                                    // Şarkı adını temizle (Sanatçı - Şarkı Adı)
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
    PostMessageW(g_hDlg, WM_APP_SEARCH_DONE, 0, 0);
    return 0;
}

// Bütün YouTube küçük resimlerini (thumbnails) paralel ve anında indiren iş parçacığı
typedef struct {
    int itemIndex;
    LONG searchGen;
    wchar_t id[64];
} SingleThumbTask;

static DWORD WINAPI SingleThumbThread(LPVOID param) {
    SingleThumbTask *task = (SingleThumbTask*)param;
    OleInitialize(NULL);

    // YouTube 16:9 hızlı küçük resim (mqdefault 320x180, default 120x90)
    wchar_t thumbUrl[256];
    wsprintfW(thumbUrl, L"https://i.ytimg.com/vi/%s/mqdefault.jpg", task->id);

    IPicture *pPic = NULL;
    HRESULT hr = OleLoadPicturePath(thumbUrl, NULL, 0, 0, &IID_IPicture, (void**)&pPic);
    if (FAILED(hr) || !pPic) {
        wsprintfW(thumbUrl, L"https://i.ytimg.com/vi/%s/default.jpg", task->id);
        hr = OleLoadPicturePath(thumbUrl, NULL, 0, 0, &IID_IPicture, (void**)&pPic);
    }

    if (SUCCEEDED(hr) && pPic) {
        if (task->searchGen == g_searchGen && task->itemIndex < g_resultCount && wcscmp(g_results[task->itemIndex].id, task->id) == 0) {
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

static DWORD WINAPI DownloadThread(LPVOID param) {
    DownloadParam *dp = (DownloadParam*)param;

    wchar_t cmdLine[2048];
    // --write-thumbnail ile kapak resmi şarkının yanına kaydedilir
    wsprintfW(cmdLine, L"yt-dlp.exe --no-warnings -f \"ba[ext=m4a]/ba\" --write-thumbnail --no-playlist -o \"%s\\%%(title)s.%%(ext)s\" \"%s\"", dp->folder, dp->url);
    free(dp);

    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = {0};
    BOOL success = FALSE;
    if (CreateProcessW(NULL, cmdLine, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD exitCode = 0;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        success = (exitCode == 0);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    g_isDownloading = FALSE;
    PostMessageW(g_hDlg, WM_APP_DOWN_DONE, (WPARAM)success, 0);
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
            wsprintfW(lbl, L"İndirme Yeri: %s", g_downloadFolder);
            SetWindowTextW(g_hLblFolder, lbl);
        }
        CoTaskMemFree(pidl);
    }
}

static void DrawRoundedCard(HDC hdc, const RECT *rc, COLORREF bg, COLORREF border, int radius) {
    HBRUSH hBr = CreateSolidBrush(bg);
    HPEN hPen = CreatePen(PS_SOLID, 1, border);
    HBRUSH oldB = (HBRUSH)SelectObject(hdc, hBr);
    HPEN oldP = (HPEN)SelectObject(hdc, hPen);
    RoundRect(hdc, rc->left, rc->top, rc->right, rc->bottom, radius, radius);
    SelectObject(hdc, oldB);
    SelectObject(hdc, oldP);
    DeleteObject(hBr);
    DeleteObject(hPen);
}

const wchar_t* Search_GetDownloadFolder(void) {
    return g_downloadFolder;
}

static int GetSearchWindowDpi(HWND hWnd) {
    typedef UINT(WINAPI *GetDpiForWindowProc)(HWND);
    GetDpiForWindowProc pGetDpi = (GetDpiForWindowProc)GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow");
    int dpi = pGetDpi ? pGetDpi(hWnd) : 0;
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

    // Sağ Hero Kart Genişliği: Pencereye ve DPI'a göre orantılı
    int cardW = MulDiv(280, dpi, 96);
    if (cardW > clientW / 2) cardW = clientW / 2;
    if (cardW < MulDiv(240, dpi, 96)) cardW = MulDiv(240, dpi, 96);

    int cardRight  = clientW - pad;
    int cardLeft   = cardRight - cardW;
    int cardTop    = pad;
    int cardBottom = clientH - pad;

    // Sol Alan Genişliği
    int leftX = pad;
    int leftW = cardLeft - gap - leftX;
    if (leftW < MulDiv(260, dpi, 96)) leftW = MulDiv(260, dpi, 96);

    // Üst Arama Satırı
    int btnSearchW = MulDiv(90, dpi, 96);
    int searchH    = MulDiv(34, dpi, 96);
    int editW      = leftW - btnSearchW - MulDiv(8, dpi, 96);

    if (g_hEditQuery) MoveWindow(g_hEditQuery, leftX + 4, pad + 4, editW - 8, searchH - 8, TRUE);
    if (g_hBtnSearch) MoveWindow(g_hBtnSearch, leftX + editW, pad, btnSearchW, searchH, TRUE);

    // İkinci Satır: Klasör ve Klasör Seç butonu
    int row2Y      = pad + searchH + MulDiv(8, dpi, 96);
    int btnFolderW = MulDiv(110, dpi, 96);
    int row2H      = MulDiv(28, dpi, 96);
    int folderLblW = leftW - btnFolderW - MulDiv(8, dpi, 96);

    if (g_hLblFolder) MoveWindow(g_hLblFolder, leftX + 2, row2Y + 4, folderLblW - 4, row2H - 4, TRUE);
    if (g_hBtnFolder) MoveWindow(g_hBtnFolder, leftX + folderLblW, row2Y, btnFolderW, row2H, TRUE);

    // Alt Durum Çubuğu
    int statH = MulDiv(22, dpi, 96);
    int statY = clientH - pad - statH;
    if (g_hLblStatus) MoveWindow(g_hLblStatus, leftX + 2, statY, leftW - 4, statH, TRUE);

    // Sonuç Listesi (Listbox)
    int listY = row2Y + row2H + MulDiv(8, dpi, 96);
    int listH = statY - listY - MulDiv(8, dpi, 96);
    if (g_hListResults) MoveWindow(g_hListResults, leftX, listY, leftW, listH, TRUE);

    // Sağ Hero Kart içindeki İndir Butonu
    int cardPad  = MulDiv(14, dpi, 96);
    int btnDownH = MulDiv(46, dpi, 96);
    int btnDownW = cardW - (cardPad * 2);
    int btnDownX = cardLeft + cardPad;
    int btnDownY = cardBottom - cardPad - btnDownH;
    if (g_hBtnDownload) MoveWindow(g_hBtnDownload, btnDownX, btnDownY, btnDownW, btnDownH, TRUE);
}

// Panodaki metni (Ctrl+V veya sağ tık ile) temiz ve satır atlamasız yapıştıran fonksiyon
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
                        if (pText[i] == L'\r' || pText[i] == L'\n') {
                            clean[i] = L' ';
                        } else {
                            clean[i] = pText[i];
                        }
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
    HWND hDlg = (HWND)dwRefData;

    if (msg == WM_KEYDOWN) {
        if (wParam == VK_RETURN) {
            if (hDlg && g_hBtnSearch) {
                SendMessageW(hDlg, WM_COMMAND, MAKEWPARAM(IDC_SEARCH_BTN, BN_CLICKED), (LPARAM)g_hBtnSearch);
            }
            return 0;
        }
        // Ctrl+V (Yapıştır)
        if (wParam == 'V' && (GetKeyState(VK_CONTROL) & 0x8000)) {
            DoPaste(hWnd);
            return 0;
        }
        // Ctrl+A (Tümünü Seç)
        if (wParam == 'A' && (GetKeyState(VK_CONTROL) & 0x8000)) {
            SendMessageW(hWnd, EM_SETSEL, 0, -1);
            return 0;
        }
        // Ctrl+C (Kopyala)
        if (wParam == 'C' && (GetKeyState(VK_CONTROL) & 0x8000)) {
            SendMessageW(hWnd, WM_COPY, 0, 0);
            return 0;
        }
        // Ctrl+X (Kes)
        if (wParam == 'X' && (GetKeyState(VK_CONTROL) & 0x8000)) {
            SendMessageW(hWnd, WM_CUT, 0, 0);
            return 0;
        }
    }
    else if (msg == WM_CHAR) {
        // Kontrol tuşlarının Windows Edit kutusunda bip sesi çıkarmasını engelle
        if (wParam == 1 || wParam == 22 || wParam == 3 || wParam == 24) {
            return 0;
        }
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

    switch (msg) {
        case WM_CREATE: {
            g_hDlg = hWnd;
            int dpi = GetSearchWindowDpi(hWnd);

            BOOL useDark = TRUE;
            DwmSetWindowAttribute(hWnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &useDark, sizeof(useDark));
            DwmSetWindowAttribute(hWnd, 19, &useDark, sizeof(useDark));

            hBrBg   = CreateSolidBrush(COLOR_SEARCH_BG);
            hBrBox  = CreateSolidBrush(COLOR_SEARCH_BOX);
            hBrList = CreateSolidBrush(COLOR_SEARCH_LIST);

            int sMain  = -MulDiv(10, dpi, 72);
            int sBold  = -MulDiv(10, dpi, 72);
            int sSmall = -MulDiv(9,  dpi, 72);

            const wchar_t *fontFamily = L"Segoe UI";

            hFont      = CreateFontW(sMain,  0, 0, 0, FW_NORMAL,   FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, fontFamily);
            hFontBold  = CreateFontW(sBold,  0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, fontFamily);
            hFontSmall = CreateFontW(sSmall, 0, 0, 0, FW_NORMAL,   FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, fontFamily);

            g_hEditQuery = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 0, 0, 10, 10, hWnd, (HMENU)IDC_SEARCH_EDIT, GetModuleHandle(NULL), NULL);
            g_hBtnSearch = CreateWindowExW(0, L"BUTTON", L"🔍 Ara", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0, 0, 10, 10, hWnd, (HMENU)IDC_SEARCH_BTN, GetModuleHandle(NULL), NULL);

            wchar_t folderTxt[MAX_PATH + 32];
            wsprintfW(folderTxt, L"İndirme Yeri: %s", g_downloadFolder);
            g_hLblFolder = CreateWindowExW(0, L"STATIC", folderTxt, WS_CHILD | WS_VISIBLE | SS_LEFT | SS_ENDELLIPSIS, 0, 0, 10, 10, hWnd, (HMENU)IDC_SEARCH_FOLDER_LBL, GetModuleHandle(NULL), NULL);
            g_hBtnFolder = CreateWindowExW(0, L"BUTTON", L"Klasör Seç...", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0, 0, 10, 10, hWnd, (HMENU)IDC_SEARCH_FOLDER_BTN, GetModuleHandle(NULL), NULL);

            // LBS_NOINTEGRALHEIGHT beyaz boşluk kalmasını engeller!
            g_hListResults = CreateWindowExW(0, L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS | LBS_NOINTEGRALHEIGHT, 0, 0, 10, 10, hWnd, (HMENU)IDC_SEARCH_LIST, GetModuleHandle(NULL), NULL);
            SendMessageW(g_hListResults, LB_SETITEMHEIGHT, 0, MulDiv(58, dpi, 96));

            g_hLblStatus = CreateWindowExW(0, L"STATIC", L"Şarkı veya sanatçı adı yazıp Ara'ya basın.", WS_CHILD | WS_VISIBLE | SS_LEFT | SS_ENDELLIPSIS, 0, 0, 10, 10, hWnd, (HMENU)IDC_SEARCH_STAT, GetModuleHandle(NULL), NULL);

            g_hBtnDownload = CreateWindowExW(0, L"BUTTON", L"📥  İndir ve Ekle", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0, 0, 10, 10, hWnd, (HMENU)IDC_SEARCH_DOWN, GetModuleHandle(NULL), NULL);

            SendMessageW(g_hEditQuery, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessageW(g_hEditQuery, 0x1501 /* EM_SETCUEBANNER */, TRUE, (LPARAM)L"Şarkı veya sanatçı adı yazın (Ctrl+V ile yapıştırın)...");
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
            InvalidateRect(hWnd, NULL, TRUE);
            return 0;
        }

        case WM_GETMINMAXINFO: {
            MINMAXINFO *mmi = (MINMAXINFO*)lParam;
            int dpi = GetSearchWindowDpi(hWnd);
            mmi->ptMinTrackSize.x = MulDiv(680, dpi, 96);
            mmi->ptMinTrackSize.y = MulDiv(460, dpi, 96);
            return 0;
        }

        case WM_ERASEBKGND: {
            HDC hdc = (HDC)wParam;
            RECT rc;
            GetClientRect(hWnd, &rc);
            FillRect(hdc, &rc, hBrBg);
            return 1;
        }

        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORDLG: {
            HDC hdc = (HDC)wParam;
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(167, 167, 167));
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
                BOOL isPressed = (dis->itemState & ODS_SELECTED);
                BOOL isDisabled = (dis->itemState & ODS_DISABLED);

                if (dis->CtlID == IDC_SEARCH_DOWN) {
                    COLORREF bg, border, txtCol;
                    if (isDisabled) {
                        bg = RGB(36, 36, 40);
                        border = RGB(46, 46, 52);
                        txtCol = RGB(120, 120, 128);
                    } else if (isPressed) {
                        bg = SPOTIFY_GREEN_DARK;
                        border = bg;
                        txtCol = RGB(255, 255, 255);
                    } else {
                        bg = SPOTIFY_GREEN; // İkonik Spotify Yeşili
                        border = bg;
                        txtCol = RGB(255, 255, 255);
                    }
                    DrawRoundedCard(dis->hDC, &dis->rcItem, bg, border, 8);
                    SetBkMode(dis->hDC, TRANSPARENT);
                    SelectObject(dis->hDC, hFontBold);
                    SetTextColor(dis->hDC, txtCol);
                    DrawTextW(dis->hDC, L"📥  İndir ve Ekle", -1, &dis->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                    return TRUE;
                }
                else if (dis->CtlID == IDC_SEARCH_BTN) {
                    COLORREF bg = isPressed ? RGB(56, 56, 64) : (isDisabled ? RGB(26, 26, 30) : RGB(36, 36, 42));
                    COLORREF border = RGB(48, 48, 56);
                    COLORREF txtCol = isDisabled ? RGB(95, 95, 102) : RGB(255, 255, 255);
                    DrawRoundedCard(dis->hDC, &dis->rcItem, bg, border, 8);
                    SetBkMode(dis->hDC, TRANSPARENT);
                    SelectObject(dis->hDC, hFontBold);
                    SetTextColor(dis->hDC, txtCol);
                    DrawTextW(dis->hDC, L"🔍 Ara", -1, &dis->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                    return TRUE;
                }
                else if (dis->CtlID == IDC_SEARCH_FOLDER_BTN) {
                    COLORREF bg = isPressed ? RGB(44, 44, 50) : RGB(32, 32, 36);
                    COLORREF border = RGB(46, 46, 52);
                    DrawRoundedCard(dis->hDC, &dis->rcItem, bg, border, 6);
                    SetBkMode(dis->hDC, TRANSPARENT);
                    SelectObject(dis->hDC, hFontSmall);
                    SetTextColor(dis->hDC, RGB(220, 220, 225));
                    DrawTextW(dis->hDC, L"Klasör Seç...", -1, &dis->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                    return TRUE;
                }
            }
            else if (dis->CtlType == ODT_LISTBOX && dis->itemID != (UINT)-1) {
                int idx = dis->itemID;
                BOOL isSel = (dis->itemState & ODS_SELECTED);
                // Seçili öğede asil Spotify yeşili parıltısı ve yeşil çerçeve
                COLORREF bg = isSel ? RGB(24, 42, 30) : ((idx % 2 == 0) ? RGB(22, 22, 26) : RGB(26, 26, 30));
                COLORREF border = isSel ? SPOTIFY_GREEN : bg;

                RECT rcRow = dis->rcItem;
                InflateRect(&rcRow, -2, -2);
                DrawRoundedCard(dis->hDC, &rcRow, bg, border, 6);

                if (idx < g_resultCount) {
                    SearchItem *si = &g_results[idx];
                    int dpi = GetSearchWindowDpi(hWnd);
                    int rowH = rcRow.bottom - rcRow.top;

                    // 16:9 Oranında Gerçek YouTube Thumbnail Boyutu (76x44)
                    int thumbW = MulDiv(76, dpi, 96);
                    int thumbH = MulDiv(44, dpi, 96);
                    int thumbX = rcRow.left + MulDiv(6, dpi, 96);
                    int thumbY = rcRow.top + (rowH - thumbH) / 2;
                    RECT rcThumb = { thumbX, thumbY, thumbX + thumbW, thumbY + thumbH };

                    // Küçük resim arka plan kartı
                    DrawRoundedCard(dis->hDC, &rcThumb, RGB(14, 14, 16), RGB(36, 36, 42), 4);

                    if (si->pThumb) {
                        OLE_XSIZE_HIMETRIC cx;
                        OLE_YSIZE_HIMETRIC cy;
                        si->pThumb->lpVtbl->get_Width(si->pThumb, &cx);
                        si->pThumb->lpVtbl->get_Height(si->pThumb, &cy);
                        si->pThumb->lpVtbl->Render(si->pThumb, dis->hDC, rcThumb.left + 1, rcThumb.top + 1, thumbW - 2, thumbH - 2, 0, cy, cx, -cy, NULL);
                    } else {
                        // Resim henüz inerken zarif play ikonu
                        SetBkMode(dis->hDC, TRANSPARENT);
                        SelectObject(dis->hDC, hFontSmall);
                        SetTextColor(dis->hDC, RGB(110, 110, 120));
                        DrawTextW(dis->hDC, L"▶", -1, &rcThumb, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                    }

                    // YouTube Tarzı Süre Rozeti (Thumbnail'in sağ alt köşesinde siyah rozet)
                    if (si->duration > 0) {
                        wchar_t durBuf[16];
                        wsprintfW(durBuf, L"%02d:%02d", si->duration / 60, si->duration % 60);
                        int badgeW = MulDiv(38, dpi, 96);
                        int badgeH = MulDiv(15, dpi, 96);
                        RECT rcDurBadge = { rcThumb.right - badgeW - 2, rcThumb.bottom - badgeH - 2, rcThumb.right - 2, rcThumb.bottom - 2 };
                        DrawRoundedCard(dis->hDC, &rcDurBadge, RGB(10, 10, 12), RGB(10, 10, 12), 3);
                        SetBkMode(dis->hDC, TRANSPARENT);
                        SelectObject(dis->hDC, hFontSmall);
                        SetTextColor(dis->hDC, RGB(255, 255, 255));
                        DrawTextW(dis->hDC, durBuf, -1, &rcDurBadge, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                    }

                    // Şarkı Başlığı ve Sanatçı/Kanal Bilgisi
                    int textLeft = rcThumb.right + MulDiv(10, dpi, 96);
                    int textRight = rcRow.right - MulDiv(10, dpi, 96);

                    int midY = rcRow.top + rowH / 2;
                    RECT rcTitle = { textLeft, rcRow.top + MulDiv(5, dpi, 96), textRight, midY + MulDiv(2, dpi, 96) };
                    RECT rcSub   = { textLeft, midY + MulDiv(2, dpi, 96), textRight, rcRow.bottom - MulDiv(5, dpi, 96) };

                    SetBkMode(dis->hDC, TRANSPARENT);
                    SelectObject(dis->hDC, hFontBold);
                    SetTextColor(dis->hDC, isSel ? RGB(255, 255, 255) : RGB(238, 238, 242));
                    DrawTextW(dis->hDC, si->cleanTitle, -1, &rcTitle, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

                    SelectObject(dis->hDC, hFontSmall);
                    SetTextColor(dis->hDC, isSel ? SPOTIFY_GREEN : RGB(150, 150, 158));
                    DrawTextW(dis->hDC, si->uploader, -1, &rcSub, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                }
                return TRUE;
            }
            break;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);

            RECT rcClient;
            GetClientRect(hWnd, &rcClient);
            // EN ÖNEMLİ DÜZELTME: Bütün beyazlıkları yok eden tam arka plan boyaması!
            FillRect(hdc, &rcClient, hBrBg);

            int clientW = rcClient.right - rcClient.left;
            int clientH = rcClient.bottom - rcClient.top;

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

            int btnSearchW = MulDiv(90, dpi, 96);
            int searchH    = MulDiv(34, dpi, 96);
            int editW      = leftW - btnSearchW - MulDiv(8, dpi, 96);

            // Arama kutusu arka planı ve şık Apple çerçevesi
            RECT rcEditBorder = { leftX, pad, leftX + editW, pad + searchH };
            DrawRoundedCard(hdc, &rcEditBorder, COLOR_SEARCH_BOX, COLOR_SEARCH_BORDER, 8);

            // ListBox çerçevesi
            int row2Y = pad + searchH + MulDiv(8, dpi, 96);
            int row2H = MulDiv(28, dpi, 96);
            int statH = MulDiv(22, dpi, 96);
            int statY = clientH - pad - statH;
            int listY = row2Y + row2H + MulDiv(8, dpi, 96);
            int listH = statY - listY - MulDiv(8, dpi, 96);

            RECT rcListBorder = { leftX - 1, listY - 1, leftX + leftW + 1, listY + listH + 1 };
            DrawRoundedCard(hdc, &rcListBorder, COLOR_SEARCH_LIST, COLOR_SEARCH_BORDER, 8);

            // Sağ Apple Music / Spotify Hero Kart Bölgesi
            RECT rcCard = { cardLeft, cardTop, cardRight, cardBottom };
            DrawRoundedCard(hdc, &rcCard, COLOR_SEARCH_CARD, COLOR_SEARCH_BORDER, 10);

            // Thumbnail Kutusu (16:9 oranı)
            int cardPad = MulDiv(14, dpi, 96);
            int thumbW  = cardW - (cardPad * 2);
            int thumbH  = (thumbW * 9) / 16;
            RECT rcThumb = { cardLeft + cardPad, cardTop + cardPad, cardLeft + cardPad + thumbW, cardTop + cardPad + thumbH };
            DrawRoundedCard(hdc, &rcThumb, RGB(14, 14, 16), COLOR_SEARCH_BORDER, 8);

            int sel = (int)SendMessageW(g_hListResults, LB_GETCURSEL, 0, 0);

            IPicture *curPic = (sel >= 0 && sel < g_resultCount) ? g_results[sel].pThumb : NULL;
            if (curPic) {
                OLE_XSIZE_HIMETRIC cx;
                OLE_YSIZE_HIMETRIC cy;
                curPic->lpVtbl->get_Width(curPic, &cx);
                curPic->lpVtbl->get_Height(curPic, &cy);
                curPic->lpVtbl->Render(curPic, hdc, rcThumb.left + 1, rcThumb.top + 1, thumbW - 2, thumbH - 2, 0, cy, cx, -cy, NULL);
            } else {
                SetBkMode(hdc, TRANSPARENT);
                SelectObject(hdc, hFontSmall);
                SetTextColor(hdc, RGB(120, 120, 128));
                const wchar_t *holderTxt = (sel >= 0) ? L"Kapak resmi yükleniyor..." : L"🎵 Şarkı Seçildiğinde Kapak Burada Görünür";
                DrawTextW(hdc, holderTxt, -1, &rcThumb, DT_CENTER | DT_VCENTER | DT_WORDBREAK);
            }

            // Şarkı Detay Bilgileri
            if (sel >= 0 && sel < g_resultCount) {
                SetBkMode(hdc, TRANSPARENT);
                int metaY  = rcThumb.bottom + MulDiv(12, dpi, 96);
                int titleH = MulDiv(48, dpi, 96);

                RECT rcTitle = { rcThumb.left, metaY, rcThumb.right, metaY + titleH };
                SelectObject(hdc, hFontBold);
                SetTextColor(hdc, RGB(255, 255, 255));
                DrawTextW(hdc, g_results[sel].cleanTitle, -1, &rcTitle, DT_LEFT | DT_TOP | DT_WORDBREAK | DT_END_ELLIPSIS);

                int artistY = metaY + titleH + MulDiv(4, dpi, 96);
                int artistH = MulDiv(22, dpi, 96);
                RECT rcArtist = { rcThumb.left, artistY, rcThumb.right, artistY + artistH };
                SelectObject(hdc, hFont);
                SetTextColor(hdc, RGB(167, 167, 167));
                DrawTextW(hdc, g_results[sel].uploader, -1, &rcArtist, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS);

                int durY = artistY + artistH + MulDiv(4, dpi, 96);
                int durH = MulDiv(20, dpi, 96);
                RECT rcMeta = { rcThumb.left, durY, rcThumb.right, durY + durH };
                SelectObject(hdc, hFontSmall);
                SetTextColor(hdc, SPOTIFY_GREEN); // Spotify Yeşil Vurgu
                wchar_t metaTxt[128];
                int m = g_results[sel].duration / 60;
                int s = g_results[sel].duration % 60;
                wsprintfW(metaTxt, L"Süre: %02d:%02d  •  Kaynak: %s", m, s, g_results[sel].source);
                DrawTextW(hdc, metaTxt, -1, &rcMeta, DT_LEFT | DT_TOP | DT_SINGLELINE);
            }

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
                GetWindowTextW(g_hEditQuery, query, 512);
                if (wcslen(query) == 0) break;

                FreeSearchResults();
                InterlockedIncrement(&g_searchGen);

                g_isSearching = TRUE;
                EnableWindow(g_hBtnSearch, FALSE);
                EnableWindow(g_hBtnDownload, FALSE);
                SendMessageW(g_hListResults, LB_RESETCONTENT, 0, 0);
                SetWindowTextW(g_hLblStatus, L"YouTube taranıyor, lütfen bekleyin...");
                InvalidateRect(hWnd, NULL, FALSE);

                wchar_t *qCopy = _wcsdup(query);
                CreateThread(NULL, 0, SearchThread, qCopy, 0, NULL);
            }
            else if (wmId == IDC_SEARCH_LIST) {
                if (wmEvent == LBN_SELCHANGE) {
                    int sel = (int)SendMessageW(g_hListResults, LB_GETCURSEL, 0, 0);
                    BOOL hasSel = (sel >= 0 && sel < g_resultCount);
                    EnableWindow(g_hBtnDownload, hasSel && !g_isDownloading);
                    InvalidateRect(hWnd, NULL, FALSE);
                } else if (wmEvent == LBN_DBLCLK) {
                    SendMessageW(hWnd, WM_COMMAND, MAKEWPARAM(IDC_SEARCH_DOWN, BN_CLICKED), (LPARAM)g_hBtnDownload);
                }
            }
            else if (wmId == IDC_SEARCH_DOWN) {
                int sel = (int)SendMessageW(g_hListResults, LB_GETCURSEL, 0, 0);
                if (sel < 0 || sel >= g_resultCount || g_isDownloading) break;

                g_isDownloading = TRUE;
                EnableWindow(g_hBtnDownload, FALSE);
                EnableWindow(g_hBtnSearch, FALSE);

                wchar_t statusTxt[256];
                wsprintfW(statusTxt, L"İndiriliyor: %s...", g_results[sel].cleanTitle);
                SetWindowTextW(g_hLblStatus, statusTxt);

                DownloadParam *dp = (DownloadParam*)malloc(sizeof(DownloadParam));
                wcsncpy(dp->url, g_results[sel].url, 511);
                dp->url[511] = L'\0';
                wcsncpy(dp->folder, g_downloadFolder, MAX_PATH - 1);
                dp->folder[MAX_PATH - 1] = L'\0';

                CreateThread(NULL, 0, DownloadThread, dp, 0, NULL);
            }
            break;
        }

        case WM_APP_SEARCH_DONE: {
            EnableWindow(g_hBtnSearch, TRUE);
            SendMessageW(g_hListResults, LB_RESETCONTENT, 0, 0);

            if (g_resultCount == 0) {
                SetWindowTextW(g_hLblStatus, L"Sonuç bulunamadı.");
            } else {
                for (int i = 0; i < g_resultCount; i++) {
                    wchar_t itemText[512];
                    int m = g_results[i].duration / 60;
                    int s = g_results[i].duration % 60;
                    if (g_results[i].duration > 0) {
                        wsprintfW(itemText, L"%s [%02d:%02d] • %s", g_results[i].cleanTitle, m, s, g_results[i].uploader);
                    } else {
                        wsprintfW(itemText, L"%s • %s", g_results[i].cleanTitle, g_results[i].uploader);
                    }
                    SendMessageW(g_hListResults, LB_ADDSTRING, 0, (LPARAM)itemText);
                }
                wchar_t stat[128];
                wsprintfW(stat, L"%d müzik bulundu. Doğrudan tıklayıp indirebilirsiniz.", g_resultCount);
                SetWindowTextW(g_hLblStatus, stat);

                // İlk şarkıyı otomatik seç
                SendMessageW(g_hListResults, LB_SETCURSEL, 0, 0);
                EnableWindow(g_hBtnDownload, TRUE);

                // Bütün YouTube küçük resimlerini (thumbnails) hemen arka planda paralel çekmeye başla
                LONG curGen = g_searchGen;
                for (int i = 0; i < g_resultCount; i++) {
                    SingleThumbTask *task = (SingleThumbTask*)malloc(sizeof(SingleThumbTask));
                    task->itemIndex = i;
                    task->searchGen = curGen;
                    wcsncpy(task->id, g_results[i].id, 63);
                    task->id[63] = L'\0';
                    HANDLE hT = CreateThread(NULL, 0, SingleThumbThread, task, 0, NULL);
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

            BOOL success = (BOOL)wParam;
            if (success) {
                SetWindowTextW(g_hLblStatus, L"İndirme tamamlandı! Şarkı çalma listesinin en üstüne eklendi.");
                if (IsWindow(g_hParent)) {
                    PostMessageW(g_hParent, WM_APP_FILE_DOWNLOADED, 0, 0);
                }
            } else {
                SetWindowTextW(g_hLblStatus, L"İndirme başarısız oldu! Lütfen tekrar deneyin.");
            }
            break;
        }

        case WM_DESTROY: {
            RemoveWindowSubclass(g_hEditQuery, SearchEditSubclassProc, 1);
            FreeSearchResults();
            if (hBrBg) DeleteObject(hBrBg);
            if (hBrBox) DeleteObject(hBrBox);
            if (hBrList) DeleteObject(hBrList);
            if (hFont) DeleteObject(hFont);
            if (hFontBold) DeleteObject(hFontBold);
            if (hFontSmall) DeleteObject(hFontSmall);
            g_hDlg = NULL;
            SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
            return 0;
        }
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

void Search_ShowDialog(HWND hParentWnd, const wchar_t *downloadFolder) {
    if (g_hDlg) {
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
    HICON hMainIcon = LoadIconW(hInst, MAKEINTRESOURCEW(1));
    HICON hSmallIcon = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(1), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);

    WNDCLASSEXW wc = { sizeof(wc) };
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = SearchWndProc;
    wc.hInstance = hInst;
    wc.hIcon = hMainIcon ? hMainIcon : LoadIcon(NULL, IDI_APPLICATION);
    wc.hIconSm = hSmallIcon ? hSmallIcon : wc.hIcon;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(COLOR_SEARCH_BG);
    wc.lpszClassName = L"SeloPlayerSearch";
    RegisterClassExW(&wc);

    RECT rcParent;
    GetWindowRect(hParentWnd, &rcParent);
    int x = rcParent.left + 15;
    int y = rcParent.top + 15;

    int screenDpi = 96;
    HDC hdcScreen = GetDC(NULL);
    if (hdcScreen) {
        screenDpi = GetDeviceCaps(hdcScreen, LOGPIXELSY);
        ReleaseDC(NULL, hdcScreen);
    }
    if (screenDpi <= 0) screenDpi = 96;

    int dlgW = MulDiv(820, screenDpi, 96);
    int dlgH = MulDiv(540, screenDpi, 96);

    g_hDlg = CreateWindowExW(
        WS_EX_TOPMOST,
        L"SeloPlayerSearch",
        L"Müzik Ara & İndir (Spotify Yeşil Tasarım)",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        x, y, dlgW, dlgH,
        hParentWnd, NULL, hInst, NULL
    );

    if (hMainIcon) SendMessageW(g_hDlg, WM_SETICON, ICON_BIG, (LPARAM)hMainIcon);
    if (hSmallIcon) SendMessageW(g_hDlg, WM_SETICON, ICON_SMALL, (LPARAM)hSmallIcon);
}
