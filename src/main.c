#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <dwmapi.h>
#include <olectl.h>
#include <stdio.h>
#include <stdlib.h>
#include "player.h"
#include "playlist.h"
#include "config.h"
#include "search.h"
#include "updater.h"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "mfplay.lib")

#ifndef EM_SETCUEBANNER
#define EM_SETCUEBANNER 0x1501
#endif

// Renk Paleti (Apple Dark Mode Foundation - Gradient Olmayan Saf & Asil Katmanlar)
#define COLOR_BG          RGB(16, 16, 18)     // Apple Deep OLED Dark
#define COLOR_HEADER      RGB(24, 24, 26)     // Apple Top & Bottom Bars
#define COLOR_CARD        RGB(28, 28, 32)     // Apple Elevated Item Card
#define COLOR_CARD_HOVER  RGB(40, 40, 46)     // Apple Interactive Hover
#define COLOR_CARD_SEL    RGB(48, 48, 56)     // Apple Selected Item
#define COLOR_ACCENT      RGB(252, 60, 68)    // Apple Music Vibrant Red (#FC3C44)
#define COLOR_ACCENT_HOV  RGB(255, 82, 90)
#define COLOR_TEXT_WHITE  RGB(255, 255, 255)  // Apple Crisp White
#define COLOR_TEXT_MUTED  RGB(152, 152, 157)  // Apple System Gray (#98989D)
#define COLOR_TEXT_DIM    RGB(105, 105, 110)  // Apple System Gray 3 (#69696E)
#define COLOR_BAR_BG      RGB(42, 42, 46)     // Apple Scrubber Track Background
#define COLOR_SIDEBAR     RGB(20, 20, 22)     // Apple macOS Sidebar
#define COLOR_BORDER      RGB(44, 44, 48)     // Apple Hairline Separator

#define IDT_UPDATE_TIMER   1001
#define IDC_HEADER_SEARCH  1010
#define SIDEBAR_WIDTH      240

static int g_dpi = 96;

static inline int GetItemHeight(void) {
    return MulDiv(58, g_dpi, 96);
}

static HWND g_hWnd = NULL;
static HWND g_hSearchEdit = NULL;
static WNDPROC g_oldSearchEditProc = NULL;
static HBRUSH g_hBrSearchEdit = NULL;

static HFONT g_hFontTitle = NULL;
static HFONT g_hFontMain = NULL;
static HFONT g_hFontBold = NULL;
static HFONT g_hFontSmall = NULL;

static void InitDpiFonts(int dpi) {
    if (dpi <= 0) dpi = 96;
    g_dpi = dpi;

    if (g_hFontTitle) DeleteObject(g_hFontTitle);
    if (g_hFontBold) DeleteObject(g_hFontBold);
    if (g_hFontMain) DeleteObject(g_hFontMain);
    if (g_hFontSmall) DeleteObject(g_hFontSmall);

    // Font boyutları: Gerçek fiziksel DPI bazlı (Bulanıklık sıfır)
    int sTitle = -MulDiv(15, dpi, 72);
    int sBold  = -MulDiv(11, dpi, 72);
    int sMain  = -MulDiv(11, dpi, 72);
    int sSmall = -MulDiv(9,  dpi, 72);

    const wchar_t *fontFamily = L"Segoe UI";

    g_hFontTitle = CreateFontW(sTitle, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, fontFamily);
    g_hFontBold  = CreateFontW(sBold,  0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, fontFamily);
    g_hFontMain  = CreateFontW(sMain,  0, 0, 0, FW_NORMAL,   FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, fontFamily);
    g_hFontSmall = CreateFontW(sSmall, 0, 0, 0, FW_NORMAL,   FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, fontFamily);
}

static IPicture *g_pMainThumb = NULL;
static HICON g_hAppIcon = NULL;
static AppConfig g_cfg;
static int g_currentTrack = -1;
static int g_selectedTrack = -1;
static int g_hoverTrack = -1;
static int g_scrollOffset = 0;

static BOOL g_sidebarOpen = FALSE;
static int  g_sidebarHoverItem = -1;

static BOOL g_isDraggingSeek = FALSE;
static int  g_seekDragSec = 0;
static BOOL g_isDraggingVol = FALSE;

// Sağdaki kaydırma çubuğu durumu
static BOOL g_isDraggingScrollbar = FALSE;
static int  g_scrollbarDragStartY = 0;
static int  g_scrollbarDragStartOffset = 0;
static BOOL g_scrollbarHover = FALSE;
static RECT rcScrollbarTrack;
static RECT rcScrollbarThumb;

// Canlı Şarkı Arama / Filtreleme
static int  g_filterIndices[8192];
static int  g_filterCount = 0;
static BOOL g_isFiltered = FALSE;
static wchar_t g_searchQuery[128] = {0};

// Liste içi sürükle-bırak (sıralama değiştirme)
static int   g_dragFromIndex = -1;
static int   g_dragTargetIndex = -1;
static BOOL  g_isDraggingList = FALSE;
static POINT g_mouseDownPt = {0};

// Buton koordinat bölgeleri
static RECT rcHamburger;
static RECT rcSearchBox;
static RECT rcPrev, rcPlay, rcNext;
static RECT rcSeekBar, rcVolBar;

static void TrimProcessMemory(void) {
    SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
}

static void InvalidateVolumeArea(HWND hWnd) {
    RECT rcClient;
    GetClientRect(hWnd, &rcClient);
    int bottomBarH = MulDiv(60, g_dpi, 96);
    int marginL = MulDiv(30, g_dpi, 96);
    RECT rcVolArea = { rcVolBar.left - marginL, rcClient.bottom - bottomBarH, rcClient.right, rcClient.bottom };
    InvalidateRect(hWnd, &rcVolArea, FALSE);
}

static void FormatTime(int totalSec, wchar_t *buf, int bufLen) {
    if (totalSec < 0) totalSec = 0;
    int m = totalSec / 60;
    int s = totalSec % 60;
    wsprintfW(buf, L"%02d:%02d", m, s);
}

static void ApplyFilter(const wchar_t *query) {
    if (!query || !query[0]) {
        g_isFiltered = FALSE;
        g_filterCount = 0;
        g_searchQuery[0] = L'\0';
    } else {
        g_isFiltered = TRUE;
        g_filterCount = 0;
        wcsncpy(g_searchQuery, query, 127);
        g_searchQuery[127] = L'\0';

        int total = Playlist_Count();
        for (int i = 0; i < total && g_filterCount < 8192; i++) {
            const PlaylistItem *item = Playlist_Get(i);
            if (item && StrStrIW(item->title, query)) {
                g_filterIndices[g_filterCount++] = i;
            }
        }
    }
    g_scrollOffset = 0;
    InvalidateRect(g_hWnd, NULL, FALSE);
}

static void LoadTrackThumbnail(const wchar_t *filePath) {
    if (g_pMainThumb) {
        g_pMainThumb->lpVtbl->Release(g_pMainThumb);
        g_pMainThumb = NULL;
    }
    if (!filePath || !filePath[0]) return;

    wchar_t base[MAX_PATH];
    wcsncpy(base, filePath, MAX_PATH - 1);
    base[MAX_PATH - 1] = L'\0';
    PathRemoveExtensionW(base);

    static const wchar_t *exts[] = { L".jpg", L".jpeg", L".png", L".webp" };
    for (int i = 0; i < 4; i++) {
        wchar_t candidate[MAX_PATH];
        wsprintfW(candidate, L"%s%s", base, exts[i]);
        if (GetFileAttributesW(candidate) != INVALID_FILE_ATTRIBUTES) {
            OleLoadPicturePath(candidate, NULL, 0, 0, &IID_IPicture, (void**)&g_pMainThumb);
            if (g_pMainThumb) break;
        }
    }
}

static void PlayTrack(int index) {
    if (index < 0 || index >= Playlist_Count()) return;
    const PlaylistItem *item = Playlist_Get(index);
    if (!item) return;

    g_currentTrack = index;
    g_selectedTrack = index;
    LoadTrackThumbnail(item->filePath);
    Player_OpenFile(item->filePath);
    InvalidateRect(g_hWnd, NULL, FALSE);
}

static void SelectFolderDialog(void) {
    BROWSEINFOW bi = {0};
    bi.hwndOwner = g_hWnd;
    bi.lpszTitle = L"Müzik Klasörünü Seçin";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

    PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
    if (pidl) {
        wchar_t folderPath[MAX_PATH];
        if (SHGetPathFromIDListW(pidl, folderPath)) {
            wcsncpy(g_cfg.lastFolder, folderPath, MAX_PATH - 1);
            g_cfg.lastFolder[MAX_PATH - 1] = L'\0';
            Config_Save(&g_cfg, L"config.json");

            Player_Stop();
            Playlist_Clear();
            Playlist_ScanFolder(folderPath);
            TrimProcessMemory();
            g_currentTrack = -1;
            g_selectedTrack = Playlist_Count() > 0 ? 0 : -1;
            g_scrollOffset = 0;
            ApplyFilter(g_searchQuery);
            InvalidateRect(g_hWnd, NULL, FALSE);
        }
        CoTaskMemFree(pidl);
    }
}

static void ImportM3UDialog(void) {
    wchar_t szFile[MAX_PATH] = {0};
    OPENFILENAMEW ofn = { sizeof(ofn) };
    ofn.hwndOwner = g_hWnd;
    ofn.lpstrFilter = L"M3U Çalma Listesi (*.m3u;*.m3u8)\0*.m3u;*.m3u8\0Tüm Dosyalar (*.*)\0*.*\0";
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (GetOpenFileNameW(&ofn)) {
        int added = Playlist_ImportM3U(szFile);
        if (added > 0) {
            if (g_selectedTrack < 0) g_selectedTrack = 0;
            ApplyFilter(g_searchQuery);
            InvalidateRect(g_hWnd, NULL, FALSE);
        }
    }
}

static void ExportM3UDialog(void) {
    if (Playlist_Count() == 0) {
        MessageBoxW(g_hWnd, L"Çalma listesinde kaydedilecek şarkı yok.", L"Bilgi", MB_OK | MB_ICONINFORMATION);
        return;
    }
    wchar_t szFile[MAX_PATH] = L"calma_listem.m3u8";
    OPENFILENAMEW ofn = { sizeof(ofn) };
    ofn.hwndOwner = g_hWnd;
    ofn.lpstrFilter = L"M3U8 Çalma Listesi (*.m3u8)\0*.m3u8\0M3U Çalma Listesi (*.m3u)\0*.m3u\0";
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT;
    ofn.lpstrDefExt = L"m3u8";

    if (GetSaveFileNameW(&ofn)) {
        if (Playlist_ExportM3U(szFile)) {
            MessageBoxW(g_hWnd, L"Çalma listesi başarıyla kaydedildi!", L"Başarılı", MB_OK | MB_ICONINFORMATION);
        }
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

static void DrawIconPlay(HDC hdc, int cx, int cy, COLORREF col) {
    HBRUSH hBr = CreateSolidBrush(col);
    HPEN hPen = CreatePen(PS_SOLID, 1, col);
    HBRUSH oldBr = (HBRUSH)SelectObject(hdc, hBr);
    HPEN oldPen = (HPEN)SelectObject(hdc, hPen);

    POINT pts[3] = { { cx - 3, cy - 7 }, { cx - 3, cy + 7 }, { cx + 6, cy } };
    Polygon(hdc, pts, 3);

    SelectObject(hdc, oldBr);
    SelectObject(hdc, oldPen);
    DeleteObject(hBr);
    DeleteObject(hPen);
}

static void DrawIconPause(HDC hdc, int cx, int cy, COLORREF col) {
    HBRUSH hBr = CreateSolidBrush(col);
    HPEN hPen = CreatePen(PS_SOLID, 1, col);
    HBRUSH oldBr = (HBRUSH)SelectObject(hdc, hBr);
    HPEN oldPen = (HPEN)SelectObject(hdc, hPen);

    Rectangle(hdc, cx - 4, cy - 6, cx - 1, cy + 7);
    Rectangle(hdc, cx + 2, cy - 6, cx + 5, cy + 7);

    SelectObject(hdc, oldBr);
    SelectObject(hdc, oldPen);
    DeleteObject(hBr);
    DeleteObject(hPen);
}

static void DrawIconPrev(HDC hdc, int cx, int cy, COLORREF col) {
    HBRUSH hBr = CreateSolidBrush(col);
    HPEN hPen = CreatePen(PS_SOLID, 1, col);
    HBRUSH oldBr = (HBRUSH)SelectObject(hdc, hBr);
    HPEN oldPen = (HPEN)SelectObject(hdc, hPen);

    Rectangle(hdc, cx - 6, cy - 6, cx - 4, cy + 7);
    POINT pts[3] = { { cx + 5, cy - 6 }, { cx + 5, cy + 6 }, { cx - 2, cy } };
    Polygon(hdc, pts, 3);

    SelectObject(hdc, oldBr);
    SelectObject(hdc, oldPen);
    DeleteObject(hBr);
    DeleteObject(hPen);
}

static void DrawIconNext(HDC hdc, int cx, int cy, COLORREF col) {
    HBRUSH hBr = CreateSolidBrush(col);
    HPEN hPen = CreatePen(PS_SOLID, 1, col);
    HBRUSH oldBr = (HBRUSH)SelectObject(hdc, hBr);
    HPEN oldPen = (HPEN)SelectObject(hdc, hPen);

    POINT pts[3] = { { cx - 5, cy - 6 }, { cx - 5, cy + 6 }, { cx + 2, cy } };
    Polygon(hdc, pts, 3);
    Rectangle(hdc, cx + 4, cy - 6, cx + 6, cy + 7);

    SelectObject(hdc, oldBr);
    SelectObject(hdc, oldPen);
    DeleteObject(hBr);
    DeleteObject(hPen);
}



static void DrawIconSpeaker(HDC hdc, int cx, int cy, COLORREF col) {
    HBRUSH hBr = CreateSolidBrush(col);
    HPEN hPen = CreatePen(PS_SOLID, 1, col);
    HBRUSH oldBr = (HBRUSH)SelectObject(hdc, hBr);
    HPEN oldPen = (HPEN)SelectObject(hdc, hPen);

    Rectangle(hdc, cx - 6, cy - 3, cx - 2, cy + 4);
    POINT pts[4] = { { cx - 2, cy - 3 }, { cx + 2, cy - 6 }, { cx + 2, cy + 7 }, { cx - 2, cy + 4 } };
    Polygon(hdc, pts, 4);

    SelectObject(hdc, oldBr);
    SelectObject(hdc, oldPen);
    DeleteObject(hBr);
    DeleteObject(hPen);
}

static void DrawIconMagnifier(HDC hdc, int cx, int cy, COLORREF col) {
    HPEN hPen = CreatePen(PS_SOLID, 2, col);
    HPEN oldPen = (HPEN)SelectObject(hdc, hPen);
    HBRUSH oldBr = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));

    Ellipse(hdc, cx - 6, cy - 6, cx + 4, cy + 4);
    MoveToEx(hdc, cx + 2, cy + 2, NULL);
    LineTo(hdc, cx + 7, cy + 7);

    SelectObject(hdc, oldBr);
    SelectObject(hdc, oldPen);
    DeleteObject(hPen);
}

static void RenderUI(HDC hdc, const RECT *clientRc) {
    int w = clientRc->right - clientRc->left;
    int h = clientRc->bottom - clientRc->top;

    // Arka planı doldur
    HBRUSH hBrBg = CreateSolidBrush(COLOR_BG);
    FillRect(hdc, clientRc, hBrBg);
    DeleteObject(hBrBg);

    // 1. UST HEADER BAR (Yükseklik: 60px)
    RECT rcHeader = { 0, 0, w, 60 };
    HBRUSH hBrHdr = CreateSolidBrush(COLOR_HEADER);
    FillRect(hdc, &rcHeader, hBrHdr);
    DeleteObject(hBrHdr);

    HPEN hPenBorder = CreatePen(PS_SOLID, 1, COLOR_BORDER);
    HPEN oldPen = (HPEN)SelectObject(hdc, hPenBorder);
    MoveToEx(hdc, 0, 60, NULL);
    LineTo(hdc, w, 60);
    SelectObject(hdc, oldPen);
    DeleteObject(hPenBorder);

    // Hamburger Menü Butonu (Sol: 15, Üst: 13, 34x34)
    rcHamburger = (RECT){ 15, 13, 49, 47 };
    DrawRoundedCard(hdc, &rcHamburger, g_sidebarOpen ? COLOR_ACCENT : COLOR_CARD, COLOR_BORDER, 6);
    
    COLORREF burgerCol = g_sidebarOpen ? COLOR_BG : COLOR_TEXT_WHITE;
    HPEN hPenBurger = CreatePen(PS_SOLID, 2, burgerCol);
    HPEN oldP = (HPEN)SelectObject(hdc, hPenBurger);
    int bx = 23, by = 22;
    for (int line = 0; line < 3; line++) {
        MoveToEx(hdc, bx, by + line * 6, NULL);
        LineTo(hdc, bx + 18, by + line * 6);
    }
    SelectObject(hdc, oldP);
    DeleteObject(hPenBurger);

    // Sağ Üst: Şarkı Arama Çubuğu (0 FPS drop yerine gelen modern arama kutusu)
    int searchW = 195;
    int searchX = w - searchW - 15;
    rcSearchBox = (RECT){ searchX, 14, searchX + searchW, 46 };
    DrawRoundedCard(hdc, &rcSearchBox, RGB(36, 36, 40), COLOR_BORDER, 8);
    DrawIconMagnifier(hdc, searchX + 14, 30, COLOR_TEXT_MUTED);

    // Başlık ve Çalan Şarkı Bilgisi (Sol taraf)
    SetBkMode(hdc, TRANSPARENT);
    SetBkColor(hdc, COLOR_HEADER);

    RECT rcTitle = { 65, 8, searchX - 15, 32 };
    SelectObject(hdc, g_hFontTitle);
    SetTextColor(hdc, COLOR_TEXT_WHITE);
    if (g_currentTrack >= 0 && g_currentTrack < Playlist_Count()) {
        const PlaylistItem *cur = Playlist_Get(g_currentTrack);
        DrawTextW(hdc, cur->title, -1, &rcTitle, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    } else {
        DrawTextW(hdc, L"Selo Player", -1, &rcTitle, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    RECT rcSub = { 65, 33, searchX - 15, 52 };
    SelectObject(hdc, g_hFontSmall);
    SetTextColor(hdc, COLOR_TEXT_MUTED);
    wchar_t statusBuf[256];
    if (g_isFiltered) {
        wsprintfW(statusBuf, L"Arama Sonucu: %d şarkı bulundu", g_filterCount);
    } else if (g_currentTrack >= 0) {
        wsprintfW(statusBuf, L"%d / %d Şarkı • %s", g_currentTrack + 1, Playlist_Count(), Player_IsPlaying() ? L"Çalıyor" : L"Duraklatıldı");
    } else {
        wsprintfW(statusBuf, L"Toplam %d şarkı hazır", Playlist_Count());
    }
    DrawTextW(hdc, statusBuf, -1, &rcSub, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    // 2. ORTA: ŞARKI LİSTESİ (65px ile h - 105px arası)
    int itemHeight = GetItemHeight();
    int listTop = 65;
    int listBottom = h - 105;
    int visibleCount = (listBottom - listTop) / itemHeight;
    int totalCount = g_isFiltered ? g_filterCount : Playlist_Count();

    if (totalCount == 0) {
        RECT rcEmpty = { 0, listTop, w, listBottom };
        SetBkColor(hdc, COLOR_BG);
        SelectObject(hdc, g_hFontMain);
        SetTextColor(hdc, COLOR_TEXT_MUTED);
        if (g_isFiltered) {
            DrawTextW(hdc, L"Aradığınız kriterlere uygun şarkı bulunamadı.\n(Aramayı temizlemek için kutuyu boşaltın)", -1, &rcEmpty, DT_CENTER | DT_VCENTER);
        } else {
            DrawTextW(hdc, L"Çalma listeniz boş.\n\nMenüden (≡) klasör seçebilir, M3U ekleyebilir\nveya buraya şarkıları sürükleyip bırakabilirsiniz.", -1, &rcEmpty, DT_CENTER | DT_VCENTER);
        }
    } else {
        int itemPad = MulDiv(6, g_dpi, 96);
        for (int i = 0; i < visibleCount + 1; i++) {
            int displayIdx = g_scrollOffset + i;
            if (displayIdx >= totalCount) break;

            int actualTrackIdx = g_isFiltered ? g_filterIndices[displayIdx] : displayIdx;
            int itemY = listTop + i * itemHeight;
            RECT rcItem = { 15, itemY, w - 24, itemY + itemHeight - itemPad };

            COLORREF itemBg = COLOR_CARD;
            COLORREF itemBorder = COLOR_BORDER;
            if (actualTrackIdx == g_selectedTrack) {
                itemBg = COLOR_CARD_SEL;
                itemBorder = COLOR_ACCENT;
            } else if (actualTrackIdx == g_hoverTrack) {
                itemBg = COLOR_CARD_HOVER;
            }

            DrawRoundedCard(hdc, &rcItem, itemBg, itemBorder, 8);

            // Sürükle-bırak hedef çizgisi
            if (g_isDraggingList && actualTrackIdx == g_dragTargetIndex) {
                int lineY = (g_dragFromIndex < g_dragTargetIndex) ? rcItem.bottom + 2 : rcItem.top - 2;
                RECT rcLine = { rcItem.left, lineY - 1, rcItem.right, lineY + 2 };
                HBRUSH hBrLine = CreateSolidBrush(COLOR_ACCENT);
                FillRect(hdc, &rcLine, hBrLine);
                DeleteObject(hBrLine);
            }

            // Çalan şarkı sol Apple hap şeridi
            if (actualTrackIdx == g_currentTrack) {
                int stripW = MulDiv(3, g_dpi, 96);
                if (stripW < 3) stripW = 3;
                RECT rcAccent = { rcItem.left + MulDiv(3, g_dpi, 96), rcItem.top + MulDiv(8, g_dpi, 96), rcItem.left + MulDiv(3, g_dpi, 96) + stripW, rcItem.bottom - MulDiv(8, g_dpi, 96) };
                DrawRoundedCard(hdc, &rcAccent, COLOR_ACCENT, COLOR_ACCENT, 2);
            }

            // Numara veya Çalma ikonu
            int numW = MulDiv(42, g_dpi, 96);
            RECT rcNum = { rcItem.left + MulDiv(6, g_dpi, 96), rcItem.top, rcItem.left + numW, rcItem.bottom };
            SetBkColor(hdc, itemBg);
            if (actualTrackIdx == g_currentTrack) {
                int cx = (rcNum.left + rcNum.right) / 2;
                int cy = (rcNum.top + rcNum.bottom) / 2;
                if (Player_IsPlaying()) {
                    DrawIconPlay(hdc, cx, cy, COLOR_ACCENT);
                } else {
                    DrawIconPause(hdc, cx, cy, COLOR_ACCENT);
                }
            } else {
                SelectObject(hdc, g_hFontSmall);
                SetTextColor(hdc, COLOR_TEXT_DIM);
                wchar_t numBuf[16];
                wsprintfW(numBuf, L"%02d", displayIdx + 1);
                DrawTextW(hdc, numBuf, -1, &rcNum, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }

            // Şarkı Başlığı ve Sanatçı (Apple İki Satırlı Akıllı Düzen)
            const PlaylistItem *item = Playlist_Get(actualTrackIdx);
            if (item) {
                int textLeft = rcItem.left + numW + MulDiv(4, g_dpi, 96);
                int textRight = rcItem.right - MulDiv(14, g_dpi, 96);
                int cardH = rcItem.bottom - rcItem.top;
                int midY = rcItem.top + (cardH / 2);

                const wchar_t *dash = wcsstr(item->title, L" - ");
                if (dash) {
                    int artistLen = (int)(dash - item->title);
                    wchar_t artistPart[256];
                    if (artistLen > 255) artistLen = 255;
                    wcsncpy(artistPart, item->title, artistLen);
                    artistPart[artistLen] = L'\0';
                    const wchar_t *titlePart = dash + 3;

                    RECT rcSongTitle = { textLeft, rcItem.top + MulDiv(4, g_dpi, 96), textRight, midY + MulDiv(1, g_dpi, 96) };
                    RECT rcArtist = { textLeft, midY, textRight, rcItem.bottom - MulDiv(4, g_dpi, 96) };

                    SetBkColor(hdc, itemBg);
                    SelectObject(hdc, actualTrackIdx == g_currentTrack ? g_hFontBold : g_hFontMain);
                    SetTextColor(hdc, actualTrackIdx == g_currentTrack ? COLOR_ACCENT : COLOR_TEXT_WHITE);
                    DrawTextW(hdc, titlePart, -1, &rcSongTitle, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

                    SelectObject(hdc, g_hFontSmall);
                    SetTextColor(hdc, actualTrackIdx == g_currentTrack ? RGB(255, 175, 180) : COLOR_TEXT_MUTED);
                    DrawTextW(hdc, artistPart, -1, &rcArtist, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                } else {
                    RECT rcName = { textLeft, rcItem.top, textRight, rcItem.bottom };
                    SetBkColor(hdc, itemBg);
                    SelectObject(hdc, actualTrackIdx == g_currentTrack ? g_hFontBold : g_hFontMain);
                    SetTextColor(hdc, actualTrackIdx == g_currentTrack ? COLOR_ACCENT : COLOR_TEXT_WHITE);
                    DrawTextW(hdc, item->title, -1, &rcName, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                }
            }
        }
    }

    // SAĞDAKİ KAYDIRMA ÇUBUĞU (Apple macOS Floating Scrollbar)
    int sbLeft = w - 12;
    int sbRight = w - 6;
    int sbTop = listTop + 6;
    int sbBottom = listBottom - 6;
    rcScrollbarTrack = (RECT){ sbLeft, sbTop, sbRight, sbBottom };

    if (totalCount > visibleCount) {
        int trackH = sbBottom - sbTop;
        int thumbH = (int)((float)visibleCount / totalCount * trackH);
        if (thumbH < 28) thumbH = 28;

        int maxScroll = totalCount - visibleCount;
        float scrollRatio = (maxScroll > 0) ? ((float)g_scrollOffset / (float)maxScroll) : 0.0f;
        int thumbY = sbTop + (int)(scrollRatio * (trackH - thumbH));
        rcScrollbarThumb = (RECT){ sbLeft, thumbY, sbRight, thumbY + thumbH };

        COLORREF thumbCol = g_isDraggingScrollbar ? COLOR_ACCENT : (g_scrollbarHover ? RGB(120, 120, 128) : RGB(65, 65, 72));
        DrawRoundedCard(hdc, &rcScrollbarThumb, thumbCol, thumbCol, 3);
    } else {
        rcScrollbarThumb = (RECT){ 0, 0, 0, 0 };
    }

    // 3. ALT: KONTROL BAR (Yükseklik: 102px)
    RECT rcCtrlBar = { 0, h - 102, w, h };
    HBRUSH hBrCtrl = CreateSolidBrush(COLOR_HEADER);
    FillRect(hdc, &rcCtrlBar, hBrCtrl);
    DeleteObject(hBrCtrl);

    HPEN hPenCtrl = CreatePen(PS_SOLID, 1, COLOR_BORDER);
    oldPen = (HPEN)SelectObject(hdc, hPenCtrl);
    MoveToEx(hdc, 0, h - 102, NULL);
    LineTo(hdc, w, h - 102);
    SelectObject(hdc, oldPen);
    DeleteObject(hPenCtrl);

    // İlerleme Çubuğu (Seek Bar)
    int seekY = h - 90;
    rcSeekBar = (RECT){ 65, seekY, w - 65, seekY + 6 };
    DrawRoundedCard(hdc, &rcSeekBar, COLOR_BAR_BG, COLOR_BAR_BG, 3);

    int duration = Player_GetDuration();
    int position = g_isDraggingSeek ? g_seekDragSec : Player_GetPosition();
    if (position < 0) position = 0;
    if (duration > 0 && position > duration) position = duration;

    float progressRatio = (duration > 0) ? ((float)position / (float)duration) : 0.0f;
    if (progressRatio > 1.0f) progressRatio = 1.0f;

    int totalTrackW = rcSeekBar.right - rcSeekBar.left;
    int seekFillW = (int)(totalTrackW * progressRatio);
    if (seekFillW > 0) {
        RECT rcSeekFill = { rcSeekBar.left, rcSeekBar.top, rcSeekBar.left + seekFillW, rcSeekBar.bottom };
        DrawRoundedCard(hdc, &rcSeekFill, COLOR_ACCENT, COLOR_ACCENT, 3);
    }

    // Beyaz Thumb yuvarlağı (Her zaman net ve modern görünüm)
    if (duration > 0 || g_isDraggingSeek) {
        int thumbX = rcSeekBar.left + seekFillW;
        if (thumbX < rcSeekBar.left) thumbX = rcSeekBar.left;
        if (thumbX > rcSeekBar.right) thumbX = rcSeekBar.right;
        int thumbY = rcSeekBar.top + 3;
        int thumbRadius = 6;

        HBRUSH hBrThumb = CreateSolidBrush(COLOR_TEXT_WHITE);
        HPEN hPenThumb = CreatePen(PS_SOLID, 1, COLOR_ACCENT);
        HBRUSH oldB = (HBRUSH)SelectObject(hdc, hBrThumb);
        HPEN oldP = (HPEN)SelectObject(hdc, hPenThumb);

        Ellipse(hdc, thumbX - thumbRadius, thumbY - thumbRadius, thumbX + thumbRadius, thumbY + thumbRadius);

        SelectObject(hdc, oldB);
        SelectObject(hdc, oldP);
        DeleteObject(hBrThumb);
        DeleteObject(hPenThumb);
    }

    // Süre Metinleri (Sol: Geçen Süre, Sağ: Toplam Süre)
    wchar_t timePosStr[32], timeDurStr[32];
    FormatTime(position, timePosStr, 32);
    FormatTime(duration, timeDurStr, 32);

    SetBkColor(hdc, COLOR_HEADER);
    SelectObject(hdc, g_hFontSmall);
    SetTextColor(hdc, COLOR_TEXT_MUTED);

    RECT rcTimePos = { 10, seekY - 6, 60, seekY + 12 };
    DrawTextW(hdc, timePosStr, -1, &rcTimePos, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

    RECT rcTimeDur = { w - 60, seekY - 6, w - 10, seekY + 12 };
    DrawTextW(hdc, timeDurStr, -1, &rcTimeDur, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // OYNATMA BUTONLARI (Kusursuz 3 Bölgeli Modern Tasarım - Sıfır Çakışma)
    int btnY = h - 68;
    int centerX = w / 2;

    // Bölge 1 (Sol): Şu An Çalan Şarkı Bilgisi & Kapak Resmi (Sade ve Şık Görünüm)
    // Bölge 1 (Sol): Şu An Çalan Şarkı Bilgisi & Kapak Resmi (Apple Music Now Playing)
    if (g_currentTrack >= 0 && g_currentTrack < Playlist_Count()) {
        const PlaylistItem *cur = Playlist_Get(g_currentTrack);

        RECT rcThumbBox = { 20, btnY - 2, 60, btnY + 38 };
        DrawRoundedCard(hdc, &rcThumbBox, COLOR_CARD, COLOR_BORDER, 8);

        if (g_pMainThumb) {
            OLE_XSIZE_HIMETRIC cx;
            OLE_YSIZE_HIMETRIC cy;
            g_pMainThumb->lpVtbl->get_Width(g_pMainThumb, &cx);
            g_pMainThumb->lpVtbl->get_Height(g_pMainThumb, &cy);
            g_pMainThumb->lpVtbl->Render(g_pMainThumb, hdc, rcThumbBox.left + 1, rcThumbBox.top + 1, 38, 38, 0, cy, cx, -cy, NULL);
        } else if (g_hAppIcon) {
            DrawIconEx(hdc, rcThumbBox.left + 2, rcThumbBox.top + 2, g_hAppIcon, 36, 36, 0, NULL, DI_NORMAL);
        } else {
            SelectObject(hdc, g_hFontSmall);
            SetTextColor(hdc, COLOR_TEXT_MUTED);
            SetBkMode(hdc, TRANSPARENT);
            DrawTextW(hdc, L"🎵", -1, &rcThumbBox, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }

        const wchar_t *dash = wcsstr(cur->title, L" - ");
        RECT rcTrackInfo = { 68, btnY, centerX - 80, btnY + 18 };
        RECT rcTrackSub  = { 68, btnY + 18, centerX - 80, btnY + 36 };

        SetBkColor(hdc, COLOR_HEADER);
        if (dash) {
            int aLen = (int)(dash - cur->title);
            wchar_t aPart[256];
            if (aLen > 255) aLen = 255;
            wcsncpy(aPart, cur->title, aLen);
            aPart[aLen] = L'\0';
            const wchar_t *tPart = dash + 3;

            SelectObject(hdc, g_hFontBold);
            SetTextColor(hdc, COLOR_TEXT_WHITE);
            DrawTextW(hdc, tPart, -1, &rcTrackInfo, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

            SelectObject(hdc, g_hFontSmall);
            SetTextColor(hdc, COLOR_TEXT_MUTED);
            DrawTextW(hdc, aPart, -1, &rcTrackSub, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        } else {
            SelectObject(hdc, g_hFontBold);
            SetTextColor(hdc, COLOR_TEXT_WHITE);
            DrawTextW(hdc, cur->title, -1, &rcTrackInfo, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

            SelectObject(hdc, g_hFontSmall);
            SetTextColor(hdc, Player_IsPlaying() ? COLOR_ACCENT : COLOR_TEXT_MUTED);
            DrawTextW(hdc, Player_IsPlaying() ? L"Çalıyor" : L"Duraklatıldı", -1, &rcTrackSub, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        }
    } else {
        RECT rcThumbBox = { 20, btnY - 2, 60, btnY + 38 };
        DrawRoundedCard(hdc, &rcThumbBox, COLOR_CARD, COLOR_BORDER, 8);
        if (g_hAppIcon) {
            DrawIconEx(hdc, rcThumbBox.left + 2, rcThumbBox.top + 2, g_hAppIcon, 36, 36, 0, NULL, DI_NORMAL);
        }
        RECT rcTrackInfo = { 68, btnY, centerX - 80, btnY + 18 };
        RECT rcTrackSub  = { 68, btnY + 18, centerX - 80, btnY + 36 };
        SetBkColor(hdc, COLOR_HEADER);
        SelectObject(hdc, g_hFontBold);
        SetTextColor(hdc, COLOR_TEXT_WHITE);
        DrawTextW(hdc, L"Selo Player", -1, &rcTrackInfo, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        SelectObject(hdc, g_hFontSmall);
        SetTextColor(hdc, COLOR_TEXT_MUTED);
        DrawTextW(hdc, L"Müzik seçin", -1, &rcTrackSub, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    // Bölge 2 (Merkez): Önceki - Oynat/Duraklat - Sonraki (Apple Music Standardı)
    rcPrev = (RECT){ centerX - 66, btnY + 1, centerX - 30, btnY + 37 };
    rcPlay = (RECT){ centerX - 22, btnY - 3, centerX + 22, btnY + 41 };
    rcNext = (RECT){ centerX + 30, btnY + 1, centerX + 66, btnY + 37 };

    // Önceki Butonu
    DrawRoundedCard(hdc, &rcPrev, COLOR_CARD, COLOR_BORDER, 8);
    DrawIconPrev(hdc, (rcPrev.left + rcPrev.right) / 2, (rcPrev.top + rcPrev.bottom) / 2, COLOR_TEXT_WHITE);

    // Oynat/Duraklat Butonu (44x44 Tam Daire Apple Kırmızı - Saf Beyaz İkon)
    HBRUSH hBrPlay = CreateSolidBrush(COLOR_ACCENT);
    HPEN hPenPlay = CreatePen(PS_SOLID, 1, COLOR_ACCENT_HOV);
    HBRUSH oldB = (HBRUSH)SelectObject(hdc, hBrPlay);
    HPEN oldP2 = (HPEN)SelectObject(hdc, hPenPlay);
    Ellipse(hdc, rcPlay.left, rcPlay.top, rcPlay.right, rcPlay.bottom);
    SelectObject(hdc, oldB);
    SelectObject(hdc, oldP2);
    DeleteObject(hBrPlay);
    DeleteObject(hPenPlay);

    int playCx = (rcPlay.left + rcPlay.right) / 2;
    int playCy = (rcPlay.top + rcPlay.bottom) / 2;
    if (Player_IsPlaying()) {
        DrawIconPause(hdc, playCx, playCy, COLOR_TEXT_WHITE);
    } else {
        DrawIconPlay(hdc, playCx, playCy, COLOR_TEXT_WHITE);
    }

    // Sonraki Butonu
    DrawRoundedCard(hdc, &rcNext, COLOR_CARD, COLOR_BORDER, 8);
    DrawIconNext(hdc, (rcNext.left + rcNext.right) / 2, (rcNext.top + rcNext.bottom) / 2, COLOR_TEXT_WHITE);

    // Bölge 3 (Sağ): Ses Kontrolü (Sağa yaslı, asla butonlarla çakışmaz)
    int volRight = w - 15;
    int volW = MulDiv(80, g_dpi, 96);
    int txtW = MulDiv(38, g_dpi, 96);
    int volBarRight = volRight - txtW;
    int volBarLeft = volBarRight - volW;
    int barH = MulDiv(6, g_dpi, 96);
    if (barH < 6) barH = 6;
    int barY = btnY + MulDiv(16, g_dpi, 96);
    rcVolBar = (RECT){ volBarLeft, barY, volBarRight, barY + barH };

    DrawIconSpeaker(hdc, volBarLeft - MulDiv(14, g_dpi, 96), (rcVolBar.top + rcVolBar.bottom) / 2, COLOR_TEXT_MUTED);

    DrawRoundedCard(hdc, &rcVolBar, COLOR_BAR_BG, COLOR_BAR_BG, barH / 2);
    int curVol = Player_GetVolume();
    if (curVol < 0) curVol = 0;
    if (curVol > 100) curVol = 100;
    int volFillW = (int)((rcVolBar.right - rcVolBar.left) * (curVol / 100.0f));
    if (volFillW > 0) {
        RECT rcVolFill = { rcVolBar.left, rcVolBar.top, rcVolBar.left + volFillW, rcVolBar.bottom };
        DrawRoundedCard(hdc, &rcVolFill, COLOR_ACCENT, COLOR_ACCENT, barH / 2);
    }

    // Ses Thumb Yuvarlağı (Apple standardı daire buton)
    int thumbX = rcVolBar.left + volFillW;
    if (thumbX < rcVolBar.left) thumbX = rcVolBar.left;
    if (thumbX > rcVolBar.right) thumbX = rcVolBar.right;
    int thumbY = (rcVolBar.top + rcVolBar.bottom) / 2;
    int thumbRadius = MulDiv(5, g_dpi, 96);
    if (thumbRadius < 5) thumbRadius = 5;

    HBRUSH hBrThumb = CreateSolidBrush(COLOR_TEXT_WHITE);
    HPEN hPenThumb = CreatePen(PS_SOLID, 1, COLOR_ACCENT);
    HBRUSH oldBVol = (HBRUSH)SelectObject(hdc, hBrThumb);
    HPEN oldPVol = (HPEN)SelectObject(hdc, hPenThumb);

    Ellipse(hdc, thumbX - thumbRadius, thumbY - thumbRadius, thumbX + thumbRadius, thumbY + thumbRadius);

    SelectObject(hdc, oldBVol);
    SelectObject(hdc, oldPVol);
    DeleteObject(hBrThumb);
    DeleteObject(hPenThumb);

    wchar_t volTxt[16];
    wsprintfW(volTxt, L"%%%d", curVol);
    RECT rcVolTxt = { volBarRight + MulDiv(4, g_dpi, 96), btnY, volRight, btnY + MulDiv(38, g_dpi, 96) };
    SetBkColor(hdc, COLOR_HEADER);
    SetBkMode(hdc, TRANSPARENT);
    SelectObject(hdc, g_hFontSmall);
    SetTextColor(hdc, COLOR_TEXT_MUTED);
    DrawTextW(hdc, volTxt, -1, &rcVolTxt, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

    // 4. HAMBURGER YAN PANEL
    if (g_sidebarOpen) {
        RECT rcSide = { 0, 60, SIDEBAR_WIDTH, h };
        HBRUSH hBrSide = CreateSolidBrush(COLOR_SIDEBAR);
        FillRect(hdc, &rcSide, hBrSide);
        DeleteObject(hBrSide);

        HPEN hPenSide = CreatePen(PS_SOLID, 1, COLOR_BORDER);
        oldPen = (HPEN)SelectObject(hdc, hPenSide);
        MoveToEx(hdc, SIDEBAR_WIDTH, 60, NULL);
        LineTo(hdc, SIDEBAR_WIDTH, h);
        SelectObject(hdc, oldPen);
        DeleteObject(hPenSide);

        const wchar_t *menuItems[] = {
            L"📁  Klasör Seç",
            L"📥  M3U İçe Aktar",
            L"📤  M3U Dışa Aktar",
            L"🔍  Müzik Ara & İndir (MYT)",
            L"🧹  Listeyi Temizle",
            L"🔄  Güncellemeleri Denetle"
        };
        int menuCount = 6;

        for (int m = 0; m < menuCount; m++) {
            RECT rcMenu = { 10, 75 + m * 48, SIDEBAR_WIDTH - 10, 75 + m * 48 + 40 };
            COLORREF mBg = (m == g_sidebarHoverItem) ? COLOR_CARD_HOVER : COLOR_SIDEBAR;
            DrawRoundedCard(hdc, &rcMenu, mBg, (m == g_sidebarHoverItem) ? COLOR_BORDER : COLOR_SIDEBAR, 6);

            SetBkColor(hdc, mBg);
            SelectObject(hdc, g_hFontMain);
            SetTextColor(hdc, (m == 3) ? COLOR_ACCENT : COLOR_TEXT_WHITE);
            RECT rcText = { rcMenu.left + 14, rcMenu.top, rcMenu.right, rcMenu.bottom };
            DrawTextW(hdc, menuItems[m], -1, &rcText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        }
    }
}

static LRESULT CALLBACK SearchEditSubclassProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_KEYDOWN) {
        if (wParam == VK_RETURN) {
            int total = g_isFiltered ? g_filterCount : Playlist_Count();
            if (total > 0) {
                int firstIdx = g_isFiltered ? g_filterIndices[0] : 0;
                PlayTrack(firstIdx);
            }
            return 0;
        } else if (wParam == VK_ESCAPE) {
            SetWindowTextW(hWnd, L"");
            ApplyFilter(L"");
            SetFocus(g_hWnd);
            return 0;
        } else if (wParam == VK_DOWN) {
            SetFocus(g_hWnd);
            return 0;
        }
    }
    return CallWindowProcW(g_oldSearchEditProc, hWnd, msg, wParam, lParam);
}

static LRESULT CALLBACK MainWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            g_hWnd = hWnd;

            BOOL useDark = TRUE;
            DwmSetWindowAttribute(hWnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &useDark, sizeof(useDark));
            DwmSetWindowAttribute(hWnd, 19, &useDark, sizeof(useDark));

            typedef UINT(WINAPI *GetDpiForWindowProc)(HWND);
            GetDpiForWindowProc pGetDpi = (GetDpiForWindowProc)GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow");
            int dpi = pGetDpi ? pGetDpi(hWnd) : 96;
            if (dpi <= 0) dpi = 96;
            InitDpiFonts(dpi);

            g_hAppIcon = (HICON)LoadImageW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(1), IMAGE_ICON, 36, 36, LR_DEFAULTCOLOR);

            g_hBrSearchEdit = CreateSolidBrush(RGB(36, 36, 40));

            // Sağ üst arama kutusu
            g_hSearchEdit = CreateWindowExW(
                0, L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                0, 0, 0, 0,
                hWnd, (HMENU)IDC_HEADER_SEARCH, GetModuleHandle(NULL), NULL
            );
            SendMessageW(g_hSearchEdit, WM_SETFONT, (WPARAM)g_hFontMain, TRUE);
            SendMessageW(g_hSearchEdit, EM_SETCUEBANNER, TRUE, (LPARAM)L"Şarkılarda ara...");

            g_oldSearchEditProc = (WNDPROC)SetWindowLongPtrW(g_hSearchEdit, GWLP_WNDPROC, (LONG_PTR)SearchEditSubclassProc);

            Player_Init(hWnd);
            Playlist_Init();

            Config_Load(&g_cfg, L"config.json");
            Player_SetVolume(g_cfg.volume);

            if (g_cfg.lastFolder[0] && GetFileAttributesW(g_cfg.lastFolder) != INVALID_FILE_ATTRIBUTES) {
                Playlist_ScanFolder(g_cfg.lastFolder);
                TrimProcessMemory();
                if (Playlist_Count() > 0) {
                    g_selectedTrack = 0;
                }
            } else {
                wchar_t defaultMusicDir[MAX_PATH];
                GetModuleFileNameW(NULL, defaultMusicDir, MAX_PATH);
                PathRemoveFileSpecW(defaultMusicDir);
                PathAppendW(defaultMusicDir, L"Müzikler");
                if (GetFileAttributesW(defaultMusicDir) == INVALID_FILE_ATTRIBUTES) {
                    GetModuleFileNameW(NULL, defaultMusicDir, MAX_PATH);
                    PathRemoveFileSpecW(defaultMusicDir);
                    PathAppendW(defaultMusicDir, L"Muzikler");
                }
                if (GetFileAttributesW(defaultMusicDir) != INVALID_FILE_ATTRIBUTES) {
                    wcsncpy(g_cfg.lastFolder, defaultMusicDir, MAX_PATH - 1);
                    g_cfg.lastFolder[MAX_PATH - 1] = L'\0';
                    Config_Save(&g_cfg, L"config.json");
                    Playlist_ScanFolder(g_cfg.lastFolder);
                    TrimProcessMemory();
                    if (Playlist_Count() > 0) {
                        g_selectedTrack = 0;
                    }
                }
            }

            DragAcceptFiles(hWnd, TRUE);
            SetTimer(hWnd, IDT_UPDATE_TIMER, 250, NULL);

            // Başlangıçta eski geçici güncelleme dosyalarını temizle ve güncelleme kontrolü yap
            Updater_Init();
            Updater_CheckAsync(hWnd, FALSE);
            return 0;
        }

        case WM_CTLCOLOREDIT: {
            HWND hCtrl = (HWND)lParam;
            if (hCtrl == g_hSearchEdit) {
                HDC hdcEdit = (HDC)wParam;
                SetBkColor(hdcEdit, RGB(36, 36, 40));
                SetTextColor(hdcEdit, COLOR_TEXT_WHITE);
                return (LRESULT)g_hBrSearchEdit;
            }
            break;
        }

        case WM_COMMAND: {
            if (LOWORD(wParam) == IDC_HEADER_SEARCH && HIWORD(wParam) == EN_CHANGE) {
                wchar_t q[128] = {0};
                GetWindowTextW(g_hSearchEdit, q, 128);
                ApplyFilter(q);
                return 0;
            }
            break;
        }

        case 0x02E0 /* WM_DPICHANGED */: {
            int newDpi = HIWORD(wParam);
            InitDpiFonts(newDpi);
            RECT *prc = (RECT*)lParam;
            SetWindowPos(hWnd, NULL, prc->left, prc->top, prc->right - prc->left, prc->bottom - prc->top, SWP_NOZORDER | SWP_NOACTIVATE);
            InvalidateRect(hWnd, NULL, FALSE);
            return 0;
        }

        case WM_ACTIVATE: {
            if (LOWORD(wParam) == WA_INACTIVE) {
                SetTimer(hWnd, IDT_UPDATE_TIMER, 1000, NULL);
                TrimProcessMemory();
            } else {
                SetTimer(hWnd, IDT_UPDATE_TIMER, 250, NULL);
            }
            break;
        }

        case WM_SIZE: {
            if (wParam == SIZE_MINIMIZED) {
                SetTimer(hWnd, IDT_UPDATE_TIMER, 1500, NULL);
                TrimProcessMemory();
            } else {
                int w = LOWORD(lParam);
                int searchW = 195;
                int searchX = w - searchW - 15;
                if (g_hSearchEdit) {
                    MoveWindow(g_hSearchEdit, searchX + 28, 20, searchW - 36, 20, TRUE);
                }
                InvalidateRect(hWnd, NULL, FALSE);
            }
            break;
        }

        case WM_TIMER: {
            if (wParam == IDT_UPDATE_TIMER) {
                static int s_trimCounter = 0;
                if (++s_trimCounter >= 40) {
                    s_trimCounter = 0;
                    TrimProcessMemory();
                }

                if (Player_IsPlaying() || g_isDraggingSeek) {
                    RECT rcClient;
                    GetClientRect(hWnd, &rcClient);
                    RECT rcSeekArea = { 0, rcClient.bottom - 102, rcClient.right, rcClient.bottom - 60 };
                    InvalidateRect(hWnd, &rcSeekArea, FALSE);
                    RECT rcHdr = { 65, 0, rcClient.right - 220, 60 };
                    InvalidateRect(hWnd, &rcHdr, FALSE);
                }
            }
            return 0;
        }

        case WM_APP_TRACK_ENDED: {
            int nextIdx = Playlist_FindNext(g_currentTrack, g_cfg.shuffle, g_cfg.repeat);
            if (nextIdx >= 0) {
                PlayTrack(nextIdx);
            } else {
                Player_Stop();
                InvalidateRect(hWnd, NULL, FALSE);
            }
            return 0;
        }

        case WM_APP_FILE_DOWNLOADED: {
            const wchar_t *downFolder = Search_GetDownloadFolder();
            if (downFolder && downFolder[0]) {
                wcsncpy(g_cfg.lastFolder, downFolder, MAX_PATH - 1);
                g_cfg.lastFolder[MAX_PATH - 1] = L'\0';
                Config_Save(&g_cfg, L"config.json");
            }

            if (g_cfg.lastFolder[0]) {
                int oldCount = Playlist_Count();
                Playlist_ScanFolder(g_cfg.lastFolder);
                TrimProcessMemory();
                int newCount = Playlist_Count();

                // Eğer aktif bir arama filtresi varsa temizle ki indirilen şarkı görünsün
                if (g_isFiltered || g_searchQuery[0]) {
                    g_searchQuery[0] = L'\0';
                    SetWindowTextW(g_hSearchEdit, L"");
                    ApplyFilter(L"");
                }

                if (newCount > oldCount) {
                    int addedCount = newCount - oldCount;
                    // YENİ İNDİRİLEN ŞARKILARI EN ÜSTE (0. SIRAYA) TAŞI
                    for (int i = 0; i < addedCount; i++) {
                        Playlist_Move(oldCount + i, 0);
                    }
                    if (g_currentTrack >= 0) {
                        g_currentTrack += addedCount;
                    }
                    g_selectedTrack = 0;
                    g_scrollOffset = 0;
                }
                InvalidateRect(hWnd, NULL, FALSE);
            }
            return 0;
        }

        case WM_APP_UPDATE_CHECK_DONE: {
            UpdateInfo *info = (UpdateInfo*)lParam;
            if (info) {
                if (info->hasUpdate) {
                    wchar_t prompt[2048];
                    if (info->changelog[0]) {
                        wsprintfW(prompt, L"SeloPlayer için yeni bir güncelleme mevcut!\n\n"
                                          L"Mevcut Sürüm: v%s\n"
                                          L"Yeni Sürüm: v%s\n\n"
                                          L"Yenilikler:\n%s\n\n"
                                          L"Şimdi otomatik olarak indirip güncellemek istiyor musunuz?",
                                  APP_VERSION_STR, info->newVersion, info->changelog);
                    } else {
                        wsprintfW(prompt, L"SeloPlayer için yeni bir güncelleme mevcut!\n\n"
                                          L"Mevcut Sürüm: v%s\n"
                                          L"Yeni Sürüm: v%s\n\n"
                                          L"Şimdi otomatik olarak indirip güncellemek istiyor musunuz?",
                                  APP_VERSION_STR, info->newVersion);
                    }

                    if (MessageBoxW(hWnd, prompt, L"Güncelleme Mevcut", MB_YESNO | MB_ICONINFORMATION) == IDYES) {
                        Updater_PerformUpdate(hWnd, info->downloadUrl);
                    }
                } else if (info->isManual) {
                    if (info->checkSuccess) {
                        wchar_t msg[256];
                        wsprintfW(msg, L"Zaten en güncel sürümü kullanıyorsunuz.\n(Mevcut Sürüm: v%s)", APP_VERSION_STR);
                        MessageBoxW(hWnd, msg, L"Güncelleme Kontrolü", MB_OK | MB_ICONINFORMATION);
                    } else {
                        MessageBoxW(hWnd, L"Güncelleme sunucusuna erişilemedi.\nLütfen internet bağlantınızı kontrol edin.", L"Güncelleme Kontrolü", MB_OK | MB_ICONWARNING);
                    }
                }
                free(info);
            }
            return 0;
        }

        case WM_DROPFILES: {
            HDROP hDrop = (HDROP)wParam;
            UINT count = DragQueryFileW(hDrop, 0xFFFFFFFF, NULL, 0);
            for (UINT i = 0; i < count; i++) {
                wchar_t filePath[MAX_PATH];
                DragQueryFileW(hDrop, i, filePath, MAX_PATH);
                DWORD attr = GetFileAttributesW(filePath);
                if (attr != INVALID_FILE_ATTRIBUTES) {
                    if (attr & FILE_ATTRIBUTE_DIRECTORY) {
                        Playlist_ScanFolder(filePath);
                    } else {
                        const wchar_t *ext = PathFindExtensionW(filePath);
                        if (_wcsicmp(ext, L".m3u") == 0 || _wcsicmp(ext, L".m3u8") == 0) {
                            Playlist_ImportM3U(filePath);
                        } else {
                            Playlist_Add(filePath);
                        }
                    }
                }
            }
            DragFinish(hDrop);
            TrimProcessMemory();
            if (g_selectedTrack < 0 && Playlist_Count() > 0) {
                g_selectedTrack = 0;
            }
            ApplyFilter(g_searchQuery);
            InvalidateRect(hWnd, NULL, FALSE);
            return 0;
        }

        case WM_MOUSEWHEEL: {
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            POINT pt = { LOWORD(lParam), HIWORD(lParam) };
            ScreenToClient(hWnd, &pt);

            if (PtInRect(&rcVolBar, pt) || pt.y >= rcSeekBar.top - 10) {
                int vol = Player_GetVolume() + (delta > 0 ? 5 : -5);
                Player_SetVolume(vol);
                g_cfg.volume = Player_GetVolume();
                Config_Save(&g_cfg, L"config.json");
                InvalidateVolumeArea(hWnd);
            } else {
                int total = g_isFiltered ? g_filterCount : Playlist_Count();
                RECT rcClient;
                GetClientRect(hWnd, &rcClient);
                int visible = (rcClient.bottom - 105 - 65) / GetItemHeight();
                int maxScroll = total - visible;
                if (maxScroll < 0) maxScroll = 0;

                g_scrollOffset -= (delta > 0 ? 2 : -2);
                if (g_scrollOffset < 0) g_scrollOffset = 0;
                if (g_scrollOffset > maxScroll) g_scrollOffset = maxScroll;
                InvalidateRect(hWnd, NULL, FALSE);
            }
            return 0;
        }

        case WM_MOUSEMOVE: {
            POINT pt = { LOWORD(lParam), HIWORD(lParam) };

            // Sağ kaydırma çubuğu sürükleme
            if (g_isDraggingScrollbar) {
                int trackH = rcScrollbarTrack.bottom - rcScrollbarTrack.top;
                int thumbH = rcScrollbarThumb.bottom - rcScrollbarThumb.top;
                int totalItems = g_isFiltered ? g_filterCount : Playlist_Count();
                RECT rcClient;
                GetClientRect(hWnd, &rcClient);
                int visibleItems = (rcClient.bottom - 105 - 65) / GetItemHeight();
                int maxScroll = totalItems - visibleItems;
                if (maxScroll > 0 && trackH > thumbH) {
                    int dy = pt.y - g_scrollbarDragStartY;
                    float dRatio = (float)dy / (float)(trackH - thumbH);
                    int newOffset = g_scrollbarDragStartOffset + (int)(dRatio * maxScroll);
                    if (newOffset < 0) newOffset = 0;
                    if (newOffset > maxScroll) newOffset = maxScroll;
                    if (newOffset != g_scrollOffset) {
                        g_scrollOffset = newOffset;
                        InvalidateRect(hWnd, NULL, FALSE);
                    }
                }
                return 0;
            }

            // Kaydırma çubuğu hover tespiti
            BOOL sbHover = PtInRect(&rcScrollbarTrack, pt);
            if (sbHover != g_scrollbarHover) {
                g_scrollbarHover = sbHover;
                InvalidateRect(hWnd, &rcScrollbarTrack, FALSE);
            }

            // İlerleme çubuğu sürükleme
            if (g_isDraggingSeek) {
                int duration = Player_GetDuration();
                if (duration > 0) {
                    float ratio = (float)(pt.x - rcSeekBar.left) / (float)(rcSeekBar.right - rcSeekBar.left);
                    if (ratio < 0.0f) ratio = 0.0f;
                    if (ratio > 1.0f) ratio = 1.0f;
                    g_seekDragSec = (int)(ratio * duration);
                    RECT rcClient;
                    GetClientRect(hWnd, &rcClient);
                    RECT rcSeekArea = { 0, rcClient.bottom - 102, rcClient.right, rcClient.bottom - 60 };
                    InvalidateRect(hWnd, &rcSeekArea, FALSE);
                }
                return 0;
            }

            // Ses çubuğu sürükleme
            if (g_isDraggingVol) {
                float ratio = (float)(pt.x - rcVolBar.left) / (float)(rcVolBar.right - rcVolBar.left);
                if (ratio < 0.0f) ratio = 0.0f;
                if (ratio > 1.0f) ratio = 1.0f;
                int vol = (int)(ratio * 100);
                Player_SetVolume(vol);
                g_cfg.volume = vol;
                InvalidateVolumeArea(hWnd);
                return 0;
            }

            // Liste içi şarkı sıralamasını değiştirme sürüklemesi
            if ((wParam & MK_LBUTTON) && g_dragFromIndex >= 0 && !g_isFiltered) {
                if (!g_isDraggingList) {
                    if (abs(pt.y - g_mouseDownPt.y) > 7) {
                        g_isDraggingList = TRUE;
                    }
                }
                if (g_isDraggingList) {
                    int listTop = 65;
                    int target = g_scrollOffset + ((pt.y - listTop) / GetItemHeight());
                    if (target < 0) target = 0;
                    if (target >= Playlist_Count()) target = Playlist_Count() - 1;
                    if (target != g_dragTargetIndex) {
                        g_dragTargetIndex = target;
                        InvalidateRect(hWnd, NULL, FALSE);
                    }
                    return 0;
                }
            }

            // Hamburger hover
            if (g_sidebarOpen && pt.x < SIDEBAR_WIDTH && pt.y >= 75) {
                int item = (pt.y - 75) / 48;
                if (item >= 0 && item < 6) {
                    if (g_sidebarHoverItem != item) {
                        g_sidebarHoverItem = item;
                        InvalidateRect(hWnd, NULL, FALSE);
                    }
                }
                return 0;
            } else if (g_sidebarHoverItem != -1) {
                g_sidebarHoverItem = -1;
                InvalidateRect(hWnd, NULL, FALSE);
            }

            // Liste item hover
            RECT rcClient;
            GetClientRect(hWnd, &rcClient);
            int listTop = 65;
            int listBottom = rcClient.bottom - 105;

            if (pt.y >= listTop && pt.y <= listBottom && pt.x >= 15 && pt.x <= rcClient.right - 24) {
                int displayIdx = g_scrollOffset + ((pt.y - listTop) / GetItemHeight());
                int totalCount = g_isFiltered ? g_filterCount : Playlist_Count();
                if (displayIdx < totalCount) {
                    int actualIdx = g_isFiltered ? g_filterIndices[displayIdx] : displayIdx;
                    if (actualIdx != g_hoverTrack) {
                        g_hoverTrack = actualIdx;
                        InvalidateRect(hWnd, NULL, FALSE);
                    }
                }
            } else if (g_hoverTrack != -1) {
                g_hoverTrack = -1;
                InvalidateRect(hWnd, NULL, FALSE);
            }
            return 0;
        }

        case WM_LBUTTONDOWN: {
            POINT pt = { LOWORD(lParam), HIWORD(lParam) };
            SetCapture(hWnd);

            // 1. Hamburger Aç / Kapa Butonu
            if (PtInRect(&rcHamburger, pt)) {
                g_sidebarOpen = !g_sidebarOpen;
                InvalidateRect(hWnd, NULL, FALSE);
                return 0;
            }

            // 2. Sidebar açıkken tıklama kontrolü
            if (g_sidebarOpen) {
                if (pt.x < SIDEBAR_WIDTH && pt.y >= 75) {
                    int item = (pt.y - 75) / 48;
                    g_sidebarOpen = FALSE;
                    InvalidateRect(hWnd, NULL, FALSE);

                    if (item == 0) SelectFolderDialog();
                    else if (item == 1) ImportM3UDialog();
                    else if (item == 2) ExportM3UDialog();
                    else if (item == 3) Search_ShowDialog(hWnd, g_cfg.lastFolder);
                    else if (item == 4) {
                        Playlist_Clear();
                        Player_Stop();
                        g_currentTrack = -1;
                        g_selectedTrack = -1;
                        ApplyFilter(L"");
                        InvalidateRect(hWnd, NULL, FALSE);
                    }
                    else if (item == 5) {
                        Updater_CheckAsync(hWnd, TRUE);
                    }
                    return 0;
                } else if (pt.x >= SIDEBAR_WIDTH) {
                    g_sidebarOpen = FALSE;
                    InvalidateRect(hWnd, NULL, FALSE);
                    return 0;
                }
            }

            // 3. Sağdaki Kaydırma Çubuğu (Scrollbar) Tıklaması
            if (PtInRect(&rcScrollbarTrack, pt)) {
                if (PtInRect(&rcScrollbarThumb, pt)) {
                    g_isDraggingScrollbar = TRUE;
                    g_scrollbarDragStartY = pt.y;
                    g_scrollbarDragStartOffset = g_scrollOffset;
                    InvalidateRect(hWnd, &rcScrollbarTrack, FALSE);
                } else {
                    int trackH = rcScrollbarTrack.bottom - rcScrollbarTrack.top;
                    int thumbH = rcScrollbarThumb.bottom - rcScrollbarThumb.top;
                    int totalItems = g_isFiltered ? g_filterCount : Playlist_Count();
                    RECT rcClient;
                    GetClientRect(hWnd, &rcClient);
                    int visibleItems = (rcClient.bottom - 105 - 65) / GetItemHeight();
                    int maxScroll = totalItems - visibleItems;
                    if (maxScroll > 0 && trackH > thumbH) {
                        float r = (float)(pt.y - rcScrollbarTrack.top - thumbH / 2) / (float)(trackH - thumbH);
                        if (r < 0.0f) r = 0.0f;
                        if (r > 1.0f) r = 1.0f;
                        g_scrollOffset = (int)(r * maxScroll);
                        g_isDraggingScrollbar = TRUE;
                        g_scrollbarDragStartY = pt.y;
                        g_scrollbarDragStartOffset = g_scrollOffset;
                        InvalidateRect(hWnd, NULL, FALSE);
                    }
                }
                return 0;
            }

            // 4. Oynatma Kontrolleri
            if (PtInRect(&rcPlay, pt)) {
                if (Player_IsPlaying()) {
                    Player_Pause();
                } else {
                    if (g_currentTrack >= 0) {
                        Player_Play();
                    } else if (Playlist_Count() > 0) {
                        PlayTrack(g_selectedTrack >= 0 ? g_selectedTrack : 0);
                    }
                }
                InvalidateRect(hWnd, NULL, FALSE);
                return 0;
            }
            if (PtInRect(&rcPrev, pt)) {
                int prev = Playlist_FindPrev(g_currentTrack);
                if (prev >= 0) PlayTrack(prev);
                return 0;
            }
            if (PtInRect(&rcNext, pt)) {
                int next = Playlist_FindNext(g_currentTrack, g_cfg.shuffle, g_cfg.repeat);
                if (next >= 0) PlayTrack(next);
                return 0;
            }

            // 5. Seek Bar (Genişletilmiş kolay tıklama alanı & anında atlama)
            if (pt.y >= rcSeekBar.top - 12 && pt.y <= rcSeekBar.bottom + 12 && pt.x >= rcSeekBar.left - 10 && pt.x <= rcSeekBar.right + 10) {
                int duration = Player_GetDuration();
                if (duration > 0) {
                    float ratio = (float)(pt.x - rcSeekBar.left) / (float)(rcSeekBar.right - rcSeekBar.left);
                    if (ratio < 0.0f) ratio = 0.0f;
                    if (ratio > 1.0f) ratio = 1.0f;
                    g_seekDragSec = (int)(ratio * duration);
                    g_isDraggingSeek = TRUE;
                    Player_SetPosition(g_seekDragSec);

                    RECT rcClient;
                    GetClientRect(hWnd, &rcClient);
                    RECT rcSeekArea = { 0, rcClient.bottom - 102, rcClient.right, rcClient.bottom - 60 };
                    InvalidateRect(hWnd, &rcSeekArea, FALSE);
                }
                return 0;
            }

            // 6. Ses Barı
            if (pt.y >= rcVolBar.top - 8 && pt.y <= rcVolBar.bottom + 8 && pt.x >= rcVolBar.left - 5 && pt.x <= rcVolBar.right + 5) {
                g_isDraggingVol = TRUE;
                float ratio = (float)(pt.x - rcVolBar.left) / (float)(rcVolBar.right - rcVolBar.left);
                if (ratio < 0.0f) ratio = 0.0f;
                if (ratio > 1.0f) ratio = 1.0f;
                int vol = (int)(ratio * 100);
                Player_SetVolume(vol);
                g_cfg.volume = vol;
                Config_Save(&g_cfg, L"config.json");
                InvalidateVolumeArea(hWnd);
                return 0;
            }

            // 7. Şarkı Listesi: TEK TIKLAMAYLA ANINDA ÇAL + SÜRÜKLEME BAŞLAT
            RECT rcClient;
            GetClientRect(hWnd, &rcClient);
            int listTop = 65;
            int listBottom = rcClient.bottom - 105;
            if (pt.y >= listTop && pt.y <= listBottom && pt.x >= 15 && pt.x <= rcClient.right - 24) {
                int displayIdx = g_scrollOffset + ((pt.y - listTop) / GetItemHeight());
                int totalCount = g_isFiltered ? g_filterCount : Playlist_Count();
                if (displayIdx < totalCount) {
                    int actualIdx = g_isFiltered ? g_filterIndices[displayIdx] : displayIdx;
                    g_dragFromIndex = actualIdx;
                    g_dragTargetIndex = actualIdx;
                    g_mouseDownPt = pt;
                    g_isDraggingList = FALSE;

                    // TEK TIKLAMAYLA ÇAL
                    PlayTrack(actualIdx);
                }
            }
            return 0;
        }

        case WM_LBUTTONUP: {
            ReleaseCapture();
            if (g_isDraggingScrollbar) {
                g_isDraggingScrollbar = FALSE;
                InvalidateRect(hWnd, &rcScrollbarTrack, FALSE);
            }
            if (g_isDraggingSeek) {
                g_isDraggingSeek = FALSE;
                Player_SetPosition(g_seekDragSec);
                RECT rcClient;
                GetClientRect(hWnd, &rcClient);
                RECT rcSeekArea = { 0, rcClient.bottom - 102, rcClient.right, rcClient.bottom - 60 };
                InvalidateRect(hWnd, &rcSeekArea, FALSE);
            }
            if (g_isDraggingVol) {
                g_isDraggingVol = FALSE;
                Config_Save(&g_cfg, L"config.json");
            }
            if (g_isDraggingList && g_dragFromIndex >= 0 && g_dragTargetIndex >= 0 && !g_isFiltered) {
                if (g_dragFromIndex != g_dragTargetIndex) {
                    Playlist_Move(g_dragFromIndex, g_dragTargetIndex);
                    if (g_currentTrack == g_dragFromIndex) {
                        g_currentTrack = g_dragTargetIndex;
                    } else if (g_currentTrack > g_dragFromIndex && g_currentTrack <= g_dragTargetIndex) {
                        g_currentTrack--;
                    } else if (g_currentTrack < g_dragFromIndex && g_currentTrack >= g_dragTargetIndex) {
                        g_currentTrack++;
                    }
                    g_selectedTrack = g_dragTargetIndex;
                }
                g_isDraggingList = FALSE;
                g_dragFromIndex = -1;
                g_dragTargetIndex = -1;
                InvalidateRect(hWnd, NULL, FALSE);
            } else {
                g_dragFromIndex = -1;
                g_dragTargetIndex = -1;
                g_isDraggingList = FALSE;
            }
            return 0;
        }

        case WM_KEYDOWN: {
            switch (wParam) {
                case VK_SPACE: {
                    if (Player_IsPlaying()) Player_Pause();
                    else if (g_currentTrack >= 0) Player_Play();
                    else if (Playlist_Count() > 0) PlayTrack(g_selectedTrack >= 0 ? g_selectedTrack : 0);
                    InvalidateRect(hWnd, NULL, FALSE);
                    return 0;
                }
                case VK_LEFT: {
                    int pos = Player_GetPosition() - 5;
                    Player_SetPosition(pos < 0 ? 0 : pos);
                    RECT rcClient;
                    GetClientRect(hWnd, &rcClient);
                    RECT rcSeekArea = { 0, rcClient.bottom - 102, rcClient.right, rcClient.bottom - 60 };
                    InvalidateRect(hWnd, &rcSeekArea, FALSE);
                    return 0;
                }
                case VK_RIGHT: {
                    int pos = Player_GetPosition() + 5;
                    int dur = Player_GetDuration();
                    Player_SetPosition(pos > dur ? dur : pos);
                    RECT rcClient;
                    GetClientRect(hWnd, &rcClient);
                    RECT rcSeekArea = { 0, rcClient.bottom - 102, rcClient.right, rcClient.bottom - 60 };
                    InvalidateRect(hWnd, &rcSeekArea, FALSE);
                    return 0;
                }
                case VK_UP: {
                    int vol = Player_GetVolume() + 5;
                    Player_SetVolume(vol);
                    g_cfg.volume = Player_GetVolume();
                    Config_Save(&g_cfg, L"config.json");
                    InvalidateVolumeArea(hWnd);
                    return 0;
                }
                case VK_DOWN: {
                    int vol = Player_GetVolume() - 5;
                    Player_SetVolume(vol);
                    g_cfg.volume = Player_GetVolume();
                    Config_Save(&g_cfg, L"config.json");
                    InvalidateVolumeArea(hWnd);
                    return 0;
                }
                case VK_RETURN: {
                    if (g_selectedTrack >= 0 && g_selectedTrack < Playlist_Count()) {
                        PlayTrack(g_selectedTrack);
                    }
                    return 0;
                }
                case VK_DELETE: {
                    if (g_selectedTrack >= 0 && g_selectedTrack < Playlist_Count()) {
                        if (g_selectedTrack == g_currentTrack) {
                            Player_Stop();
                            g_currentTrack = -1;
                        } else if (g_currentTrack > g_selectedTrack) {
                            g_currentTrack--;
                        }
                        Playlist_Remove(g_selectedTrack);
                        if (g_selectedTrack >= Playlist_Count()) {
                            g_selectedTrack = Playlist_Count() - 1;
                        }
                        ApplyFilter(g_searchQuery);
                        InvalidateRect(hWnd, NULL, FALSE);
                    }
                    return 0;
                }
                default: {
                    if ((wParam >= 'A' && wParam <= 'Z') || (wParam >= '0' && wParam <= '9')) {
                        int found = Playlist_FindByPrefix((wchar_t)wParam, g_selectedTrack);
                        if (found >= 0) {
                            g_selectedTrack = found;
                            RECT rcClient;
                            GetClientRect(hWnd, &rcClient);
                            int visible = (rcClient.bottom - 105 - 65) / GetItemHeight();
                            if (found < g_scrollOffset || found >= g_scrollOffset + visible) {
                                g_scrollOffset = found - visible / 2;
                                if (g_scrollOffset < 0) g_scrollOffset = 0;
                            }
                            InvalidateRect(hWnd, NULL, FALSE);
                        }
                    }
                    break;
                }
            }
            break;
        }

        case WM_GETMINMAXINFO: {
            MINMAXINFO *mmi = (MINMAXINFO*)lParam;
            mmi->ptMinTrackSize.x = 420;
            mmi->ptMinTrackSize.y = 520;
            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);

            RECT rcClient;
            GetClientRect(hWnd, &rcClient);
            if (rcClient.right <= 0 || rcClient.bottom <= 0) {
                EndPaint(hWnd, &ps);
                return 0;
            }

            // Çift Tamponlu Çizim
            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBitmap = CreateCompatibleBitmap(hdc, rcClient.right, rcClient.bottom);
            HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);

            RenderUI(memDC, &rcClient);

            BitBlt(hdc, 0, 0, rcClient.right, rcClient.bottom, memDC, 0, 0, SRCCOPY);

            SelectObject(memDC, oldBitmap);
            DeleteObject(memBitmap);
            DeleteDC(memDC);

            EndPaint(hWnd, &ps);
            return 0;
        }

        case WM_ERASEBKGND:
            return 1;

        case WM_CLOSE:
            DestroyWindow(hWnd);
            return 0;

        case WM_DESTROY: {
            KillTimer(hWnd, IDT_UPDATE_TIMER);
            Player_Shutdown();
            Playlist_Free();

            if (g_hFontTitle) DeleteObject(g_hFontTitle);
            if (g_hFontBold) DeleteObject(g_hFontBold);
            if (g_hFontMain) DeleteObject(g_hFontMain);
            if (g_hFontSmall) DeleteObject(g_hFontSmall);
            if (g_hBrSearchEdit) DeleteObject(g_hBrSearchEdit);
            if (g_pMainThumb) {
                g_pMainThumb->lpVtbl->Release(g_pMainThumb);
                g_pMainThumb = NULL;
            }
            if (g_hAppIcon) {
                DestroyIcon(g_hAppIcon);
                g_hAppIcon = NULL;
            }

            PostQuitMessage(0);
            return 0;
        }
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    // 1. Per-Monitor High-DPI Awareness (Windows %125/%150 ölçekteki bulanıklığı yok eder)
    typedef BOOL(WINAPI *SetDpiCtxProc)(DPI_AWARENESS_CONTEXT);
    HMODULE hUser = GetModuleHandleW(L"user32.dll");
    if (hUser) {
        SetDpiCtxProc pSetDpi = (SetDpiCtxProc)GetProcAddress(hUser, "SetProcessDpiAwarenessContext");
        if (pSetDpi) {
            pSetDpi(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        } else {
            SetProcessDPIAware();
        }
    }

    OleInitialize(NULL);

    SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS);

    INITCOMMONCONTROLSEX icex = { sizeof(icex), ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icex);

    HINSTANCE hInst = GetModuleHandleW(NULL);
    HICON hMainIcon = LoadIconW(hInst, MAKEINTRESOURCEW(1));
    HICON hSmallIcon = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(1), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);

    WNDCLASSEXW wc = { sizeof(wc) };
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = MainWndProc;
    wc.hInstance = hInst;
    wc.hIcon = hMainIcon ? hMainIcon : LoadIcon(NULL, IDI_APPLICATION);
    wc.hIconSm = hSmallIcon ? hSmallIcon : wc.hIcon;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = L"SeloPlayerNativeClass";

    RegisterClassExW(&wc);

    int screenDpi = 96;
    HDC hdcScreen = GetDC(NULL);
    if (hdcScreen) {
        screenDpi = GetDeviceCaps(hdcScreen, LOGPIXELSY);
        ReleaseDC(NULL, hdcScreen);
    }
    if (screenDpi <= 0) screenDpi = 96;

    int winW = MulDiv(520, screenDpi, 96);
    int winH = MulDiv(680, screenDpi, 96);

    HWND hWnd = CreateWindowExW(
        WS_EX_ACCEPTFILES,
        L"SeloPlayerNativeClass",
        L"Selo Player",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, winW, winH,
        NULL, NULL, hInst, NULL
    );

    if (!hWnd) {
        OleUninitialize();
        return 1;
    }

    if (hMainIcon) SendMessageW(hWnd, WM_SETICON, ICON_BIG, (LPARAM)hMainIcon);
    if (hSmallIcon) SendMessageW(hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hSmallIcon);

    int show = (nCmdShow == 0 || nCmdShow == SW_HIDE) ? SW_SHOWNORMAL : nCmdShow;
    ShowWindow(hWnd, show);
    UpdateWindow(hWnd);

    TrimProcessMemory();

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    OleUninitialize();
    return (int)msg.wParam;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    return wWinMain(hInstance, hPrevInstance, GetCommandLineW(), nCmdShow);
}
