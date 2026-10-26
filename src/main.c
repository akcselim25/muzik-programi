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
#include <stdint.h>
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

// =============================================================================
// TASARIM 6 — YÜZEN KAPSÜL BAR & SPOTIFY YEŞİLİ RENK PALETİ
// =============================================================================
#define COLOR_BG            RGB(11, 11, 15)      // Saf Derin Kadife Siyah (#0B0B0F)
#define COLOR_ROW_HOVER     RGB(18, 19, 25)      // Şarkı Satırı Hover
#define COLOR_ROW_ACTIVE    RGB(14, 28, 20)      // Çalan Şarkı Satırı (Zümrüt Işıltılı)
#define COLOR_ROW_SEL       RGB(16, 17, 23)      // Seçili Şarkı Satırı
#define COLOR_CAPSULE_BG    RGB(20, 21, 28)      // Yüzen Alt Kapsül Kartı (#14151C)
#define COLOR_CAPSULE_BORD  RGB(42, 44, 58)      // Kapsül Cam Kenarlık (#2A2C3A)
#define COLOR_SIDEBAR_BG    RGB(14, 15, 20)      // Sol Çekmece Katmanı
#define COLOR_CARD_SURF     RGB(20, 22, 29)      // Yükseltilmiş Kart Yüzeyi
#define COLOR_CARD_HOVER    RGB(27, 30, 39)      // Kart Hover Yüzeyi
#define COLOR_CTRL_HOVER    RGB(32, 35, 46)      // İkon Düğme Hover
#define COLOR_CTRL_PRESS    RGB(42, 46, 60)      // İkon Düğme Basılı
#define COLOR_ACCENT        RGB(29, 185, 84)     // Spotify Yeşili (#1DB954)
#define COLOR_ACCENT_HOV    RGB(30, 215, 96)     // Parlak Yeşil (#1ED760)
#define COLOR_ACCENT_DARK   RGB(20, 142, 64)     // Koyu Basılı Yeşil
#define COLOR_AMBER         RGB(245, 176, 65)    // Bekleyen Davet Vurgusu
#define COLOR_DANGER        RGB(235, 87, 87)     // Ayrıl / Kaldır Vurgusu
#define COLOR_TEXT_WHITE    RGB(240, 240, 246)   // Birincil Parlak Metin
#define COLOR_TEXT_MUTED    RGB(124, 126, 140)   // İkincil Sanatçı Metni
#define COLOR_TEXT_DIM      RGB(88, 90, 104)     // Üçüncül Numara / Süre Metni
#define COLOR_BAR_BG        RGB(42, 44, 56)      // İlerleme / Ses Çubuğu Arka Planı
#define COLOR_BAR_HOVER     RGB(58, 62, 78)      // Çubuk Hover Arka Planı
#define COLOR_BORDER        RGB(34, 36, 48)      // İnce Kenarlık
#define COLOR_BORDER_LIGHT  RGB(46, 50, 66)      // Vurgulu Kenarlık
#define COLOR_INPUT_BG      RGB(19, 19, 26)      // Arama & Giriş Kutusu İç Yüzeyi (#13131A)

#define IDT_UPDATE_TIMER        1001
#define IDT_ANIM_TIMER          1002
#define IDC_HEADER_SEARCH       1010
#define IDC_FRIEND_ADD_EDIT     1020

#define IDM_CTX_PLAY            2001
#define IDM_CTX_OPEN_FOLDER     2002
#define IDM_CTX_REMOVE          2003

#define WM_APP_JAM_AUTODL_DONE  (WM_APP + 450)

typedef enum {
    ZONE_NONE = 0,
    ZONE_HAMBURGER,
    ZONE_SEARCH_CLEAR,
    ZONE_PREV,
    ZONE_PLAY,
    ZONE_NEXT,
    ZONE_REPEAT,
    ZONE_SPEAKER,
    ZONE_SEEK,
    ZONE_VOL,
    ZONE_JAM_LEAVE,
    ZONE_JAM_MISSING_DL,
    ZONE_TOAST_ACCEPT,
    ZONE_TOAST_REJECT,
    ZONE_FRIENDS_BACK,
    ZONE_FRIENDS_REFRESH,
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
static inline int   S(int px)    { return MulDiv(px, g_dpi, 96); }
static inline float SF(float px) { return px * ((float)g_dpi / 96.0f); }
static inline int GetHeaderH(void)   { return S(46); }
static inline int GetCapsuleH(void)  { return S(52); }
static inline int GetItemH(void)     { return S(38); }
static inline int GetSidebarW(void)  { return S(260); }

static HWND    g_hWnd = NULL;
static HWND    g_hSearchEdit = NULL;
static WNDPROC g_oldSearchEditProc = NULL;
static HBRUSH  g_hBrInputBg = NULL;

static HFONT g_hFontTitle = NULL;
static HFONT g_hFontMain = NULL;
static HFONT g_hFontBold = NULL;
static HFONT g_hFontSmall = NULL;
static HFONT g_hFontSmallBold = NULL;
static HFONT g_hFontMicro = NULL;

// 32-Bit Doğrudan Piksel Çift Tamponlama
static HDC       g_backDC = NULL;
static HBITMAP   g_backBmp = NULL;
static HBITMAP   g_backOldBmp = NULL;
static uint32_t *g_backPixels = NULL;
static int       g_backW = 0;
static int       g_backH = 0;
static RECT      g_pxClip = {0};

static HICON     g_hAppIcon = NULL;
static AppConfig g_cfg;
static int       g_currentTrack = -1;
static int       g_selectedTrack = -1;
static int       g_hoverTrack = -1;
static int       g_preMuteVol = 80;

// Sıfır FPS Düşüşü / Espor Modu Durum Takibi
static BOOL g_isWindowActive = TRUE;
static BOOL g_isMinimized = FALSE;
static BOOL g_socialNetworkStarted = FALSE;

// 60 FPS Kesintiye Uğratılabilir Yay Fiziği (Durduğunda %0.000 İşlemci!)
static float g_scrollPosPx = 0.0f;
static float g_scrollTargetPx = 0.0f;
static float g_scrollVelPx = 0.0f;

static BOOL  g_sidebarOpen = FALSE;
static float g_sidebarAnim = 0.0f;
static float g_sidebarVel = 0.0f;
static int   g_sidebarHoverItem = -1;

static BOOL  g_animTimerActive = FALSE;
static DWORD g_lastAnimTick = 0;

static UiZone g_hotZone = ZONE_NONE;
static UiZone g_pressedZone = ZONE_NONE;

static BOOL g_isDraggingSeek = FALSE;
static int  g_seekDragSec = 0;
static BOOL g_isDraggingVol = FALSE;

static BOOL g_isDraggingScrollbar = FALSE;
static int  g_scrollbarDragGrabOffsetY = 0;
static BOOL g_scrollbarHover = FALSE;
static RECT rcScrollbarTrack;
static RECT rcScrollbarThumb;

// Canlı Arama / Filtreleme
static int    *g_filterIndices = NULL;
static int     g_filterCap = 0;
static int     g_filterCount = 0;
static BOOL    g_isFiltered = FALSE;
static wchar_t g_searchQuery[128] = {0};

// Liste İçi Sürükle-Bırak
static int   g_dragFromIndex = -1;
static int   g_dragTargetIndex = -1;
static BOOL  g_isDraggingList = FALSE;
static POINT g_mouseDownPt = {0};

// Arkadaşlar & Jam Durumu
static SidebarView g_sidebarView = SIDEBAR_VIEW_MENU;
static FriendsTab  g_friendsTab  = FRIENDS_TAB_LIST;
static HWND        g_hFriendAddEdit = NULL;
static WNDPROC     g_oldFriendAddEditProc = NULL;
static wchar_t     g_friendAddMsg[128] = {0};
static DWORD       g_copiedTagTick = 0;
static DWORD       g_lastFriendsRefreshTick = 0;
static BOOL        g_isSyncingFromRemote = FALSE;

// Jam Otomatik Şarkı İndirme Durumu
static volatile LONG g_jamAutoDlBusy = 0;
static wchar_t       g_jamAutoDlTrack[256] = {0};

// Tıklama (Hit-Test) Bölgeleri
static RECT rcHamburger, rcSearchBox, rcSearchClear;
static RECT rcCapsule, rcPrev, rcPlay, rcNext, rcRepeat;
static RECT rcSeekBar, rcSeekHit, rcVolBar, rcVolHit, rcSpeaker;
static RECT rcToastBox, rcToastAccept, rcToastReject;
static RECT rcJamStage, rcJamLeave, rcJamMissingBtn;
static RECT rcFriendsBack, rcFriendsRefresh, rcCopyTag;
static RECT rcFriendsTab0, rcFriendsTab1, rcFriendsTab2;
static RECT rcFriendSendBtn;
static RECT rcFriendActionBtns[MAX_FRIENDS];
static RECT rcFriendRemoveBtns[MAX_FRIENDS];
static RECT rcReqAcceptBtns[MAX_REQUESTS];
static RECT rcReqRejectBtns[MAX_REQUESTS];

// =============================================================================
// YAZI TİPLERİ
// =============================================================================
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
    if (g_hFontMicro)      DeleteObject(g_hFontMicro);

    int sTitle     = -MulDiv(12, dpi, 72);
    int sMain      = -MulDiv(10, dpi, 72);
    int sSmall     = -MulDiv(9,  dpi, 72);
    int sMicro     = -MulDiv(8,  dpi, 72);

    const wchar_t *uiFont   = FontExists(L"Segoe UI Variable Text") ? L"Segoe UI Variable Text" : L"Segoe UI";
    const wchar_t *dispFont = FontExists(L"Segoe UI Variable Display") ? L"Segoe UI Variable Display" : uiFont;

    g_hFontTitle      = CreateFontW(sTitle, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, dispFont);
    g_hFontBold       = CreateFontW(sMain,  0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, uiFont);
    g_hFontMain       = CreateFontW(sMain,  0, 0, 0, FW_NORMAL,   FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, uiFont);
    g_hFontSmall      = CreateFontW(sSmall, 0, 0, 0, FW_NORMAL,   FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, uiFont);
    g_hFontSmallBold  = CreateFontW(sSmall, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, uiFont);
    g_hFontMicro      = CreateFontW(sMicro, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, uiFont);
}

// =============================================================================
// DOĞRUDAN PİKSEL ANTI-ALIASED (SDF) GRAFİK MOTORU
// =============================================================================
static void EnsureBackbuffer(HDC refDC, int w, int h) {
    if (w <= 0 || h <= 0) return;
    if (g_backDC && g_backBmp && g_backPixels && g_backW == w && g_backH == h) return;

    if (g_backDC) {
        if (g_backOldBmp) SelectObject(g_backDC, g_backOldBmp);
        if (g_backBmp) DeleteObject(g_backBmp);
        DeleteDC(g_backDC);
        g_backDC = NULL; g_backBmp = NULL; g_backOldBmp = NULL; g_backPixels = NULL;
    }

    BITMAPINFO bi = {0};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h; // Top-down DIB: y=0 üst satırdır
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    g_backDC = CreateCompatibleDC(refDC);
    if (g_backDC) {
        g_backBmp = CreateDIBSection(g_backDC, &bi, DIB_RGB_COLORS, (void**)&g_backPixels, NULL, 0);
        if (g_backBmp && g_backPixels) {
            g_backOldBmp = (HBITMAP)SelectObject(g_backDC, g_backBmp);
            g_backW = w;
            g_backH = h;
            g_pxClip = (RECT){ 0, 0, w, h };
        } else {
            if (g_backBmp) DeleteObject(g_backBmp);
            DeleteDC(g_backDC);
            g_backDC = NULL; g_backBmp = NULL; g_backPixels = NULL;
        }
    }
}

static void FreeBackbuffer(void) {
    if (g_backDC) {
        if (g_backOldBmp) SelectObject(g_backDC, g_backOldBmp);
        if (g_backBmp) DeleteObject(g_backBmp);
        DeleteDC(g_backDC);
        g_backDC = NULL; g_backBmp = NULL; g_backOldBmp = NULL; g_backPixels = NULL;
        g_backW = 0; g_backH = 0;
    }
}

static inline void SetPixelClip(int left, int top, int right, int bottom) {
    if (left < 0) left = 0;
    if (top < 0) top = 0;
    if (right > g_backW) right = g_backW;
    if (bottom > g_backH) bottom = g_backH;
    g_pxClip = (RECT){ left, top, right, bottom };
}

static inline void ResetPixelClip(void) {
    g_pxClip = (RECT){ 0, 0, g_backW, g_backH };
}

static inline uint32_t ColorToBGRA(COLORREF c) {
    return ((uint32_t)GetBValue(c)) | (((uint32_t)GetGValue(c)) << 8) | (((uint32_t)GetRValue(c)) << 16) | 0xFF000000u;
}

static inline void BlendPixelBGRA(uint32_t *dst, uint32_t sr, uint32_t sg, uint32_t sb, uint32_t alpha256) {
    if (alpha256 >= 256) {
        *dst = sb | (sg << 8) | (sr << 16) | 0xFF000000u;
        return;
    }
    if (alpha256 == 0) return;
    uint32_t d = *dst;
    uint32_t db = d & 0xFFu;
    uint32_t dg = (d >> 8) & 0xFFu;
    uint32_t dr = (d >> 16) & 0xFFu;
    uint32_t inv = 256u - alpha256;
    uint32_t ob = (sb * alpha256 + db * inv) >> 8;
    uint32_t og = (sg * alpha256 + dg * inv) >> 8;
    uint32_t or_ = (sr * alpha256 + dr * inv) >> 8;
    *dst = ob | (og << 8) | (or_ << 16) | 0xFF000000u;
}

static void FillSolidPx(int x, int y, int w, int h, COLORREF col) {
    if (!g_backPixels || w <= 0 || h <= 0) return;
    int x0 = (x < g_pxClip.left) ? g_pxClip.left : x;
    int y0 = (y < g_pxClip.top)  ? g_pxClip.top  : y;
    int x1 = (x + w > g_pxClip.right)  ? g_pxClip.right  : (x + w);
    int y1 = (y + h > g_pxClip.bottom) ? g_pxClip.bottom : (y + h);
    if (x0 >= x1 || y0 >= y1) return;

    uint32_t pxVal = ColorToBGRA(col);
    for (int py = y0; py < y1; py++) {
        uint32_t *row = g_backPixels + py * g_backW;
        for (int px = x0; px < x1; px++) {
            row[px] = pxVal;
        }
    }
}

static void DimRectPx(int x, int y, int w, int h, BYTE dimAlpha) {
    if (!g_backPixels || dimAlpha == 0 || w <= 0 || h <= 0) return;
    int x0 = (x < 0) ? 0 : x;
    int y0 = (y < 0) ? 0 : y;
    int x1 = (x + w > g_backW) ? g_backW : (x + w);
    int y1 = (y + h > g_backH) ? g_backH : (y + h);
    uint32_t keep = 256u - (uint32_t)dimAlpha;
    for (int py = y0; py < y1; py++) {
        uint32_t *row = g_backPixels + py * g_backW;
        for (int px = x0; px < x1; px++) {
            uint32_t d = row[px];
            uint32_t b = ((d & 0xFFu) * keep) >> 8;
            uint32_t g = (((d >> 8) & 0xFFu) * keep) >> 8;
            uint32_t r = (((d >> 16) & 0xFFu) * keep) >> 8;
            row[px] = b | (g << 8) | (r << 16) | 0xFF000000u;
        }
    }
}

// Pürüzsüz SDF Yuvarlatılmış Dikdörtgen
static void DrawSmoothRoundRect(const RECT *rc, int radiusPx, COLORREF fillCol, BYTE fillAlpha, COLORREF borderCol, BYTE borderAlpha) {
    if (!g_backPixels || !rc) return;
    int w = rc->right - rc->left;
    int h = rc->bottom - rc->top;
    if (w <= 0 || h <= 0) return;

    int x0 = (rc->left < g_pxClip.left) ? g_pxClip.left : rc->left;
    int y0 = (rc->top  < g_pxClip.top)  ? g_pxClip.top  : rc->top;
    int x1 = (rc->right  > g_pxClip.right)  ? g_pxClip.right  : rc->right;
    int y1 = (rc->bottom > g_pxClip.bottom) ? g_pxClip.bottom : rc->bottom;
    if (x0 >= x1 || y0 >= y1) return;

    float hw = (float)w * 0.5f;
    float hh = (float)h * 0.5f;
    float cx = (float)rc->left + hw;
    float cy = (float)rc->top  + hh;
    float rad = (float)radiusPx;
    if (rad > hw) rad = hw;
    if (rad > hh) rad = hh;
    if (rad < 0.0f) rad = 0.0f;

    float bx = hw - rad;
    float by = hh - rad;

    uint32_t fr = GetRValue(fillCol),   fg = GetGValue(fillCol),   fb = GetBValue(fillCol);
    uint32_t br = GetRValue(borderCol), bg = GetGValue(borderCol), bb = GetBValue(borderCol);
    uint32_t fA256 = ((uint32_t)fillAlpha * 256u + 127u) / 255u;
    uint32_t bA256 = ((uint32_t)borderAlpha * 256u + 127u) / 255u;
    uint32_t solidPx = ColorToBGRA(fillCol);

    for (int py = y0; py < y1; py++) {
        float dy = fabsf(((float)py + 0.5f) - cy) - by;
        uint32_t *row = g_backPixels + py * g_backW;

        if (dy <= -1.2f && borderAlpha == 0 && fillAlpha == 255) {
            for (int px = x0; px < x1; px++) {
                float dx = fabsf(((float)px + 0.5f) - cx) - bx;
                if (dx <= rad - 0.6f) {
                    row[px] = solidPx;
                } else {
                    float dist = dx - rad;
                    if (dist < 0.6f) {
                        float cov = 0.5f - dist;
                        if (cov > 1.0f) cov = 1.0f;
                        if (cov > 0.0f) BlendPixelBGRA(&row[px], fr, fg, fb, (uint32_t)(cov * 256.0f));
                    }
                }
            }
            continue;
        }

        for (int px = x0; px < x1; px++) {
            float dx = fabsf(((float)px + 0.5f) - cx) - bx;
            float dist;
            if (dx <= 0.0f && dy <= 0.0f) {
                dist = (dx > dy) ? dx : dy;
                dist -= rad;
            } else {
                float ox = (dx > 0.0f) ? dx : 0.0f;
                float oy = (dy > 0.0f) ? dy : 0.0f;
                dist = sqrtf(ox * ox + oy * oy) - rad;
            }

            if (dist >= 0.65f) continue;

            float outerCov = 0.5f - dist;
            if (outerCov < 0.0f) outerCov = 0.0f;
            if (outerCov > 1.0f) outerCov = 1.0f;

            if (borderAlpha > 0) {
                float innerCov = 0.5f - (dist + 1.1f);
                if (innerCov < 0.0f) innerCov = 0.0f;
                if (innerCov > 1.0f) innerCov = 1.0f;

                float borderWeight = outerCov - innerCov;
                if (innerCov > 0.0f && fA256 > 0) {
                    BlendPixelBGRA(&row[px], fr, fg, fb, (uint32_t)(innerCov * (float)fA256));
                }
                if (borderWeight > 0.0f) {
                    BlendPixelBGRA(&row[px], br, bg, bb, (uint32_t)(borderWeight * (float)bA256));
                }
            } else if (fA256 > 0) {
                BlendPixelBGRA(&row[px], fr, fg, fb, (uint32_t)(outerCov * (float)fA256));
            }
        }
    }
}

static void DrawSdfCircle(float cx, float cy, float radius, COLORREF col, BYTE alpha) {
    if (!g_backPixels || radius <= 0.0f || alpha == 0) return;
    int x0 = (int)floorf(cx - radius - 1.0f);
    int y0 = (int)floorf(cy - radius - 1.0f);
    int x1 = (int)ceilf(cx + radius + 1.0f);
    int y1 = (int)ceilf(cy + radius + 1.0f);
    if (x0 < g_pxClip.left)   x0 = g_pxClip.left;
    if (y0 < g_pxClip.top)    y0 = g_pxClip.top;
    if (x1 > g_pxClip.right)  x1 = g_pxClip.right;
    if (y1 > g_pxClip.bottom) y1 = g_pxClip.bottom;
    if (x0 >= x1 || y0 >= y1) return;

    uint32_t sr = GetRValue(col), sg = GetGValue(col), sb = GetBValue(col);
    uint32_t a256 = ((uint32_t)alpha * 256u + 127u) / 255u;

    for (int py = y0; py < y1; py++) {
        float dy = ((float)py + 0.5f) - cy;
        uint32_t *row = g_backPixels + py * g_backW;
        for (int px = x0; px < x1; px++) {
            float dx = ((float)px + 0.5f) - cx;
            float dist = sqrtf(dx * dx + dy * dy) - radius;
            if (dist < 0.65f) {
                float cov = 0.5f - dist;
                if (cov < 0.0f) cov = 0.0f;
                if (cov > 1.0f) cov = 1.0f;
                BlendPixelBGRA(&row[px], sr, sg, sb, (uint32_t)(cov * (float)a256));
            }
        }
    }
}

static void DrawHairlineH(int x1, int x2, int y, COLORREF col, BYTE alpha) {
    if (!g_backPixels || y < g_pxClip.top || y >= g_pxClip.bottom) return;
    if (x1 < g_pxClip.left)  x1 = g_pxClip.left;
    if (x2 > g_pxClip.right) x2 = g_pxClip.right;
    if (x1 >= x2) return;
    uint32_t sr = GetRValue(col), sg = GetGValue(col), sb = GetBValue(col);
    uint32_t a256 = ((uint32_t)alpha * 256u + 127u) / 255u;
    uint32_t *row = g_backPixels + y * g_backW;
    for (int px = x1; px < x2; px++) {
        BlendPixelBGRA(&row[px], sr, sg, sb, a256);
    }
}

static void DrawHairlineV(int x, int y1, int y2, COLORREF col, BYTE alpha) {
    if (!g_backPixels || x < g_pxClip.left || x >= g_pxClip.right) return;
    if (y1 < g_pxClip.top)    y1 = g_pxClip.top;
    if (y2 > g_pxClip.bottom) y2 = g_pxClip.bottom;
    if (y1 >= y2) return;
    uint32_t sr = GetRValue(col), sg = GetGValue(col), sb = GetBValue(col);
    uint32_t a256 = ((uint32_t)alpha * 256u + 127u) / 255u;
    for (int py = y1; py < y2; py++) {
        BlendPixelBGRA(&g_backPixels[py * g_backW + x], sr, sg, sb, a256);
    }
}

// Subpixel Anti-Aliased Doğru Parçası (Yuvarlak Uçlu)
static void DrawSdfLine(float x1, float y1, float x2, float y2, float halfThick, COLORREF col, BYTE alpha) {
    if (!g_backPixels || alpha == 0) return;
    float minX = (x1 < x2 ? x1 : x2) - halfThick - 1.0f;
    float maxX = (x1 > x2 ? x1 : x2) + halfThick + 1.0f;
    float minY = (y1 < y2 ? y1 : y2) - halfThick - 1.0f;
    float maxY = (y1 > y2 ? y1 : y2) + halfThick + 1.0f;

    int ix0 = (int)floorf(minX); if (ix0 < g_pxClip.left)   ix0 = g_pxClip.left;
    int iy0 = (int)floorf(minY); if (iy0 < g_pxClip.top)    iy0 = g_pxClip.top;
    int ix1 = (int)ceilf(maxX);  if (ix1 > g_pxClip.right)  ix1 = g_pxClip.right;
    int iy1 = (int)ceilf(maxY);  if (iy1 > g_pxClip.bottom) iy1 = g_pxClip.bottom;
    if (ix0 >= ix1 || iy0 >= iy1) return;

    float vx = x2 - x1, vy = y2 - y1;
    float len2 = vx * vx + vy * vy;
    uint32_t sr = GetRValue(col), sg = GetGValue(col), sb = GetBValue(col);
    uint32_t a256 = ((uint32_t)alpha * 256u + 127u) / 255u;

    for (int py = iy0; py < iy1; py++) {
        float pyc = (float)py + 0.5f;
        uint32_t *row = g_backPixels + py * g_backW;
        for (int px = ix0; px < ix1; px++) {
            float pxc = (float)px + 0.5f;
            float t = (len2 > 0.0001f) ? (((pxc - x1) * vx + (pyc - y1) * vy) / len2) : 0.0f;
            if (t < 0.0f) t = 0.0f; else if (t > 1.0f) t = 1.0f;
            float dx = pxc - (x1 + t * vx);
            float dy = pyc - (y1 + t * vy);
            float dist = sqrtf(dx * dx + dy * dy) - halfThick;
            if (dist < 0.65f) {
                float cov = 0.5f - dist;
                if (cov < 0.0f) cov = 0.0f;
                if (cov > 1.0f) cov = 1.0f;
                BlendPixelBGRA(&row[px], sr, sg, sb, (uint32_t)(cov * (float)a256));
            }
        }
    }
}

// Subpixel Anti-Aliased İçi Dolu Üçgen
static void DrawSdfTriangle(float x1, float y1, float x2, float y2, float x3, float y3, COLORREF col, BYTE alpha) {
    if (!g_backPixels || alpha == 0) return;
    float minX = fminf(x1, fminf(x2, x3)) - 1.0f;
    float maxX = fmaxf(x1, fmaxf(x2, x3)) + 1.0f;
    float minY = fminf(y1, fminf(y2, y3)) - 1.0f;
    float maxY = fmaxf(y1, fmaxf(y2, y3)) + 1.0f;

    int ix0 = (int)floorf(minX); if (ix0 < g_pxClip.left)   ix0 = g_pxClip.left;
    int iy0 = (int)floorf(minY); if (iy0 < g_pxClip.top)    iy0 = g_pxClip.top;
    int ix1 = (int)ceilf(maxX);  if (ix1 > g_pxClip.right)  ix1 = g_pxClip.right;
    int iy1 = (int)ceilf(maxY);  if (iy1 > g_pxClip.bottom) iy1 = g_pxClip.bottom;
    if (ix0 >= ix1 || iy0 >= iy1) return;

    // Saat yönü sıralamasını garanti et
    if ((x2 - x1) * (y3 - y1) - (y2 - y1) * (x3 - x1) < 0.0f) {
        float tx = x2; x2 = x3; x3 = tx;
        float ty = y2; y2 = y3; y3 = ty;
    }

    uint32_t sr = GetRValue(col), sg = GetGValue(col), sb = GetBValue(col);
    uint32_t a256 = ((uint32_t)alpha * 256u + 127u) / 255u;

    for (int py = iy0; py < iy1; py++) {
        float pyc = (float)py + 0.5f;
        uint32_t *row = g_backPixels + py * g_backW;
        for (int px = ix0; px < ix1; px++) {
            float pxc = (float)px + 0.5f;

            // 3 kenarın mesafesi
            float d1 = (pxc - x1) * (y2 - y1) - (pyc - y1) * (x2 - x1);
            float d2 = (pxc - x2) * (y3 - y2) - (pyc - y2) * (x3 - x2);
            float d3 = (pxc - x3) * (y1 - y3) - (pyc - y3) * (x1 - x3);

            if (d1 <= 0.0f && d2 <= 0.0f && d3 <= 0.0f) {
                // İçeride
                float dist1 = -d1 / sqrtf((y2 - y1) * (y2 - y1) + (x2 - x1) * (x2 - x1));
                float dist2 = -d2 / sqrtf((y3 - y2) * (y3 - y2) + (x3 - x2) * (x3 - x2));
                float dist3 = -d3 / sqrtf((y1 - y3) * (y1 - y3) + (x1 - x3) * (x1 - x3));
                float minDist = fminf(dist1, fminf(dist2, dist3));
                float cov = 0.5f + minDist;
                if (cov > 1.0f) cov = 1.0f;
                BlendPixelBGRA(&row[px], sr, sg, sb, (uint32_t)(cov * (float)a256));
            } else {
                // Dışarıda kenar yumuşatma
                float dist1 = d1 > 0.0f ? (d1 / sqrtf((y2 - y1) * (y2 - y1) + (x2 - x1) * (x2 - x1))) : 0.0f;
                float dist2 = d2 > 0.0f ? (d2 / sqrtf((y3 - y2) * (y3 - y2) + (x3 - x2) * (x3 - x2))) : 0.0f;
                float dist3 = d3 > 0.0f ? (d3 / sqrtf((y1 - y3) * (y1 - y3) + (x1 - x3) * (x1 - x3))) : 0.0f;
                float maxOut = fmaxf(dist1, fmaxf(dist2, dist3));
                if (maxOut < 0.65f) {
                    float cov = 0.5f - maxOut;
                    if (cov > 0.0f) BlendPixelBGRA(&row[px], sr, sg, sb, (uint32_t)(cov * (float)a256));
                }
            }
        }
    }
}

// =============================================================================
// VEKTÖREL VE KESKİN İKON ÇİZİMLERİ (HER SİSTEMDE NET VE SIFIR HARİCİ YAZI TİPİ)
// =============================================================================
static void DrawVecHamburger(float x, float y, COLORREF col) {
    float barW = (float)S(16);
    float hThick = SF(0.9f);
    DrawSdfLine(x, y, x + barW, y, hThick, col, 240);
    DrawSdfLine(x, y + SF(5.2f), x + barW, y + SF(5.2f), hThick, col, 240);
    DrawSdfLine(x, y + SF(10.4f), x + barW * 0.70f, y + SF(10.4f), hThick, col, 240);
}

static void DrawVecSearch(float cx, float cy, COLORREF col) {
    float r = SF(4.8f);
    float hThick = SF(0.85f);
    // Halka
    int x0 = (int)floorf(cx - r - 2.0f);
    int y0 = (int)floorf(cy - r - 2.0f);
    int x1 = (int)ceilf(cx + r + 2.0f);
    int y1 = (int)ceilf(cy + r + 2.0f);
    if (x0 < g_pxClip.left)   x0 = g_pxClip.left;
    if (y0 < g_pxClip.top)    y0 = g_pxClip.top;
    if (x1 > g_pxClip.right)  x1 = g_pxClip.right;
    if (y1 > g_pxClip.bottom) y1 = g_pxClip.bottom;

    uint32_t sr = GetRValue(col), sg = GetGValue(col), sb = GetBValue(col);
    for (int py = y0; py < y1; py++) {
        float dy = ((float)py + 0.5f) - cy;
        uint32_t *row = g_backPixels + py * g_backW;
        for (int px = x0; px < x1; px++) {
            float dx = ((float)px + 0.5f) - cx;
            float dist = fabsf(sqrtf(dx * dx + dy * dy) - r) - hThick;
            if (dist < 0.65f) {
                float cov = 0.5f - dist;
                if (cov < 0.0f) cov = 0.0f;
                if (cov > 1.0f) cov = 1.0f;
                BlendPixelBGRA(&row[px], sr, sg, sb, (uint32_t)(cov * 255.0f));
            }
        }
    }
    // Sap
    DrawSdfLine(cx + r * 0.70f, cy + r * 0.70f, cx + r + SF(4.5f), cy + r + SF(4.5f), hThick, col, 240);
}

static void DrawVecCross(float cx, float cy, float sz, COLORREF col) {
    float hThick = SF(0.85f);
    float hs = sz * 0.5f;
    DrawSdfLine(cx - hs, cy - hs, cx + hs, cy + hs, hThick, col, 230);
    DrawSdfLine(cx - hs, cy + hs, cx + hs, cy - hs, hThick, col, 230);
}

static void DrawVecPlay(float cx, float cy, float sz, COLORREF col) {
    float hs = sz * 0.5f;
    float x1 = cx - hs * 0.65f, y1 = cy - hs;
    float x2 = cx - hs * 0.65f, y2 = cy + hs;
    float x3 = cx + hs * 0.90f, y3 = cy;
    DrawSdfTriangle(x1, y1, x2, y2, x3, y3, col, 255);
}

static void DrawVecPause(float cx, float cy, float w, float h, COLORREF col) {
    float hw = w * 0.5f, hh = h * 0.5f;
    float barW = w * 0.32f;
    RECT r1 = { (int)(cx - hw), (int)(cy - hh), (int)(cx - hw + barW), (int)(cy + hh) };
    RECT r2 = { (int)(cx + hw - barW), (int)(cy - hh), (int)(cx + hw), (int)(cy + hh) };
    DrawSmoothRoundRect(&r1, (int)SF(1.5f), col, 255, 0, 0);
    DrawSmoothRoundRect(&r2, (int)SF(1.5f), col, 255, 0, 0);
}

static void DrawVecPrev(float cx, float cy, float sz, COLORREF col) {
    float hs = sz * 0.5f;
    float hThick = SF(0.9f);
    DrawSdfLine(cx - hs * 0.85f, cy - hs * 0.85f, cx - hs * 0.85f, cy + hs * 0.85f, hThick, col, 255);
    DrawSdfTriangle(cx - hs * 0.70f, cy, cx + hs * 0.85f, cy - hs * 0.90f, cx + hs * 0.85f, cy + hs * 0.90f, col, 255);
}

static void DrawVecNext(float cx, float cy, float sz, COLORREF col) {
    float hs = sz * 0.5f;
    float hThick = SF(0.9f);
    DrawSdfLine(cx + hs * 0.85f, cy - hs * 0.85f, cx + hs * 0.85f, cy + hs * 0.85f, hThick, col, 255);
    DrawSdfTriangle(cx + hs * 0.70f, cy, cx - hs * 0.85f, cy - hs * 0.90f, cx - hs * 0.85f, cy + hs * 0.90f, col, 255);
}

static void DrawVecRepeat(float cx, float cy, float sz, COLORREF col) {
    float w = sz * 0.95f;
    float h = sz * 0.62f;
    float x = cx - w * 0.5f;
    float y = cy - h * 0.5f;
    float hThick = SF(0.85f);

    // Üst şerit
    DrawSdfLine(x, cy + SF(1.0f), x, y + SF(2.0f), hThick, col, 255);
    DrawSdfLine(x, y, x + w - SF(2.5f), y, hThick, col, 255);
    DrawSdfTriangle(x + w + SF(1.2f), y, x + w - SF(3.2f), y - SF(2.8f), x + w - SF(3.2f), y + SF(2.8f), col, 255);

    // Alt şerit
    DrawSdfLine(x + w, cy - SF(1.0f), x + w, y + h - SF(2.0f), hThick, col, 255);
    DrawSdfLine(x + w, y + h, x + SF(2.5f), y + h, hThick, col, 255);
    DrawSdfTriangle(x - SF(1.2f), y + h, x + SF(3.2f), y + h - SF(2.8f), x + SF(3.2f), y + h + SF(2.8f), col, 255);
}

static void DrawVecSpeaker(float cx, float cy, COLORREF col) {
    float hThick = SF(0.8f);
    float x1 = cx - SF(5.5f), y1 = cy - SF(2.2f);
    float x2 = cx - SF(2.5f), y2 = cy - SF(2.2f);
    float x3 = cx + SF(1.0f), y3 = cy - SF(5.5f);
    float x4 = cx + SF(1.0f), y4 = cy + SF(5.5f);
    float x5 = cx - SF(2.5f), y5 = cy + SF(2.2f);
    float x6 = cx - SF(5.5f), y6 = cy + SF(2.2f);

    DrawSdfTriangle(x1, y1, x2, y2, x6, y6, col, 255);
    DrawSdfTriangle(x2, y2, x5, y5, x6, y6, col, 255);
    DrawSdfTriangle(x2, y2, x3, y3, x4, y4, col, 255);
    DrawSdfTriangle(x2, y2, x4, y4, x5, y5, col, 255);

    // Ses Dalgası (yay)
    DrawSdfLine(cx + SF(3.0f), cy - SF(3.0f), cx + SF(4.5f), cy, hThick, col, 255);
    DrawSdfLine(cx + SF(4.5f), cy, cx + SF(3.0f), cy + SF(3.0f), hThick, col, 255);
}

// =============================================================================
// OPTİMİZE ZAMANLAYICI VE OYUN MODU YÖNETİMİ
// =============================================================================
static BOOL IsSocialRefreshActive(void) {
    return ((g_sidebarOpen && g_sidebarView == SIDEBAR_VIEW_FRIENDS) || Jam_GetState() != JAM_STATE_NONE);
}

static void UpdateTimerSchedule(HWND hWnd) {
    if (!hWnd) return;

    // Pencere simge durumundaysa veya arka plandaysa (örneğin oyundayken):
    if (!g_isWindowActive || g_isMinimized) {
        if (g_animTimerActive) {
            KillTimer(hWnd, IDT_ANIM_TIMER);
            g_animTimerActive = FALSE;
            g_scrollPosPx = g_scrollTargetPx;
            g_sidebarAnim = g_sidebarOpen ? 1.0f : 0.0f;
        }

        // Jam aktif değilse IDT_UPDATE_TIMER'ı TAMAMEN KAPAT! (%0.000 CPU)
        if (Jam_GetState() == JAM_STATE_NONE) {
            KillTimer(hWnd, IDT_UPDATE_TIMER);
        } else {
            // Jam aktifse 2000ms'de bir hafif timeout kontrolü (arka planda InvalidateRect çağrılmaz!)
            SetTimer(hWnd, IDT_UPDATE_TIMER, 2000, NULL);
        }
        return;
    }

    // Pencere önde ve odaktayken:
    if (Player_IsPlaying() || IsSocialRefreshActive()) {
        SetTimer(hWnd, IDT_UPDATE_TIMER, 1000, NULL); // 1 Hz (250ms değil!)
    } else {
        KillTimer(hWnd, IDT_UPDATE_TIMER);
    }
}

static void EnsureSocialNetworkStarted(void) {
    if (g_socialNetworkStarted) return;
    wchar_t myInitTag[32] = {0};
    Friends_GetMyTag(myInitTag, 32);
    char myInitKey[64] = {0};
    Network_TagToTopicKeyW(myInitTag, myInitKey, sizeof(myInitKey));
    Network_Init(g_hWnd, myInitKey);

    char myInboxTopic[160];
    snprintf(myInboxTopic, sizeof(myInboxTopic), "seloplayer/u/%s", myInitKey);
    Network_Subscribe(myInboxTopic);
    g_socialNetworkStarted = TRUE;
}

static void TriggerImmediateFriendsRefresh(void) {
    EnsureSocialNetworkStarted();
    g_lastFriendsRefreshTick = GetTickCount();
    Friends_RequestAllStatuses();
}

static void SyncLocalTrackToFriends(void) {
    const wchar_t *curTitle = (g_currentTrack >= 0 && g_currentTrack < Playlist_Count()) ? Playlist_Get(g_currentTrack)->title : L"";
    Friends_SetLocalTrackState(Player_IsPlaying(), curTitle);
}

static int GetMaxScrollPx(HWND hWnd) {
    RECT rc;
    GetClientRect(hWnd, &rc);
    int listTop = GetHeaderH();
    int listBottom = rc.bottom;
    int viewH = listBottom - listTop;
    if (viewH < 0) viewH = 0;
    int totalCount = g_isFiltered ? g_filterCount : Playlist_Count();
    int contentH = totalCount * GetItemH() + S(84); // Alttaki kapsülün arkasında kalmaması için ek pay
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
    BOOL shouldShow = (g_sidebarAnim > 0.05f && g_sidebarView == SIDEBAR_VIEW_FRIENDS && g_friendsTab == FRIENDS_TAB_ADD);
    if (!shouldShow) {
        if (IsWindowVisible(g_hFriendAddEdit)) ShowWindow(g_hFriendAddEdit, SW_HIDE);
        return;
    }
    int sbW = GetSidebarW();
    int offX = (int)((g_sidebarAnim - 1.0f) * (float)sbW);
    int inputTop = GetHeaderH() + S(176);
    MoveWindow(g_hFriendAddEdit, offX + S(18), inputTop + S(6), sbW - S(36), S(24), TRUE);
    if (!IsWindowVisible(g_hFriendAddEdit)) ShowWindow(g_hFriendAddEdit, SW_SHOW);
}

static void StartAnimLoop(HWND hWnd) {
    if (!g_animTimerActive && hWnd && g_isWindowActive && !g_isMinimized) {
        g_lastAnimTick = GetTickCount();
        SetTimer(hWnd, IDT_ANIM_TIMER, 16, NULL);
        g_animTimerActive = TRUE;
    }
}

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
    if (g_hWnd && g_isWindowActive && !g_isMinimized) InvalidateRect(g_hWnd, NULL, FALSE);
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
    int viewH = rc.bottom - GetHeaderH();
    int itemH = GetItemH();
    float itemTop = (float)(displayIdx * itemH);
    float itemBot = itemTop + (float)itemH;

    if (itemTop < g_scrollTargetPx) {
        g_scrollTargetPx = itemTop;
        ClampScrollTargets(g_hWnd);
        StartAnimLoop(g_hWnd);
    } else if (itemBot > g_scrollTargetPx + (float)viewH - (float)S(80)) {
        g_scrollTargetPx = itemBot - (float)viewH + (float)S(84);
        ClampScrollTargets(g_hWnd);
        StartAnimLoop(g_hWnd);
    }
}

static void BroadcastCurrentJamTrack(int posSec) {
    if (!Jam_IsActive()) return;
    if (g_currentTrack >= 0 && g_currentTrack < Playlist_Count()) {
        const PlaylistItem *it = Playlist_Get(g_currentTrack);
        if (it) {
            wchar_t ytId[16] = {0};
            Playlist_ExtractYouTubeId(it->filePath, ytId, 16);
            Jam_BroadcastState(Player_IsPlaying(), it->title, ytId, posSec);
        }
    }
}

static void PlayTrack(int index) {
    if (index < 0 || index >= Playlist_Count()) return;
    const PlaylistItem *item = Playlist_Get(index);
    if (!item) return;

    g_currentTrack = index;
    g_selectedTrack = index;
    Player_OpenFile(item->filePath);
    SyncLocalTrackToFriends();

    if (Jam_IsActive() && !g_isSyncingFromRemote && item->title[0] != 0) {
        wchar_t ytId[16] = {0};
        Playlist_ExtractYouTubeId(item->filePath, ytId, 16);
        Jam_SetSyncStatus(L"✓ Canlı senkronize", FALSE, NULL);
        Jam_BroadcastState(TRUE, item->title, ytId, 0);
    }

    if (g_hWnd && g_isWindowActive && !g_isMinimized) {
        InvalidateRect(g_hWnd, NULL, FALSE);
    }
    UpdateTimerSchedule(g_hWnd);
}

// =============================================================================
// JAM OTOMATİK EKSİK ŞARKI İNDİRİCİ
// =============================================================================
typedef struct {
    wchar_t title[256];
    wchar_t ytId[16];
    wchar_t folder[MAX_PATH];
} JamAutoDlTask;

static void EnsureDefaultMusicFolder(wchar_t *outFolder, int maxLen) {
    if (g_cfg.lastFolder[0] && GetFileAttributesW(g_cfg.lastFolder) != INVALID_FILE_ATTRIBUTES) {
        wcsncpy(outFolder, g_cfg.lastFolder, maxLen - 1);
        outFolder[maxLen - 1] = L'\0';
        return;
    }
    GetModuleFileNameW(NULL, outFolder, maxLen);
    PathRemoveFileSpecW(outFolder);
    PathAppendW(outFolder, L"Müzikler");
    CreateDirectoryW(outFolder, NULL);
    wcsncpy(g_cfg.lastFolder, outFolder, MAX_PATH - 1);
    g_cfg.lastFolder[MAX_PATH - 1] = L'\0';
    Config_Save(&g_cfg, L"config.json");
}

static DWORD WINAPI JamAutoDownloadThread(LPVOID param) {
    JamAutoDlTask *task = (JamAutoDlTask*)param;
    BOOL success = FALSE;

    wchar_t targetSpec[320];
    if (task->ytId[0] && wcslen(task->ytId) == 11) {
        _snwprintf(targetSpec, 319, L"https://www.youtube.com/watch?v=%s", task->ytId);
    } else {
        _snwprintf(targetSpec, 319, L"ytsearch1:%s", task->title);
    }
    targetSpec[319] = L'\0';

    wchar_t cmdLine[2048];
    _snwprintf(cmdLine, 2047,
               L"yt-dlp.exe --no-warnings -f \"ba[ext=m4a]/ba\" --write-thumbnail --no-playlist -o \"%s\\%%(title)s [%%(id)s].%%(ext)s\" \"%s\"",
               task->folder, targetSpec);
    cmdLine[2047] = L'\0';

    STARTUPINFOW si = {0};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = {0};
    if (CreateProcessW(NULL, cmdLine, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 45000);
        DWORD exitCode = 1;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        success = (exitCode == 0);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    if (g_hWnd && IsWindow(g_hWnd)) {
        PostMessageW(g_hWnd, WM_APP_JAM_AUTODL_DONE, (WPARAM)success, (LPARAM)task);
    } else {
        free(task);
        InterlockedExchange(&g_jamAutoDlBusy, 0);
    }
    return 0;
}

static void StartJamAutoDownload(const wchar_t *trackTitle, const wchar_t *ytId) {
    if (!trackTitle || !trackTitle[0]) return;
    if (InterlockedCompareExchange(&g_jamAutoDlBusy, 1, 0) != 0) {
        return;
    }

    JamAutoDlTask *task = (JamAutoDlTask*)calloc(1, sizeof(JamAutoDlTask));
    if (!task) {
        InterlockedExchange(&g_jamAutoDlBusy, 0);
        return;
    }
    wcsncpy(task->title, trackTitle, 255);
    if (ytId) wcsncpy(task->ytId, ytId, 15);
    EnsureDefaultMusicFolder(task->folder, MAX_PATH);

    wcsncpy(g_jamAutoDlTrack, trackTitle, 255);
    g_jamAutoDlTrack[255] = L'\0';

    wchar_t statusMsg[128];
    _snwprintf(statusMsg, 127, L"⚡ \"%s\" sende yok • Otomatik indiriliyor...", trackTitle);
    statusMsg[127] = L'\0';
    Jam_SetSyncStatus(statusMsg, TRUE, NULL);

    HANDLE hThread = CreateThread(NULL, 0, JamAutoDownloadThread, task, 0, NULL);
    if (hThread) {
        CloseHandle(hThread);
    } else {
        free(task);
        InterlockedExchange(&g_jamAutoDlBusy, 0);
    }
}

static void ApplyRemoteJamSync(const JamRemoteSync *rs) {
    if (!rs || !rs->valid || !Jam_IsActive()) return;

    g_isSyncingFromRemote = TRUE;

    if (rs->posOnly) {
        if (rs->ytId[0] == L'S') {
            if (rs->posSec >= 0) Player_SetPosition(rs->posSec);
        } else if (!rs->isPlaying) {
            if (Player_IsPlaying()) Player_Pause();
            if (rs->posSec >= 0) Player_SetPosition(rs->posSec);
            Jam_SetSyncStatus(L"⏸ Ortak duraklatıldı", FALSE, NULL);
        } else {
            if (rs->posSec >= 0) Player_SetPosition(rs->posSec);
            if (!Player_IsPlaying()) Player_Play();
            Jam_SetSyncStatus(L"✓ Canlı senkronize", FALSE, NULL);
        }
        SyncLocalTrackToFriends();
        g_isSyncingFromRemote = FALSE;
        return;
    }

    if (!rs->trackTitle[0] && !rs->ytId[0]) {
        g_isSyncingFromRemote = FALSE;
        return;
    }

    if (g_currentTrack >= 0 && Playlist_IsSameTrack(g_currentTrack, rs->trackTitle, rs->ytId)) {
        int curPos = Player_GetPosition();
        if (rs->posSec >= 0 && abs(curPos - rs->posSec) > 2) {
            Player_SetPosition(rs->posSec);
        }
        if (rs->isPlaying && !Player_IsPlaying()) {
            Player_Play();
        } else if (!rs->isPlaying && Player_IsPlaying()) {
            Player_Pause();
        }
        Jam_SetSyncStatus(rs->isPlaying ? L"✓ Canlı senkronize" : L"⏸ Ortak duraklatıldı", FALSE, NULL);
        SyncLocalTrackToFriends();
        g_isSyncingFromRemote = FALSE;
        return;
    }

    int found = Playlist_FindBestMatch(rs->trackTitle, rs->ytId);
    if (found >= 0) {
        PlayTrack(found);
        if (rs->posSec > 0) Player_SetPosition(rs->posSec);
        if (!rs->isPlaying) Player_Pause();
        ScrollTrackIntoView(found);
        Jam_SetSyncStatus(rs->isPlaying ? L"✓ Canlı senkronize" : L"⏸ Ortak duraklatıldı", FALSE, NULL);
        SyncLocalTrackToFriends();
    } else {
        Player_Stop();
        g_currentTrack = -1;
        SyncLocalTrackToFriends();
        StartJamAutoDownload(rs->trackTitle, rs->ytId);
    }

    g_isSyncingFromRemote = FALSE;
}

static void ToggleSidebar(BOOL open) {
    g_sidebarOpen = open;
    if (!g_isWindowActive || g_isMinimized) {
        g_sidebarAnim = open ? 1.0f : 0.0f;
    }
    if (!open && g_hFriendAddEdit) {
        ShowWindow(g_hFriendAddEdit, SW_HIDE);
    }
    if (open && g_sidebarView == SIDEBAR_VIEW_FRIENDS) {
        TriggerImmediateFriendsRefresh();
    }
    if (g_hWnd) StartAnimLoop(g_hWnd);
}

static void OpenFriendsDrawer(void) {
    EnsureSocialNetworkStarted();
    g_sidebarView = SIDEBAR_VIEW_FRIENDS;
    g_sidebarOpen = TRUE;
    if (!g_isWindowActive || g_isMinimized) {
        g_sidebarAnim = 1.0f;
    }
    TriggerImmediateFriendsRefresh();
    UpdateSidebarEditPosition();
    if (g_hWnd) {
        StartAnimLoop(g_hWnd);
        InvalidateRect(g_hWnd, NULL, FALSE);
    }
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
            SyncLocalTrackToFriends();
            InvalidateRect(g_hWnd, NULL, FALSE);
        }
        CoTaskMemFree(pidl);
    }
}

static void ImportM3UDialog(void) {
    wchar_t szFile[MAX_PATH] = {0};
    OPENFILENAMEW ofn;
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
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
    OPENFILENAMEW ofn;
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
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
    } else {
        EnsureSocialNetworkStarted();
        if (Friends_SendRequest(inTag)) {
            wcscpy(g_friendAddMsg, L"✓ Arkadaşlık isteği gönderildi!");
            SetWindowTextW(g_hFriendAddEdit, L"");
        } else {
            wcscpy(g_friendAddMsg, L"İstek gönderilemedi. Etiketi veya interneti kontrol edin.");
        }
    }
    InvalidateRect(hWnd, NULL, FALSE);
}

// =============================================================================
// MODERN ARAYÜZ ÇİZİM MOTORU (TASARIM 6 + SPOTIFY YEŞİLİ #1DB954)
// =============================================================================
static void RenderUI(HDC hdc, const RECT *clientRc) {
    int w = clientRc->right - clientRc->left;
    int h = clientRc->bottom - clientRc->top;
    int headerH = GetHeaderH();
    int itemH   = GetItemH();
    JamState jamState = Jam_GetState();

    SetBkMode(hdc, TRANSPARENT);
    ResetPixelClip();

    // 0. Saf Derin Kadife Siyah Zemin
    FillSolidPx(0, 0, w, h, COLOR_BG);

    // =========================================================================
    // 1. ÜST ÇUBUK: ÇERÇEVESİZ, MINIMAL & FERAH (46px)
    // =========================================================================
    rcHamburger = (RECT){ S(14), (headerH - S(32)) / 2, S(46), (headerH + S(32)) / 2 };
    {
        BOOL hHot = (g_hotZone == ZONE_HAMBURGER);
        BOOL hPrs = (g_pressedZone == ZONE_HAMBURGER);
        if (hHot || hPrs || g_sidebarOpen) {
            DrawSmoothRoundRect(&rcHamburger, S(8), hPrs ? COLOR_CTRL_PRESS : COLOR_CTRL_HOVER, 255, COLOR_BORDER_LIGHT, 120);
        }
        DrawVecHamburger((float)(rcHamburger.left + S(7)), (float)(rcHamburger.top + S(10)),
                         g_sidebarOpen ? COLOR_ACCENT_HOV : (hHot ? COLOR_TEXT_WHITE : RGB(215, 215, 225)));

        if (Friends_GetPendingCount() > 0 && !g_sidebarOpen) {
            DrawSdfCircle((float)(rcHamburger.right - S(5)), (float)(rcHamburger.top + S(5)), SF(3.5f), COLOR_ACCENT_HOV, 255);
        }
    }

    // Arama Kapsülü (Sağda, Tasarım 6'daki kompakt yuvarlak hap)
    int searchW = (w >= S(560)) ? S(208) : S(156);
    int searchH = S(28);
    int searchX = w - searchW - S(14);
    int searchY = (headerH - searchH) / 2;
    rcSearchBox = (RECT){ searchX, searchY, searchX + searchW, searchY + searchH };

    BOOL searchFocused = (GetFocus() == g_hSearchEdit);
    DrawSmoothRoundRect(&rcSearchBox, searchH / 2, COLOR_INPUT_BG, 255,
                        searchFocused ? COLOR_ACCENT : COLOR_BORDER,
                        searchFocused ? 230 : 150);

    DrawVecSearch((float)(searchX + S(14)), (float)(searchY + searchH / 2),
                  searchFocused ? COLOR_ACCENT_HOV : COLOR_TEXT_DIM);

    if (g_searchQuery[0]) {
        rcSearchClear = (RECT){ rcSearchBox.right - S(26), searchY + S(2), rcSearchBox.right - S(4), searchY + searchH - S(2) };
        DrawVecCross((float)(rcSearchClear.left + rcSearchClear.right) * 0.5f,
                     (float)(rcSearchClear.top + rcSearchClear.bottom) * 0.5f,
                     SF(7.5f), (g_hotZone == ZONE_SEARCH_CLEAR) ? COLOR_TEXT_WHITE : COLOR_TEXT_MUTED);
    } else {
        rcSearchClear = (RECT){0};
    }

    // Canlı Jam Bildirim Rozeti (Yalnızca Jam aktifken sol üstte menünün yanında yer kaplar!)
    rcJamStage = (RECT){0};
    rcJamLeave = (RECT){0};
    rcJamMissingBtn = (RECT){0};

    if (jamState != JAM_STATE_NONE) {
        BOOL isInviting = (jamState == JAM_STATE_INVITING);
        int jLeft = S(52);
        int jRight = searchX - S(12);
        if (jRight > jLeft + S(120)) {
            rcJamStage = (RECT){ jLeft, searchY, jRight, searchY + searchH };
            COLORREF jBg   = isInviting ? RGB(32, 26, 15) : RGB(14, 33, 21);
            COLORREF jBord = isInviting ? COLOR_AMBER     : COLOR_ACCENT;
            DrawSmoothRoundRect(&rcJamStage, searchH / 2, jBg, 255, jBord, 200);

            // Canlı Yeşil Nokta
            float jDotX = (float)(rcJamStage.left + S(12));
            float jDotY = (float)(rcJamStage.top + rcJamStage.bottom) * 0.5f;
            DrawSdfCircle(jDotX, jDotY, SF(3.5f), isInviting ? COLOR_AMBER : COLOR_ACCENT_HOV, 255);

            // Ayrıl Düğmesi
            rcJamLeave = (RECT){ rcJamStage.right - S(50), rcJamStage.top + S(2), rcJamStage.right - S(4), rcJamStage.bottom - S(2) };
            BOOL lvHot = (g_hotZone == ZONE_JAM_LEAVE);
            DrawSmoothRoundRect(&rcJamLeave, S(6), lvHot ? RGB(68, 28, 32) : RGB(26, 30, 38), 240, 0, 0);
            SelectObject(hdc, g_hFontMicro);
            SetTextColor(hdc, lvHot ? RGB(255, 160, 160) : COLOR_TEXT_WHITE);
            DrawTextW(hdc, isInviting ? L"İptal" : L"Ayrıl", -1, &rcJamLeave, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            int textRight = rcJamLeave.left - S(6);

            // Eksik şarkı indir düğmesi
            wchar_t missingTitle[256] = {0};
            Jam_GetMissingTrack(missingTitle, 256);
            if (!isInviting && missingTitle[0] && !Jam_IsDownloadingTrack()) {
                rcJamMissingBtn = (RECT){ rcJamLeave.left - S(52), rcJamStage.top + S(2), rcJamLeave.left - S(4), rcJamStage.bottom - S(2) };
                BOOL mHot = (g_hotZone == ZONE_JAM_MISSING_DL);
                DrawSmoothRoundRect(&rcJamMissingBtn, S(6), mHot ? COLOR_ACCENT_HOV : COLOR_ACCENT, 255, 0, 0);
                SelectObject(hdc, g_hFontMicro);
                SetTextColor(hdc, RGB(8, 12, 10));
                DrawTextW(hdc, L"İndir", -1, &rcJamMissingBtn, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                textRight = rcJamMissingBtn.left - S(6);
            }

            wchar_t partnerName[64] = {0};
            Jam_GetPartnerName(partnerName, 64);
            wchar_t jamTxt[160];
            if (isInviting) {
                _snwprintf(jamTxt, 159, L"%s bekleniyor...", partnerName[0] ? partnerName : L"Arkadaşın");
            } else {
                wchar_t syncSub[128] = {0};
                Jam_GetSyncStatus(syncSub, 128);
                _snwprintf(jamTxt, 159, L"%s ile Jam  ·  %s", partnerName[0] ? partnerName : L"Ortak", syncSub[0] ? syncSub : L"Canlı");
            }
            jamTxt[159] = L'\0';

            RECT rcJMsg = { rcJamStage.left + S(22), rcJamStage.top, textRight, rcJamStage.bottom };
            SelectObject(hdc, g_hFontMicro);
            SetTextColor(hdc, isInviting ? COLOR_AMBER : COLOR_ACCENT_HOV);
            DrawTextW(hdc, jamTxt, -1, &rcJMsg, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        }
    }

    // =========================================================================
    // 2. ORTA BÖLGE: ÇERÇEVESİZ FERAH ŞARKI LİSTESİ (TASARIM 6 SÜTUNSUZ AKIŞI)
    // =========================================================================
    int listTop    = headerH + S(2);
    int listBottom = h;
    int totalCount = g_isFiltered ? g_filterCount : Playlist_Count();

    int savedList = SaveDC(hdc);
    IntersectClipRect(hdc, 0, listTop, w, listBottom);
    SetPixelClip(0, listTop, w, listBottom);

    if (totalCount == 0) {
        int midY = (listTop + listBottom) / 2 - S(20);
        RECT rcEmpty = { S(24), midY - S(40), w - S(24), midY + S(40) };
        SetBkMode(hdc, TRANSPARENT);
        SelectObject(hdc, g_hFontBold);
        SetTextColor(hdc, COLOR_TEXT_MUTED);
        DrawTextW(hdc, g_isFiltered ? L"Eşleşen şarkı bulunamadı" : L"Müzik Klasörü Seçmek İçin Sol Üstteki ☰ Menüsünü Açın",
                  -1, &rcEmpty, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    } else {
        int scrollPx = (int)(g_scrollPosPx + 0.5f);
        int firstIdx = scrollPx / itemH;
        if (firstIdx < 0) firstIdx = 0;
        int padX = S(16);

        for (int displayIdx = firstIdx; displayIdx < totalCount; displayIdx++) {
            int itemY = listTop + displayIdx * itemH - scrollPx;
            if (itemY >= listBottom) break;
            if (itemY + itemH < listTop) continue;

            int actualIdx = g_isFiltered ? g_filterIndices[displayIdx] : displayIdx;
            RECT rcRow = { padX, itemY + S(1), w - padX, itemY + itemH - S(1) };

            BOOL isPlayingTrack = (actualIdx == g_currentTrack);
            BOOL isSelected     = (actualIdx == g_selectedTrack);
            BOOL isHovered      = (actualIdx == g_hoverTrack && g_sidebarAnim < 0.05f);

            if (isPlayingTrack) {
                DrawSmoothRoundRect(&rcRow, S(8), COLOR_ROW_ACTIVE, 255, 0, 0);
            } else if (isHovered) {
                DrawSmoothRoundRect(&rcRow, S(8), COLOR_ROW_HOVER, 255, 0, 0);
            } else if (isSelected) {
                DrawSmoothRoundRect(&rcRow, S(8), COLOR_ROW_SEL, 200, 0, 0);
            }

            if (g_isDraggingList && !g_isFiltered && displayIdx == g_dragTargetIndex) {
                DrawHairlineH(rcRow.left + S(8), rcRow.right - S(8), rcRow.top, COLOR_ACCENT_HOV, 255);
            }

            const PlaylistItem *item = Playlist_Get(actualIdx);
            if (!item) continue;

            wchar_t artistStr[128] = {0};
            wchar_t songStr[160]   = {0};
            Playlist_SplitArtistSong(item->title, artistStr, 128, songStr, 160);
            if (!artistStr[0]) wcscpy(artistStr, L"Yerel Parça");

            // Çalan Şarkıda Sol Kenarda Canlı Yeşil Nokta (Tasarım 6 İmza Vurgusu!)
            int textLeft = rcRow.left + S(14);
            if (isPlayingTrack) {
                float dotY = (float)(rcRow.top + rcRow.bottom) * 0.5f;
                DrawSdfCircle((float)(rcRow.left + S(12)), dotY, SF(3.5f), COLOR_ACCENT_HOV, 255);
                textLeft = rcRow.left + S(26);
            }

            // Sağda Sanatçı İsmi
            int artistW = (w >= S(520)) ? S(180) : S(120);
            int artistRight = rcRow.right - S(14);
            int artistLeft  = artistRight - artistW;
            RECT rcArtist   = { artistLeft, rcRow.top, artistRight, rcRow.bottom };

            SelectObject(hdc, g_hFontMain);
            SetTextColor(hdc, isHovered ? RGB(165, 168, 185) : COLOR_TEXT_MUTED);
            DrawTextW(hdc, artistStr, -1, &rcArtist, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

            // Solda Şarkı Adı
            RECT rcSong = { textLeft, rcRow.top, artistLeft - S(12), rcRow.bottom };
            SelectObject(hdc, g_hFontBold);
            SetTextColor(hdc, isPlayingTrack ? COLOR_ACCENT_HOV : COLOR_TEXT_WHITE);
            DrawTextW(hdc, songStr, -1, &rcSong, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        }

        // İnce Modern Kaydırma Çubuğu (Scrollbar)
        int maxScrollPx = GetMaxScrollPx(g_hWnd);
        if (maxScrollPx > 0) {
            int sbW = (g_scrollbarHover || g_isDraggingScrollbar) ? S(5) : S(3);
            int sbRight = w - S(4);
            int sbTop = listTop + S(4);
            int sbBottom = listBottom - S(12);
            int trackH = sbBottom - sbTop;

            rcScrollbarTrack = (RECT){ w - S(12), sbTop, w, sbBottom };

            if (trackH > S(24)) {
                int viewH = listBottom - listTop;
                int contentH = totalCount * itemH + S(84);
                int thumbH = (int)(((float)viewH / (float)contentH) * (float)trackH);
                if (thumbH < S(28)) thumbH = S(28);
                if (thumbH > trackH) thumbH = trackH;

                float ratio = g_scrollPosPx / (float)maxScrollPx;
                if (ratio < 0.0f) ratio = 0.0f;
                if (ratio > 1.0f) ratio = 1.0f;
                int thumbTop = sbTop + (int)(ratio * (float)(trackH - thumbH));

                rcScrollbarThumb = (RECT){ sbRight - sbW, thumbTop, sbRight, thumbTop + thumbH };
                COLORREF sbCol = g_isDraggingScrollbar ? COLOR_ACCENT : (g_scrollbarHover ? RGB(115, 120, 140) : RGB(46, 48, 62));
                DrawSmoothRoundRect(&rcScrollbarThumb, sbW / 2, sbCol, 220, 0, 0);
            }
        } else {
            rcScrollbarTrack = (RECT){0};
            rcScrollbarThumb = (RECT){0};
        }
    }

    RestoreDC(hdc, savedList);
    SetBkMode(hdc, TRANSPARENT);
    ResetPixelClip();

    // =========================================================================
    // 3. ALT BÖLGE: YÜZEN KOMPAKT KAPSÜL OYNATICI (TASARIM 6 İMZASI)
    // =========================================================================
    int capH = GetCapsuleH();
    int capW = (w - S(28) < S(560)) ? (w - S(28)) : S(560);
    int capX = (w - capW) / 2;
    int capY = h - capH - S(16);
    rcCapsule = (RECT){ capX, capY, capX + capW, capY + capH };

    BOOL jamCapsuleGlow = (jamState == JAM_STATE_ACTIVE);
    DrawSmoothRoundRect(&rcCapsule, capH / 2, COLOR_CAPSULE_BG, 255,
                        jamCapsuleGlow ? COLOR_ACCENT : COLOR_CAPSULE_BORD,
                        jamCapsuleGlow ? 180 : 160);

    float cy = (float)(capY + capH / 2);

    // --- SOL: SPOTIFY YEŞİLİ OYNAT / DURAKLAT DAİRESİ (34x34) ---
    float playR = SF(17.0f);
    float playCx = (float)(capX + S(26));
    rcPlay = (RECT){ (int)(playCx - playR), (int)(cy - playR), (int)(playCx + playR), (int)(cy + playR) };
    {
        BOOL pHot = (g_hotZone == ZONE_PLAY);
        BOOL pPrs = (g_pressedZone == ZONE_PLAY);
        COLORREF playBg = pPrs ? COLOR_ACCENT_DARK : (pHot ? COLOR_ACCENT_HOV : COLOR_ACCENT);
        DrawSdfCircle(playCx, cy, playR, playBg, 255);

        COLORREF iconCol = RGB(8, 12, 10); // Spotify kontrast siyah ikonu
        if (Player_IsPlaying()) {
            DrawVecPause(playCx, cy, SF(10.0f), SF(11.0f), iconCol);
        } else {
            DrawVecPlay(playCx + SF(1.0f), cy, SF(11.0f), iconCol);
        }
    }

    // --- SOL YAN: ŞARKI ADI & SANATÇI ---
    int metaLeft  = capX + S(52);
    int metaW     = (capW >= S(500)) ? S(135) : S(95);
    int metaRight = metaLeft + metaW;
    RECT rcDTitle = { metaLeft, capY + S(9),  metaRight, capY + S(26) };
    RECT rcDSub   = { metaLeft, capY + S(26), metaRight, capY + S(43) };

    const PlaylistItem *cur = (g_currentTrack >= 0 && g_currentTrack < Playlist_Count()) ? Playlist_Get(g_currentTrack) : NULL;
    wchar_t dSong[160] = L"Hazır";
    wchar_t dArt[128]  = L"Parça seçin";
    if (cur) {
        Playlist_SplitArtistSong(cur->title, dArt, 128, dSong, 160);
        if (!dArt[0]) wcscpy(dArt, L"Yerel Parça");
    }

    SelectObject(hdc, g_hFontSmallBold);
    SetTextColor(hdc, COLOR_TEXT_WHITE);
    DrawTextW(hdc, dSong, -1, &rcDTitle, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    // İlerleme çubuğu üzerine gelindiğinde süreyi alt satırda akıllıca göster!
    int duration = Player_GetDuration();
    int position = g_isDraggingSeek ? g_seekDragSec : Player_GetPosition();
    if (duration > 0 && position > duration) position = duration;

    BOOL seekHover = (g_hotZone == ZONE_SEEK || g_isDraggingSeek);
    if (seekHover && duration > 0) {
        wchar_t tPos[16], tDur[16], tFull[48];
        FormatTime(position, tPos);
        FormatTime(duration, tDur);
        _snwprintf(tFull, 47, L"%s / %s", tPos, tDur);
        tFull[47] = L'\0';
        SelectObject(hdc, g_hFontMicro);
        SetTextColor(hdc, COLOR_ACCENT_HOV);
        DrawTextW(hdc, tFull, -1, &rcDSub, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    } else {
        SelectObject(hdc, g_hFontMicro);
        SetTextColor(hdc, COLOR_TEXT_MUTED);
        DrawTextW(hdc, dArt, -1, &rcDSub, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }

    // --- SAĞ: KONTROL DÜĞMELERİ (SAĞDAN SOLA ANKRAJLI) ---
    int curVol = Player_GetVolume();

    // Ses Çubuğu
    int volW = S(38);
    int volRight = rcCapsule.right - S(16);
    int volLeft  = volRight - volW;
    BOOL volActive = (g_hotZone == ZONE_VOL || g_isDraggingVol);
    int vBarH = volActive ? S(5) : S(3);
    rcVolBar = (RECT){ volLeft, (int)cy - vBarH / 2, volRight, (int)cy + (vBarH - vBarH / 2) };
    rcVolHit = (RECT){ volLeft - S(4), capY + S(12), volRight + S(6), capY + capH - S(12) };

    DrawSmoothRoundRect(&rcVolBar, vBarH / 2, volActive ? COLOR_BAR_HOVER : COLOR_BAR_BG, 255, 0, 0);
    int vFillW = (int)((float)volW * ((float)curVol / 100.0f));
    if (vFillW > 0) {
        RECT rcVFill = { rcVolBar.left, rcVolBar.top, rcVolBar.left + vFillW, rcVolBar.bottom };
        DrawSmoothRoundRect(&rcVFill, vBarH / 2, volActive ? COLOR_ACCENT_HOV : RGB(220, 220, 230), 255, 0, 0);
    }

    // Hoparlör (Sessiz)
    int spkSz = S(22);
    rcSpeaker = (RECT){ volLeft - spkSz - S(2), (int)cy - spkSz / 2, volLeft - S(2), (int)cy + spkSz / 2 };
    DrawVecSpeaker((float)(rcSpeaker.left + rcSpeaker.right) * 0.5f, cy,
                   (curVol == 0) ? COLOR_DANGER : ((g_hotZone == ZONE_SPEAKER) ? COLOR_TEXT_WHITE : COLOR_TEXT_MUTED));

    // Tekrar (Repeat)
    int btnSz = S(24);
    rcRepeat = (RECT){ rcSpeaker.left - btnSz - S(2), (int)cy - btnSz / 2, rcSpeaker.left - S(2), (int)cy + btnSz / 2 };
    COLORREF repCol = (g_cfg.repeat > 0) ? COLOR_ACCENT_HOV : ((g_hotZone == ZONE_REPEAT) ? COLOR_TEXT_WHITE : COLOR_TEXT_MUTED);
    DrawVecRepeat((float)(rcRepeat.left + rcRepeat.right) * 0.5f, cy, SF(13.0f), repCol);
    if (g_cfg.repeat == 2) {
        RECT rcOne = { rcRepeat.right - S(8), rcRepeat.top, rcRepeat.right, rcRepeat.top + S(10) };
        SelectObject(hdc, g_hFontMicro);
        SetTextColor(hdc, COLOR_ACCENT_HOV);
        DrawTextW(hdc, L"1", -1, &rcOne, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    // Sonraki (Next)
    rcNext = (RECT){ rcRepeat.left - btnSz - S(2), (int)cy - btnSz / 2, rcRepeat.left - S(2), (int)cy + btnSz / 2 };
    DrawVecNext((float)(rcNext.left + rcNext.right) * 0.5f, cy, SF(10.5f),
                (g_hotZone == ZONE_NEXT) ? COLOR_TEXT_WHITE : RGB(195, 195, 208));

    // Önceki (Prev)
    rcPrev = (RECT){ rcNext.left - btnSz - S(2), (int)cy - btnSz / 2, rcNext.left - S(2), (int)cy + btnSz / 2 };
    DrawVecPrev((float)(rcPrev.left + rcPrev.right) * 0.5f, cy, SF(10.5f),
                (g_hotZone == ZONE_PREV) ? COLOR_TEXT_WHITE : RGB(195, 195, 208));

    // --- ORTA: İNCE YEŞİL İLERLEME ÇUBUĞU ---
    int seekLeft  = metaRight + S(12);
    int seekRight = rcPrev.left - S(14);
    if (seekRight > seekLeft + S(40)) {
        int sBarH = seekHover ? S(5) : S(3);
        rcSeekBar = (RECT){ seekLeft, (int)cy - sBarH / 2, seekRight, (int)cy + (sBarH - sBarH / 2) };
        rcSeekHit = (RECT){ seekLeft - S(4), capY + S(10), seekRight + S(4), capY + capH - S(10) };

        DrawSmoothRoundRect(&rcSeekBar, sBarH / 2, seekHover ? COLOR_BAR_HOVER : COLOR_BAR_BG, 255, 0, 0);

        float progRatio = (duration > 0) ? ((float)position / (float)duration) : 0.0f;
        if (progRatio < 0.0f) progRatio = 0.0f;
        if (progRatio > 1.0f) progRatio = 1.0f;
        int sFillW = (int)((float)(seekRight - seekLeft) * progRatio);
        if (sFillW > 0) {
            RECT rcSFill = { rcSeekBar.left, rcSeekBar.top, rcSeekBar.left + sFillW, rcSeekBar.bottom };
            DrawSmoothRoundRect(&rcSFill, sBarH / 2, seekHover ? COLOR_ACCENT_HOV : COLOR_ACCENT, 255, 0, 0);
        }
        if (seekHover && duration > 0) {
            DrawSdfCircle((float)(rcSeekBar.left + sFillW), cy, SF(4.5f), COLOR_TEXT_WHITE, 255);
        }
    } else {
        rcSeekBar = rcSeekHit = (RECT){0};
    }

    // =========================================================================
    // 4. SOL ÇEKMECE: ☰ MENÜ VE ARKADAŞLAR / JAM MERKEZİ
    // =========================================================================
    if (g_sidebarAnim > 0.005f) {
        int sbW  = GetSidebarW();
        int offX = (int)((g_sidebarAnim - 1.0f) * (float)sbW);

        BYTE scrimAlpha = (BYTE)(g_sidebarAnim * 140.0f);
        DimRectPx(0, 0, w, h, scrimAlpha);

        int clipRight = offX + sbW + 1;
        if (clipRight > w) clipRight = w;
        int savedSide = SaveDC(hdc);
        IntersectClipRect(hdc, 0, 0, clipRight, h);
        SetPixelClip(0, 0, clipRight, h);

        FillSolidPx(offX, 0, sbW, h, COLOR_SIDEBAR_BG);
        DrawHairlineV(offX + sbW - 1, 0, h, COLOR_BORDER, 200);

        if (g_sidebarView == SIDEBAR_VIEW_MENU) {
            static const struct { const wchar_t *label; BOOL accent; } items[] = {
                { L"Müzik Klasörü Seç",      FALSE },
                { L"Müzik Ara ve İndir",     TRUE  },
                { L"Arkadaşlar ve Jam",      TRUE  },
                { L"M3U İçe Aktar",          FALSE },
                { L"M3U Dışa Aktar",         FALSE },
                { L"Listeyi Temizle",        FALSE },
                { L"Güncellemeleri Denetle", FALSE }
            };

            RECT rcMenuTitle = { offX + S(18), S(14), offX + sbW - S(18), S(38) };
            SelectObject(hdc, g_hFontTitle);
            SetTextColor(hdc, COLOR_TEXT_WHITE);
            DrawTextW(hdc, L"Selo Player", -1, &rcMenuTitle, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            for (int m = 0; m < 7; m++) {
                int itemTop = S(48) + m * S(44);
                RECT rcMenu = { offX + S(12), itemTop, offX + sbW - S(12), itemTop + S(38) };
                BOOL isHov = (m == g_sidebarHoverItem);

                if (isHov || items[m].accent) {
                    COLORREF mFill = isHov ? COLOR_CARD_HOVER : RGB(14, 24, 18);
                    COLORREF mBord = items[m].accent ? COLOR_ACCENT : COLOR_BORDER;
                    DrawSmoothRoundRect(&rcMenu, S(8), mFill, isHov ? 255 : 160, mBord, isHov ? 160 : 70);
                }

                RECT rcLbl = { rcMenu.left + S(14), rcMenu.top, rcMenu.right - S(32), rcMenu.bottom };
                SelectObject(hdc, g_hFontBold);
                SetTextColor(hdc, items[m].accent ? COLOR_ACCENT_HOV : (isHov ? COLOR_TEXT_WHITE : COLOR_TEXT_MUTED));
                DrawTextW(hdc, items[m].label, -1, &rcLbl, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

                if (m == 2 && Friends_GetPendingCount() > 0) {
                    wchar_t bTxt[16];
                    _snwprintf(bTxt, 15, L"%d", Friends_GetPendingCount());
                    bTxt[15] = L'\0';
                    RECT rcBadge = { rcMenu.right - S(26), rcMenu.top + S(10), rcMenu.right - S(8), rcMenu.bottom - S(10) };
                    DrawSmoothRoundRect(&rcBadge, S(8), COLOR_ACCENT, 255, 0, 0);
                    SelectObject(hdc, g_hFontMicro);
                    SetTextColor(hdc, RGB(8, 12, 10));
                    DrawTextW(hdc, bTxt, -1, &rcBadge, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                }
            }
        }
        else if (g_sidebarView == SIDEBAR_VIEW_FRIENDS) {
            // Başlık: Geri + "Arkadaşlar & Jam" + Yenile
            rcFriendsBack = (RECT){ offX + S(12), S(10), offX + S(38), S(36) };
            DrawSmoothRoundRect(&rcFriendsBack, S(6), (g_hotZone == ZONE_FRIENDS_BACK) ? COLOR_CARD_HOVER : 0, 255, 0, 0);
            DrawVecCross((float)(rcFriendsBack.left + rcFriendsBack.right) * 0.5f,
                         (float)(rcFriendsBack.top + rcFriendsBack.bottom) * 0.5f,
                         SF(8.0f), COLOR_TEXT_WHITE);

            rcFriendsRefresh = (RECT){ offX + sbW - S(38), S(10), offX + sbW - S(12), S(36) };
            DrawSmoothRoundRect(&rcFriendsRefresh, S(6), (g_hotZone == ZONE_FRIENDS_REFRESH) ? COLOR_CARD_HOVER : 0, 255, 0, 0);
            DrawSdfCircle((float)(rcFriendsRefresh.left + rcFriendsRefresh.right) * 0.5f,
                          (float)(rcFriendsRefresh.top + rcFriendsRefresh.bottom) * 0.5f,
                          SF(4.5f), COLOR_TEXT_MUTED, 255);

            RECT rcFHead = { offX + S(42), S(10), offX + sbW - S(42), S(36) };
            SelectObject(hdc, g_hFontTitle);
            SetTextColor(hdc, COLOR_TEXT_WHITE);
            DrawTextW(hdc, L"Arkadaşlar & Jam", -1, &rcFHead, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

            // Kullanıcı Kimlik Kartı
            RECT rcMyCard = { offX + S(12), S(44), offX + sbW - S(12), S(88) };
            DrawSmoothRoundRect(&rcMyCard, S(10), COLOR_CARD_SURF, 255, COLOR_BORDER, 140);

            wchar_t myName[64] = {0}, myTag[32] = {0};
            Friends_GetMyName(myName, 64);
            Friends_GetMyTag(myTag, 32);

            BOOL recentlyCopied = (g_copiedTagTick != 0 && (GetTickCount() - g_copiedTagTick) < 2000);
            rcCopyTag = (RECT){ rcMyCard.right - S(74), rcMyCard.top + S(10), rcMyCard.right - S(8), rcMyCard.bottom - S(10) };
            DrawSmoothRoundRect(&rcCopyTag, S(6),
                                recentlyCopied ? COLOR_ACCENT : ((g_hotZone == ZONE_COPY_TAG) ? COLOR_CARD_HOVER : RGB(28, 32, 42)),
                                255, 0, 0);
            SelectObject(hdc, g_hFontMicro);
            SetTextColor(hdc, recentlyCopied ? RGB(8, 12, 10) : COLOR_TEXT_WHITE);
            DrawTextW(hdc, recentlyCopied ? L"✓ Kopyalandı" : L"Kopyala", -1, &rcCopyTag, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            RECT rcMyLbl = { rcMyCard.left + S(12), rcMyCard.top + S(7),  rcCopyTag.left - S(4), rcMyCard.top + S(22) };
            RECT rcMyTag = { rcMyCard.left + S(12), rcMyCard.top + S(22), rcCopyTag.left - S(4), rcMyCard.bottom - S(6) };
            SelectObject(hdc, g_hFontMicro);
            SetTextColor(hdc, COLOR_TEXT_DIM);
            DrawTextW(hdc, L"SENİN ETİKETİN", -1, &rcMyLbl, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            SelectObject(hdc, g_hFontBold);
            SetTextColor(hdc, COLOR_TEXT_WHITE);
            DrawTextW(hdc, myTag, -1, &rcMyTag, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

            // Sekmeler (Liste | İstekler | Ekle)
            RECT rcTabTray = { offX + S(12), S(96), offX + sbW - S(12), S(128) };
            DrawSmoothRoundRect(&rcTabTray, S(8), RGB(17, 19, 25), 255, COLOR_BORDER, 120);

            int trayW = (rcTabTray.right - rcTabTray.left - S(6)) / 3;
            rcFriendsTab0 = (RECT){ rcTabTray.left + S(3),              rcTabTray.top + S(2), rcTabTray.left + S(3) + trayW,     rcTabTray.bottom - S(2) };
            rcFriendsTab1 = (RECT){ rcFriendsTab0.right,                rcTabTray.top + S(2), rcFriendsTab0.right + trayW,       rcTabTray.bottom - S(2) };
            rcFriendsTab2 = (RECT){ rcFriendsTab1.right,                rcTabTray.top + S(2), rcTabTray.right - S(3),            rcTabTray.bottom - S(2) };

            const RECT *tabRects[3] = { &rcFriendsTab0, &rcFriendsTab1, &rcFriendsTab2 };
            wchar_t tab0Txt[32], tab1Txt[32];
            _snwprintf(tab0Txt, 31, L"Liste (%d)", Friends_GetFriendCount());
            tab0Txt[31] = L'\0';
            if (Friends_GetPendingCount() > 0) {
                _snwprintf(tab1Txt, 31, L"İstek (%d)", Friends_GetPendingCount());
            } else {
                wcscpy(tab1Txt, L"İstekler");
            }
            tab1Txt[31] = L'\0';
            const wchar_t *tabLabels[3] = { tab0Txt, tab1Txt, L"+ Ekle" };

            for (int t = 0; t < 3; t++) {
                BOOL sel = ((int)g_friendsTab == t);
                if (sel) {
                    DrawSmoothRoundRect(tabRects[t], S(6), COLOR_CARD_HOVER, 255, COLOR_ACCENT, 140);
                }
                SelectObject(hdc, sel ? g_hFontSmallBold : g_hFontSmall);
                SetTextColor(hdc, sel ? COLOR_ACCENT_HOV : COLOR_TEXT_MUTED);
                DrawTextW(hdc, tabLabels[t], -1, (RECT*)tabRects[t], DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }

            int contentTop = S(136);

            // SEKME 0: ARKADAŞ LİSTESİ
            if (g_friendsTab == FRIENDS_TAB_LIST) {
                int fCount = Friends_GetFriendCount();
                for (int k = 0; k < MAX_FRIENDS; k++) {
                    rcFriendActionBtns[k] = (RECT){0};
                    rcFriendRemoveBtns[k] = (RECT){0};
                }

                if (fCount == 0) {
                    RECT rcEmptyF = { offX + S(16), contentTop + S(20), offX + sbW - S(16), contentTop + S(110) };
                    SelectObject(hdc, g_hFontSmall);
                    SetTextColor(hdc, COLOR_TEXT_MUTED);
                    DrawTextW(hdc, L"Henüz ekli arkadaşın yok.\n\n'+ Ekle' sekmesinden arkadaşının etiketini girerek davet gönderebilirsin.", -1, &rcEmptyF, DT_CENTER | DT_TOP | DT_WORDBREAK);
                } else {
                    wchar_t jamPartner[32] = {0};
                    Jam_GetPartnerTag(jamPartner, 32);

                    for (int i = 0; i < fCount; i++) {
                        int cardY = contentTop + i * S(54);
                        if (cardY + S(48) > h - S(8)) break;

                        const FriendItem *fi = Friends_GetFriend(i);
                        if (!fi) continue;

                        BOOL inJamWithFriend = (jamState != JAM_STATE_NONE && _wcsicmp(jamPartner, fi->tag) == 0);
                        RECT rcFC = { offX + S(12), cardY, offX + sbW - S(12), cardY + S(48) };
                        DrawSmoothRoundRect(&rcFC, S(8),
                                            inJamWithFriend ? RGB(14, 34, 22) : COLOR_CARD_SURF, 255,
                                            inJamWithFriend ? COLOR_ACCENT : COLOR_BORDER, 140);

                        // Çevrimiçi Durum Noktası
                        float avX = (float)(rcFC.left + S(16));
                        float avY = (float)(rcFC.top + rcFC.bottom) * 0.5f;
                        DrawSdfCircle(avX, avY, (float)S(4.0f), fi->isOnline ? COLOR_ACCENT_HOV : RGB(88, 92, 105), 255);

                        // Jam Düğmesi & Sil Düğmesi
                        rcFriendRemoveBtns[i] = (RECT){ rcFC.right - S(26), cardY + S(12), rcFC.right - S(6), cardY + S(36) };
                        DrawVecCross((float)(rcFriendRemoveBtns[i].left + rcFriendRemoveBtns[i].right) * 0.5f,
                                     (float)(rcFriendRemoveBtns[i].top + rcFriendRemoveBtns[i].bottom) * 0.5f,
                                     SF(6.5f), COLOR_TEXT_DIM);

                        rcFriendActionBtns[i] = (RECT){ rcFC.right - S(72), cardY + S(11), rcFC.right - S(28), cardY + S(37) };
                        COLORREF actBg = inJamWithFriend ? RGB(68, 28, 32) : (fi->isOnline ? COLOR_ACCENT : RGB(28, 32, 42));
                        DrawSmoothRoundRect(&rcFriendActionBtns[i], S(6), actBg, 255, 0, 0);
                        SelectObject(hdc, g_hFontMicro);
                        SetTextColor(hdc, inJamWithFriend ? RGB(255, 165, 165) : (fi->isOnline ? RGB(8, 12, 10) : COLOR_TEXT_DIM));
                        DrawTextW(hdc, inJamWithFriend ? L"Ayrıl" : L"Jam", -1, &rcFriendActionBtns[i], DT_CENTER | DT_VCENTER | DT_SINGLELINE);

                        RECT rcFN = { rcFC.left + S(28), cardY + S(6),  rcFriendActionBtns[i].left - S(4), cardY + S(24) };
                        RECT rcFS = { rcFC.left + S(28), cardY + S(24), rcFriendActionBtns[i].left - S(4), cardY + S(42) };

                        SelectObject(hdc, g_hFontBold);
                        SetTextColor(hdc, COLOR_TEXT_WHITE);
                        DrawTextW(hdc, fi->name, -1, &rcFN, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

                        wchar_t subLine[160];
                        if (!fi->isOnline) {
                            wcscpy(subLine, L"Çevrimdışı");
                        } else if (fi->isPlaying && fi->currentTrack[0]) {
                            _snwprintf(subLine, 159, L"♫ %s", fi->currentTrack);
                            subLine[159] = L'\0';
                        } else {
                            wcscpy(subLine, L"Çevrimiçi");
                        }

                        SelectObject(hdc, g_hFontMicro);
                        SetTextColor(hdc, !fi->isOnline ? COLOR_TEXT_DIM : (fi->isPlaying ? COLOR_ACCENT_HOV : COLOR_TEXT_MUTED));
                        DrawTextW(hdc, subLine, -1, &rcFS, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                    }
                }
            }
            // SEKME 1: GELEN İSTEKLER
            else if (g_friendsTab == FRIENDS_TAB_REQUESTS) {
                int pCount = Friends_GetPendingCount();
                for (int k = 0; k < MAX_REQUESTS; k++) {
                    rcReqAcceptBtns[k] = (RECT){0};
                    rcReqRejectBtns[k] = (RECT){0};
                }

                if (pCount == 0) {
                    RECT rcEmptyR = { offX + S(16), contentTop + S(24), offX + sbW - S(16), contentTop + S(90) };
                    SelectObject(hdc, g_hFontSmall);
                    SetTextColor(hdc, COLOR_TEXT_MUTED);
                    DrawTextW(hdc, L"Bekleyen arkadaşlık isteği yok.", -1, &rcEmptyR, DT_CENTER | DT_TOP | DT_WORDBREAK);
                } else {
                    for (int i = 0; i < pCount; i++) {
                        int cardY = contentTop + i * S(54);
                        if (cardY + S(48) > h - S(8)) break;

                        const FriendRequest *rq = Friends_GetPending(i);
                        if (!rq) continue;

                        RECT rcCard = { offX + S(12), cardY, offX + sbW - S(12), cardY + S(48) };
                        DrawSmoothRoundRect(&rcCard, S(8), COLOR_CARD_SURF, 255, COLOR_BORDER, 130);

                        RECT rcName = { rcCard.left + S(12), cardY + S(6),  rcCard.right - S(64), cardY + S(24) };
                        RECT rcTag  = { rcCard.left + S(12), cardY + S(24), rcCard.right - S(64), cardY + S(42) };

                        SelectObject(hdc, g_hFontBold);
                        SetTextColor(hdc, COLOR_TEXT_WHITE);
                        DrawTextW(hdc, rq->name, -1, &rcName, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

                        SelectObject(hdc, g_hFontMicro);
                        SetTextColor(hdc, COLOR_TEXT_MUTED);
                        DrawTextW(hdc, rq->tag, -1, &rcTag, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

                        rcReqAcceptBtns[i] = (RECT){ rcCard.right - S(58), cardY + S(10), rcCard.right - S(32), cardY + S(38) };
                        DrawSmoothRoundRect(&rcReqAcceptBtns[i], S(6), COLOR_ACCENT, 255, 0, 0);
                        SelectObject(hdc, g_hFontMicro);
                        SetTextColor(hdc, RGB(8, 12, 10));
                        DrawTextW(hdc, L"Kabul", -1, &rcReqAcceptBtns[i], DT_CENTER | DT_VCENTER | DT_SINGLELINE);

                        rcReqRejectBtns[i] = (RECT){ rcCard.right - S(28), cardY + S(10), rcCard.right - S(6), cardY + S(38) };
                        DrawSmoothRoundRect(&rcReqRejectBtns[i], S(6), COLOR_CTRL_HOVER, 255, COLOR_BORDER, 120);
                        DrawVecCross((float)(rcReqRejectBtns[i].left + rcReqRejectBtns[i].right) * 0.5f,
                                     (float)(rcReqRejectBtns[i].top + rcReqRejectBtns[i].bottom) * 0.5f,
                                     SF(6.5f), COLOR_TEXT_MUTED);
                    }
                }
            }
            // SEKME 2: ARKADAŞ EKLE
            else if (g_friendsTab == FRIENDS_TAB_ADD) {
                RECT rcLbl = { offX + S(14), contentTop + S(2),  offX + sbW - S(14), contentTop + S(20) };
                RECT rcSub = { offX + S(14), contentTop + S(20), offX + sbW - S(14), contentTop + S(38) };
                SelectObject(hdc, g_hFontBold);
                SetTextColor(hdc, COLOR_TEXT_WHITE);
                DrawTextW(hdc, L"Etiket ile Arkadaş Ekle", -1, &rcLbl, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

                SelectObject(hdc, g_hFontSmall);
                SetTextColor(hdc, COLOR_TEXT_MUTED);
                DrawTextW(hdc, L"Arkadaşının etiketini gir (Örn: Selim#8421)", -1, &rcSub, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

                RECT rcInputBox = { offX + S(12), S(176), offX + sbW - S(12), S(212) };
                BOOL addFocused = (GetFocus() == g_hFriendAddEdit);
                DrawSmoothRoundRect(&rcInputBox, S(8), COLOR_INPUT_BG, 255,
                                    addFocused ? COLOR_ACCENT : COLOR_BORDER, addFocused ? 220 : 140);

                rcFriendSendBtn = (RECT){ offX + S(12), S(222), offX + sbW - S(12), S(258) };
                COLORREF btnCol = (g_pressedZone == ZONE_FRIEND_SEND) ? COLOR_ACCENT_DARK :
                                  ((g_hotZone == ZONE_FRIEND_SEND) ? COLOR_ACCENT_HOV : COLOR_ACCENT);
                DrawSmoothRoundRect(&rcFriendSendBtn, S(8), btnCol, 255, 0, 0);
                SelectObject(hdc, g_hFontBold);
                SetTextColor(hdc, RGB(8, 12, 10));
                DrawTextW(hdc, L"İstek Gönder", -1, &rcFriendSendBtn, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

                if (g_friendAddMsg[0]) {
                    RECT rcMsg = { offX + S(14), S(268), offX + sbW - S(14), S(320) };
                    SelectObject(hdc, g_hFontSmall);
                    SetTextColor(hdc, wcsstr(g_friendAddMsg, L"gönderildi") ? COLOR_ACCENT_HOV : COLOR_DANGER);
                    DrawTextW(hdc, g_friendAddMsg, -1, &rcMsg, DT_LEFT | DT_WORDBREAK);
                }
            }
        }

        RestoreDC(hdc, savedSide);
        SetBkMode(hdc, TRANSPARENT);
    ResetPixelClip();
    }

    // =========================================================================
    // 5. GELEN CANLI JAM DAVETİ YÜZEN BİLDİRİMİ
    // =========================================================================
    if (Jam_HasPendingInvite()) {
        wchar_t invName[64] = {0}, invTag[32] = {0};
        Jam_GetPendingInviteInfo(invName, invTag);

        int bannerW = S(380);
        if (bannerW > w - S(24)) bannerW = w - S(24);
        int topY = S(10);
        rcToastBox = (RECT){ (w - bannerW) / 2, topY, (w + bannerW) / 2, topY + S(46) };
        DrawSmoothRoundRect(&rcToastBox, S(12), RGB(16, 38, 26), 250, COLOR_ACCENT_HOV, 220);

        DrawSdfCircle((float)(rcToastBox.left + S(16)), (float)(topY + S(23)), SF(4.5f), COLOR_ACCENT_HOV, 255);

        wchar_t toastTitle[128];
        _snwprintf(toastTitle, 127, L"%s seni Jam'e davet etti!", invName[0] ? invName : invTag);
        toastTitle[127] = L'\0';

        rcToastAccept = (RECT){ rcToastBox.right - S(96), rcToastBox.top + S(9), rcToastBox.right - S(42), rcToastBox.bottom - S(9) };
        DrawSmoothRoundRect(&rcToastAccept, S(6), (g_hotZone == ZONE_TOAST_ACCEPT) ? COLOR_ACCENT_HOV : COLOR_ACCENT, 255, 0, 0);
        SelectObject(hdc, g_hFontSmallBold);
        SetTextColor(hdc, RGB(8, 12, 10));
        DrawTextW(hdc, L"Katıl", -1, &rcToastAccept, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        rcToastReject = (RECT){ rcToastBox.right - S(36), rcToastBox.top + S(9), rcToastBox.right - S(8), rcToastBox.bottom - S(9) };
        DrawSmoothRoundRect(&rcToastReject, S(6), COLOR_CTRL_HOVER, 255, COLOR_BORDER, 140);
        DrawVecCross((float)(rcToastReject.left + rcToastReject.right) * 0.5f,
                     (float)(rcToastReject.top + rcToastReject.bottom) * 0.5f,
                     SF(7.0f), COLOR_TEXT_MUTED);

        RECT rcToastTxt = { rcToastBox.left + S(28), rcToastBox.top, rcToastAccept.left - S(6), rcToastBox.bottom };
        SelectObject(hdc, g_hFontBold);
        SetTextColor(hdc, COLOR_TEXT_WHITE);
        DrawTextW(hdc, toastTitle, -1, &rcToastTxt, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    } else {
        rcToastBox = rcToastAccept = rcToastReject = (RECT){0};
    }
}

// =============================================================================
// HIT-TEST VE TIKLAMA HESABI
// =============================================================================
static UiZone HitTestZones(POINT pt) {
    if (Jam_HasPendingInvite()) {
        if (PtInRect(&rcToastAccept, pt)) return ZONE_TOAST_ACCEPT;
        if (PtInRect(&rcToastReject, pt)) return ZONE_TOAST_REJECT;
    }
    if (PtInRect(&rcHamburger, pt)) return ZONE_HAMBURGER;
    if (g_searchQuery[0] && PtInRect(&rcSearchClear, pt)) return ZONE_SEARCH_CLEAR;

    if (g_sidebarOpen && pt.x < GetSidebarW()) {
        if (g_sidebarView == SIDEBAR_VIEW_FRIENDS) {
            if (PtInRect(&rcFriendsBack, pt))    return ZONE_FRIENDS_BACK;
            if (PtInRect(&rcFriendsRefresh, pt)) return ZONE_FRIENDS_REFRESH;
            if (PtInRect(&rcCopyTag, pt))        return ZONE_COPY_TAG;
            if (PtInRect(&rcFriendsTab0, pt))    return ZONE_FTAB0;
            if (PtInRect(&rcFriendsTab1, pt))    return ZONE_FTAB1;
            if (PtInRect(&rcFriendsTab2, pt))    return ZONE_FTAB2;
            if (g_friendsTab == FRIENDS_TAB_ADD && PtInRect(&rcFriendSendBtn, pt)) return ZONE_FRIEND_SEND;
        }
        return ZONE_NONE;
    }
    if (g_sidebarOpen) return ZONE_NONE;

    if (Jam_GetState() != JAM_STATE_NONE) {
        if (rcJamLeave.right > 0 && PtInRect(&rcJamLeave, pt)) return ZONE_JAM_LEAVE;
        if (rcJamMissingBtn.right > 0 && PtInRect(&rcJamMissingBtn, pt)) return ZONE_JAM_MISSING_DL;
    }

    // Yüzen Kapsül İçi Hit-Test
    if (PtInRect(&rcCapsule, pt)) {
        if (PtInRect(&rcPlay, pt))    return ZONE_PLAY;
        if (PtInRect(&rcPrev, pt))    return ZONE_PREV;
        if (PtInRect(&rcNext, pt))    return ZONE_NEXT;
        if (PtInRect(&rcRepeat, pt))  return ZONE_REPEAT;
        if (rcSpeaker.right > 0 && PtInRect(&rcSpeaker, pt)) return ZONE_SPEAKER;
        if (rcVolHit.right > 0 && PtInRect(&rcVolHit, pt))   return ZONE_VOL;
        if (rcSeekHit.right > 0 && PtInRect(&rcSeekHit, pt)) return ZONE_SEEK;
        return ZONE_NONE;
    }

    return ZONE_NONE;
}

static int HitTestTrackRowIndex(HWND hWnd, POINT pt, int *outDisplayIdx) {
    if (PtInRect(&rcCapsule, pt)) return -1; // Kapsülün arkasını tıklatmaz!

    RECT rcClient;
    GetClientRect(hWnd, &rcClient);
    int listTop    = GetHeaderH() + S(2);
    int listBottom = rcClient.bottom;
    if (pt.y < listTop || pt.y >= listBottom || pt.x < S(16) || pt.x > rcClient.right - S(16)) {
        return -1;
    }
    int displayIdx = (int)(((float)(pt.y - listTop) + g_scrollPosPx) / (float)GetItemH());
    int totalCount = g_isFiltered ? g_filterCount : Playlist_Count();
    if (displayIdx < 0 || displayIdx >= totalCount) return -1;
    if (outDisplayIdx) *outDisplayIdx = displayIdx;
    return g_isFiltered ? g_filterIndices[displayIdx] : displayIdx;
}

// =============================================================================
// GİRİŞ KUTULARI VE ALT SINIFLAMA
// =============================================================================
static LRESULT CALLBACK SearchBoxSubclassProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_KEYDOWN) {
        if (wParam == VK_ESCAPE) {
            SetWindowTextW(hWnd, L"");
            ApplyFilter(L"");
            SetFocus(g_hWnd);
            return 0;
        } else if (wParam == VK_RETURN) {
            if (g_isFiltered && g_filterCount > 0) {
                PlayTrack(g_filterIndices[0]);
            }
            SetFocus(g_hWnd);
            return 0;
        }
    } else if (msg == WM_CHAR && (wParam == VK_RETURN || wParam == VK_ESCAPE)) {
        return 0;
    } else if (msg == WM_SETFOCUS || msg == WM_KILLFOCUS) {
        InvalidateRect(hWnd, NULL, FALSE);
        if (g_hWnd) InvalidateRect(g_hWnd, &rcSearchBox, FALSE);
    } else if (msg == WM_PAINT) {
        LRESULT res = CallWindowProcW(g_oldSearchEditProc, hWnd, msg, wParam, lParam);
        if (GetWindowTextLengthW(hWnd) == 0 && GetFocus() != hWnd) {
            HDC hdc = GetDC(hWnd);
            if (hdc) {
                RECT rc;
                GetClientRect(hWnd, &rc);
                SetBkMode(hdc, TRANSPARENT);
                HGDIOBJ oldF = SelectObject(hdc, g_hFontMain);
                SetTextColor(hdc, COLOR_TEXT_DIM);
                DrawTextW(hdc, L"Ara...", -1, &rc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                SelectObject(hdc, oldF);
                ReleaseDC(hWnd, hdc);
            }
        }
        return res;
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
    } else if (msg == WM_SETFOCUS || msg == WM_KILLFOCUS) {
        InvalidateRect(hWnd, NULL, FALSE);
        if (g_hWnd) InvalidateRect(g_hWnd, NULL, FALSE);
    } else if (msg == WM_PAINT) {
        LRESULT res = CallWindowProcW(g_oldFriendAddEditProc, hWnd, msg, wParam, lParam);
        if (GetWindowTextLengthW(hWnd) == 0 && GetFocus() != hWnd) {
            HDC hdc = GetDC(hWnd);
            if (hdc) {
                RECT rc;
                GetClientRect(hWnd, &rc);
                SetBkMode(hdc, TRANSPARENT);
                HGDIOBJ oldF = SelectObject(hdc, g_hFontMain);
                SetTextColor(hdc, COLOR_TEXT_DIM);
                DrawTextW(hdc, L"Örn: Selim#8421", -1, &rc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                SelectObject(hdc, oldF);
                ReleaseDC(hWnd, hdc);
            }
        }
        return res;
    }
    return CallWindowProcW(g_oldFriendAddEditProc, hWnd, msg, wParam, lParam);
}

static void LayoutHeaderInputs(int clientW) {
    int headerH  = GetHeaderH();
    int searchW  = (clientW >= S(560)) ? S(208) : S(156);
    int searchH  = S(28);
    int searchX  = clientW - searchW - S(14);
    int searchY  = (headerH - searchH) / 2;
    int rightPad = g_searchQuery[0] ? S(28) : S(10);
    if (g_hSearchEdit) {
        MoveWindow(g_hSearchEdit, searchX + S(28), searchY + S(4), searchW - S(28) - rightPad, searchH - S(8), TRUE);
    }
    UpdateSidebarEditPosition();
}

// =============================================================================
// ANA PENCERE OLAY DÖNGÜSÜ
// =============================================================================
static LRESULT CALLBACK MainWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            g_hWnd = hWnd;

            BOOL useDark = TRUE;
            DwmSetWindowAttribute(hWnd, 20, &useDark, sizeof(useDark));
            DwmSetWindowAttribute(hWnd, 19, &useDark, sizeof(useDark));
            DWORD cornerPref = 2; // DWMWCP_ROUND
            DwmSetWindowAttribute(hWnd, 33, &cornerPref, sizeof(cornerPref));

            typedef UINT(WINAPI *GetDpiForWindowProc)(HWND);
            GetDpiForWindowProc pGetDpi = (GetDpiForWindowProc)GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow");
            int dpi = pGetDpi ? (int)pGetDpi(hWnd) : 96;
            if (dpi <= 0) dpi = 96;
            InitDpiFonts(dpi);

            g_hAppIcon   = (HICON)LoadImageW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(1), IMAGE_ICON, S(36), S(36), LR_DEFAULTCOLOR);
            g_hBrInputBg = CreateSolidBrush(COLOR_INPUT_BG);

            g_hSearchEdit = CreateWindowExW(
                0, L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                0, 0, 0, 0,
                hWnd, (HMENU)IDC_HEADER_SEARCH, GetModuleHandle(NULL), NULL
            );
            SendMessageW(g_hSearchEdit, WM_SETFONT, (WPARAM)g_hFontMain, TRUE);
            SendMessageW(g_hSearchEdit, EM_SETCUEBANNER, TRUE, (LPARAM)L"Ara...");
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

            // Eğer kayıtlı arkadaş varsa gelen kutusunu arka planda uyandır
            if (Friends_GetFriendCount() > 0) {
                EnsureSocialNetworkStarted();
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
            UpdateTimerSchedule(hWnd);

            Updater_Init();
            Updater_CheckAsync(hWnd, FALSE);
            return 0;
        }

        case WM_CTLCOLOREDIT: {
            HWND hCtrl = (HWND)lParam;
            if (hCtrl == g_hSearchEdit || hCtrl == g_hFriendAddEdit) {
                HDC hdcEdit = (HDC)wParam;
                SetBkColor(hdcEdit, COLOR_INPUT_BG);
                SetTextColor(hdcEdit, COLOR_TEXT_WHITE);
                return (LRESULT)g_hBrInputBg;
            }
            break;
        }

        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            if (wmId == IDC_HEADER_SEARCH && HIWORD(wParam) == EN_CHANGE) {
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
            if (wmId == IDM_CTX_PLAY && g_selectedTrack >= 0) {
                PlayTrack(g_selectedTrack);
                return 0;
            }
            if (wmId == IDM_CTX_OPEN_FOLDER && g_selectedTrack >= 0) {
                const PlaylistItem *it = Playlist_Get(g_selectedTrack);
                if (it && it->filePath[0]) {
                    wchar_t param[MAX_PATH + 32];
                    _snwprintf(param, MAX_PATH + 31, L"/select,\"%s\"", it->filePath);
                    param[MAX_PATH + 31] = L'\0';
                    ShellExecuteW(hWnd, L"open", L"explorer.exe", param, NULL, SW_SHOWNORMAL);
                }
                return 0;
            }
            if (wmId == IDM_CTX_REMOVE && g_selectedTrack >= 0) {
                SendMessageW(hWnd, WM_KEYDOWN, VK_DELETE, 0);
                return 0;
            }
            break;
        }

        case WM_APPCOMMAND: {
            int cmd = GET_APPCOMMAND_LPARAM(lParam);
            switch (cmd) {
                case APPCOMMAND_MEDIA_PLAY_PAUSE:
                case APPCOMMAND_MEDIA_PLAY:
                case APPCOMMAND_MEDIA_PAUSE:
                    if (Player_IsPlaying()) {
                        Player_Pause();
                        SyncLocalTrackToFriends();
                        if (Jam_IsActive()) Jam_BroadcastPause(Player_GetPosition());
                    } else if (g_currentTrack >= 0) {
                        Player_Play();
                        SyncLocalTrackToFriends();
                        BroadcastCurrentJamTrack(Player_GetPosition());
                    } else if (Playlist_Count() > 0) {
                        PlayTrack(g_selectedTrack >= 0 ? g_selectedTrack : 0);
                    }
                    if (g_isWindowActive && !g_isMinimized && rcCapsule.right > 0) {
                        InvalidateRect(hWnd, &rcCapsule, FALSE);
                    }
                    UpdateTimerSchedule(hWnd);
                    return 1;
                case APPCOMMAND_MEDIA_NEXTTRACK: {
                    int next = Playlist_FindNext(g_currentTrack, g_cfg.shuffle, g_cfg.repeat == 2 ? 1 : g_cfg.repeat);
                    if (next >= 0) { PlayTrack(next); ScrollTrackIntoView(next); }
                    return 1;
                }
                case APPCOMMAND_MEDIA_PREVIOUSTRACK: {
                    int prev = Playlist_FindPrev(g_currentTrack);
                    if (prev >= 0) { PlayTrack(prev); ScrollTrackIntoView(prev); }
                    return 1;
                }
                case APPCOMMAND_MEDIA_STOP:
                    Player_Stop();
                    SyncLocalTrackToFriends();
                    if (g_isWindowActive && !g_isMinimized) InvalidateRect(hWnd, NULL, FALSE);
                    UpdateTimerSchedule(hWnd);
                    return 1;
            }
            break;
        }

        case 0x02E0 /* WM_DPICHANGED */: {
            int newDpi = HIWORD(wParam);
            InitDpiFonts(newDpi);
            if (g_hSearchEdit)    SendMessageW(g_hSearchEdit, WM_SETFONT, (WPARAM)g_hFontMain, TRUE);
            if (g_hFriendAddEdit) SendMessageW(g_hFriendAddEdit, WM_SETFONT, (WPARAM)g_hFontMain, TRUE);
            FreeBackbuffer();
            RECT *prc = (RECT*)lParam;
            SetWindowPos(hWnd, NULL, prc->left, prc->top, prc->right - prc->left, prc->bottom - prc->top, SWP_NOZORDER | SWP_NOACTIVATE);
            InvalidateRect(hWnd, NULL, FALSE);
            return 0;
        }

        case WM_ACTIVATE: {
            BOOL wasActive = g_isWindowActive;
            g_isWindowActive = (LOWORD(wParam) != WA_INACTIVE);
            if (g_isWindowActive && !wasActive) {
                // Pencereye dönüldü: Tek karede hemen yenile ve gerekirse timer'ı aç
                InvalidateRect(hWnd, NULL, FALSE);
            }
            UpdateTimerSchedule(hWnd);
            break;
        }

        case WM_SIZE: {
            if (wParam == SIZE_MINIMIZED) {
                g_isMinimized = TRUE;
                UpdateTimerSchedule(hWnd);
                FreeBackbuffer();
                SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
            } else {
                g_isMinimized = FALSE;
                int w = LOWORD(lParam);
                ClampScrollTargets(hWnd);
                LayoutHeaderInputs(w);
                UpdateTimerSchedule(hWnd);
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

                BOOL mScroll  = StepSpring(&g_scrollPosPx, &g_scrollVelPx, g_scrollTargetPx,           24.0f, 1.0f,  dt, 0.35f);
                BOOL mSidebar = StepSpring(&g_sidebarAnim, &g_sidebarVel,  g_sidebarOpen ? 1.0f : 0.0f, 26.0f, 0.94f, dt, 0.003f);

                UpdateSidebarEditPosition();
                InvalidateRect(hWnd, NULL, FALSE);

                if (!mScroll && !mSidebar) {
                    KillTimer(hWnd, IDT_ANIM_TIMER);
                    g_animTimerActive = FALSE;
                }
                return 0;
            }

            if (wParam == IDT_UPDATE_TIMER) {
                DWORD now = GetTickCount();

                if (IsSocialRefreshActive()) {
                    Jam_CheckTimeouts();
                    Friends_CheckTimeouts();

                    if (now - g_lastFriendsRefreshTick >= 4000) {
                        g_lastFriendsRefreshTick = now;
                        Friends_RequestAllStatuses();
                    }

                    if (g_isWindowActive && !g_isMinimized) {
                        InvalidateRect(hWnd, NULL, FALSE);
                    }
                } else if (Jam_HasPendingInvite()) {
                    if (g_isWindowActive && !g_isMinimized) {
                        InvalidateRect(hWnd, &rcToastBox, FALSE);
                    }
                }

                if (Player_IsPlaying() || g_isDraggingSeek) {
                    if (g_isWindowActive && !g_isMinimized) {
                        // Sadece kapsül alanını yenile! (500.000 px yerine ~25.000 px!)
                        if (rcCapsule.right > rcCapsule.left) {
                            InvalidateRect(hWnd, &rcCapsule, FALSE);
                        } else {
                            InvalidateRect(hWnd, NULL, FALSE);
                        }
                    }
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
                SyncLocalTrackToFriends();
                if (g_isWindowActive && !g_isMinimized) InvalidateRect(hWnd, NULL, FALSE);
            }
            UpdateTimerSchedule(hWnd);
            return 0;
        }

        case WM_APP_NETWORK_EVENT: {
            NetEvent *evt = (NetEvent*)wParam;
            if (evt) {
                if (evt->type == NET_EVENT_CONNECTED) {
                    if (IsSocialRefreshActive()) {
                        TriggerImmediateFriendsRefresh();
                    }
                } else if (evt->type == NET_EVENT_MESSAGE) {
                    Friends_HandleNetworkMessage(evt->topic, evt->payload);
                    Jam_HandleNetworkMessage(evt->topic, evt->payload);

                    if (Jam_PollNeedSendCurrentState()) {
                        BroadcastCurrentJamTrack(Player_GetPosition());
                    }

                    JamRemoteSync rs;
                    if (Jam_PollRemoteSync(&rs)) {
                        ApplyRemoteJamSync(&rs);
                    }
                }
                free(evt);
                if (g_isWindowActive && !g_isMinimized) InvalidateRect(hWnd, NULL, FALSE);
            }
            return 0;
        }

        case WM_APP_JAM_AUTODL_DONE: {
            BOOL ok = (BOOL)wParam;
            JamAutoDlTask *task = (JamAutoDlTask*)lParam;
            InterlockedExchange(&g_jamAutoDlBusy, 0);

            if (task) {
                if (ok && g_cfg.lastFolder[0]) {
                    Playlist_ScanFolder(g_cfg.lastFolder);
                    ApplyFilter(g_searchQuery);

                    int found = Playlist_FindBestMatch(task->title, task->ytId);
                    if (found >= 0 && Jam_IsActive()) {
                        g_isSyncingFromRemote = TRUE;
                        PlayTrack(found);
                        g_isSyncingFromRemote = FALSE;
                        ScrollTrackIntoView(found);

                        Jam_SetSyncStatus(L"✓ Otomatik indirildi • Canlı saniye eşitleniyor...", FALSE, NULL);
                        Jam_RequestPartnerStateNow();
                    } else if (Jam_IsActive()) {
                        Jam_SetSyncStatus(L"⚠ Şarkı bulunamadı • 'İndir' ile manuel ekleyebilirsin", FALSE, task->title);
                    }
                } else if (Jam_IsActive()) {
                    Jam_SetSyncStatus(L"⚠ Otomatik indirilemedi • 'İndir' düğmesini kullan", FALSE, task->title);
                }
                free(task);
            }
            if (g_isWindowActive && !g_isMinimized) InvalidateRect(hWnd, NULL, FALSE);
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

                    wchar_t missing[256] = {0};
                    Jam_GetMissingTrack(missing, 256);
                    if (Jam_IsActive() && missing[0]) {
                        int mIdx = Playlist_FindBestMatch(missing, NULL);
                        if (mIdx >= 0) {
                            g_isSyncingFromRemote = TRUE;
                            PlayTrack(mIdx);
                            g_isSyncingFromRemote = FALSE;
                            Jam_SetSyncStatus(L"✓ Canlı senkronize", FALSE, NULL);
                            Jam_RequestPartnerStateNow();
                        }
                    }
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

            if (PtInRect(&rcCapsule, pt)) {
                int vol = Player_GetVolume() + (delta > 0 ? 5 : -5);
                Player_SetVolume(vol);
                if (Player_GetVolume() > 0) g_preMuteVol = Player_GetVolume();
                g_cfg.volume = Player_GetVolume();
                Config_Save(&g_cfg, L"config.json");
                InvalidateRect(hWnd, &rcCapsule, FALSE);
            } else if (!g_sidebarOpen || pt.x >= GetSidebarW()) {
                float step = ((float)delta / 120.0f) * ((float)GetItemH() * 2.2f);
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

            if (g_isDraggingScrollbar) {
                RECT rcClient;
                GetClientRect(hWnd, &rcClient);
                int sbTop = GetHeaderH() + S(4);
                int sbBottom = rcClient.bottom - S(12);
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
                    g_seekDragSec = (int)(ratio * (float)duration);
                    InvalidateRect(hWnd, &rcCapsule, FALSE);
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
                    InvalidateRect(hWnd, &rcCapsule, FALSE);
                }
                return 0;
            }

            if ((wParam & MK_LBUTTON) && g_dragFromIndex >= 0 && !g_isFiltered) {
                if (!g_isDraggingList && abs(pt.y - g_mouseDownPt.y) > S(7)) {
                    g_isDraggingList = TRUE;
                }
                if (g_isDraggingList) {
                    int listTop = GetHeaderH() + S(2);
                    int target = (int)(((float)(pt.y - listTop) + g_scrollPosPx) / (float)GetItemH());
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

            BOOL sbHov = (!g_sidebarOpen && rcScrollbarTrack.right > 0 && PtInRect(&rcScrollbarTrack, pt));
            if (sbHov != g_scrollbarHover) {
                g_scrollbarHover = sbHov;
                needRepaint = TRUE;
            }

            int newSideHover = -1;
            if (g_sidebarOpen && g_sidebarView == SIDEBAR_VIEW_MENU && pt.x < GetSidebarW() && pt.y >= S(48)) {
                int m = (pt.y - S(48)) / S(44);
                if (m >= 0 && m < 7) newSideHover = m;
            }
            if (newSideHover != g_sidebarHoverItem) {
                g_sidebarHoverItem = newSideHover;
                needRepaint = TRUE;
            }

            int newHoverTrack = (!g_sidebarOpen && !sbHov) ? HitTestTrackRowIndex(hWnd, pt, NULL) : -1;
            if (newHoverTrack != g_hoverTrack) {
                g_hoverTrack = newHoverTrack;
                needRepaint = TRUE;
            }

            if (needRepaint) {
                InvalidateRect(hWnd, NULL, FALSE);
            }
            return 0;
        }

        case WM_RBUTTONUP: {
            POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
            if (!g_sidebarOpen && !PtInRect(&rcCapsule, pt)) {
                int rowIdx = HitTestTrackRowIndex(hWnd, pt, NULL);
                if (rowIdx >= 0) {
                    g_selectedTrack = rowIdx;
                    InvalidateRect(hWnd, NULL, FALSE);

                    HMENU hMenu = CreatePopupMenu();
                    if (hMenu) {
                        AppendMenuW(hMenu, MF_STRING, IDM_CTX_PLAY,        L"Şimdi Çal");
                        AppendMenuW(hMenu, MF_STRING, IDM_CTX_OPEN_FOLDER, L"Dosya Konumunu Aç");
                        AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
                        AppendMenuW(hMenu, MF_STRING, IDM_CTX_REMOVE,      L"Listeden Kaldır");

                        POINT scrPt = pt;
                        ClientToScreen(hWnd, &scrPt);
                        TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, scrPt.x, scrPt.y, 0, hWnd, NULL);
                        DestroyMenu(hMenu);
                    }
                    return 0;
                }
            }
            break;
        }

        case WM_LBUTTONDOWN: {
            POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
            SetFocus(hWnd);
            SetCapture(hWnd);

            UiZone hit = HitTestZones(pt);
            g_pressedZone = hit;

            // Jam Davet Bildirimi
            if (hit == ZONE_TOAST_ACCEPT) {
                EnsureSocialNetworkStarted();
                Jam_AcceptInvite();
                TriggerImmediateFriendsRefresh();
                StartAnimLoop(hWnd);
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

            // Hamburger Menü
            if (hit == ZONE_HAMBURGER) {
                if (!g_sidebarOpen) g_sidebarView = SIDEBAR_VIEW_MENU;
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

            // Yan Çekmece Tıklamaları
            if (g_sidebarOpen) {
                int sbW = GetSidebarW();
                if (pt.x >= sbW) {
                    ToggleSidebar(FALSE);
                    g_pressedZone = ZONE_NONE;
                    ReleaseCapture();
                    return 0;
                }

                if (g_sidebarView == SIDEBAR_VIEW_MENU) {
                    if (pt.y >= S(48)) {
                        int item = (pt.y - S(48)) / S(44);
                        g_pressedZone = ZONE_NONE;
                        ReleaseCapture();
                        if (item == 0) { ToggleSidebar(FALSE); SelectFolderDialog(); }
                        else if (item == 1) { ToggleSidebar(FALSE); Search_ShowDialog(hWnd, g_cfg.lastFolder); }
                        else if (item == 2) { OpenFriendsDrawer(); }
                        else if (item == 3) { ToggleSidebar(FALSE); ImportM3UDialog(); }
                        else if (item == 4) { ToggleSidebar(FALSE); ExportM3UDialog(); }
                        else if (item == 5) {
                            ToggleSidebar(FALSE);
                            Playlist_Clear();
                            Player_Stop();
                            g_currentTrack = -1;
                            g_selectedTrack = -1;
                            SyncLocalTrackToFriends();
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
                    if (hit == ZONE_FRIENDS_REFRESH) {
                        TriggerImmediateFriendsRefresh();
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
                        TriggerImmediateFriendsRefresh();
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
                        g_friendAddMsg[0] = L'\0';
                        UpdateSidebarEditPosition();
                        if (g_hFriendAddEdit) SetFocus(g_hFriendAddEdit);
                        InvalidateRect(hWnd, NULL, FALSE);
                        return 0;
                    }
                    if (hit == ZONE_FRIEND_SEND) {
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
                                    if (Jam_GetState() != JAM_STATE_NONE && _wcsicmp(partnerTag, fItem->tag) == 0) {
                                        Jam_Leave();
                                        StartAnimLoop(hWnd);
                                    } else {
                                        EnsureSocialNetworkStarted();
                                        Jam_InviteFriend(fItem->tag, fItem->name);
                                        ToggleSidebar(FALSE);
                                        StartAnimLoop(hWnd);
                                    }
                                }
                                g_pressedZone = ZONE_NONE;
                                ReleaseCapture();
                                InvalidateRect(hWnd, NULL, FALSE);
                                return 0;
                            }
                            if (PtInRect(&rcFriendRemoveBtns[fi], pt)) {
                                const FriendItem *fItem = Friends_GetFriend(fi);
                                if (fItem) {
                                    wchar_t msgBuf[128];
                                    _snwprintf(msgBuf, 127, L"%s arkadaş listenden çıkarılsın mı?", fItem->name);
                                    msgBuf[127] = L'\0';
                                    g_pressedZone = ZONE_NONE;
                                    ReleaseCapture();
                                    if (MessageBoxW(hWnd, msgBuf, L"Arkadaşı Kaldır", MB_YESNO | MB_ICONQUESTION) == IDYES) {
                                        Friends_Remove(fItem->tag);
                                        InvalidateRect(hWnd, NULL, FALSE);
                                    }
                                }
                                return 0;
                            }
                        }
                    }
                    else if (g_friendsTab == FRIENDS_TAB_REQUESTS) {
                        for (int pi = 0; pi < Friends_GetPendingCount(); pi++) {
                            if (PtInRect(&rcReqAcceptBtns[pi], pt)) {
                                const FriendRequest *rq = Friends_GetPending(pi);
                                if (rq) {
                                    EnsureSocialNetworkStarted();
                                    Friends_AcceptRequest(rq->tag);
                                    g_friendsTab = FRIENDS_TAB_LIST;
                                }
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

            // Canlı Jam Sahne Çubuğu
            if (hit == ZONE_JAM_LEAVE) {
                Jam_Leave();
                StartAnimLoop(hWnd);
                g_pressedZone = ZONE_NONE;
                ReleaseCapture();
                InvalidateRect(hWnd, NULL, FALSE);
                return 0;
            }
            if (hit == ZONE_JAM_MISSING_DL) {
                wchar_t missing[256] = {0};
                Jam_GetMissingTrack(missing, 256);
                g_pressedZone = ZONE_NONE;
                ReleaseCapture();
                if (missing[0]) {
                    Search_ShowDialogWithQuery(hWnd, g_cfg.lastFolder, missing);
                } else {
                    Search_ShowDialog(hWnd, g_cfg.lastFolder);
                }
                return 0;
            }

            // Kaydırma Çubuğu
            if (GetMaxScrollPx(hWnd) > 0 && rcScrollbarTrack.right > 0 && PtInRect(&rcScrollbarTrack, pt)) {
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

            // Alt Yüzen Kapsül Kontrolleri
            if (hit == ZONE_PLAY) {
                if (Player_IsPlaying()) {
                    Player_Pause();
                    SyncLocalTrackToFriends();
                    if (Jam_IsActive()) Jam_BroadcastPause(Player_GetPosition());
                } else {
                    if (g_currentTrack >= 0) {
                        Player_Play();
                        SyncLocalTrackToFriends();
                        BroadcastCurrentJamTrack(Player_GetPosition());
                    } else if (Playlist_Count() > 0) {
                        PlayTrack(g_selectedTrack >= 0 ? g_selectedTrack : 0);
                    }
                }
                InvalidateRect(hWnd, &rcCapsule, FALSE);
                UpdateTimerSchedule(hWnd);
                return 0;
            }
            if (hit == ZONE_PREV) {
                int prev = Playlist_FindPrev(g_currentTrack);
                if (prev >= 0) { PlayTrack(prev); ScrollTrackIntoView(prev); }
                InvalidateRect(hWnd, &rcCapsule, FALSE);
                return 0;
            }
            if (hit == ZONE_NEXT) {
                int next = Playlist_FindNext(g_currentTrack, g_cfg.shuffle, g_cfg.repeat == 2 ? 1 : g_cfg.repeat);
                if (next >= 0) { PlayTrack(next); ScrollTrackIntoView(next); }
                InvalidateRect(hWnd, &rcCapsule, FALSE);
                return 0;
            }
            if (hit == ZONE_REPEAT) {
                g_cfg.repeat = (g_cfg.repeat + 1) % 3;
                Config_Save(&g_cfg, L"config.json");
                InvalidateRect(hWnd, &rcCapsule, FALSE);
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
                InvalidateRect(hWnd, &rcCapsule, FALSE);
                return 0;
            }
            if (hit == ZONE_SEEK) {
                int duration = Player_GetDuration();
                if (duration > 0 && rcSeekBar.right > rcSeekBar.left) {
                    float ratio = (float)(pt.x - rcSeekBar.left) / (float)(rcSeekBar.right - rcSeekBar.left);
                    if (ratio < 0.0f) ratio = 0.0f;
                    if (ratio > 1.0f) ratio = 1.0f;
                    g_seekDragSec = (int)(ratio * (float)duration);
                    g_isDraggingSeek = TRUE;
                    Player_SetPosition(g_seekDragSec);
                    if (Jam_IsActive()) Jam_BroadcastSeek(g_seekDragSec);
                    InvalidateRect(hWnd, &rcCapsule, FALSE);
                }
                return 0;
            }
            if (hit == ZONE_VOL) {
                g_isDraggingVol = TRUE;
                SendMessageW(hWnd, WM_MOUSEMOVE, wParam, lParam);
                return 0;
            }

            // Şarkı Satırı Tıklaması
            int actualIdx = HitTestTrackRowIndex(hWnd, pt, NULL);
            if (actualIdx >= 0) {
                g_dragFromIndex = actualIdx;
                g_dragTargetIndex = actualIdx;
                g_mouseDownPt = pt;
                g_isDraggingList = FALSE;
                PlayTrack(actualIdx);
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
                    InvalidateRect(hWnd, &rcCapsule, FALSE);
                    return 0;
                }
                if (wParam == 'S' || wParam == 's') {
                    g_cfg.shuffle = !g_cfg.shuffle;
                    Config_Save(&g_cfg, L"config.json");
                    InvalidateRect(hWnd, &rcCapsule, FALSE);
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
                        SyncLocalTrackToFriends();
                        if (Jam_IsActive()) Jam_BroadcastPause(Player_GetPosition());
                    } else if (g_currentTrack >= 0) {
                        Player_Play();
                        SyncLocalTrackToFriends();
                        BroadcastCurrentJamTrack(Player_GetPosition());
                    } else if (Playlist_Count() > 0) {
                        PlayTrack(g_selectedTrack >= 0 ? g_selectedTrack : 0);
                    }
                    InvalidateRect(hWnd, &rcCapsule, FALSE);
                    UpdateTimerSchedule(hWnd);
                    return 0;
                }
                case VK_LEFT: {
                    int pos = Player_GetPosition() - 5;
                    if (pos < 0) pos = 0;
                    Player_SetPosition(pos);
                    if (Jam_IsActive()) Jam_BroadcastSeek(pos);
                    InvalidateRect(hWnd, &rcCapsule, FALSE);
                    return 0;
                }
                case VK_RIGHT: {
                    int pos = Player_GetPosition() + 5;
                    int dur = Player_GetDuration();
                    if (dur > 0 && pos > dur) pos = dur;
                    Player_SetPosition(pos);
                    if (Jam_IsActive()) Jam_BroadcastSeek(pos);
                    InvalidateRect(hWnd, &rcCapsule, FALSE);
                    return 0;
                }
                case VK_UP: {
                    int vol = Player_GetVolume() + 5;
                    Player_SetVolume(vol);
                    if (Player_GetVolume() > 0) g_preMuteVol = Player_GetVolume();
                    g_cfg.volume = Player_GetVolume();
                    Config_Save(&g_cfg, L"config.json");
                    InvalidateRect(hWnd, &rcCapsule, FALSE);
                    return 0;
                }
                case VK_DOWN: {
                    int vol = Player_GetVolume() - 5;
                    Player_SetVolume(vol);
                    if (Player_GetVolume() > 0) g_preMuteVol = Player_GetVolume();
                    g_cfg.volume = Player_GetVolume();
                    Config_Save(&g_cfg, L"config.json");
                    InvalidateRect(hWnd, &rcCapsule, FALSE);
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
                        SyncLocalTrackToFriends();
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
            mmi->ptMinTrackSize.y = S(480);
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
            if (g_backDC && g_backPixels) {
                RenderUI(g_backDC, &rcClient);
                GdiFlush();
                BitBlt(hdc, 0, 0, rcClient.right, rcClient.bottom, g_backDC, 0, 0, SRCCOPY);
            } else {
                RenderUI(hdc, &rcClient);
            }

            EndPaint(hWnd, &ps);
            return 0;
        }

        case WM_PRINTCLIENT: {
            HDC hdc = (HDC)wParam;
            RECT rcClient;
            GetClientRect(hWnd, &rcClient);
            EnsureBackbuffer(hdc, rcClient.right, rcClient.bottom);
            if (g_backDC && g_backPixels) {
                RenderUI(g_backDC, &rcClient);
                GdiFlush();
                BitBlt(hdc, 0, 0, rcClient.right, rcClient.bottom, g_backDC, 0, 0, SRCCOPY);
            } else {
                RenderUI(hdc, &rcClient);
            }
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

            Jam_Leave();
            Friends_NotifyOffline();
            Network_Close();

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
            if (g_hFontMicro)      DeleteObject(g_hFontMicro);
            if (g_hBrInputBg)      DeleteObject(g_hBrInputBg);
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
    HICON hMainIcon  = LoadIconW(hInst, MAKEINTRESOURCEW(1));
    HICON hSmallIcon = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(1), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);

    WNDCLASSEXW wc;
    memset(&wc, 0, sizeof(wc));
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

    int winW = MulDiv(680, screenDpi, 96);
    int winH = MulDiv(720, screenDpi, 96);

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

    if (hMainIcon)  SendMessageW(hWnd, WM_SETICON, ICON_BIG,   (LPARAM)hMainIcon);
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
