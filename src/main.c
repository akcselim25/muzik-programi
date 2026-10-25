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
#include <math.h>
#include <stdlib.h>
#include "player.h"
#include "playlist.h"
#include "config.h"
#include "search.h"
#include "updater.h"
#include "network.h"
#include "friends.h"
#include "jam.h"

#ifndef EM_SETCUEBANNER
#define EM_SETCUEBANNER 0x1501
#endif

// Renk Paleti — Apple Dark Materials & Spotify Iconic Green
#define COLOR_BG            RGB(16, 16, 18)     // Saf Koyu Tuval (#101012)
#define COLOR_HEADER        RGB(22, 22, 25)     // Üst Bar Katmanı (#161619)
#define COLOR_DOCK          RGB(20, 20, 23)     // Alt Oynatıcı Dock (#141417)
#define COLOR_SIDEBAR       RGB(22, 22, 26)     // Yan Çekmece Katmanı (#16161A)
#define COLOR_CARD          RGB(28, 28, 33)     // Yükseltilmiş Kart
#define COLOR_ROW_HOVER     RGB(28, 28, 34)     // Şarkı Satırı Hover (Çerçevesiz Yumuşak Katman)
#define COLOR_ROW_ACTIVE    RGB(20, 38, 27)     // Çalan Şarkı Satırı (Spotify Yeşil Işıltılı Yüzey)
#define COLOR_ROW_SEL       RGB(26, 26, 32)     // Seçili Satır
#define COLOR_CTRL_HOVER    RGB(36, 36, 42)     // İkon Buton Hover Hapı
#define COLOR_CTRL_PRESS    RGB(46, 46, 54)     // İkon Buton Basılı Hapı
#define COLOR_ACCENT        RGB(29, 185, 84)    // Spotify Yeşili (#1DB954)
#define COLOR_ACCENT_HOV    RGB(30, 215, 96)    // Parlak Yeşil Hover (#1ED760)
#define COLOR_ACCENT_DARK   RGB(21, 148, 66)    // Basılı Koyu Yeşil (#159442)
#define COLOR_TEXT_WHITE    RGB(248, 248, 250)  // Apple Kırık Beyaz (Göz Yormayan Netlik)
#define COLOR_TEXT_MUTED    RGB(160, 160, 168)  // İkincil Sanatçı/Bilgi Metni
#define COLOR_TEXT_DIM      RGB(108, 108, 116)  // Üçüncül Numara/Zaman Metni
#define COLOR_BAR_BG        RGB(46, 46, 52)     // İlerleme/Ses Çubuğu Arka Planı
#define COLOR_BAR_HOVER     RGB(62, 62, 70)     // Çubuk Hover Arka Planı
#define COLOR_BORDER        RGB(38, 38, 44)     // Apple İnce Ayırıcı Çizgi (Hairline)
#define COLOR_SEARCH_PILL   RGB(32, 32, 37)     // Arama Kutusu İç Yüzeyi

#define IDT_UPDATE_TIMER    1001
#define IDT_ANIM_TIMER      1002
#define IDC_HEADER_SEARCH   1010
#define IDC_FRIEND_ADD_EDIT 1020

// Etkileşim Bölgeleri (Anında Pointer-Down & Hover Tepkisi İçin)
typedef enum {
    ZONE_NONE = 0,
    ZONE_HAMBURGER,
    ZONE_QUICK_DOWNLOAD,
    ZONE_SEARCH_CLEAR,
    ZONE_SHUFFLE,
    ZONE_PREV,
    ZONE_PLAY,
    ZONE_NEXT,
    ZONE_REPEAT,
    ZONE_SPEAKER,
    ZONE_SEEK,
    ZONE_VOL,
    ZONE_JAM_LEAVE,
    ZONE_TOAST_ACCEPT,
    ZONE_TOAST_REJECT,
    ZONE_FRIENDS_BACK,
    ZONE_COPY_TAG,
    ZONE_FTAB0,
    ZONE_FTAB1,
    ZONE_FTAB2,
    ZONE_FRIEND_SEND
} UiZone;

typedef enum {
    SIDEBAR_VIEW_MENU = 0,
    SIDEBAR_VIEW_FRIENDS
} SidebarView;

typedef enum {
    FRIENDS_TAB_LIST = 0,
    FRIENDS_TAB_REQUESTS,
    FRIENDS_TAB_ADD
} FriendsTab;

static int g_dpi = 96;
static inline int S(int px) { return MulDiv(px, g_dpi, 96); }
static inline int GetHeaderH(void)   { return S(58); }
static inline int GetDockH(void)     { return S(96); }
static inline int GetItemH(void)     { return S(54); }
static inline int GetSidebarW(void)  { return S(256); }

static HWND g_hWnd = NULL;
static HWND g_hSearchEdit = NULL;
static WNDPROC g_oldSearchEditProc = NULL;
static HBRUSH g_hBrSearchEdit = NULL;

static HFONT g_hFontTitle = NULL;
static HFONT g_hFontMain = NULL;
static HFONT g_hFontBold = NULL;
static HFONT g_hFontSmall = NULL;
static HFONT g_hFontSmallBold = NULL;
static HFONT g_hFontIcons = NULL;
static HFONT g_hFontIconsSmall = NULL;
static HFONT g_hFontIconsLarge = NULL;

// Kalıcı GDI Çift Tampon (Her WM_PAINT'te Bitmap oluşturup silme maliyetini sıfırlar!)
static HDC     g_backDC = NULL;
static HBITMAP g_backBmp = NULL;
static HBITMAP g_backOldBmp = NULL;
static int     g_backW = 0;
static int     g_backH = 0;

// Kalıcı 1x1 Karartma Katmanı (Sidebar Scrim AlphaBlend için sıfır tahsisat)
static HDC     g_scrimDC = NULL;
static HBITMAP g_scrimBmp = NULL;
static HBITMAP g_scrimOldBmp = NULL;

static IPicture *g_pMainThumb = NULL;
static HICON g_hAppIcon = NULL;
static AppConfig g_cfg;
static int g_currentTrack = -1;
static int g_selectedTrack = -1;
static int g_hoverTrack = -1;
static int g_preMuteVol = 80;

// Apple Yay Fiziği (60 FPS Kesintiye Uğratılabilir Kaydırma & Çekmece Animasyonu; Durduğunda %0.0 CPU!)
static float g_scrollPosPx = 0.0f;
static float g_scrollTargetPx = 0.0f;
static float g_scrollVelPx = 0.0f;

static BOOL  g_sidebarOpen = FALSE;
static float g_sidebarAnim = 0.0f;   // 0.0 = kapalı, 1.0 = açık
static float g_sidebarVel = 0.0f;
static int   g_sidebarHoverItem = -1;
static BOOL  g_animTimerActive = FALSE;
static DWORD g_lastAnimTick = 0;

static UiZone g_hotZone = ZONE_NONE;
static UiZone g_pressedZone = ZONE_NONE;

static BOOL g_isDraggingSeek = FALSE;
static int  g_seekDragSec = 0;
static BOOL g_isDraggingVol = FALSE;

// Kaydırma çubuğu durumu
static BOOL g_isDraggingScrollbar = FALSE;
static int  g_scrollbarDragGrabOffsetY = 0;
static BOOL g_scrollbarHover = FALSE;
static RECT rcScrollbarTrack;
static RECT rcScrollbarThumb;

// Canlı Şarkı Arama / Filtreleme
static int  *g_filterIndices = NULL;
static int   g_filterCap = 0;
static int   g_filterCount = 0;
static BOOL  g_isFiltered = FALSE;
static wchar_t g_searchQuery[128] = {0};

// Liste içi sürükle-bırak (sıralama değiştirme)
static int   g_dragFromIndex = -1;
static int   g_dragTargetIndex = -1;
static BOOL  g_isDraggingList = FALSE;
static POINT g_mouseDownPt = {0};

// Arkadaşlar & Jam durumu
static SidebarView g_sidebarView = SIDEBAR_VIEW_MENU;
static FriendsTab  g_friendsTab  = FRIENDS_TAB_LIST;
static HWND        g_hFriendAddEdit = NULL;
static WNDPROC     g_oldFriendAddEditProc = NULL;
static wchar_t     g_friendAddMsg[128] = {0};
static DWORD       g_copiedTagTick = 0;
static BOOL        g_isSyncingFromRemote = FALSE;

// Hit-test dikdörtgenleri
static RECT rcHamburger, rcQuickDownload, rcSearchBox, rcSearchClear;
static RECT rcShuffle, rcPrev, rcPlay, rcNext, rcRepeat;
static RECT rcSeekBar, rcSeekHit, rcVolBar, rcVolHit, rcSpeaker;
static RECT rcToastBox, rcToastAccept, rcToastReject;
static RECT rcJamPill, rcJamLeave;
static RECT rcFriendsBack, rcCopyTag;
static RECT rcFriendsTab0, rcFriendsTab1, rcFriendsTab2;
static RECT rcFriendSendBtn;
static RECT rcFriendActionBtns[MAX_FRIENDS];
static RECT rcReqAcceptBtns[MAX_REQUESTS];
static RECT rcReqRejectBtns[MAX_REQUESTS];

static int CALLBACK EnumFontCheckProc(const LOGFONTW *lpelfe, const TEXTMETRICW *lpntme, DWORD FontType, LPARAM lParam) {
    (void)lpelfe; (void)lpntme; (void)FontType;
    *(BOOL*)lParam = TRUE;
    return 0;
}

static BOOL FontExists(const wchar_t *faceName) {
    BOOL found = FALSE;
    LOGFONTW lf = {0};
    lf.lfCharSet = DEFAULT_CHARSET;
    wcsncpy(lf.lfFaceName, faceName, LF_FACESIZE - 1);
    HDC hdc = GetDC(NULL);
    if (hdc) {
        EnumFontFamiliesExW(hdc, &lf, (FONTENUMPROCW)EnumFontCheckProc, (LPARAM)&found, 0);
        ReleaseDC(NULL, hdc);
    }
    return found;
}

static void InitDpiFonts(int dpi) {
    if (dpi <= 0) dpi = 96;
    g_dpi = dpi;

    if (g_hFontTitle)      DeleteObject(g_hFontTitle);
    if (g_hFontBold)       DeleteObject(g_hFontBold);
    if (g_hFontMain)       DeleteObject(g_hFontMain);
    if (g_hFontSmall)      DeleteObject(g_hFontSmall);
    if (g_hFontSmallBold)  DeleteObject(g_hFontSmallBold);
    if (g_hFontIcons)      DeleteObject(g_hFontIcons);
    if (g_hFontIconsSmall) DeleteObject(g_hFontIconsSmall);
    if (g_hFontIconsLarge) DeleteObject(g_hFontIconsLarge);

    int sTitle     = -MulDiv(13, dpi, 72);
    int sMain      = -MulDiv(10, dpi, 72);
    int sSmall     = -MulDiv(9,  dpi, 72);
    int sIconSmall = -MulDiv(10, dpi, 72);
    int sIcon      = -MulDiv(13, dpi, 72);
    int sIconLarge = -MulDiv(16, dpi, 72);

    const wchar_t *uiFont   = FontExists(L"Segoe UI Variable Text") ? L"Segoe UI Variable Text" : L"Segoe UI";
    const wchar_t *dispFont = FontExists(L"Segoe UI Variable Display") ? L"Segoe UI Variable Display" : uiFont;
    const wchar_t *iconFont = FontExists(L"Segoe Fluent Icons") ? L"Segoe Fluent Icons" : L"Segoe MDL2 Assets";

    g_hFontTitle      = CreateFontW(sTitle,     0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, dispFont);
    g_hFontBold       = CreateFontW(sMain,      0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, uiFont);
    g_hFontMain       = CreateFontW(sMain,      0, 0, 0, FW_NORMAL,   FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, uiFont);
    g_hFontSmall      = CreateFontW(sSmall,     0, 0, 0, FW_NORMAL,   FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, uiFont);
    g_hFontSmallBold  = CreateFontW(sSmall,     0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, uiFont);
    g_hFontIconsSmall = CreateFontW(sIconSmall, 0, 0, 0, FW_NORMAL,   FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, iconFont);
    g_hFontIcons      = CreateFontW(sIcon,      0, 0, 0, FW_NORMAL,   FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, iconFont);
    g_hFontIconsLarge = CreateFontW(sIconLarge, 0, 0, 0, FW_NORMAL,   FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, iconFont);
}

static void EnsureBackbuffer(HDC refDC, int w, int h) {
    if (w <= 0 || h <= 0) return;
    if (g_backDC && g_backBmp && g_backW == w && g_backH == h) return;

    if (g_backDC) {
        if (g_backOldBmp) SelectObject(g_backDC, g_backOldBmp);
        if (g_backBmp) DeleteObject(g_backBmp);
        DeleteDC(g_backDC);
    }
    g_backDC = CreateCompatibleDC(refDC);
    g_backBmp = CreateCompatibleBitmap(refDC, w, h);
    g_backOldBmp = (HBITMAP)SelectObject(g_backDC, g_backBmp);
    g_backW = w;
    g_backH = h;

    if (!g_scrimDC) {
        BITMAPINFO bi = {0};
        bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth = 1;
        bi.bmiHeader.biHeight = -1;
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        RGBQUAD *px = NULL;
        g_scrimDC = CreateCompatibleDC(refDC);
        g_scrimBmp = CreateDIBSection(g_scrimDC, &bi, DIB_RGB_COLORS, (void**)&px, NULL, 0);
        if (g_scrimBmp && px) {
            px[0].rgbBlue = 0;
            px[0].rgbGreen = 0;
            px[0].rgbRed = 0;
            px[0].rgbReserved = 255;
            g_scrimOldBmp = (HBITMAP)SelectObject(g_scrimDC, g_scrimBmp);
        }
    }
}

static void FreeBackbuffer(void) {
    if (g_backDC) {
        if (g_backOldBmp) SelectObject(g_backDC, g_backOldBmp);
        if (g_backBmp) DeleteObject(g_backBmp);
        DeleteDC(g_backDC);
        g_backDC = NULL; g_backBmp = NULL; g_backOldBmp = NULL;
        g_backW = 0; g_backH = 0;
    }
    if (g_scrimDC) {
        if (g_scrimOldBmp) SelectObject(g_scrimDC, g_scrimOldBmp);
        if (g_scrimBmp) DeleteObject(g_scrimBmp);
        DeleteDC(g_scrimDC);
        g_scrimDC = NULL; g_scrimBmp = NULL; g_scrimOldBmp = NULL;
    }
}

static inline void FillFastRect(HDC hdc, const RECT *rc, COLORREF color) {
    COLORREF old = SetBkColor(hdc, color);
    ExtTextOutW(hdc, 0, 0, ETO_OPAQUE, rc, NULL, 0, NULL);
    SetBkColor(hdc, old);
}

static inline void DrawFastRoundRect(HDC hdc, const RECT *rc, COLORREF bg, COLORREF border, int radius) {
    HGDIOBJ oldBr = SelectObject(hdc, GetStockObject(DC_BRUSH));
    HGDIOBJ oldPen = SelectObject(hdc, GetStockObject(DC_PEN));
    SetDCBrushColor(hdc, bg);
    SetDCPenColor(hdc, border);
    RoundRect(hdc, rc->left, rc->top, rc->right, rc->bottom, radius, radius);
    SelectObject(hdc, oldBr);
    SelectObject(hdc, oldPen);
}

static inline void DrawFastLine(HDC hdc, int x1, int y1, int x2, int y2, COLORREF color) {
    HGDIOBJ oldPen = SelectObject(hdc, GetStockObject(DC_PEN));
    SetDCPenColor(hdc, color);
    MoveToEx(hdc, x1, y1, NULL);
    LineTo(hdc, x2, y2);
    SelectObject(hdc, oldPen);
}

static void DrawAACircle(HDC hdc, int cx, int cy, int radius, COLORREF color) {
    if (radius <= 0) return;
    int diam = radius * 2 + 2;
    BITMAPINFO bi = {0};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = diam;
    bi.bmiHeader.biHeight = -diam;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    RGBQUAD *pixels = NULL;
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP hBitmap = CreateDIBSection(memDC, &bi, DIB_RGB_COLORS, (void**)&pixels, NULL, 0);
    if (!hBitmap || !pixels) {
        if (memDC) DeleteDC(memDC);
        return;
    }

    HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, hBitmap);
    BYTE r = GetRValue(color);
    BYTE g = GetGValue(color);
    BYTE b = GetBValue(color);

    float center = (float)diam * 0.5f - 0.5f;
    float fRad   = (float)radius;
    float inR2   = (fRad - 0.7f) * (fRad - 0.7f);
    float outR2  = (fRad + 0.7f) * (fRad + 0.7f);

    for (int y = 0; y < diam; y++) {
        float dy = (float)y - center;
        float dy2 = dy * dy;
        RGBQUAD *row = &pixels[y * diam];
        for (int x = 0; x < diam; x++) {
            float dx = (float)x - center;
            float d2 = dx * dx + dy2;
            if (d2 >= outR2) {
                *(DWORD*)&row[x] = 0;
            } else if (d2 <= inR2) {
                row[x].rgbBlue     = b;
                row[x].rgbGreen    = g;
                row[x].rgbRed      = r;
                row[x].rgbReserved = 255;
            } else {
                float dist = sqrtf(d2);
                float alpha = (fRad + 0.7f - dist) * (1.0f / 1.4f);
                if (alpha < 0.0f) alpha = 0.0f;
                if (alpha > 1.0f) alpha = 1.0f;
                row[x].rgbReserved = (BYTE)(alpha * 255.0f);
                row[x].rgbRed      = (BYTE)(r * alpha);
                row[x].rgbGreen    = (BYTE)(g * alpha);
                row[x].rgbBlue     = (BYTE)(b * alpha);
            }
        }
    }

    BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    AlphaBlend(hdc, cx - diam / 2, cy - diam / 2, diam, diam, memDC, 0, 0, diam, diam, bf);

    SelectObject(memDC, oldBmp);
    DeleteObject(hBitmap);
    DeleteDC(memDC);
}

static int GetMaxScrollPx(HWND hWnd) {
    RECT rc;
    GetClientRect(hWnd, &rc);
    int listTop = GetHeaderH();
    int listBottom = rc.bottom - GetDockH();
    int viewH = listBottom - listTop;
    if (viewH < 0) viewH = 0;
    int totalCount = g_isFiltered ? g_filterCount : Playlist_Count();
    int contentH = totalCount * GetItemH() + S(12);
    int maxPx = contentH - viewH;
    return (maxPx > 0) ? maxPx : 0;
}

static void ClampScrollTargets(HWND hWnd) {
    float maxPx = (float)GetMaxScrollPx(hWnd);
    if (g_scrollTargetPx < 0.0f) g_scrollTargetPx = 0.0f;
    if (g_scrollTargetPx > maxPx) g_scrollTargetPx = maxPx;
    if (g_scrollPosPx < 0.0f) g_scrollPosPx = 0.0f;
    if (g_scrollPosPx > maxPx) g_scrollPosPx = maxPx;
}

static void UpdateSidebarEditPosition(void) {
    if (!g_hFriendAddEdit) return;
    BOOL shouldShow = (g_sidebarAnim > 0.02f && g_sidebarView == SIDEBAR_VIEW_FRIENDS && g_friendsTab == FRIENDS_TAB_ADD);
    if (!shouldShow) {
        if (IsWindowVisible(g_hFriendAddEdit)) ShowWindow(g_hFriendAddEdit, SW_HIDE);
        return;
    }
    int sbW = GetSidebarW();
    int offX = (int)((g_sidebarAnim - 1.0f) * (float)sbW);
    MoveWindow(g_hFriendAddEdit, offX + S(16), GetHeaderH() + S(182), sbW - S(32), S(24), TRUE);
    if (!IsWindowVisible(g_hFriendAddEdit)) ShowWindow(g_hFriendAddEdit, SW_SHOW);
}

static void StartAnimLoop(HWND hWnd) {
    if (!g_animTimerActive) {
        g_lastAnimTick = GetTickCount();
        SetTimer(hWnd, IDT_ANIM_TIMER, 15, NULL);
        g_animTimerActive = TRUE;
    }
}

// Apple Critically-Damped Spring Integrator (Kesintiye Uğratılabilir Fiziksel Hareket)
static BOOL StepSpring(float *pos, float *vel, float target, float omega, float damping, float dt, float eps) {
    float disp = *pos - target;
    if (fabsf(disp) <= eps && fabsf(*vel) <= eps * 8.0f) {
        *pos = target;
        *vel = 0.0f;
        return FALSE;
    }
    float accel = -omega * omega * disp - 2.0f * damping * omega * (*vel);
    *vel += accel * dt;
    *pos += (*vel) * dt;
    if (fabsf(*pos - target) <= eps && fabsf(*vel) <= eps * 8.0f) {
        *pos = target;
        *vel = 0.0f;
        return FALSE;
    }
    return TRUE;
}

static void CopyToClipboard(HWND hWnd, const wchar_t *text) {
    if (!text || !OpenClipboard(hWnd)) return;
    EmptyClipboard();
    size_t len = (wcslen(text) + 1) * sizeof(wchar_t);
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
    if (hMem) {
        void *ptr = GlobalLock(hMem);
        if (ptr) {
            memcpy(ptr, text, len);
            GlobalUnlock(hMem);
            SetClipboardData(CF_UNICODETEXT, hMem);
        }
    }
    CloseClipboard();
}

static void FormatTime(int totalSec, wchar_t *buf) {
    if (totalSec < 0) totalSec = 0;
    int m = totalSec / 60;
    int s = totalSec % 60;
    _snwprintf(buf, 15, L"%d:%02d", m, s);
    buf[15] = L'\0';
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
        if (total > g_filterCap) {
            int newCap = total + 256;
            int *nb = (int*)realloc(g_filterIndices, newCap * sizeof(int));
            if (nb) {
                g_filterIndices = nb;
                g_filterCap = newCap;
            }
        }
        for (int i = 0; i < total && g_filterCount < g_filterCap; i++) {
            const PlaylistItem *item = Playlist_Get(i);
            if (item && StrStrIW(item->title, query)) {
                g_filterIndices[g_filterCount++] = i;
            }
        }
    }
    g_scrollPosPx = 0.0f;
    g_scrollTargetPx = 0.0f;
    g_scrollVelPx = 0.0f;
    if (g_hWnd) InvalidateRect(g_hWnd, NULL, FALSE);
}

static void ScrollTrackIntoView(int trackIdx) {
    if (!g_hWnd || trackIdx < 0) return;
    int displayIdx = trackIdx;
    if (g_isFiltered) {
        displayIdx = -1;
        for (int i = 0; i < g_filterCount; i++) {
            if (g_filterIndices[i] == trackIdx) { displayIdx = i; break; }
        }
        if (displayIdx < 0) return;
    }

    RECT rc;
    GetClientRect(g_hWnd, &rc);
    int viewH = (rc.bottom - GetDockH()) - GetHeaderH();
    int itemH = GetItemH();
    float itemTop = (float)(displayIdx * itemH);
    float itemBot = itemTop + (float)itemH;

    if (itemTop < g_scrollTargetPx) {
        g_scrollTargetPx = itemTop;
        ClampScrollTargets(g_hWnd);
        StartAnimLoop(g_hWnd);
    } else if (itemBot > g_scrollTargetPx + (float)viewH - (float)S(8)) {
        g_scrollTargetPx = itemBot - (float)viewH + (float)S(12);
        ClampScrollTargets(g_hWnd);
        StartAnimLoop(g_hWnd);
    }
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

    static const wchar_t *exts[] = { L".jpg", L".jpeg", L".png" };
    for (int i = 0; i < 3; i++) {
        wchar_t candidate[MAX_PATH];
        _snwprintf(candidate, MAX_PATH - 1, L"%s%s", base, exts[i]);
        candidate[MAX_PATH - 1] = L'\0';
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
    if (Jam_IsActive() && !g_isSyncingFromRemote && item->title[0] != 0) {
        Jam_BroadcastPlay(item->title, 0);
    }
    InvalidateRect(g_hWnd, NULL, FALSE);
}

static void HandleJamSyncMessage(const char *topic, const char *payload) {
    (void)topic;
    if (!Jam_IsActive() || !payload) return;

    wchar_t myTag[32] = {0};
    Friends_GetMyTag(myTag, 32);
    char uMyTag[128] = {0};
    WideCharToMultiByte(CP_UTF8, 0, myTag, -1, uMyTag, 127, NULL, NULL);

    if (strncmp(payload, "SYNC_PLAY|", 10) == 0) {
        char sender[64] = {0};
        char track[128] = {0};
        int pos = 0;
        const char *p1 = payload + 10;
        const char *p2 = strchr(p1, '|');
        if (p2) {
            int sLen = (int)(p2 - p1);
            if (sLen > 63) sLen = 63;
            strncpy(sender, p1, sLen);
            if (strcmp(sender, uMyTag) == 0) return;

            const char *p3 = strchr(p2 + 1, '|');
            if (p3) {
                int tLen = (int)(p3 - (p2 + 1));
                if (tLen > 127) tLen = 127;
                strncpy(track, p2 + 1, tLen);
                pos = atoi(p3 + 1);
            } else {
                strncpy(track, p2 + 1, 127);
            }

            wchar_t wTrack[128] = {0};
            MultiByteToWideChar(CP_UTF8, 0, track, -1, wTrack, 127);

            BOOL isSame = FALSE;
            if (g_currentTrack >= 0 && g_currentTrack < Playlist_Count()) {
                if (_wcsicmp(Playlist_Get(g_currentTrack)->title, wTrack) == 0) {
                    isSame = TRUE;
                }
            }

            g_isSyncingFromRemote = TRUE;
            if (isSame) {
                int curPos = Player_GetPosition();
                if (abs(curPos - pos) > 2) {
                    Player_SetPosition(pos);
                }
                if (!Player_IsPlaying()) {
                    Player_Play();
                }
            } else {
                int found = -1;
                for (int i = 0; i < Playlist_Count(); i++) {
                    if (_wcsicmp(Playlist_Get(i)->title, wTrack) == 0 ||
                        StrStrIW(Playlist_Get(i)->title, wTrack) != NULL ||
                        StrStrIW(wTrack, Playlist_Get(i)->title) != NULL) {
                        found = i;
                        break;
                    }
                }
                if (found >= 0) {
                    PlayTrack(found);
                    if (pos > 0) Player_SetPosition(pos);
                }
            }
            g_isSyncingFromRemote = FALSE;
        }
    }
    else if (strncmp(payload, "SYNC_PAUSE|", 11) == 0) {
        char sender[64] = {0};
        const char *p1 = payload + 11;
        const char *p2 = strchr(p1, '|');
        if (p2) {
            int sLen = (int)(p2 - p1);
            if (sLen > 63) sLen = 63;
            strncpy(sender, p1, sLen);
            if (strcmp(sender, uMyTag) == 0) return;
            int pos = atoi(p2 + 1);
            g_isSyncingFromRemote = TRUE;
            if (Player_IsPlaying()) Player_Pause();
            if (pos >= 0) Player_SetPosition(pos);
            g_isSyncingFromRemote = FALSE;
        }
    }
    else if (strncmp(payload, "SYNC_SEEK|", 10) == 0) {
        char sender[64] = {0};
        const char *p1 = payload + 10;
        const char *p2 = strchr(p1, '|');
        if (p2) {
            int sLen = (int)(p2 - p1);
            if (sLen > 63) sLen = 63;
            strncpy(sender, p1, sLen);
            if (strcmp(sender, uMyTag) == 0) return;
            int pos = atoi(p2 + 1);
            g_isSyncingFromRemote = TRUE;
            if (pos >= 0) Player_SetPosition(pos);
            g_isSyncingFromRemote = FALSE;
        }
    }
    else if (strncmp(payload, "SYNC_LEAVE|", 11) == 0) {
        const char *sender = payload + 11;
        if (strcmp(sender, uMyTag) != 0) {
            Jam_Leave();
        }
    }
}

static void ToggleSidebar(BOOL open) {
    g_sidebarOpen = open;
    if (!open && g_hFriendAddEdit) {
        ShowWindow(g_hFriendAddEdit, SW_HIDE);
    }
    if (g_hWnd) StartAnimLoop(g_hWnd);
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
            g_currentTrack = -1;
            g_selectedTrack = Playlist_Count() > 0 ? 0 : -1;
            g_scrollPosPx = 0.0f;
            g_scrollTargetPx = 0.0f;
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

static void SubmitFriendRequest(HWND hWnd) {
    wchar_t inTag[64] = {0};
    GetWindowTextW(g_hFriendAddEdit, inTag, 63);
    StrTrimW(inTag, L" \t\r\n");
    int tlen = (int)wcslen(inTag);

    if (tlen == 0) {
        wcscpy(g_friendAddMsg, L"Lütfen bir kullanıcı etiketi yazın.");
    } else if (wcschr(inTag, L'#') == NULL) {
        wcscpy(g_friendAddMsg, L"Etiket '#' içermelidir (Örn: Selim#8421)");
    } else if (Friends_SendRequest(inTag)) {
        wcscpy(g_friendAddMsg, L"Arkadaşlık isteği gönderildi!");
        SetWindowTextW(g_hFriendAddEdit, L"");
    } else {
        wcscpy(g_friendAddMsg, L"İstek gönderilemedi. Bağlantıyı ve etiketi kontrol edin.");
    }
    InvalidateRect(hWnd, NULL, FALSE);
}

static void DrawIconButtonPill(HDC hdc, const RECT *rc, UiZone zone, const wchar_t *glyph, COLORREF activeColor, BOOL showActiveDot) {
    BOOL isHot = (g_hotZone == zone);
    BOOL isPress = (g_pressedZone == zone);
    if (isHot || isPress) {
        int cx = (rc->left + rc->right) / 2;
        int cy = (rc->top + rc->bottom) / 2;
        int rad = (rc->right - rc->left) / 2;
        DrawAACircle(hdc, cx, cy, rad, isPress ? COLOR_CTRL_PRESS : COLOR_CTRL_HOVER);
    }
    SetBkMode(hdc, TRANSPARENT);
    SelectObject(hdc, g_hFontIcons);
    SetTextColor(hdc, activeColor != 0 ? activeColor : (isHot ? COLOR_TEXT_WHITE : RGB(195, 195, 204)));
    RECT rcGlyph = *rc;
    if (isPress) OffsetRect(&rcGlyph, 0, 1);
    DrawTextW(hdc, glyph, -1, &rcGlyph, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    if (showActiveDot) {
        DrawAACircle(hdc, (rc->left + rc->right) / 2, rc->bottom - S(3), S(2), COLOR_ACCENT);
    }
}

static void RenderUI(HDC hdc, const RECT *clientRc) {
    int w = clientRc->right - clientRc->left;
    int h = clientRc->bottom - clientRc->top;
    int headerH = GetHeaderH();
    int dockH   = GetDockH();
    int itemH   = GetItemH();

    // 0. Tuval Arka Planı
    FillFastRect(hdc, clientRc, COLOR_BG);

    // =========================================================================
    // 1. ORTA BÖLGE: ŞARKI LİSTESİ (Kırpılmış & Pürüzsüz Piksel Yay Kaydırmalı)
    // =========================================================================
    int listTop    = headerH;
    int listBottom = h - dockH;
    int totalCount = g_isFiltered ? g_filterCount : Playlist_Count();

    int savedDC = SaveDC(hdc);
    IntersectClipRect(hdc, 0, listTop, w, listBottom);

    if (totalCount == 0) {
        RECT rcEmpty = { S(24), listTop, w - S(24), listBottom };
        SetBkMode(hdc, TRANSPARENT);
        SelectObject(hdc, g_hFontMain);
        SetTextColor(hdc, COLOR_TEXT_MUTED);
        if (g_isFiltered) {
            DrawTextW(hdc, L"Aramanızla eşleşen şarkı bulunamadı.", -1, &rcEmpty, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        } else {
            RECT rcEmptyTitle = { S(24), (listTop + listBottom) / 2 - S(36), w - S(24), (listTop + listBottom) / 2 - S(10) };
            RECT rcEmptySub   = { S(24), (listTop + listBottom) / 2 - S(6),  w - S(24), (listTop + listBottom) / 2 + S(48) };
            SelectObject(hdc, g_hFontTitle);
            SetTextColor(hdc, COLOR_TEXT_WHITE);
            DrawTextW(hdc, L"Kitaplığınız Boş", -1, &rcEmptyTitle, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(hdc, g_hFontSmall);
            SetTextColor(hdc, COLOR_TEXT_MUTED);
            DrawTextW(hdc, L"Üstteki '+ İndir' butonuyla YouTube'dan şarkı indirebilir,\nmenüden klasör seçebilir veya dosyaları buraya sürükleyebilirsiniz.", -1, &rcEmptySub, DT_CENTER | DT_TOP | DT_WORDBREAK);
        }
    } else {
        int scrollPx = (int)(g_scrollPosPx + 0.5f);
        int firstIdx = scrollPx / itemH;
        if (firstIdx < 0) firstIdx = 0;
        int endY = listBottom;
        int padX = S(12);
        int rowGap = S(3);

        for (int displayIdx = firstIdx; displayIdx < totalCount; displayIdx++) {
            int itemY = listTop + S(6) + displayIdx * itemH - scrollPx;
            if (itemY >= endY) break;
            if (itemY + itemH < listTop) continue;

            int actualIdx = g_isFiltered ? g_filterIndices[displayIdx] : displayIdx;
            RECT rcItem = { padX, itemY, w - S(18), itemY + itemH - rowGap };

            BOOL isPlayingTrack = (actualIdx == g_currentTrack);
            BOOL isSelected     = (actualIdx == g_selectedTrack);
            BOOL isHovered      = (actualIdx == g_hoverTrack && g_sidebarAnim < 0.1f);

            // Apple & Spotify Kuralı: Yalnızca etkileşimdeki veya aktif satırda yumuşak yüzey, sıfır çerçeve kalabalığı!
            if (isPlayingTrack) {
                DrawFastRoundRect(hdc, &rcItem, COLOR_ROW_ACTIVE, RGB(28, 66, 40), S(8));
                int stripW = S(3);
                if (stripW < 3) stripW = 3;
                RECT rcStrip = { rcItem.left + S(4), rcItem.top + S(11), rcItem.left + S(4) + stripW, rcItem.bottom - S(11) };
                DrawFastRoundRect(hdc, &rcStrip, COLOR_ACCENT, COLOR_ACCENT, S(2));
            } else if (isHovered) {
                DrawFastRoundRect(hdc, &rcItem, COLOR_ROW_HOVER, COLOR_ROW_HOVER, S(8));
            } else if (isSelected) {
                DrawFastRoundRect(hdc, &rcItem, COLOR_ROW_SEL, COLOR_ROW_SEL, S(8));
            }

            // Sürükle-bırak hedef göstergesi
            if (g_isDraggingList && actualIdx == g_dragTargetIndex && g_dragFromIndex != g_dragTargetIndex) {
                int lineY = (g_dragFromIndex < g_dragTargetIndex) ? rcItem.bottom + 1 : rcItem.top - 1;
                RECT rcLine = { rcItem.left + S(8), lineY - 1, rcItem.right - S(8), lineY + 1 };
                FillFastRect(hdc, &rcLine, COLOR_ACCENT);
            }

            // Sol: Sıra numarası veya Oynatma ikonu
            int numColW = S(40);
            RECT rcNum = { rcItem.left + S(6), rcItem.top, rcItem.left + numColW, rcItem.bottom };
            SetBkMode(hdc, TRANSPARENT);
            if (isPlayingTrack) {
                SelectObject(hdc, g_hFontIconsSmall);
                SetTextColor(hdc, COLOR_ACCENT_HOV);
                DrawTextW(hdc, Player_IsPlaying() ? L"\uE768" : L"\uE769", -1, &rcNum, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            } else if (isHovered) {
                SelectObject(hdc, g_hFontIconsSmall);
                SetTextColor(hdc, COLOR_TEXT_WHITE);
                DrawTextW(hdc, L"\uE768", -1, &rcNum, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            } else {
                SelectObject(hdc, g_hFontSmall);
                SetTextColor(hdc, COLOR_TEXT_DIM);
                wchar_t numBuf[16];
                _snwprintf(numBuf, 15, L"%d", displayIdx + 1);
                numBuf[15] = L'\0';
                DrawTextW(hdc, numBuf, -1, &rcNum, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }

            // Sağ: Şarkı Başlığı & Sanatçı (TAM SANATÇI İSMİ — KESİNLİKLE BOZULMAZ!)
            const PlaylistItem *item = Playlist_Get(actualIdx);
            if (item) {
                int textLeft  = rcItem.left + numColW + S(4);
                int textRight = rcItem.right - S(12);
                int rowMidY   = (rcItem.top + rcItem.bottom) / 2;

                const wchar_t *dash = wcsstr(item->title, L" - ");
                if (dash) {
                    int artistLen = (int)(dash - item->title);
                    wchar_t artistPart[256];
                    if (artistLen > 255) artistLen = 255;
                    if (artistLen < 0) artistLen = 0;
                    wcsncpy(artistPart, item->title, artistLen);
                    artistPart[artistLen] = L'\0';
                    const wchar_t *titlePart = dash + 3;

                    RECT rcSongTitle = { textLeft, rcItem.top + S(5), textRight, rowMidY + S(1) };
                    RECT rcArtist    = { textLeft, rowMidY + S(1), textRight, rcItem.bottom - S(5) };

                    SelectObject(hdc, g_hFontBold);
                    SetTextColor(hdc, isPlayingTrack ? COLOR_ACCENT_HOV : COLOR_TEXT_WHITE);
                    DrawTextW(hdc, titlePart, -1, &rcSongTitle, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

                    SelectObject(hdc, g_hFontSmall);
                    SetTextColor(hdc, isPlayingTrack ? RGB(175, 225, 190) : COLOR_TEXT_MUTED);
                    DrawTextW(hdc, artistPart, -1, &rcArtist, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                } else {
                    RECT rcName = { textLeft, rcItem.top, textRight, rcItem.bottom };
                    SelectObject(hdc, g_hFontBold);
                    SetTextColor(hdc, isPlayingTrack ? COLOR_ACCENT_HOV : COLOR_TEXT_WHITE);
                    DrawTextW(hdc, item->title, -1, &rcName, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                }
            }
        }
    }

    // Sağ Yüzen İnce Kaydırma Çubuğu (Apple macOS Overlay Scrollbar)
    int maxScrollPx = GetMaxScrollPx(g_hWnd);
    int sbW = (g_scrollbarHover || g_isDraggingScrollbar) ? S(7) : S(4);
    if (sbW < 4) sbW = 4;
    int sbRight  = w - S(4);
    int sbLeft   = sbRight - sbW;
    int sbTop    = listTop + S(6);
    int sbBottom = listBottom - S(6);
    rcScrollbarTrack = (RECT){ w - S(14), sbTop, w - S(2), sbBottom };

    if (maxScrollPx > 0 && sbBottom > sbTop) {
        int trackH = sbBottom - sbTop;
        int viewH  = listBottom - listTop;
        int contentH = viewH + maxScrollPx;
        int thumbH = (int)((float)viewH / (float)contentH * (float)trackH);
        if (thumbH < S(28)) thumbH = S(28);
        if (thumbH > trackH) thumbH = trackH;

        float ratio = g_scrollPosPx / (float)maxScrollPx;
        if (ratio < 0.0f) ratio = 0.0f;
        if (ratio > 1.0f) ratio = 1.0f;
        int thumbY = sbTop + (int)(ratio * (float)(trackH - thumbH));
        rcScrollbarThumb = (RECT){ sbLeft, thumbY, sbRight, thumbY + thumbH };

        COLORREF thumbCol = g_isDraggingScrollbar ? COLOR_ACCENT : (g_scrollbarHover ? RGB(130, 130, 140) : RGB(68, 68, 76));
        DrawFastRoundRect(hdc, &rcScrollbarThumb, thumbCol, thumbCol, sbW);
    } else {
        rcScrollbarThumb = (RECT){0};
    }

    RestoreDC(hdc, savedDC);

    // =========================================================================
    // 2. ÜST HEADER BAR (Temiz Apple Kimliği + Hızlı İndir + Yuvarlak Arama Hapı)
    // =========================================================================
    RECT rcHeader = { 0, 0, w, headerH };
    FillFastRect(hdc, &rcHeader, COLOR_HEADER);
    DrawFastLine(hdc, 0, headerH - 1, w, headerH - 1, COLOR_BORDER);

    // Sol: Hamburger Menüsü (+ Bekleyen arkadaş isteği yeşil nokta göstergesi)
    int hbSize = S(34);
    int hbY = (headerH - hbSize) / 2;
    rcHamburger = (RECT){ S(12), hbY, S(12) + hbSize, hbY + hbSize };
    {
        COLORREF hbBg = g_sidebarOpen ? COLOR_ACCENT : (g_pressedZone == ZONE_HAMBURGER ? COLOR_CTRL_PRESS : (g_hotZone == ZONE_HAMBURGER ? COLOR_CTRL_HOVER : COLOR_CARD));
        DrawFastRoundRect(hdc, &rcHamburger, hbBg, g_sidebarOpen ? COLOR_ACCENT : COLOR_BORDER, S(8));
        SetBkMode(hdc, TRANSPARENT);
        SelectObject(hdc, g_hFontIcons);
        SetTextColor(hdc, g_sidebarOpen ? RGB(10, 10, 12) : COLOR_TEXT_WHITE);
        DrawTextW(hdc, L"\uE700", -1, &rcHamburger, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        if (!g_sidebarOpen && Friends_GetPendingCount() > 0) {
            DrawAACircle(hdc, rcHamburger.right - S(3), rcHamburger.top + S(3), S(4), COLOR_ACCENT_HOV);
        }
    }

    // Sağ: Arama Hapı + Hızlı '+ İndir' Butonu
    int searchW = S(188);
    int searchH = S(32);
    int searchX = w - searchW - S(12);
    int searchY = (headerH - searchH) / 2;
    rcSearchBox = (RECT){ searchX, searchY, searchX + searchW, searchY + searchH };
    DrawFastRoundRect(hdc, &rcSearchBox, COLOR_SEARCH_PILL, g_isFiltered ? COLOR_ACCENT : COLOR_BORDER, S(16));

    RECT rcMag = { searchX + S(8), searchY, searchX + S(28), searchY + searchH };
    SetBkMode(hdc, TRANSPARENT);
    SelectObject(hdc, g_hFontIconsSmall);
    SetTextColor(hdc, g_isFiltered ? COLOR_ACCENT : COLOR_TEXT_MUTED);
    DrawTextW(hdc, L"\uE721", -1, &rcMag, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    if (g_searchQuery[0]) {
        rcSearchClear = (RECT){ rcSearchBox.right - S(26), searchY + S(4), rcSearchBox.right - S(4), searchY + searchH - S(4) };
        SelectObject(hdc, g_hFontIconsSmall);
        SetTextColor(hdc, (g_hotZone == ZONE_SEARCH_CLEAR) ? COLOR_TEXT_WHITE : COLOR_TEXT_MUTED);
        DrawTextW(hdc, L"\uE711", -1, &rcSearchClear, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    } else {
        rcSearchClear = (RECT){0};
    }

    // Hızlı '+ İndir' Butonu (Arama kutusunun hemen solunda)
    int dlBtnW = S(76);
    int dlBtnX = searchX - dlBtnW - S(8);
    rcQuickDownload = (RECT){ dlBtnX, searchY, dlBtnX + dlBtnW, searchY + searchH };
    {
        BOOL isHot = (g_hotZone == ZONE_QUICK_DOWNLOAD);
        BOOL isPress = (g_pressedZone == ZONE_QUICK_DOWNLOAD);
        COLORREF dlBg = isPress ? COLOR_ACCENT_DARK : (isHot ? RGB(28, 52, 36) : COLOR_CARD);
        COLORREF dlBorder = (isHot || isPress) ? COLOR_ACCENT : COLOR_BORDER;
        DrawFastRoundRect(hdc, &rcQuickDownload, dlBg, dlBorder, S(16));
        SelectObject(hdc, g_hFontSmallBold);
        SetTextColor(hdc, isPress ? RGB(10, 10, 12) : COLOR_ACCENT_HOV);
        DrawTextW(hdc, L"+ İndir", -1, &rcQuickDownload, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    // Sol-Orta: Uygulama Başlığı ve Kitaplık Özeti (Alt bar ile kendini tekrar etmez!)
    int titleRight = dlBtnX - S(10);
    if (titleRight > rcHamburger.right + S(12)) {
        RECT rcAppTitle = { rcHamburger.right + S(12), S(8), titleRight, S(31) };
        RECT rcAppSub   = { rcHamburger.right + S(12), S(31), titleRight, headerH - S(7) };
        SetBkMode(hdc, TRANSPARENT);
        SelectObject(hdc, g_hFontTitle);
        SetTextColor(hdc, COLOR_TEXT_WHITE);
        DrawTextW(hdc, L"Selo Player", -1, &rcAppTitle, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        wchar_t subBuf[128];
        if (g_isFiltered) {
            _snwprintf(subBuf, 127, L"%d sonuç gösteriliyor", g_filterCount);
        } else {
            _snwprintf(subBuf, 127, L"%d şarkı kitaplıkta", Playlist_Count());
        }
        subBuf[127] = L'\0';
        SelectObject(hdc, g_hFontSmall);
        SetTextColor(hdc, COLOR_TEXT_MUTED);
        DrawTextW(hdc, subBuf, -1, &rcAppSub, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }

    // =========================================================================
    // 3. ALT OYNATICI DOCK (Apple Music & Spotify Kusursuz 3 Bölgeli Kontroller)
    // =========================================================================
    int dockTop = h - dockH;
    RECT rcDock = { 0, dockTop, w, h };
    FillFastRect(hdc, &rcDock, COLOR_DOCK);
    DrawFastLine(hdc, 0, dockTop, w, dockTop, COLOR_BORDER);

    // Üst Kısım: Etkileşimli İnce Scrubber Seek Bar (Hover'da büyüyen Apple/Spotify çubuğu)
    int seekY = dockTop + S(16);
    int timeW = S(44);
    int seekLeft  = S(14) + timeW;
    int seekRight = w - S(14) - timeW;
    BOOL seekActive = (g_hotZone == ZONE_SEEK || g_isDraggingSeek);
    int seekBarH = seekActive ? S(6) : S(4);
    if (seekBarH < 4) seekBarH = 4;

    rcSeekBar = (RECT){ seekLeft, seekY - seekBarH / 2, seekRight, seekY + (seekBarH - seekBarH / 2) };
    rcSeekHit = (RECT){ seekLeft - S(8), seekY - S(10), seekRight + S(8), seekY + S(10) };

    DrawFastRoundRect(hdc, &rcSeekBar, seekActive ? COLOR_BAR_HOVER : COLOR_BAR_BG, seekActive ? COLOR_BAR_HOVER : COLOR_BAR_BG, seekBarH);

    int duration = Player_GetDuration();
    int position = g_isDraggingSeek ? g_seekDragSec : Player_GetPosition();
    if (position < 0) position = 0;
    if (duration > 0 && position > duration) position = duration;

    float progRatio = (duration > 0) ? ((float)position / (float)duration) : 0.0f;
    if (progRatio < 0.0f) progRatio = 0.0f;
    if (progRatio > 1.0f) progRatio = 1.0f;

    int seekTrackW = rcSeekBar.right - rcSeekBar.left;
    int seekFillW  = (int)(seekTrackW * progRatio);
    if (seekFillW > 0) {
        RECT rcFill = { rcSeekBar.left, rcSeekBar.top, rcSeekBar.left + seekFillW, rcSeekBar.bottom };
        COLORREF fillCol = seekActive ? COLOR_ACCENT_HOV : COLOR_ACCENT;
        DrawFastRoundRect(hdc, &rcFill, fillCol, fillCol, seekBarH);
    }

    if ((duration > 0 && seekActive) || g_isDraggingSeek) {
        int tx = rcSeekBar.left + seekFillW;
        if (tx < rcSeekBar.left) tx = rcSeekBar.left;
        if (tx > rcSeekBar.right) tx = rcSeekBar.right;
        DrawAACircle(hdc, tx, seekY, g_isDraggingSeek ? S(7) : S(6), COLOR_TEXT_WHITE);
    }

    wchar_t tPos[16], tDur[16];
    FormatTime(position, tPos);
    FormatTime(duration, tDur);
    SetBkMode(hdc, TRANSPARENT);
    SelectObject(hdc, g_hFontSmall);
    SetTextColor(hdc, COLOR_TEXT_DIM);

    RECT rcTPos = { S(10), seekY - S(9), seekLeft - S(6), seekY + S(9) };
    RECT rcTDur = { seekRight + S(6), seekY - S(9), w - S(10), seekY + S(9) };
    DrawTextW(hdc, tPos, -1, &rcTPos, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    DrawTextW(hdc, tDur, -1, &rcTDur, DT_LEFT  | DT_VCENTER | DT_SINGLELINE);

    // Alt Kısım: 3 Bölgeli Kontroller
    int rowCenterY = dockTop + S(58);
    int centerX    = w / 2;

    // --- BÖLGE 2 (MERKEZ): Çerçevesiz Zarif Medya İkonları + Pürüzsüz Yeşil Play Dairesi ---
    int playRad = (g_pressedZone == ZONE_PLAY) ? S(19) : (g_hotZone == ZONE_PLAY ? S(21) : S(20));
    int iconBox = S(32);

    rcPlay    = (RECT){ centerX - S(22), rowCenterY - S(22), centerX + S(22), rowCenterY + S(22) };
    rcPrev    = (RECT){ centerX - S(60), rowCenterY - iconBox / 2, centerX - S(60) + iconBox, rowCenterY + iconBox / 2 };
    rcShuffle = (RECT){ centerX - S(96), rowCenterY - iconBox / 2, centerX - S(96) + iconBox, rowCenterY + iconBox / 2 };
    rcNext    = (RECT){ centerX + S(60) - iconBox, rowCenterY - iconBox / 2, centerX + S(60), rowCenterY + iconBox / 2 };
    rcRepeat  = (RECT){ centerX + S(96) - iconBox, rowCenterY - iconBox / 2, centerX + S(96), rowCenterY + iconBox / 2 };

    DrawIconButtonPill(hdc, &rcShuffle, ZONE_SHUFFLE, L"\uE8B1", g_cfg.shuffle ? COLOR_ACCENT_HOV : 0, g_cfg.shuffle != 0);
    DrawIconButtonPill(hdc, &rcPrev,    ZONE_PREV,    L"\uE892", 0, FALSE);

    // Ana Hero Oynat/Duraklat Dairesi
    COLORREF playCol = (g_pressedZone == ZONE_PLAY) ? COLOR_ACCENT_DARK : ((g_hotZone == ZONE_PLAY) ? COLOR_ACCENT_HOV : COLOR_ACCENT);
    DrawAACircle(hdc, centerX, rowCenterY, playRad, playCol);
    SetBkMode(hdc, TRANSPARENT);
    SelectObject(hdc, g_hFontIconsLarge);
    SetTextColor(hdc, RGB(10, 10, 12));
    if (Player_IsPlaying()) {
        DrawTextW(hdc, L"\uE769", -1, &rcPlay, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    } else {
        RECT rcPlayGlyph = rcPlay;
        OffsetRect(&rcPlayGlyph, S(1), 0);
        DrawTextW(hdc, L"\uE768", -1, &rcPlayGlyph, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    DrawIconButtonPill(hdc, &rcNext,   ZONE_NEXT,   L"\uE893", 0, FALSE);
    DrawIconButtonPill(hdc, &rcRepeat, ZONE_REPEAT, (g_cfg.repeat == 2) ? L"\uE8ED" : L"\uE8EE", (g_cfg.repeat > 0) ? COLOR_ACCENT_HOV : 0, g_cfg.repeat > 0);

    // --- BÖLGE 1 (SOL): Kapak Resmi (HALFTONE Netlik) + Şarkı & Sanatçı + Jam Rozeti ---
    int coverSz = S(44);
    RECT rcThumbBox = { S(14), rowCenterY - coverSz / 2, S(14) + coverSz, rowCenterY + coverSz / 2 };
    DrawFastRoundRect(hdc, &rcThumbBox, COLOR_CARD, COLOR_BORDER, S(8));

    if (g_currentTrack >= 0 && g_currentTrack < Playlist_Count() && g_pMainThumb) {
        OLE_XSIZE_HIMETRIC cx, cy;
        g_pMainThumb->lpVtbl->get_Width(g_pMainThumb, &cx);
        g_pMainThumb->lpVtbl->get_Height(g_pMainThumb, &cy);
        int oldMode = SetStretchBltMode(hdc, HALFTONE);
        SetBrushOrgEx(hdc, 0, 0, NULL);
        g_pMainThumb->lpVtbl->Render(g_pMainThumb, hdc, rcThumbBox.left + 1, rcThumbBox.top + 1, coverSz - 2, coverSz - 2, 0, cy, cx, -cy, NULL);
        SetStretchBltMode(hdc, oldMode);
    } else if (g_hAppIcon) {
        int icSz = S(32);
        int icPad = (coverSz - icSz) / 2;
        DrawIconEx(hdc, rcThumbBox.left + icPad, rcThumbBox.top + icPad, g_hAppIcon, icSz, icSz, 0, NULL, DI_NORMAL);
    }

    int metaLeft  = rcThumbBox.right + S(10);
    int metaRight = rcShuffle.left - S(10);
    if (metaRight > metaLeft + S(20)) {
        if (Jam_IsActive()) {
            wchar_t pName[64] = {0};
            Jam_GetPartnerName(pName, 64);
            int pillW = metaRight - metaLeft;
            if (pillW > S(148)) pillW = S(148);
            rcJamPill = (RECT){ metaLeft, rowCenterY - S(23), metaLeft + pillW, rowCenterY - S(7) };
            DrawFastRoundRect(hdc, &rcJamPill, RGB(18, 36, 24), COLOR_ACCENT, S(8));

            DrawAACircle(hdc, rcJamPill.left + S(9), (rcJamPill.top + rcJamPill.bottom) / 2, S(3), COLOR_ACCENT_HOV);
            RECT rcJT = { rcJamPill.left + S(16), rcJamPill.top, rcJamPill.right - S(36), rcJamPill.bottom };
            SelectObject(hdc, g_hFontSmall);
            SetTextColor(hdc, COLOR_ACCENT_HOV);
            DrawTextW(hdc, pName[0] ? pName : L"Jam", -1, &rcJT, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

            rcJamLeave = (RECT){ rcJamPill.right - S(35), rcJamPill.top, rcJamPill.right - S(3), rcJamPill.bottom };
            SetTextColor(hdc, (g_hotZone == ZONE_JAM_LEAVE) ? RGB(255, 160, 160) : RGB(255, 110, 110));
            DrawTextW(hdc, L"Ayrıl", -1, &rcJamLeave, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        } else {
            rcJamPill = (RECT){0};
            rcJamLeave = (RECT){0};
        }

        int infoTop = Jam_IsActive() ? (rowCenterY - S(6)) : (rowCenterY - S(19));
        RECT rcTrackTitle  = { metaLeft, infoTop, metaRight, infoTop + S(19) };
        RECT rcTrackArtist = { metaLeft, infoTop + S(18), metaRight, infoTop + S(36) };

        if (g_currentTrack >= 0 && g_currentTrack < Playlist_Count()) {
            const PlaylistItem *cur = Playlist_Get(g_currentTrack);
            const wchar_t *dash = wcsstr(cur->title, L" - ");
            if (dash) {
                int aLen = (int)(dash - cur->title);
                wchar_t aPart[256];
                if (aLen > 255) aLen = 255;
                if (aLen < 0) aLen = 0;
                wcsncpy(aPart, cur->title, aLen);
                aPart[aLen] = L'\0';

                SelectObject(hdc, g_hFontBold);
                SetTextColor(hdc, COLOR_TEXT_WHITE);
                DrawTextW(hdc, dash + 3, -1, &rcTrackTitle, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

                SelectObject(hdc, g_hFontSmall);
                SetTextColor(hdc, COLOR_TEXT_MUTED);
                DrawTextW(hdc, aPart, -1, &rcTrackArtist, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            } else {
                SelectObject(hdc, g_hFontBold);
                SetTextColor(hdc, COLOR_TEXT_WHITE);
                DrawTextW(hdc, cur->title, -1, &rcTrackTitle, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

                SelectObject(hdc, g_hFontSmall);
                SetTextColor(hdc, COLOR_TEXT_MUTED);
                DrawTextW(hdc, Player_IsPlaying() ? L"Şu an çalıyor" : L"Duraklatıldı", -1, &rcTrackArtist, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            }
        } else {
            SelectObject(hdc, g_hFontBold);
            SetTextColor(hdc, COLOR_TEXT_WHITE);
            DrawTextW(hdc, L"Hazır", -1, &rcTrackTitle, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            SelectObject(hdc, g_hFontSmall);
            SetTextColor(hdc, COLOR_TEXT_DIM);
            DrawTextW(hdc, L"Bir şarkıya tıklayın", -1, &rcTrackArtist, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        }
    }

    // --- BÖLGE 3 (SAĞ): Sessize Alma (Mute) İkonu + Etkileşimli Ses Çubuğu ---
    int volW = S(76);
    int volRight = w - S(16);
    int volLeft  = volRight - volW;
    if (volLeft > rcRepeat.right + S(34)) {
        BOOL volActive = (g_hotZone == ZONE_VOL || g_isDraggingVol);
        int vBarH = volActive ? S(6) : S(4);
        if (vBarH < 4) vBarH = 4;

        rcVolBar = (RECT){ volLeft, rowCenterY - vBarH / 2, volRight, rowCenterY + (vBarH - vBarH / 2) };
        rcVolHit = (RECT){ volLeft - S(6), rowCenterY - S(12), volRight + S(6), rowCenterY + S(12) };
        rcSpeaker = (RECT){ volLeft - S(32), rowCenterY - S(15), volLeft - S(4), rowCenterY + S(15) };

        int curVol = Player_GetVolume();
        const wchar_t *spkGlyph = (curVol == 0) ? L"\uE74F" : (curVol < 50 ? L"\uE993" : L"\uE767");
        DrawIconButtonPill(hdc, &rcSpeaker, ZONE_SPEAKER, spkGlyph, (curVol == 0) ? RGB(255, 110, 110) : 0, FALSE);

        DrawFastRoundRect(hdc, &rcVolBar, volActive ? COLOR_BAR_HOVER : COLOR_BAR_BG, volActive ? COLOR_BAR_HOVER : COLOR_BAR_BG, vBarH);
        int vFillW = (int)((rcVolBar.right - rcVolBar.left) * (curVol / 100.0f));
        if (vFillW > 0) {
            RECT rcVFill = { rcVolBar.left, rcVolBar.top, rcVolBar.left + vFillW, rcVolBar.bottom };
            COLORREF vCol = volActive ? COLOR_ACCENT_HOV : COLOR_TEXT_WHITE;
            DrawFastRoundRect(hdc, &rcVFill, vCol, vCol, vBarH);
        }
        if (volActive) {
            DrawAACircle(hdc, rcVolBar.left + vFillW, rowCenterY, S(5), COLOR_TEXT_WHITE);
        }
    }

    // =========================================================================
    // 4. APPLE AKIŞKAN YAN ÇEKMECE + YARI SAYDAM KARARTMA PERDESİ (SCRIM)
    // =========================================================================
    if (g_sidebarAnim > 0.005f) {
        int sbW = GetSidebarW();
        int offX = (int)((g_sidebarAnim - 1.0f) * (float)sbW);

        // Arka planı orantılı karart (Apple Design: "Dim to focus, separate to keep flow")
        if (g_scrimDC && g_scrimBmp) {
            BYTE scrimAlpha = (BYTE)(g_sidebarAnim * 120.0f);
            BLENDFUNCTION bf = { AC_SRC_OVER, 0, scrimAlpha, 0 };
            AlphaBlend(hdc, 0, headerH, w, h - headerH, g_scrimDC, 0, 0, 1, 1, bf);
        }

        int savedSide = SaveDC(hdc);
        IntersectClipRect(hdc, 0, headerH, offX + sbW + 1, h);

        RECT rcSide = { offX, headerH, offX + sbW, h };
        FillFastRect(hdc, &rcSide, COLOR_SIDEBAR);
        DrawFastLine(hdc, offX + sbW, headerH, offX + sbW, h, COLOR_BORDER);

        if (g_sidebarView == SIDEBAR_VIEW_MENU) {
            static const struct { const wchar_t *icon; const wchar_t *label; BOOL accent; } items[] = {
                { L"\uE8B7", L"Klasör Seç",             FALSE },
                { L"\uE896", L"Müzik Ara ve İndir",     TRUE  },
                { L"\uE716", L"Arkadaşlar ve Jam",      TRUE  },
                { L"\uE8E5", L"M3U İçe Aktar",          FALSE },
                { L"\uE74E", L"M3U Dışa Aktar",         FALSE },
                { L"\uE74D", L"Listeyi Temizle",        FALSE },
                { L"\uE72C", L"Güncellemeleri Denetle", FALSE }
            };
            int menuCount = 7;

            for (int m = 0; m < menuCount; m++) {
                int itemTop = headerH + S(12) + m * S(44);
                RECT rcMenu = { offX + S(10), itemTop, offX + sbW - S(10), itemTop + S(38) };
                BOOL isHov = (m == g_sidebarHoverItem);
                if (isHov) {
                    DrawFastRoundRect(hdc, &rcMenu, COLOR_CTRL_HOVER, COLOR_BORDER, S(8));
                }

                RECT rcIc = { rcMenu.left + S(10), rcMenu.top, rcMenu.left + S(36), rcMenu.bottom };
                SetBkMode(hdc, TRANSPARENT);
                SelectObject(hdc, g_hFontIcons);
                SetTextColor(hdc, items[m].accent ? COLOR_ACCENT_HOV : (isHov ? COLOR_TEXT_WHITE : COLOR_TEXT_MUTED));
                DrawTextW(hdc, items[m].icon, -1, &rcIc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

                RECT rcTxt = { rcMenu.left + S(40), rcMenu.top, rcMenu.right - S(34), rcMenu.bottom };
                SelectObject(hdc, items[m].accent ? g_hFontBold : g_hFontMain);
                SetTextColor(hdc, items[m].accent ? COLOR_ACCENT_HOV : COLOR_TEXT_WHITE);
                DrawTextW(hdc, items[m].label, -1, &rcTxt, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

                if (m == 2 && Friends_GetPendingCount() > 0) {
                    wchar_t bTxt[16];
                    _snwprintf(bTxt, 15, L"%d", Friends_GetPendingCount());
                    bTxt[15] = L'\0';
                    RECT rcBadge = { rcMenu.right - S(30), rcMenu.top + S(9), rcMenu.right - S(10), rcMenu.bottom - S(9) };
                    DrawFastRoundRect(hdc, &rcBadge, COLOR_ACCENT, COLOR_ACCENT, S(10));
                    SelectObject(hdc, g_hFontSmallBold);
                    SetTextColor(hdc, RGB(10, 10, 12));
                    DrawTextW(hdc, bTxt, -1, &rcBadge, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                }
            }

            // En altta sürüm bilgisi
            RECT rcVer = { offX + S(16), h - S(32), offX + sbW - S(16), h - S(10) };
            wchar_t verStr[64];
            _snwprintf(verStr, 63, L"Selo Player v%s • Saf C Motoru", APP_VERSION_STR);
            verStr[63] = L'\0';
            SelectObject(hdc, g_hFontSmall);
            SetTextColor(hdc, COLOR_TEXT_DIM);
            DrawTextW(hdc, verStr, -1, &rcVer, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        }
        else if (g_sidebarView == SIDEBAR_VIEW_FRIENDS) {
            int yBase = headerH + S(10);
            rcFriendsBack = (RECT){ offX + S(10), yBase, offX + S(68), yBase + S(28) };
            DrawFastRoundRect(hdc, &rcFriendsBack, (g_hotZone == ZONE_FRIENDS_BACK) ? COLOR_CTRL_HOVER : COLOR_CARD, COLOR_BORDER, S(6));
            SetBkMode(hdc, TRANSPARENT);
            SelectObject(hdc, g_hFontSmallBold);
            SetTextColor(hdc, COLOR_TEXT_WHITE);
            DrawTextW(hdc, L"‹ Menü", -1, &rcFriendsBack, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            RECT rcFTitle = { offX + S(76), yBase, offX + sbW - S(10), yBase + S(28) };
            SelectObject(hdc, g_hFontBold);
            SetTextColor(hdc, COLOR_TEXT_WHITE);
            DrawTextW(hdc, L"Arkadaşlar & Jam", -1, &rcFTitle, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            // Kullanıcı Profil & Etiket Kartı
            wchar_t myTag[32] = {0};
            Friends_GetMyTag(myTag, 32);

            RECT rcProfile = { offX + S(10), yBase + S(36), offX + sbW - S(10), yBase + S(78) };
            DrawFastRoundRect(hdc, &rcProfile, COLOR_CARD, COLOR_BORDER, S(8));
            DrawAACircle(hdc, rcProfile.left + S(14), (rcProfile.top + rcProfile.bottom) / 2, S(4), Network_IsConnected() ? COLOR_ACCENT_HOV : COLOR_TEXT_DIM);

            RECT rcTagText = { rcProfile.left + S(24), rcProfile.top, rcProfile.right - S(68), rcProfile.bottom };
            SelectObject(hdc, g_hFontBold);
            SetTextColor(hdc, COLOR_TEXT_WHITE);
            DrawTextW(hdc, myTag, -1, &rcTagText, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

            rcCopyTag = (RECT){ rcProfile.right - S(64), rcProfile.top + S(8), rcProfile.right - S(8), rcProfile.bottom - S(8) };
            DrawFastRoundRect(hdc, &rcCopyTag, (g_hotZone == ZONE_COPY_TAG) ? COLOR_CTRL_HOVER : COLOR_SIDEBAR, COLOR_BORDER, S(6));
            SelectObject(hdc, g_hFontSmallBold);
            SetTextColor(hdc, COLOR_ACCENT_HOV);
            BOOL copiedRecently = (g_copiedTagTick > 0 && GetTickCount() - g_copiedTagTick < 2000);
            DrawTextW(hdc, copiedRecently ? L"Kopya!" : L"Kopyala", -1, &rcCopyTag, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            // Apple Segmented Control Tarzı 3 Sekme
            RECT rcSegBg = { offX + S(10), yBase + S(86), offX + sbW - S(10), yBase + S(116) };
            DrawFastRoundRect(hdc, &rcSegBg, RGB(14, 14, 17), COLOR_BORDER, S(8));

            int segW = (rcSegBg.right - rcSegBg.left - S(6)) / 3;
            rcFriendsTab0 = (RECT){ rcSegBg.left + S(3),            rcSegBg.top + S(3), rcSegBg.left + S(3) + segW,     rcSegBg.bottom - S(3) };
            rcFriendsTab1 = (RECT){ rcSegBg.left + S(3) + segW,     rcSegBg.top + S(3), rcSegBg.left + S(3) + segW * 2, rcSegBg.bottom - S(3) };
            rcFriendsTab2 = (RECT){ rcSegBg.left + S(3) + segW * 2, rcSegBg.top + S(3), rcSegBg.right - S(3),           rcSegBg.bottom - S(3) };

            if (g_friendsTab == FRIENDS_TAB_LIST)     DrawFastRoundRect(hdc, &rcFriendsTab0, COLOR_CTRL_HOVER, COLOR_BORDER, S(6));
            if (g_friendsTab == FRIENDS_TAB_REQUESTS) DrawFastRoundRect(hdc, &rcFriendsTab1, COLOR_CTRL_HOVER, COLOR_BORDER, S(6));
            if (g_friendsTab == FRIENDS_TAB_ADD)      DrawFastRoundRect(hdc, &rcFriendsTab2, COLOR_CTRL_HOVER, COLOR_BORDER, S(6));

            SelectObject(hdc, g_hFontSmallBold);
            wchar_t t0Buf[32], t1Buf[32];
            _snwprintf(t0Buf, 31, L"Liste (%d)", Friends_GetFriendCount()); t0Buf[31] = L'\0';
            _snwprintf(t1Buf, 31, L"İstek (%d)", Friends_GetPendingCount()); t1Buf[31] = L'\0';

            SetTextColor(hdc, (g_friendsTab == FRIENDS_TAB_LIST) ? COLOR_TEXT_WHITE : COLOR_TEXT_MUTED);
            DrawTextW(hdc, t0Buf, -1, &rcFriendsTab0, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            SetTextColor(hdc, (g_friendsTab == FRIENDS_TAB_REQUESTS) ? COLOR_ACCENT_HOV : (Friends_GetPendingCount() > 0 ? COLOR_ACCENT_HOV : COLOR_TEXT_MUTED));
            DrawTextW(hdc, t1Buf, -1, &rcFriendsTab1, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            SetTextColor(hdc, (g_friendsTab == FRIENDS_TAB_ADD) ? COLOR_TEXT_WHITE : COLOR_TEXT_MUTED);
            DrawTextW(hdc, L"+ Ekle", -1, &rcFriendsTab2, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            int contentTop = yBase + S(126);

            if (g_friendsTab == FRIENDS_TAB_LIST) {
                int fCount = Friends_GetFriendCount();
                if (fCount == 0) {
                    RECT rcEmpty = { offX + S(16), contentTop + S(24), offX + sbW - S(16), contentTop + S(140) };
                    SelectObject(hdc, g_hFontSmall);
                    SetTextColor(hdc, COLOR_TEXT_MUTED);
                    DrawTextW(hdc, L"Henüz arkadaşınız yok.\n\n'+ Ekle' sekmesinden arkadaşınızın\netiketini yazıp davet gönderebilirsiniz.", -1, &rcEmpty, DT_CENTER | DT_WORDBREAK);
                } else {
                    for (int i = 0; i < fCount; i++) {
                        const FriendItem *fi = Friends_GetFriend(i);
                        if (!fi) continue;

                        int cardY = contentTop + i * S(52);
                        RECT rcCard = { offX + S(10), cardY, offX + sbW - S(10), cardY + S(46) };
                        DrawFastRoundRect(hdc, &rcCard, COLOR_CARD, COLOR_BORDER, S(8));
                        DrawAACircle(hdc, rcCard.left + S(12), cardY + S(23), S(4), fi->isOnline ? COLOR_ACCENT_HOV : COLOR_TEXT_DIM);

                        RECT rcName = { rcCard.left + S(22), cardY + S(5), rcCard.right - S(62), cardY + S(24) };
                        SelectObject(hdc, g_hFontBold);
                        SetTextColor(hdc, COLOR_TEXT_WHITE);
                        DrawTextW(hdc, fi->name, -1, &rcName, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

                        RECT rcSt = { rcCard.left + S(22), cardY + S(23), rcCard.right - S(62), cardY + S(41) };
                        SelectObject(hdc, g_hFontSmall);
                        SetTextColor(hdc, fi->isOnline ? COLOR_TEXT_MUTED : COLOR_TEXT_DIM);
                        DrawTextW(hdc, fi->currentTrack[0] ? fi->currentTrack : (fi->isOnline ? L"Çevrimiçi" : L"Çevrimdışı"), -1, &rcSt, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

                        rcFriendActionBtns[i] = (RECT){ rcCard.right - S(56), cardY + S(9), rcCard.right - S(8), cardY + S(37) };
                        wchar_t partnerTag[32] = {0};
                        Jam_GetPartnerTag(partnerTag, 32);
                        BOOL isJamming = (Jam_IsActive() && _wcsicmp(partnerTag, fi->tag) == 0);

                        if (isJamming) {
                            DrawFastRoundRect(hdc, &rcFriendActionBtns[i], RGB(48, 22, 22), RGB(170, 50, 50), S(6));
                            SelectObject(hdc, g_hFontSmallBold);
                            SetTextColor(hdc, RGB(255, 120, 120));
                            DrawTextW(hdc, L"Ayrıl", -1, &rcFriendActionBtns[i], DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                        } else if (fi->isOnline) {
                            DrawFastRoundRect(hdc, &rcFriendActionBtns[i], RGB(20, 44, 28), COLOR_ACCENT, S(6));
                            SelectObject(hdc, g_hFontSmallBold);
                            SetTextColor(hdc, COLOR_ACCENT_HOV);
                            DrawTextW(hdc, L"Jam", -1, &rcFriendActionBtns[i], DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                        }
                    }
                }
            }
            else if (g_friendsTab == FRIENDS_TAB_REQUESTS) {
                int pCount = Friends_GetPendingCount();
                if (pCount == 0) {
                    RECT rcEmpty = { offX + S(16), contentTop + S(28), offX + sbW - S(16), contentTop + S(90) };
                    SelectObject(hdc, g_hFontSmall);
                    SetTextColor(hdc, COLOR_TEXT_MUTED);
                    DrawTextW(hdc, L"Bekleyen arkadaşlık isteği yok.", -1, &rcEmpty, DT_CENTER | DT_SINGLELINE);
                } else {
                    for (int i = 0; i < pCount; i++) {
                        const FriendRequest *rq = Friends_GetPending(i);
                        if (!rq) continue;

                        int cardY = contentTop + i * S(52);
                        RECT rcCard = { offX + S(10), cardY, offX + sbW - S(10), cardY + S(46) };
                        DrawFastRoundRect(hdc, &rcCard, COLOR_CARD, COLOR_BORDER, S(8));

                        RECT rcName = { rcCard.left + S(12), cardY + S(5), rcCard.right - S(72), cardY + S(24) };
                        SelectObject(hdc, g_hFontBold);
                        SetTextColor(hdc, COLOR_TEXT_WHITE);
                        DrawTextW(hdc, rq->name, -1, &rcName, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

                        RECT rcTag = { rcCard.left + S(12), cardY + S(23), rcCard.right - S(72), cardY + S(41) };
                        SelectObject(hdc, g_hFontSmall);
                        SetTextColor(hdc, COLOR_TEXT_MUTED);
                        DrawTextW(hdc, rq->tag, -1, &rcTag, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

                        rcReqAcceptBtns[i] = (RECT){ rcCard.right - S(66), cardY + S(9), rcCard.right - S(38), cardY + S(37) };
                        DrawFastRoundRect(hdc, &rcReqAcceptBtns[i], COLOR_ACCENT, COLOR_ACCENT, S(6));
                        SelectObject(hdc, g_hFontIconsSmall);
                        SetTextColor(hdc, RGB(10, 10, 12));
                        DrawTextW(hdc, L"\uE73E", -1, &rcReqAcceptBtns[i], DT_CENTER | DT_VCENTER | DT_SINGLELINE);

                        rcReqRejectBtns[i] = (RECT){ rcCard.right - S(34), cardY + S(9), rcCard.right - S(6), cardY + S(37) };
                        DrawFastRoundRect(hdc, &rcReqRejectBtns[i], COLOR_CTRL_HOVER, COLOR_BORDER, S(6));
                        SelectObject(hdc, g_hFontIconsSmall);
                        SetTextColor(hdc, COLOR_TEXT_MUTED);
                        DrawTextW(hdc, L"\uE711", -1, &rcReqRejectBtns[i], DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                    }
                }
            }
            else if (g_friendsTab == FRIENDS_TAB_ADD) {
                RECT rcLbl = { offX + S(14), contentTop + S(2), offX + sbW - S(14), contentTop + S(22) };
                SelectObject(hdc, g_hFontBold);
                SetTextColor(hdc, COLOR_TEXT_WHITE);
                DrawTextW(hdc, L"Arkadaş Ekle", -1, &rcLbl, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

                RECT rcSub = { offX + S(14), contentTop + S(22), offX + sbW - S(14), contentTop + S(40) };
                SelectObject(hdc, g_hFontSmall);
                SetTextColor(hdc, COLOR_TEXT_MUTED);
                DrawTextW(hdc, L"Etiketi girin (Örn: Selim#8421)", -1, &rcSub, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

                RECT rcInputBox = { offX + S(10), contentTop + S(46), offX + sbW - S(10), contentTop + S(82) };
                DrawFastRoundRect(hdc, &rcInputBox, COLOR_SEARCH_PILL, COLOR_BORDER, S(8));

                rcFriendSendBtn = (RECT){ offX + S(10), contentTop + S(92), offX + sbW - S(10), contentTop + S(128) };
                COLORREF btnCol = (g_pressedZone == ZONE_FRIEND_SEND) ? COLOR_ACCENT_DARK : ((g_hotZone == ZONE_FRIEND_SEND) ? COLOR_ACCENT_HOV : COLOR_ACCENT);
                DrawFastRoundRect(hdc, &rcFriendSendBtn, btnCol, btnCol, S(8));
                SelectObject(hdc, g_hFontBold);
                SetTextColor(hdc, RGB(10, 10, 12));
                DrawTextW(hdc, L"İstek Gönder", -1, &rcFriendSendBtn, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

                if (g_friendAddMsg[0]) {
                    RECT rcMsg = { offX + S(12), contentTop + S(138), offX + sbW - S(12), contentTop + S(190) };
                    SelectObject(hdc, g_hFontSmall);
                    SetTextColor(hdc, wcsstr(g_friendAddMsg, L"gönderildi") ? COLOR_ACCENT_HOV : RGB(255, 120, 120));
                    DrawTextW(hdc, g_friendAddMsg, -1, &rcMsg, DT_LEFT | DT_WORDBREAK);
                }
            }
        }

        RestoreDC(hdc, savedSide);
    }

    // =========================================================================
    // 5. CANLI JAM DAVETİ YÜZEN BİLDİRİMİ (APPLE FLOATING BANNER)
    // =========================================================================
    if (Jam_HasPendingInvite()) {
        wchar_t invName[64] = {0};
        wchar_t invTag[32] = {0};
        Jam_GetPendingInviteInfo(invName, invTag);

        int bannerW = S(360);
        if (bannerW > w - S(24)) bannerW = w - S(24);
        rcToastBox = (RECT){ (w - bannerW) / 2, headerH + S(10), (w + bannerW) / 2, headerH + S(56) };
        DrawFastRoundRect(hdc, &rcToastBox, RGB(24, 30, 26), COLOR_ACCENT, S(12));

        wchar_t toastMsg[128];
        _snwprintf(toastMsg, 127, L"%s seni Jam'e davet etti", invName[0] ? invName : invTag);
        toastMsg[127] = L'\0';

        RECT rcToastTxt = { rcToastBox.left + S(14), rcToastBox.top, rcToastBox.right - S(102), rcToastBox.bottom };
        SetBkMode(hdc, TRANSPARENT);
        SelectObject(hdc, g_hFontBold);
        SetTextColor(hdc, COLOR_TEXT_WHITE);
        DrawTextW(hdc, toastMsg, -1, &rcToastTxt, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        rcToastAccept = (RECT){ rcToastBox.right - S(94), rcToastBox.top + S(9), rcToastBox.right - S(40), rcToastBox.bottom - S(9) };
        DrawFastRoundRect(hdc, &rcToastAccept, (g_hotZone == ZONE_TOAST_ACCEPT) ? COLOR_ACCENT_HOV : COLOR_ACCENT, COLOR_ACCENT, S(8));
        SelectObject(hdc, g_hFontSmallBold);
        SetTextColor(hdc, RGB(10, 10, 12));
        DrawTextW(hdc, L"Katıl", -1, &rcToastAccept, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        rcToastReject = (RECT){ rcToastBox.right - S(34), rcToastBox.top + S(9), rcToastBox.right - S(8), rcToastBox.bottom - S(9) };
        DrawFastRoundRect(hdc, &rcToastReject, COLOR_CTRL_HOVER, COLOR_BORDER, S(8));
        SelectObject(hdc, g_hFontIconsSmall);
        SetTextColor(hdc, COLOR_TEXT_MUTED);
        DrawTextW(hdc, L"\uE711", -1, &rcToastReject, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    } else {
        rcToastBox = (RECT){0};
    }
}

static UiZone HitTestZones(POINT pt) {
    if (Jam_HasPendingInvite()) {
        if (PtInRect(&rcToastAccept, pt)) return ZONE_TOAST_ACCEPT;
        if (PtInRect(&rcToastReject, pt)) return ZONE_TOAST_REJECT;
    }
    if (PtInRect(&rcHamburger, pt)) return ZONE_HAMBURGER;
    if (g_searchQuery[0] && PtInRect(&rcSearchClear, pt)) return ZONE_SEARCH_CLEAR;
    if (PtInRect(&rcQuickDownload, pt)) return ZONE_QUICK_DOWNLOAD;

    if (g_sidebarOpen && pt.x < GetSidebarW() && pt.y >= GetHeaderH()) {
        if (g_sidebarView == SIDEBAR_VIEW_FRIENDS) {
            if (PtInRect(&rcFriendsBack, pt)) return ZONE_FRIENDS_BACK;
            if (PtInRect(&rcCopyTag, pt))     return ZONE_COPY_TAG;
            if (PtInRect(&rcFriendsTab0, pt)) return ZONE_FTAB0;
            if (PtInRect(&rcFriendsTab1, pt)) return ZONE_FTAB1;
            if (PtInRect(&rcFriendsTab2, pt)) return ZONE_FTAB2;
            if (g_friendsTab == FRIENDS_TAB_ADD && PtInRect(&rcFriendSendBtn, pt)) return ZONE_FRIEND_SEND;
        }
        return ZONE_NONE;
    }
    if (g_sidebarOpen && pt.y >= GetHeaderH()) return ZONE_NONE;

    if (Jam_IsActive() && PtInRect(&rcJamLeave, pt)) return ZONE_JAM_LEAVE;
    if (PtInRect(&rcPlay, pt))    return ZONE_PLAY;
    if (PtInRect(&rcPrev, pt))    return ZONE_PREV;
    if (PtInRect(&rcNext, pt))    return ZONE_NEXT;
    if (PtInRect(&rcShuffle, pt)) return ZONE_SHUFFLE;
    if (PtInRect(&rcRepeat, pt))  return ZONE_REPEAT;
    if (PtInRect(&rcSpeaker, pt)) return ZONE_SPEAKER;
    if (PtInRect(&rcSeekHit, pt)) return ZONE_SEEK;
    if (PtInRect(&rcVolHit, pt))  return ZONE_VOL;
    return ZONE_NONE;
}

static LRESULT CALLBACK SearchBoxSubclassProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_KEYDOWN) {
        if (wParam == VK_RETURN) {
            int total = g_isFiltered ? g_filterCount : Playlist_Count();
            if (total > 0) {
                PlayTrack(g_isFiltered ? g_filterIndices[0] : 0);
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
    } else if (msg == WM_CHAR && (wParam == 1 || wParam == 22 || wParam == 3 || wParam == 24 || wParam == VK_RETURN || wParam == VK_ESCAPE)) {
        if (wParam == VK_RETURN || wParam == VK_ESCAPE) return 0;
    }
    return CallWindowProcW(g_oldSearchEditProc, hWnd, msg, wParam, lParam);
}

static LRESULT CALLBACK FriendEditSubclassProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_KEYDOWN) {
        if (wParam == VK_RETURN) {
            SubmitFriendRequest(g_hWnd);
            return 0;
        } else if (wParam == VK_ESCAPE) {
            ToggleSidebar(FALSE);
            SetFocus(g_hWnd);
            return 0;
        }
    } else if (msg == WM_CHAR && (wParam == VK_RETURN || wParam == VK_ESCAPE)) {
        return 0;
    }
    return CallWindowProcW(g_oldFriendAddEditProc, hWnd, msg, wParam, lParam);
}

static void LayoutHeaderInputs(int clientW) {
    int headerH = GetHeaderH();
    int searchW = S(188);
    int searchH = S(32);
    int searchX = clientW - searchW - S(12);
    int searchY = (headerH - searchH) / 2;
    int rightPad = g_searchQuery[0] ? S(28) : S(10);
    if (g_hSearchEdit) {
        MoveWindow(g_hSearchEdit, searchX + S(28), searchY + S(6), searchW - S(28) - rightPad, searchH - S(12), TRUE);
    }
    UpdateSidebarEditPosition();
}

static LRESULT CALLBACK MainWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            g_hWnd = hWnd;

            BOOL useDark = TRUE;
            DwmSetWindowAttribute(hWnd, 20, &useDark, sizeof(useDark));
            DwmSetWindowAttribute(hWnd, 19, &useDark, sizeof(useDark));

            typedef UINT(WINAPI *GetDpiForWindowProc)(HWND);
            GetDpiForWindowProc pGetDpi = (GetDpiForWindowProc)GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow");
            int dpi = pGetDpi ? (int)pGetDpi(hWnd) : 96;
            if (dpi <= 0) dpi = 96;
            InitDpiFonts(dpi);

            g_hAppIcon = (HICON)LoadImageW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(1), IMAGE_ICON, S(36), S(36), LR_DEFAULTCOLOR);
            g_hBrSearchEdit = CreateSolidBrush(COLOR_SEARCH_PILL);

            g_hSearchEdit = CreateWindowExW(
                0, L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                0, 0, 0, 0,
                hWnd, (HMENU)IDC_HEADER_SEARCH, GetModuleHandle(NULL), NULL
            );
            SendMessageW(g_hSearchEdit, WM_SETFONT, (WPARAM)g_hFontMain, TRUE);
            SendMessageW(g_hSearchEdit, EM_SETCUEBANNER, TRUE, (LPARAM)L"Şarkılarda ara...");
            g_oldSearchEditProc = (WNDPROC)SetWindowLongPtrW(g_hSearchEdit, GWLP_WNDPROC, (LONG_PTR)SearchBoxSubclassProc);

            g_hFriendAddEdit = CreateWindowExW(
                0, L"EDIT", L"",
                WS_CHILD | ES_AUTOHSCROLL,
                0, 0, 0, 0,
                hWnd, (HMENU)IDC_FRIEND_ADD_EDIT, GetModuleHandle(NULL), NULL
            );
            SendMessageW(g_hFriendAddEdit, WM_SETFONT, (WPARAM)g_hFontMain, TRUE);
            SendMessageW(g_hFriendAddEdit, EM_SETCUEBANNER, TRUE, (LPARAM)L"Selim#8421");
            g_oldFriendAddEditProc = (WNDPROC)SetWindowLongPtrW(g_hFriendAddEdit, GWLP_WNDPROC, (LONG_PTR)FriendEditSubclassProc);

            Player_Init(hWnd);
            Playlist_Init();
            Friends_Init();
            Jam_Init();

            wchar_t myInitTag[32] = {0};
            Friends_GetMyTag(myInitTag, 32);
            char myInitKey[64] = {0};
            Network_TagToTopicKeyW(myInitTag, myInitKey, sizeof(myInitKey));

            Network_Init(hWnd, myInitKey);

            char myInboxTopic[256];
            snprintf(myInboxTopic, sizeof(myInboxTopic), "seloplayer/u/%s", myInitKey);
            Network_Subscribe(myInboxTopic);

            for (int fi = 0; fi < Friends_GetFriendCount(); fi++) {
                const FriendItem *fItem = Friends_GetFriend(fi);
                if (fItem) {
                    char fKey[64] = {0};
                    Network_TagToTopicKeyW(fItem->tag, fKey, sizeof(fKey));
                    char sTopic[256];
                    snprintf(sTopic, sizeof(sTopic), "seloplayer/status/%s", fKey);
                    Network_Subscribe(sTopic);
                }
            }

            Config_Load(&g_cfg, L"config.json");
            Player_SetVolume(g_cfg.volume);
            if (g_cfg.volume > 0) g_preMuteVol = g_cfg.volume;

            if (g_cfg.lastFolder[0] && GetFileAttributesW(g_cfg.lastFolder) != INVALID_FILE_ATTRIBUTES) {
                Playlist_ScanFolder(g_cfg.lastFolder);
                if (Playlist_Count() > 0) g_selectedTrack = 0;
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
                    if (Playlist_Count() > 0) g_selectedTrack = 0;
                }
            }

            DragAcceptFiles(hWnd, TRUE);
            SetTimer(hWnd, IDT_UPDATE_TIMER, 250, NULL);

            Updater_Init();
            Updater_CheckAsync(hWnd, FALSE);
            return 0;
        }

        case WM_CTLCOLOREDIT: {
            HWND hCtrl = (HWND)lParam;
            if (hCtrl == g_hSearchEdit || hCtrl == g_hFriendAddEdit) {
                HDC hdcEdit = (HDC)wParam;
                SetBkColor(hdcEdit, COLOR_SEARCH_PILL);
                SetTextColor(hdcEdit, COLOR_TEXT_WHITE);
                return (LRESULT)g_hBrSearchEdit;
            }
            break;
        }

        case WM_COMMAND: {
            if (LOWORD(wParam) == IDC_HEADER_SEARCH && HIWORD(wParam) == EN_CHANGE) {
                wchar_t q[128] = {0};
                GetWindowTextW(g_hSearchEdit, q, 127);
                BOOL hadQuery = (g_searchQuery[0] != 0);
                ApplyFilter(q);
                if (hadQuery != (g_searchQuery[0] != 0)) {
                    RECT rc;
                    GetClientRect(hWnd, &rc);
                    LayoutHeaderInputs(rc.right);
                }
                return 0;
            }
            break;
        }

        case 0x02E0 /* WM_DPICHANGED */: {
            int newDpi = HIWORD(wParam);
            InitDpiFonts(newDpi);
            if (g_hSearchEdit) SendMessageW(g_hSearchEdit, WM_SETFONT, (WPARAM)g_hFontMain, TRUE);
            if (g_hFriendAddEdit) SendMessageW(g_hFriendAddEdit, WM_SETFONT, (WPARAM)g_hFontMain, TRUE);
            FreeBackbuffer();
            RECT *prc = (RECT*)lParam;
            SetWindowPos(hWnd, NULL, prc->left, prc->top, prc->right - prc->left, prc->bottom - prc->top, SWP_NOZORDER | SWP_NOACTIVATE);
            InvalidateRect(hWnd, NULL, FALSE);
            return 0;
        }

        case WM_ACTIVATE: {
            SetTimer(hWnd, IDT_UPDATE_TIMER, (LOWORD(wParam) == WA_INACTIVE) ? 1000 : 250, NULL);
            break;
        }

        case WM_SIZE: {
            if (wParam == SIZE_MINIMIZED) {
                SetTimer(hWnd, IDT_UPDATE_TIMER, 1500, NULL);
                FreeBackbuffer();
                SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
            } else {
                int w = LOWORD(lParam);
                ClampScrollTargets(hWnd);
                LayoutHeaderInputs(w);
                InvalidateRect(hWnd, NULL, FALSE);
            }
            break;
        }

        case WM_TIMER: {
            if (wParam == IDT_ANIM_TIMER) {
                DWORD now = GetTickCount();
                float dt = (float)(now - g_lastAnimTick) * 0.001f;
                g_lastAnimTick = now;
                if (dt <= 0.0f || dt > 0.05f) dt = 0.016f;

                BOOL movingScroll  = StepSpring(&g_scrollPosPx, &g_scrollVelPx, g_scrollTargetPx, 24.0f, 1.0f, dt, 0.35f);
                BOOL movingSidebar = StepSpring(&g_sidebarAnim, &g_sidebarVel, g_sidebarOpen ? 1.0f : 0.0f, 26.0f, 0.92f, dt, 0.003f);

                UpdateSidebarEditPosition();
                InvalidateRect(hWnd, NULL, FALSE);

                if (!movingScroll && !movingSidebar) {
                    KillTimer(hWnd, IDT_ANIM_TIMER);
                    g_animTimerActive = FALSE;
                }
                return 0;
            }

            if (wParam == IDT_UPDATE_TIMER) {
                // Yalnızca gerçek arkadaş varken durum yayınla — gereksiz ağ trafiği sıfır!
                if (Friends_GetFriendCount() > 0) {
                    static int s_statusTicks = 0;
                    static int s_lastPubTrack = -2;
                    static int s_lastPubPlay  = -1;
                    int curPlay = Player_IsPlaying();
                    s_statusTicks++;
                    if (curPlay != s_lastPubPlay || g_currentTrack != s_lastPubTrack || s_statusTicks >= 60) {
                        s_statusTicks = 0;
                        s_lastPubPlay = curPlay;
                        s_lastPubTrack = g_currentTrack;
                        Friends_CheckTimeouts();
                        const wchar_t *curTitle = (g_currentTrack >= 0 && g_currentTrack < Playlist_Count()) ? Playlist_Get(g_currentTrack)->title : L"";
                        Friends_PublishMyStatus(curPlay, curTitle, Player_GetPosition());
                    }
                }

                if (Player_IsPlaying() || g_isDraggingSeek) {
                    RECT rcClient;
                    GetClientRect(hWnd, &rcClient);
                    RECT rcDockSeek = { 0, rcClient.bottom - GetDockH(), rcClient.right, rcClient.bottom - GetDockH() + S(32) };
                    InvalidateRect(hWnd, &rcDockSeek, FALSE);
                }
            }
            return 0;
        }

        case WM_APP_TRACK_ENDED: {
            int nextIdx = Playlist_FindNext(g_currentTrack, g_cfg.shuffle, g_cfg.repeat);
            if (nextIdx >= 0) {
                PlayTrack(nextIdx);
                ScrollTrackIntoView(nextIdx);
            } else {
                Player_Stop();
                InvalidateRect(hWnd, NULL, FALSE);
            }
            return 0;
        }

        case WM_APP_NETWORK_EVENT: {
            NetEvent *evt = (NetEvent*)wParam;
            if (evt) {
                if (evt->type == NET_EVENT_MESSAGE) {
                    Friends_HandleNetworkMessage(evt->topic, evt->payload);
                    Jam_HandleNetworkMessage(evt->topic, evt->payload);
                    HandleJamSyncMessage(evt->topic, evt->payload);
                }
                free(evt);
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
                int newCount = Playlist_Count();

                if (g_isFiltered || g_searchQuery[0]) {
                    g_searchQuery[0] = L'\0';
                    SetWindowTextW(g_hSearchEdit, L"");
                    ApplyFilter(L"");
                }

                if (newCount > oldCount) {
                    int addedCount = newCount - oldCount;
                    for (int i = 0; i < addedCount; i++) {
                        Playlist_Move(oldCount + i, 0);
                    }
                    if (g_currentTrack >= 0) {
                        g_currentTrack += addedCount;
                    }
                    g_selectedTrack = 0;
                    g_scrollTargetPx = 0.0f;
                    StartAnimLoop(hWnd);
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
                        _snwprintf(prompt, 2047,
                                   L"SeloPlayer için yeni bir güncelleme mevcut!\n\n"
                                   L"Mevcut Sürüm: v%s\nYeni Sürüm: v%s\n\nYenilikler:\n%s\n\n"
                                   L"Şimdi otomatik olarak indirip güncellemek istiyor musunuz?",
                                   APP_VERSION_STR, info->newVersion, info->changelog);
                    } else {
                        _snwprintf(prompt, 2047,
                                   L"SeloPlayer için yeni bir güncelleme mevcut!\n\n"
                                   L"Mevcut Sürüm: v%s\nYeni Sürüm: v%s\n\n"
                                   L"Şimdi otomatik olarak indirip güncellemek istiyor musunuz?",
                                   APP_VERSION_STR, info->newVersion);
                    }
                    prompt[2047] = L'\0';

                    if (MessageBoxW(hWnd, prompt, L"Güncelleme Mevcut", MB_YESNO | MB_ICONINFORMATION) == IDYES) {
                        Updater_PerformUpdate(hWnd, info->downloadUrl);
                    }
                } else if (info->isManual) {
                    if (info->checkSuccess) {
                        wchar_t msgBuf[256];
                        _snwprintf(msgBuf, 255, L"Zaten en güncel sürümü kullanıyorsunuz.\n(Mevcut Sürüm: v%s)", APP_VERSION_STR);
                        msgBuf[255] = L'\0';
                        MessageBoxW(hWnd, msgBuf, L"Güncelleme Kontrolü", MB_OK | MB_ICONINFORMATION);
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
            if (g_selectedTrack < 0 && Playlist_Count() > 0) g_selectedTrack = 0;
            ApplyFilter(g_searchQuery);
            InvalidateRect(hWnd, NULL, FALSE);
            return 0;
        }

        case WM_MOUSEWHEEL: {
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
            ScreenToClient(hWnd, &pt);

            RECT rcClient;
            GetClientRect(hWnd, &rcClient);
            if (pt.y >= rcClient.bottom - GetDockH()) {
                int vol = Player_GetVolume() + (delta > 0 ? 5 : -5);
                Player_SetVolume(vol);
                if (Player_GetVolume() > 0) g_preMuteVol = Player_GetVolume();
                g_cfg.volume = Player_GetVolume();
                Config_Save(&g_cfg, L"config.json");
                RECT rcDock = { 0, rcClient.bottom - GetDockH(), rcClient.right, rcClient.bottom };
                InvalidateRect(hWnd, &rcDock, FALSE);
            } else if (!g_sidebarOpen || pt.x >= GetSidebarW()) {
                float step = ((float)delta / 120.0f) * (float)(GetItemH() * 2.2f);
                g_scrollTargetPx -= step;
                ClampScrollTargets(hWnd);
                StartAnimLoop(hWnd);
            }
            return 0;
        }

        case WM_MOUSELEAVE: {
            if (g_hotZone != ZONE_NONE || g_hoverTrack != -1 || g_scrollbarHover || g_sidebarHoverItem != -1) {
                g_hotZone = ZONE_NONE;
                g_hoverTrack = -1;
                g_scrollbarHover = FALSE;
                g_sidebarHoverItem = -1;
                InvalidateRect(hWnd, NULL, FALSE);
            }
            return 0;
        }

        case WM_MOUSEMOVE: {
            POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };

            TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hWnd, 0 };
            TrackMouseEvent(&tme);

            // 1:1 Doğrudan Kaydırma Çubuğu Sürükleme (Apple Direct Manipulation)
            if (g_isDraggingScrollbar) {
                int sbTop = GetHeaderH() + S(6);
                int sbBottom = 0;
                RECT rcClient;
                GetClientRect(hWnd, &rcClient);
                sbBottom = rcClient.bottom - GetDockH() - S(6);
                int trackH = sbBottom - sbTop;
                int thumbH = rcScrollbarThumb.bottom - rcScrollbarThumb.top;
                int maxScrollPx = GetMaxScrollPx(hWnd);
                if (maxScrollPx > 0 && trackH > thumbH) {
                    float ratio = (float)(pt.y - g_scrollbarDragGrabOffsetY - sbTop) / (float)(trackH - thumbH);
                    if (ratio < 0.0f) ratio = 0.0f;
                    if (ratio > 1.0f) ratio = 1.0f;
                    g_scrollPosPx = g_scrollTargetPx = ratio * (float)maxScrollPx;
                    g_scrollVelPx = 0.0f;
                    InvalidateRect(hWnd, NULL, FALSE);
                }
                return 0;
            }

            if (g_isDraggingSeek) {
                int duration = Player_GetDuration();
                if (duration > 0 && rcSeekBar.right > rcSeekBar.left) {
                    float ratio = (float)(pt.x - rcSeekBar.left) / (float)(rcSeekBar.right - rcSeekBar.left);
                    if (ratio < 0.0f) ratio = 0.0f;
                    if (ratio > 1.0f) ratio = 1.0f;
                    g_seekDragSec = (int)(ratio * duration);
                    RECT rcClient;
                    GetClientRect(hWnd, &rcClient);
                    RECT rcDock = { 0, rcClient.bottom - GetDockH(), rcClient.right, rcClient.bottom };
                    InvalidateRect(hWnd, &rcDock, FALSE);
                }
                return 0;
            }

            if (g_isDraggingVol) {
                if (rcVolBar.right > rcVolBar.left) {
                    float ratio = (float)(pt.x - rcVolBar.left) / (float)(rcVolBar.right - rcVolBar.left);
                    if (ratio < 0.0f) ratio = 0.0f;
                    if (ratio > 1.0f) ratio = 1.0f;
                    int vol = (int)(ratio * 100.0f + 0.5f);
                    Player_SetVolume(vol);
                    if (vol > 0) g_preMuteVol = vol;
                    g_cfg.volume = vol;
                    RECT rcClient;
                    GetClientRect(hWnd, &rcClient);
                    RECT rcDock = { rcClient.right / 2, rcClient.bottom - GetDockH(), rcClient.right, rcClient.bottom };
                    InvalidateRect(hWnd, &rcDock, FALSE);
                }
                return 0;
            }

            // Liste içi sıralama değiştirme sürüklemesi
            if ((wParam & MK_LBUTTON) && g_dragFromIndex >= 0 && !g_isFiltered) {
                if (!g_isDraggingList && abs(pt.y - g_mouseDownPt.y) > S(7)) {
                    g_isDraggingList = TRUE;
                }
                if (g_isDraggingList) {
                    int listTop = GetHeaderH() + S(6);
                    int target = (int)((pt.y - listTop + g_scrollPosPx) / (float)GetItemH());
                    if (target < 0) target = 0;
                    if (target >= Playlist_Count()) target = Playlist_Count() - 1;
                    if (target != g_dragTargetIndex) {
                        g_dragTargetIndex = target;
                        InvalidateRect(hWnd, NULL, FALSE);
                    }
                    return 0;
                }
            }

            BOOL needRepaint = FALSE;
            UiZone newHot = HitTestZones(pt);
            if (newHot != g_hotZone) {
                g_hotZone = newHot;
                needRepaint = TRUE;
            }

            BOOL sbHov = (!g_sidebarOpen && PtInRect(&rcScrollbarTrack, pt));
            if (sbHov != g_scrollbarHover) {
                g_scrollbarHover = sbHov;
                needRepaint = TRUE;
            }

            int newSideHover = -1;
            if (g_sidebarOpen && g_sidebarView == SIDEBAR_VIEW_MENU && pt.x < GetSidebarW() && pt.y >= GetHeaderH() + S(12)) {
                int m = (pt.y - (GetHeaderH() + S(12))) / S(44);
                if (m >= 0 && m < 7) newSideHover = m;
            }
            if (newSideHover != g_sidebarHoverItem) {
                g_sidebarHoverItem = newSideHover;
                needRepaint = TRUE;
            }

            int newHoverTrack = -1;
            RECT rcClient;
            GetClientRect(hWnd, &rcClient);
            int listTop = GetHeaderH();
            int listBottom = rcClient.bottom - GetDockH();
            if (!g_sidebarOpen && !sbHov && pt.y >= listTop && pt.y < listBottom && pt.x >= S(12) && pt.x <= rcClient.right - S(18)) {
                int displayIdx = (int)((pt.y - (listTop + S(6)) + g_scrollPosPx) / (float)GetItemH());
                int totalCount = g_isFiltered ? g_filterCount : Playlist_Count();
                if (displayIdx >= 0 && displayIdx < totalCount) {
                    newHoverTrack = g_isFiltered ? g_filterIndices[displayIdx] : displayIdx;
                }
            }
            if (newHoverTrack != g_hoverTrack) {
                g_hoverTrack = newHoverTrack;
                needRepaint = TRUE;
            }

            if (needRepaint) {
                InvalidateRect(hWnd, NULL, FALSE);
            }
            return 0;
        }

        case WM_LBUTTONDOWN: {
            POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
            SetFocus(hWnd);
            SetCapture(hWnd);

            UiZone hit = HitTestZones(pt);
            g_pressedZone = hit;

            // 0. Jam Davet Toast Tıklamaları
            if (hit == ZONE_TOAST_ACCEPT) {
                Jam_AcceptInvite();
                g_pressedZone = ZONE_NONE;
                ReleaseCapture();
                InvalidateRect(hWnd, NULL, FALSE);
                return 0;
            }
            if (hit == ZONE_TOAST_REJECT) {
                Jam_RejectInvite();
                g_pressedZone = ZONE_NONE;
                ReleaseCapture();
                InvalidateRect(hWnd, NULL, FALSE);
                return 0;
            }

            // 1. Üst Bar Butonları
            if (hit == ZONE_HAMBURGER) {
                ToggleSidebar(!g_sidebarOpen);
                InvalidateRect(hWnd, NULL, FALSE);
                return 0;
            }
            if (hit == ZONE_SEARCH_CLEAR) {
                SetWindowTextW(g_hSearchEdit, L"");
                ApplyFilter(L"");
                RECT rc;
                GetClientRect(hWnd, &rc);
                LayoutHeaderInputs(rc.right);
                g_pressedZone = ZONE_NONE;
                ReleaseCapture();
                return 0;
            }
            if (hit == ZONE_QUICK_DOWNLOAD) {
                InvalidateRect(hWnd, &rcQuickDownload, FALSE);
                g_pressedZone = ZONE_NONE;
                ReleaseCapture();
                Search_ShowDialog(hWnd, g_cfg.lastFolder);
                return 0;
            }

            // 2. Yan Çekmece Açıkken Etkileşim
            if (g_sidebarOpen && pt.y >= GetHeaderH()) {
                int sbW = GetSidebarW();
                if (pt.x >= sbW) {
                    ToggleSidebar(FALSE);
                    g_pressedZone = ZONE_NONE;
                    ReleaseCapture();
                    return 0;
                }

                if (g_sidebarView == SIDEBAR_VIEW_MENU) {
                    if (pt.y >= GetHeaderH() + S(12)) {
                        int item = (pt.y - (GetHeaderH() + S(12))) / S(44);
                        g_pressedZone = ZONE_NONE;
                        ReleaseCapture();
                        if (item == 0) { ToggleSidebar(FALSE); SelectFolderDialog(); }
                        else if (item == 1) { ToggleSidebar(FALSE); Search_ShowDialog(hWnd, g_cfg.lastFolder); }
                        else if (item == 2) { g_sidebarView = SIDEBAR_VIEW_FRIENDS; InvalidateRect(hWnd, NULL, FALSE); }
                        else if (item == 3) { ToggleSidebar(FALSE); ImportM3UDialog(); }
                        else if (item == 4) { ToggleSidebar(FALSE); ExportM3UDialog(); }
                        else if (item == 5) {
                            ToggleSidebar(FALSE);
                            Playlist_Clear();
                            Player_Stop();
                            g_currentTrack = -1;
                            g_selectedTrack = -1;
                            ApplyFilter(L"");
                        }
                        else if (item == 6) { ToggleSidebar(FALSE); Updater_CheckAsync(hWnd, TRUE); }
                    }
                    return 0;
                }
                else if (g_sidebarView == SIDEBAR_VIEW_FRIENDS) {
                    if (hit == ZONE_FRIENDS_BACK) {
                        g_sidebarView = SIDEBAR_VIEW_MENU;
                        UpdateSidebarEditPosition();
                        InvalidateRect(hWnd, NULL, FALSE);
                        return 0;
                    }
                    if (hit == ZONE_COPY_TAG) {
                        wchar_t myTag[32] = {0};
                        Friends_GetMyTag(myTag, 32);
                        CopyToClipboard(hWnd, myTag);
                        g_copiedTagTick = GetTickCount();
                        InvalidateRect(hWnd, NULL, FALSE);
                        return 0;
                    }
                    if (hit == ZONE_FTAB0) {
                        g_friendsTab = FRIENDS_TAB_LIST;
                        UpdateSidebarEditPosition();
                        InvalidateRect(hWnd, NULL, FALSE);
                        return 0;
                    }
                    if (hit == ZONE_FTAB1) {
                        g_friendsTab = FRIENDS_TAB_REQUESTS;
                        UpdateSidebarEditPosition();
                        InvalidateRect(hWnd, NULL, FALSE);
                        return 0;
                    }
                    if (hit == ZONE_FTAB2) {
                        g_friendsTab = FRIENDS_TAB_ADD;
                        UpdateSidebarEditPosition();
                        if (g_hFriendAddEdit) SetFocus(g_hFriendAddEdit);
                        InvalidateRect(hWnd, NULL, FALSE);
                        return 0;
                    }
                    if (hit == ZONE_FRIEND_SEND) {
                        InvalidateRect(hWnd, NULL, FALSE);
                        SubmitFriendRequest(hWnd);
                        return 0;
                    }

                    if (g_friendsTab == FRIENDS_TAB_LIST) {
                        for (int fi = 0; fi < Friends_GetFriendCount(); fi++) {
                            if (PtInRect(&rcFriendActionBtns[fi], pt)) {
                                const FriendItem *fItem = Friends_GetFriend(fi);
                                if (fItem) {
                                    wchar_t partnerTag[32] = {0};
                                    Jam_GetPartnerTag(partnerTag, 32);
                                    if (Jam_IsActive() && _wcsicmp(partnerTag, fItem->tag) == 0) {
                                        Jam_Leave();
                                    } else if (fItem->isOnline) {
                                        Jam_InviteFriend(fItem->tag, fItem->name);
                                    }
                                }
                                InvalidateRect(hWnd, NULL, FALSE);
                                return 0;
                            }
                        }
                    }
                    else if (g_friendsTab == FRIENDS_TAB_REQUESTS) {
                        for (int pi = 0; pi < Friends_GetPendingCount(); pi++) {
                            if (PtInRect(&rcReqAcceptBtns[pi], pt)) {
                                const FriendRequest *rq = Friends_GetPending(pi);
                                if (rq) Friends_AcceptRequest(rq->tag);
                                InvalidateRect(hWnd, NULL, FALSE);
                                return 0;
                            }
                            if (PtInRect(&rcReqRejectBtns[pi], pt)) {
                                const FriendRequest *rq = Friends_GetPending(pi);
                                if (rq) Friends_RejectRequest(rq->tag);
                                InvalidateRect(hWnd, NULL, FALSE);
                                return 0;
                            }
                        }
                    }
                    return 0;
                }
            }

            // 3. Kaydırma Çubuğu (Apple 1:1 Tutma Ofseti Korunarak)
            if (GetMaxScrollPx(hWnd) > 0 && PtInRect(&rcScrollbarTrack, pt)) {
                if (PtInRect(&rcScrollbarThumb, pt)) {
                    g_isDraggingScrollbar = TRUE;
                    g_scrollbarDragGrabOffsetY = pt.y - rcScrollbarThumb.top;
                } else {
                    int thumbH = rcScrollbarThumb.bottom - rcScrollbarThumb.top;
                    g_isDraggingScrollbar = TRUE;
                    g_scrollbarDragGrabOffsetY = thumbH / 2;
                    SendMessageW(hWnd, WM_MOUSEMOVE, wParam, lParam);
                }
                InvalidateRect(hWnd, NULL, FALSE);
                return 0;
            }

            // 4. Alt Oynatıcı Kontrolleri (Anında Pointer-Down Tepkisi!)
            if (hit == ZONE_JAM_LEAVE) {
                Jam_Leave();
                InvalidateRect(hWnd, NULL, FALSE);
                return 0;
            }
            if (hit == ZONE_SHUFFLE) {
                g_cfg.shuffle = !g_cfg.shuffle;
                Config_Save(&g_cfg, L"config.json");
                InvalidateRect(hWnd, NULL, FALSE);
                return 0;
            }
            if (hit == ZONE_PLAY) {
                if (Player_IsPlaying()) {
                    Player_Pause();
                    if (Jam_IsActive()) Jam_BroadcastPause(Player_GetPosition());
                } else {
                    if (g_currentTrack >= 0) {
                        Player_Play();
                        if (Jam_IsActive()) Jam_BroadcastPlay(Playlist_Get(g_currentTrack)->title, Player_GetPosition());
                    } else if (Playlist_Count() > 0) {
                        PlayTrack(g_selectedTrack >= 0 ? g_selectedTrack : 0);
                    }
                }
                InvalidateRect(hWnd, NULL, FALSE);
                return 0;
            }
            if (hit == ZONE_PREV) {
                int prev = Playlist_FindPrev(g_currentTrack);
                if (prev >= 0) { PlayTrack(prev); ScrollTrackIntoView(prev); }
                InvalidateRect(hWnd, NULL, FALSE);
                return 0;
            }
            if (hit == ZONE_NEXT) {
                int next = Playlist_FindNext(g_currentTrack, g_cfg.shuffle, g_cfg.repeat == 2 ? 1 : g_cfg.repeat);
                if (next >= 0) { PlayTrack(next); ScrollTrackIntoView(next); }
                InvalidateRect(hWnd, NULL, FALSE);
                return 0;
            }
            if (hit == ZONE_REPEAT) {
                g_cfg.repeat = (g_cfg.repeat + 1) % 3;
                Config_Save(&g_cfg, L"config.json");
                InvalidateRect(hWnd, NULL, FALSE);
                return 0;
            }
            if (hit == ZONE_SPEAKER) {
                if (Player_GetVolume() > 0) {
                    g_preMuteVol = Player_GetVolume();
                    Player_SetVolume(0);
                } else {
                    Player_SetVolume(g_preMuteVol > 0 ? g_preMuteVol : 80);
                }
                g_cfg.volume = Player_GetVolume();
                Config_Save(&g_cfg, L"config.json");
                InvalidateRect(hWnd, NULL, FALSE);
                return 0;
            }
            if (hit == ZONE_SEEK) {
                int duration = Player_GetDuration();
                if (duration > 0 && rcSeekBar.right > rcSeekBar.left) {
                    float ratio = (float)(pt.x - rcSeekBar.left) / (float)(rcSeekBar.right - rcSeekBar.left);
                    if (ratio < 0.0f) ratio = 0.0f;
                    if (ratio > 1.0f) ratio = 1.0f;
                    g_seekDragSec = (int)(ratio * duration);
                    g_isDraggingSeek = TRUE;
                    Player_SetPosition(g_seekDragSec);
                    if (Jam_IsActive()) Jam_BroadcastSeek(g_seekDragSec);
                    InvalidateRect(hWnd, NULL, FALSE);
                }
                return 0;
            }
            if (hit == ZONE_VOL) {
                g_isDraggingVol = TRUE;
                SendMessageW(hWnd, WM_MOUSEMOVE, wParam, lParam);
                return 0;
            }

            // 5. Şarkı Listesi: Tek Tıkla Anında Çal + Sürükleme Hazırlığı
            RECT rcClient;
            GetClientRect(hWnd, &rcClient);
            int listTop = GetHeaderH();
            int listBottom = rcClient.bottom - GetDockH();
            if (pt.y >= listTop && pt.y < listBottom && pt.x >= S(12) && pt.x <= rcClient.right - S(18)) {
                int displayIdx = (int)((pt.y - (listTop + S(6)) + g_scrollPosPx) / (float)GetItemH());
                int totalCount = g_isFiltered ? g_filterCount : Playlist_Count();
                if (displayIdx >= 0 && displayIdx < totalCount) {
                    int actualIdx = g_isFiltered ? g_filterIndices[displayIdx] : displayIdx;
                    g_dragFromIndex = actualIdx;
                    g_dragTargetIndex = actualIdx;
                    g_mouseDownPt = pt;
                    g_isDraggingList = FALSE;
                    PlayTrack(actualIdx);
                }
            }
            return 0;
        }

        case WM_LBUTTONUP: {
            ReleaseCapture();
            BOOL needRepaint = (g_pressedZone != ZONE_NONE);
            g_pressedZone = ZONE_NONE;

            if (g_isDraggingScrollbar) {
                g_isDraggingScrollbar = FALSE;
                needRepaint = TRUE;
            }
            if (g_isDraggingSeek) {
                g_isDraggingSeek = FALSE;
                Player_SetPosition(g_seekDragSec);
                if (Jam_IsActive()) Jam_BroadcastSeek(g_seekDragSec);
                needRepaint = TRUE;
            }
            if (g_isDraggingVol) {
                g_isDraggingVol = FALSE;
                Config_Save(&g_cfg, L"config.json");
                needRepaint = TRUE;
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
                needRepaint = TRUE;
            }
            g_isDraggingList = FALSE;
            g_dragFromIndex = -1;
            g_dragTargetIndex = -1;

            if (needRepaint) {
                InvalidateRect(hWnd, NULL, FALSE);
            }
            return 0;
        }

        case WM_KEYDOWN: {
            if (wParam == VK_ESCAPE && g_sidebarOpen) {
                ToggleSidebar(FALSE);
                return 0;
            }
            if (GetKeyState(VK_CONTROL) < 0) {
                if (wParam == 'R' || wParam == 'r') {
                    g_cfg.repeat = (g_cfg.repeat + 1) % 3;
                    Config_Save(&g_cfg, L"config.json");
                    InvalidateRect(hWnd, NULL, FALSE);
                    return 0;
                }
                if (wParam == 'S' || wParam == 's') {
                    g_cfg.shuffle = !g_cfg.shuffle;
                    Config_Save(&g_cfg, L"config.json");
                    InvalidateRect(hWnd, NULL, FALSE);
                    return 0;
                }
                if (wParam == 'F' || wParam == 'f') {
                    if (g_hSearchEdit) SetFocus(g_hSearchEdit);
                    return 0;
                }
            }
            switch (wParam) {
                case VK_SPACE: {
                    if (Player_IsPlaying()) {
                        Player_Pause();
                        if (Jam_IsActive()) Jam_BroadcastPause(Player_GetPosition());
                    } else if (g_currentTrack >= 0) {
                        Player_Play();
                        if (Jam_IsActive()) Jam_BroadcastPlay(Playlist_Get(g_currentTrack)->title, Player_GetPosition());
                    } else if (Playlist_Count() > 0) {
                        PlayTrack(g_selectedTrack >= 0 ? g_selectedTrack : 0);
                    }
                    InvalidateRect(hWnd, NULL, FALSE);
                    return 0;
                }
                case VK_LEFT: {
                    int pos = Player_GetPosition() - 5;
                    if (pos < 0) pos = 0;
                    Player_SetPosition(pos);
                    if (Jam_IsActive()) Jam_BroadcastSeek(pos);
                    InvalidateRect(hWnd, NULL, FALSE);
                    return 0;
                }
                case VK_RIGHT: {
                    int pos = Player_GetPosition() + 5;
                    int dur = Player_GetDuration();
                    if (dur > 0 && pos > dur) pos = dur;
                    Player_SetPosition(pos);
                    if (Jam_IsActive()) Jam_BroadcastSeek(pos);
                    InvalidateRect(hWnd, NULL, FALSE);
                    return 0;
                }
                case VK_UP: {
                    int vol = Player_GetVolume() + 5;
                    Player_SetVolume(vol);
                    if (Player_GetVolume() > 0) g_preMuteVol = Player_GetVolume();
                    g_cfg.volume = Player_GetVolume();
                    Config_Save(&g_cfg, L"config.json");
                    InvalidateRect(hWnd, NULL, FALSE);
                    return 0;
                }
                case VK_DOWN: {
                    int vol = Player_GetVolume() - 5;
                    Player_SetVolume(vol);
                    if (Player_GetVolume() > 0) g_preMuteVol = Player_GetVolume();
                    g_cfg.volume = Player_GetVolume();
                    Config_Save(&g_cfg, L"config.json");
                    InvalidateRect(hWnd, NULL, FALSE);
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
                            ScrollTrackIntoView(found);
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
            mmi->ptMinTrackSize.x = S(480);
            mmi->ptMinTrackSize.y = S(500);
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

            EnsureBackbuffer(hdc, rcClient.right, rcClient.bottom);
            if (g_backDC) {
                RenderUI(g_backDC, &rcClient);
                int rx = ps.rcPaint.left;
                int ry = ps.rcPaint.top;
                int rw = ps.rcPaint.right - ps.rcPaint.left;
                int rh = ps.rcPaint.bottom - ps.rcPaint.top;
                BitBlt(hdc, rx, ry, rw, rh, g_backDC, rx, ry, SRCCOPY);
            } else {
                RenderUI(hdc, &rcClient);
            }

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
            if (g_animTimerActive) KillTimer(hWnd, IDT_ANIM_TIMER);
            Player_Shutdown();
            Playlist_Free();
            FreeBackbuffer();
            if (g_filterIndices) {
                free(g_filterIndices);
                g_filterIndices = NULL;
            }

            if (g_hFontTitle)      DeleteObject(g_hFontTitle);
            if (g_hFontBold)       DeleteObject(g_hFontBold);
            if (g_hFontMain)       DeleteObject(g_hFontMain);
            if (g_hFontSmall)      DeleteObject(g_hFontSmall);
            if (g_hFontSmallBold)  DeleteObject(g_hFontSmallBold);
            if (g_hFontIcons)      DeleteObject(g_hFontIcons);
            if (g_hFontIconsSmall) DeleteObject(g_hFontIconsSmall);
            if (g_hFontIconsLarge) DeleteObject(g_hFontIconsLarge);
            if (g_hBrSearchEdit)   DeleteObject(g_hBrSearchEdit);
            if (g_pMainThumb) {
                g_pMainThumb->lpVtbl->Release(g_pMainThumb);
                g_pMainThumb = NULL;
            }
            if (g_hAppIcon) {
                DestroyIcon(g_hAppIcon);
                g_hAppIcon = NULL;
            }

            Jam_Leave();
            Network_Close();
            PostQuitMessage(0);
            return 0;
        }
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    (void)hInstance; (void)hPrevInstance; (void)pCmdLine;

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

    int winW = MulDiv(540, screenDpi, 96);
    int winH = MulDiv(680, screenDpi, 96);

    HWND hWnd = CreateWindowExW(
        WS_EX_ACCEPTFILES,
        L"SeloPlayerNativeClass",
        L"Selo Player",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
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

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    OleUninitialize();
    return (int)msg.wParam;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)lpCmdLine;
    return wWinMain(hInstance, hPrevInstance, GetCommandLineW(), nCmdShow);
}
