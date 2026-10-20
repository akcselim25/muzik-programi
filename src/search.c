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

typedef struct {
    wchar_t id[64];
    wchar_t rawTitle[256];
    wchar_t cleanTitle[256];
    wchar_t uploader[128];
    wchar_t url[512];
    wchar_t source[32]; // "YouTube", "SoundCloud"
    int duration;
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

// Canlı Kapak Resmi Değişkenleri
static IPicture *g_pThumbPic = NULL;
static int g_thumbSelIndex = -1;
static volatile BOOL g_isThumbLoading = FALSE;

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

static DWORD WINAPI SearchThread(LPVOID param) {
    wchar_t *query = (wchar_t*)param;
    g_resultCount = 0;

    // Türkçe karakterleri bozmayan ve 1.5 saniyede şimşek hızında arayan YouTube motoru
    wchar_t cmdLine[2048];
    wsprintfW(cmdLine, L"yt-dlp.exe --encoding utf-8 --no-warnings --flat-playlist --print \"%%(id)s\t%%(title)s\t%%(duration)s\t%%(uploader)s\t%%(webpage_url)s\" \"ytsearch12:%s\"", query);
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

typedef struct {
    wchar_t thumbUrl[512];
    int selIndex;
} ThumbParam;

static DWORD WINAPI ThumbLoadThread(LPVOID param) {
    ThumbParam *tp = (ThumbParam*)param;
    OleInitialize(NULL);
    IPicture *pPic = NULL;
    HRESULT hr = OleLoadPicturePath(tp->thumbUrl, NULL, 0, 0, &IID_IPicture, (void**)&pPic);
    if (SUCCEEDED(hr) && pPic) {
        PostMessageW(g_hDlg, WM_APP_THUMB_READY, (WPARAM)pPic, (LPARAM)tp->selIndex);
    } else {
        PostMessageW(g_hDlg, WM_APP_THUMB_READY, 0, (LPARAM)tp->selIndex);
    }
    free(tp);
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
    HBRUSH oldBr = (HBRUSH)SelectObject(hdc, hBr);
    HPEN oldPen = (HPEN)SelectObject(hdc, hPen);

    RoundRect(hdc, rc->left, rc->top, rc->right, rc->bottom, radius, radius);

    SelectObject(hdc, oldBr);
    SelectObject(hdc, oldPen);
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

    if (g_hEditQuery) MoveWindow(g_hEditQuery, leftX + 2, pad + 2, editW - 4, searchH - 4, TRUE);
    if (g_hBtnSearch) MoveWindow(g_hBtnSearch, leftX + editW, pad, btnSearchW, searchH, TRUE);

    // İkinci Satır: Klasör ve Klasör Seç butonu
    int row2Y      = pad + searchH + MulDiv(8, dpi, 96);
    int btnFolderW = MulDiv(110, dpi, 96);
    int row2H      = MulDiv(28, dpi, 96);
    int folderLblW = leftW - btnFolderW - MulDiv(8, dpi, 96);

    if (g_hLblFolder) MoveWindow(g_hLblFolder, leftX + 2, row2Y + 2, folderLblW - 4, row2H - 4, TRUE);
    if (g_hBtnFolder) MoveWindow(g_hBtnFolder, leftX + folderLblW, row2Y, btnFolderW, row2H, TRUE);

    // Alt Durum Çubuğu
    int statH = MulDiv(22, dpi, 96);
    int statY = clientH - pad - statH;
    if (g_hLblStatus) MoveWindow(g_hLblStatus, leftX + 2, statY, leftW - 4, statH, TRUE);

    // Sonuç Listesi (Listbox)
    int listY = row2Y + row2H + MulDiv(8, dpi, 96);
    int listH = statY - listY - MulDiv(8, dpi, 96);
    if (g_hListResults) MoveWindow(g_hListResults, leftX, listY, leftW, listH, TRUE);

    // Sağ Hero Kart içindeki İndir Butonu (Kartın en altına yapışık)
    int cardPad  = MulDiv(14, dpi, 96);
    int btnDownH = MulDiv(46, dpi, 96);
    int btnDownW = cardW - (cardPad * 2);
    int btnDownX = cardLeft + cardPad;
    int btnDownY = cardBottom - cardPad - btnDownH;
    if (g_hBtnDownload) MoveWindow(g_hBtnDownload, btnDownX, btnDownY, btnDownW, btnDownH, TRUE);
}

static LRESULT CALLBACK SearchEditSubclassProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    if (msg == WM_KEYDOWN && wParam == VK_RETURN) {
        HWND hDlg = (HWND)dwRefData;
        if (hDlg && g_hBtnSearch) {
            SendMessageW(hDlg, WM_COMMAND, MAKEWPARAM(IDC_SEARCH_BTN, BN_CLICKED), (LPARAM)g_hBtnSearch);
        }
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
            BOOL useDark = TRUE;
            DwmSetWindowAttribute(hWnd, 20, &useDark, sizeof(useDark));
            DwmSetWindowAttribute(hWnd, 19, &useDark, sizeof(useDark));

            int dpi = GetSearchWindowDpi(hWnd);

            hBrBg   = CreateSolidBrush(RGB(16, 16, 18));
            hBrBox  = CreateSolidBrush(RGB(28, 28, 34));
            hBrList = CreateSolidBrush(RGB(20, 20, 24));

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

            g_hListResults = CreateWindowExW(0, L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS, 0, 0, 10, 10, hWnd, (HMENU)IDC_SEARCH_LIST, GetModuleHandle(NULL), NULL);
            SendMessageW(g_hListResults, LB_SETITEMHEIGHT, 0, MulDiv(46, dpi, 96));

            g_hLblStatus = CreateWindowExW(0, L"STATIC", L"Şarkı veya sanatçı adı yazıp Ara'ya basın.", WS_CHILD | WS_VISIBLE | SS_LEFT | SS_ENDELLIPSIS, 0, 0, 10, 10, hWnd, (HMENU)IDC_SEARCH_STAT, GetModuleHandle(NULL), NULL);

            // Sağ paneldeki Apple İndirme Butonu (Geniş, Tok ve Saf Düz Vurgu)
            g_hBtnDownload = CreateWindowExW(0, L"BUTTON", L"📥  İndir ve Ekle", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0, 0, 10, 10, hWnd, (HMENU)IDC_SEARCH_DOWN, GetModuleHandle(NULL), NULL);

            SendMessageW(g_hEditQuery, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessageW(g_hEditQuery, 0x1501 /* EM_SETCUEBANNER */, TRUE, (LPARAM)L"Şarkı veya sanatçı adı yazın...");
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
            mmi->ptMinTrackSize.x = MulDiv(640, dpi, 96);
            mmi->ptMinTrackSize.y = MulDiv(440, dpi, 96);
            return 0;
        }

        case WM_ERASEBKGND:
            return 1;

        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORDLG: {
            HDC hdc = (HDC)wParam;
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(160, 160, 165));
            return (LRESULT)hBrBg;
        }

        case WM_CTLCOLOREDIT: {
            HDC hdc = (HDC)wParam;
            SetBkMode(hdc, OPAQUE);
            SetBkColor(hdc, RGB(28, 28, 34));
            SetTextColor(hdc, RGB(255, 255, 255));
            return (LRESULT)hBrBox;
        }

        case WM_CTLCOLORLISTBOX: {
            HDC hdc = (HDC)wParam;
            SetBkMode(hdc, OPAQUE);
            SetBkColor(hdc, RGB(20, 20, 24));
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
                        bg = RGB(36, 36, 42);
                        border = RGB(48, 48, 56);
                        txtCol = RGB(120, 120, 130);
                    } else if (isPressed) {
                        bg = RGB(225, 45, 55);
                        border = bg;
                        txtCol = RGB(255, 255, 255);
                    } else {
                        bg = RGB(252, 60, 68); // Apple Music Red
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
                    COLORREF bg = isPressed ? RGB(58, 58, 66) : (isDisabled ? RGB(28, 28, 32) : RGB(40, 40, 46));
                    COLORREF border = RGB(54, 54, 62);
                    COLORREF txtCol = isDisabled ? RGB(95, 95, 102) : RGB(255, 255, 255);
                    DrawRoundedCard(dis->hDC, &dis->rcItem, bg, border, 8);
                    SetBkMode(dis->hDC, TRANSPARENT);
                    SelectObject(dis->hDC, hFontBold);
                    SetTextColor(dis->hDC, txtCol);
                    DrawTextW(dis->hDC, L"🔍 Ara", -1, &dis->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                    return TRUE;
                }
                else if (dis->CtlID == IDC_SEARCH_FOLDER_BTN) {
                    COLORREF bg = isPressed ? RGB(46, 46, 54) : RGB(34, 34, 40);
                    COLORREF border = RGB(48, 48, 56);
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
                COLORREF bg = isSel ? RGB(44, 44, 52) : ((idx % 2 == 0) ? RGB(22, 22, 26) : RGB(26, 26, 30));
                COLORREF border = isSel ? RGB(252, 60, 68) : bg;

                RECT rcRow = dis->rcItem;
                InflateRect(&rcRow, -2, -2);
                DrawRoundedCard(dis->hDC, &rcRow, bg, border, 6);

                if (idx < g_resultCount) {
                    SearchItem *si = &g_results[idx];

                    int rowH = rcRow.bottom - rcRow.top;
                    int badgeW = 30;
                    int badgeH = 20;
                    int badgeY = rcRow.top + (rowH - badgeH) / 2;
                    RECT rcBadge = { rcRow.left + 8, badgeY, rcRow.left + 8 + badgeW, badgeY + badgeH };
                    COLORREF badgeBg = RGB(60, 16, 22);
                    COLORREF badgeTxt = RGB(255, 85, 95);
                    DrawRoundedCard(dis->hDC, &rcBadge, badgeBg, badgeBg, 4);
                    SetBkMode(dis->hDC, TRANSPARENT);
                    SelectObject(dis->hDC, hFontSmall);
                    SetTextColor(dis->hDC, badgeTxt);
                    DrawTextW(dis->hDC, L"YT", -1, &rcBadge, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

                    int durW = 60;
                    RECT rcDur = { rcRow.right - durW - 8, rcRow.top, rcRow.right - 8, rcRow.bottom };

                    int midY = rcRow.top + rowH / 2;
                    RECT rcTitle = { rcRow.left + badgeW + 16, rcRow.top + 3, rcRow.right - durW - 12, midY + 1 };
                    RECT rcSub = { rcRow.left + badgeW + 16, midY + 1, rcRow.right - durW - 12, rcRow.bottom - 3 };

                    SelectObject(dis->hDC, hFontBold);
                    SetTextColor(dis->hDC, isSel ? RGB(255, 255, 255) : RGB(240, 240, 245));
                    DrawTextW(dis->hDC, si->cleanTitle, -1, &rcTitle, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

                    SelectObject(dis->hDC, hFontSmall);
                    SetTextColor(dis->hDC, isSel ? RGB(200, 200, 210) : RGB(145, 145, 150));
                    DrawTextW(dis->hDC, si->uploader, -1, &rcSub, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

                    if (si->duration > 0) {
                        wchar_t durBuf[16];
                        wsprintfW(durBuf, L"%02d:%02d", si->duration / 60, si->duration % 60);
                        SetTextColor(dis->hDC, isSel ? RGB(220, 220, 230) : RGB(130, 130, 135));
                        DrawTextW(dis->hDC, durBuf, -1, &rcDur, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
                    }
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
            int clientW = rcClient.right - rcClient.left;
            int clientH = rcClient.bottom - rcClient.top;

            int dpi = GetSearchWindowDpi(hWnd);

            // Tüm pencere zeminini Apple Deep Dark (#101012) ile doldur (Sıfır beyazlık)
            HBRUSH hBrAll = CreateSolidBrush(RGB(16, 16, 18));
            FillRect(hdc, &rcClient, hBrAll);
            DeleteObject(hBrAll);

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

            // Arama kutusu etrafına zarif Apple çerçeve
            RECT rcEditBorder = { leftX, pad, leftX + editW, pad + searchH };
            HPEN hPenEB = CreatePen(PS_SOLID, 1, RGB(48, 48, 54));
            HBRUSH oldB1 = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
            HPEN oldP1 = (HPEN)SelectObject(hdc, hPenEB);
            RoundRect(hdc, rcEditBorder.left, rcEditBorder.top, rcEditBorder.right, rcEditBorder.bottom, 8, 8);
            SelectObject(hdc, oldB1);
            SelectObject(hdc, oldP1);
            DeleteObject(hPenEB);

            // ListBox etrafına zarif Apple çerçeve
            int row2Y = pad + searchH + MulDiv(8, dpi, 96);
            int row2H = MulDiv(28, dpi, 96);
            int statH = MulDiv(22, dpi, 96);
            int statY = clientH - pad - statH;
            int listY = row2Y + row2H + MulDiv(8, dpi, 96);
            int listH = statY - listY - MulDiv(8, dpi, 96);

            RECT rcListBorder = { leftX - 1, listY - 1, leftX + leftW + 1, listY + listH + 1 };
            HPEN hPenLB = CreatePen(PS_SOLID, 1, RGB(42, 42, 48));
            HBRUSH oldB0 = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
            HPEN oldP0 = (HPEN)SelectObject(hdc, hPenLB);
            RoundRect(hdc, rcListBorder.left, rcListBorder.top, rcListBorder.right, rcListBorder.bottom, 8, 8);
            SelectObject(hdc, oldB0);
            SelectObject(hdc, oldP0);
            DeleteObject(hPenLB);

            // Sağ Apple Music Hero Kart Bölgesi
            RECT rcCard = { cardLeft, cardTop, cardRight, cardBottom };
            DrawRoundedCard(hdc, &rcCard, RGB(24, 24, 28), RGB(42, 42, 48), 10);

            // Thumbnail Kutusu (16:9 oranı)
            int cardPad = MulDiv(14, dpi, 96);
            int thumbW  = cardW - (cardPad * 2);
            int thumbH  = (thumbW * 9) / 16;
            RECT rcThumb = { cardLeft + cardPad, cardTop + cardPad, cardLeft + cardPad + thumbW, cardTop + cardPad + thumbH };
            DrawRoundedCard(hdc, &rcThumb, RGB(16, 16, 18), RGB(46, 46, 52), 8);

            int sel = (int)SendMessageW(g_hListResults, LB_GETCURSEL, 0, 0);

            if (g_pThumbPic) {
                OLE_XSIZE_HIMETRIC cx;
                OLE_YSIZE_HIMETRIC cy;
                g_pThumbPic->lpVtbl->get_Width(g_pThumbPic, &cx);
                g_pThumbPic->lpVtbl->get_Height(g_pThumbPic, &cy);
                g_pThumbPic->lpVtbl->Render(g_pThumbPic, hdc, rcThumb.left + 1, rcThumb.top + 1, thumbW - 2, thumbH - 2, 0, cy, cx, -cy, NULL);
            } else {
                SetBkMode(hdc, TRANSPARENT);
                SelectObject(hdc, hFontSmall);
                SetTextColor(hdc, RGB(120, 120, 128));
                const wchar_t *holderTxt = g_isThumbLoading ? L"Kapak resmi yükleniyor..." : L"🎵 Şarkı Seçildiğinde Kapak Burada Görünür";
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
                SetTextColor(hdc, RGB(160, 160, 165));
                DrawTextW(hdc, g_results[sel].uploader, -1, &rcArtist, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS);

                int durY = artistY + artistH + MulDiv(4, dpi, 96);
                int durH = MulDiv(20, dpi, 96);
                RECT rcMeta = { rcThumb.left, durY, rcThumb.right, durY + durH };
                SelectObject(hdc, hFontSmall);
                SetTextColor(hdc, RGB(252, 60, 68)); // Apple Accent
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

                if (g_pThumbPic) {
                    g_pThumbPic->lpVtbl->Release(g_pThumbPic);
                    g_pThumbPic = NULL;
                }
                g_thumbSelIndex = -1;

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

                    // Kapak resmini arka planda yükle
                    if (hasSel && sel != g_thumbSelIndex) {
                        g_thumbSelIndex = sel;
                        if (g_pThumbPic) {
                            g_pThumbPic->lpVtbl->Release(g_pThumbPic);
                            g_pThumbPic = NULL;
                        }

                        InvalidateRect(hWnd, NULL, FALSE);

                        ThumbParam *tp = (ThumbParam*)malloc(sizeof(ThumbParam));
                        tp->selIndex = sel;
                        wsprintfW(tp->thumbUrl, L"https://i.ytimg.com/vi/%s/mqdefault.jpg", g_results[sel].id);

                        g_isThumbLoading = TRUE;
                        CreateThread(NULL, 0, ThumbLoadThread, tp, 0, NULL);
                    }
                } else if (wmEvent == LBN_DBLCLK) {
                    // Çift tıklama doğrudan indirmeyi başlatır
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
                wsprintfW(stat, L"%d müzik bulundu. Şarkı seçtiğinizde sağda önizleme açılır.", g_resultCount);
                SetWindowTextW(g_hLblStatus, stat);
            }
            break;
        }

        case WM_APP_THUMB_READY: {
            IPicture *pPic = (IPicture*)wParam;
            int selIdx = (int)lParam;
            g_isThumbLoading = FALSE;

            if (selIdx == g_thumbSelIndex && pPic) {
                if (g_pThumbPic) {
                    g_pThumbPic->lpVtbl->Release(g_pThumbPic);
                }
                g_pThumbPic = pPic;
                InvalidateRect(hWnd, NULL, FALSE);
            } else {
                if (pPic) pPic->lpVtbl->Release(pPic);
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
                SetWindowTextW(g_hLblStatus, L"İndirme tamamlandı! Şarkı ve kapak resmi klasöre kaydedildi.");
                if (g_hParent) {
                    PostMessageW(g_hParent, WM_APP_FILE_DOWNLOADED, 0, 0);
                }
            } else {
                SetWindowTextW(g_hLblStatus, L"İndirme başarısız oldu.");
            }
            break;
        }

        case WM_CLOSE: {
            if (g_pThumbPic) {
                g_pThumbPic->lpVtbl->Release(g_pThumbPic);
                g_pThumbPic = NULL;
            }
            DestroyWindow(hWnd);
            return 0;
        }

        case WM_DESTROY: {
            RemoveWindowSubclass(g_hEditQuery, SearchEditSubclassProc, 1);
            if (g_pThumbPic) {
                g_pThumbPic->lpVtbl->Release(g_pThumbPic);
                g_pThumbPic = NULL;
            }
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
    wc.hbrBackground = CreateSolidBrush(RGB(16, 16, 18));
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
        L"Müzik Ara & İndir (Apple Music Tasarım)",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        x, y, dlgW, dlgH,
        hParentWnd, NULL, hInst, NULL
    );

    if (hMainIcon) SendMessageW(g_hDlg, WM_SETICON, ICON_BIG, (LPARAM)hMainIcon);
    if (hSmallIcon) SendMessageW(g_hDlg, WM_SETICON, ICON_SMALL, (LPARAM)hSmallIcon);
}
