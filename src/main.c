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
// MODERN PREMİUM SİYAH VE GÜNBATIMI-NEON RENK SİSTEMİ
// =============================================================================
#define COLOR_BG_TOP        RGB(5, 5, 5)        // #050505 Ana Arka Plan
#define COLOR_BG_BOT        RGB(5, 5, 5)        // #050505 Ana Arka Plan (Minimal Düz Siyah)
#define COLOR_BG_JAM_TOP    RGB(20, 10, 24)     // Canlı Jam Aktifken Üst Mor/Pembe Aura
#define COLOR_HEADER_SURF   RGB(16, 16, 16)     // #101010 Kart ve Panel Arka Planı
#define COLOR_DOCK_SURF     RGB(16, 16, 16)     // #101010 Yüzen Alt Oynatıcı Kartı
#define COLOR_SIDEBAR_SURF  RGB(16, 16, 16)     // #101010 Sol Çekmece Katmanı
#define COLOR_CARD_SURF     RGB(16, 16, 16)     // #101010 Kart ve Panel Arka Planı
#define COLOR_CARD_HOVER    RGB(24, 24, 24)     // #181818 İkincil Yüzey (Hover)
#define COLOR_ROW_HOVER     RGB(20, 20, 20)     // Şarkı Satırı Hover
#define COLOR_ROW_ACTIVE    RGB(26, 18, 28)     // Çalan Şarkı Satırı (Zarif Mor/Pembe)
#define COLOR_ROW_SEL       RGB(24, 24, 24)     // #181818 Seçili Şarkı Satırı
#define COLOR_CTRL_HOVER    RGB(24, 24, 24)     // #181818 İkincil Yüzey (Düğme Hover)
#define COLOR_CTRL_PRESS    RGB(38, 38, 38)     // #262626 Kenarlık/Basılı Tonu
#define COLOR_ACCENT_ORANGE RGB(255, 146, 92)   // #FF925C Turuncu Vurgu
#define COLOR_ACCENT_PINK   RGB(240, 68, 184)   // #F044B8 Pembe Vurgu
#define COLOR_ACCENT_PURPLE RGB(139, 53, 255)   // #8B35FF Mor Vurgu
#define COLOR_ACCENT        RGB(255, 146, 92)   // Turuncu Vurgu (#FF925C)
#define COLOR_ACCENT_HOV    RGB(240, 68, 184)   // Pembe Vurgu (#F044B8)
#define COLOR_ACCENT_DARK   RGB(139, 53, 255)   // Mor Vurgu (#8B35FF)
#define COLOR_AMBER         RGB(255, 146, 92)   // #FF925C Bekleyen Davet / İndirme
#define COLOR_DANGER        RGB(240, 68, 184)   // #F044B8 Uyarı / Ayrıl Vurgusu
#define COLOR_TEXT_WHITE    RGB(255, 255, 255)  // #FFFFFF Ana Metin
#define COLOR_TEXT_MUTED    RGB(161, 161, 170)  // #A1A1AA İkincil Metin
#define COLOR_TEXT_DIM      RGB(113, 113, 122)  // #71717A Üçüncül Numara / Süre Metni
#define COLOR_BAR_BG        RGB(24, 24, 24)     // #181818 İlerleme / Ses Çubuğu Arka Planı
#define COLOR_BAR_HOVER     RGB(38, 38, 38)     // #262626 Çubuk Hover Arka Planı
#define COLOR_BORDER        RGB(38, 38, 38)     // #262626 Kenarlıklar
#define COLOR_BORDER_LIGHT  RGB(38, 38, 38)     // #262626 Kenarlıklar
#define COLOR_INPUT_BG      RGB(24, 24, 24)     // #181818 İkincil Yüzey (Arama & Giriş Kutusu)

#define IDT_UPDATE_TIMER        1001
#define IDT_ANIM_TIMER          1002
#define IDC_HEADER_SEARCH       1010
#define IDC_FRIEND_ADD_EDIT     1020

#define IDM_CTX_PLAY            2001
#define IDM_CTX_OPEN_FOLDER     2002
#define IDM_CTX_REMOVE          2003

#define WM_APP_POST_INIT        (WM_APP + 440)
#define WM_APP_JAM_AUTODL_DONE  (WM_APP + 450)
#define WM_APP_JAM_AUTODL_PROG  (WM_APP + 451)
#define WM_APP_PL_DL_PROGRESS   (WM_APP + 460)
#define WM_APP_PL_DL_TRACK_DONE (WM_APP + 461)
#define WM_APP_PL_DL_ALL_DONE   (WM_APP + 462)

#define MAX_IMPORT_MISSING      256

typedef enum {
    DL_BANNER_NONE = 0,
    DL_BANNER_SINGLE,
    DL_BANNER_PLAYLIST,
    DL_BANNER_UPDATE
} DlBannerMode;

typedef enum {
    ZONE_NONE = 0,
    ZONE_HAMBURGER,
    ZONE_HEADER_FRIENDS,
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
    ZONE_JAM_MISSING_DL,
    ZONE_DL_CANCEL,
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
static inline int GetHeaderH(void)   { return S(60); }
static inline int GetDockH(void)     { return S(104); }
static inline int GetItemH(void)     { return S(56); }
static inline int GetSidebarW(void)  { return S(278); }

static HWND    g_hWnd = NULL;
static HWND    g_hSearchEdit = NULL;
static WNDPROC g_oldSearchEditProc = NULL;
static HBRUSH  g_hBrInputBg = NULL;

static HFONT g_hFontBrand = NULL;
static HFONT g_hFontTitle = NULL;
static HFONT g_hFontMain = NULL;
static HFONT g_hFontBold = NULL;
static HFONT g_hFontSmall = NULL;
static HFONT g_hFontSmallBold = NULL;
static HFONT g_hFontMicro = NULL;
static HFONT g_hFontIcons = NULL;
static HFONT g_hFontIconsSmall = NULL;
static HFONT g_hFontIconsLarge = NULL;

// 32-Bit Doğrudan Piksel Erişimli Kalıcı Çift Tampon (Sıfır GDI Tahsisatı + Gerçek Anti-Aliasing)
static HDC       g_backDC = NULL;
static HBITMAP   g_backBmp = NULL;
static HBITMAP   g_backOldBmp = NULL;
static uint32_t *g_backPixels = NULL;
static int       g_backW = 0;
static int       g_backH = 0;
static RECT      g_pxClip = {0};
static BOOL      g_gdiDirty = FALSE;

static IPicture *g_pMainThumb = NULL;
static HICON     g_hAppIcon = NULL;
static AppConfig g_cfg;
static int       g_currentTrack = -1;
static int       g_selectedTrack = -1;
static int       g_hoverTrack = -1;
static int       g_pressedTrack = -1;
static int       g_preMuteVol = 80;

// Apple Akışkan Kesintiye Uğratılabilir Yay Fiziği (Durduğunda %0.0 İşlemci!)
static float g_scrollPosPx = 0.0f;
static float g_scrollTargetPx = 0.0f;
static float g_scrollVelPx = 0.0f;

static BOOL  g_sidebarOpen = FALSE;
static float g_sidebarAnim = 0.0f;
static float g_sidebarVel = 0.0f;
static int   g_sidebarHoverItem = -1;
static int   g_sidebarPressedItem = -1;
static BOOL  g_isDraggingSidebar = FALSE;
static int   g_sidebarDragStartX = 0;
static float g_sidebarDragStartAnim = 0.0f;
static DWORD g_sidebarDragLastTick = 0;
static int   g_sidebarDragLastX = 0;

static float g_jamBannerAnim = 0.0f; // 0.0 = gizli, 1.0 = açık
static float g_jamBannerVel = 0.0f;

static float g_dlBannerAnim = 0.0f;  // 0.0 = gizli, 1.0 = açık (Evrensel İndirme Çubuğu)
static float g_dlBannerVel = 0.0f;

static BOOL  g_animTimerActive = FALSE;
static DWORD g_lastAnimTick = 0;
static BOOL  g_isWindowActive = TRUE;
static BOOL  g_isMinimized = FALSE;

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
static volatile int  g_jamAutoDlPct  = 0;

// Evrensel İndirme Bildirim & İlerleme Durumu (Tekil + Paylaşılan Oynatma Listesi + Güncelleme)
static DlBannerMode  g_dlMode = DL_BANNER_NONE;
static int           g_dlPercent = 0;
static int           g_dlDoneCount = 0;
static int           g_dlTotalCount = 0;
static DWORD         g_dlFinishHideTick = 0;
static wchar_t       g_dlHeadline[160] = {0};
static wchar_t       g_dlSubline[256] = {0};

static CRITICAL_SECTION     g_plDlCs;
static BOOL                 g_plDlCsReady = FALSE;
static MissingPlaylistTrack g_plDlQueue[MAX_IMPORT_MISSING];
static int                  g_plDlHead = 0;
static int                  g_plDlTail = 0;
static volatile LONG        g_plDlRunning = 0;
static volatile LONG        g_plDlCancel  = 0;

// Tıklama (Hit-Test) Bölgeleri
static RECT rcHamburger, rcHeaderFriends, rcQuickDownload, rcSearchBox, rcSearchClear;
static RECT rcShuffle, rcPrev, rcPlay, rcNext, rcRepeat;
static RECT rcSeekBar, rcSeekHit, rcVolBar, rcVolHit, rcSpeaker, rcDock;
static RECT rcToastBox, rcToastAccept, rcToastReject;
static RECT rcJamStage, rcJamLeave, rcJamMissingBtn;
static RECT rcDlStage, rcDlCancel;
static RECT rcFriendsBack, rcFriendsRefresh, rcCopyTag;
static RECT rcFriendsTab0, rcFriendsTab1, rcFriendsTab2;
static RECT rcFriendSendBtn;
static RECT rcFriendActionBtns[MAX_FRIENDS];
static RECT rcFriendRemoveBtns[MAX_FRIENDS];
static RECT rcReqAcceptBtns[MAX_REQUESTS];
static RECT rcReqRejectBtns[MAX_REQUESTS];

// =============================================================================
// YAZI TİPİ VE YÜKSEK DPI YÖNETİMİ (TEK GEÇİŞLİ ÖNBELLEKLİ SORGULAMA)
// =============================================================================
static int CALLBACK EnumFontCheckProc(const LOGFONTW *lpelfe, const TEXTMETRICW *lpntme, DWORD FontType, LPARAM lParam) {
    (void)lpelfe; (void)lpntme; (void)FontType;
    *(BOOL*)lParam = TRUE;
    return 0;
}

static void InitDpiFonts(int dpi) {
    if (dpi <= 0) dpi = 96;
    g_dpi = dpi;

    if (g_hFontBrand)      DeleteObject(g_hFontBrand);
    if (g_hFontTitle)      DeleteObject(g_hFontTitle);
    if (g_hFontBold)       DeleteObject(g_hFontBold);
    if (g_hFontMain)       DeleteObject(g_hFontMain);
    if (g_hFontSmall)      DeleteObject(g_hFontSmall);
    if (g_hFontSmallBold)  DeleteObject(g_hFontSmallBold);
    if (g_hFontMicro)      DeleteObject(g_hFontMicro);
    if (g_hFontIcons)      DeleteObject(g_hFontIcons);
    if (g_hFontIconsSmall) DeleteObject(g_hFontIconsSmall);
    if (g_hFontIconsLarge) DeleteObject(g_hFontIconsLarge);

    static BOOL s_probed = FALSE;
    static BOOL s_hasVarText = FALSE, s_hasVarDisp = FALSE, s_hasBahnschrift = FALSE, s_hasFluent = FALSE;
    if (!s_probed) {
        s_probed = TRUE;
        HDC hdc = GetDC(NULL);
        if (hdc) {
            LOGFONTW lf = {0};
            lf.lfCharSet = DEFAULT_CHARSET;
            wcscpy(lf.lfFaceName, L"Segoe UI Variable Text");
            EnumFontFamiliesExW(hdc, &lf, (FONTENUMPROCW)EnumFontCheckProc, (LPARAM)&s_hasVarText, 0);
            wcscpy(lf.lfFaceName, L"Segoe UI Variable Display");
            EnumFontFamiliesExW(hdc, &lf, (FONTENUMPROCW)EnumFontCheckProc, (LPARAM)&s_hasVarDisp, 0);
            wcscpy(lf.lfFaceName, L"Bahnschrift");
            EnumFontFamiliesExW(hdc, &lf, (FONTENUMPROCW)EnumFontCheckProc, (LPARAM)&s_hasBahnschrift, 0);
            wcscpy(lf.lfFaceName, L"Segoe Fluent Icons");
            EnumFontFamiliesExW(hdc, &lf, (FONTENUMPROCW)EnumFontCheckProc, (LPARAM)&s_hasFluent, 0);
            ReleaseDC(NULL, hdc);
        }
    }

    int sBrand     = -MulDiv(15, dpi, 72);
    int sTitle     = -MulDiv(12, dpi, 72);
    int sMain      = -MulDiv(10, dpi, 72);
    int sSmall     = -MulDiv(9,  dpi, 72);
    int sMicro     = -MulDiv(8,  dpi, 72);
    int sIconSmall = -MulDiv(10, dpi, 72);
    int sIcon      = -MulDiv(12, dpi, 72);
    int sIconLarge = -MulDiv(15, dpi, 72);

    const wchar_t *uiFont    = s_hasVarText     ? L"Segoe UI Variable Text"    : L"Segoe UI";
    const wchar_t *dispFont  = s_hasVarDisp     ? L"Segoe UI Variable Display" : uiFont;
    const wchar_t *brandFont = s_hasBahnschrift ? L"Bahnschrift"               : dispFont;
    const wchar_t *iconFont  = s_hasFluent      ? L"Segoe Fluent Icons"        : L"Segoe MDL2 Assets";

    g_hFontBrand      = CreateFontW(sBrand,     0, 0, 0, FW_BOLD,     FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, brandFont);
    g_hFontTitle      = CreateFontW(sTitle,     0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, dispFont);
    g_hFontBold       = CreateFontW(sMain,      0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, uiFont);
    g_hFontMain       = CreateFontW(sMain,      0, 0, 0, FW_NORMAL,   FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, uiFont);
    g_hFontSmall      = CreateFontW(sSmall,     0, 0, 0, FW_NORMAL,   FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, uiFont);
    g_hFontSmallBold  = CreateFontW(sSmall,     0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, uiFont);
    g_hFontMicro      = CreateFontW(sMicro,     0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, uiFont);
    g_hFontIconsSmall = CreateFontW(sIconSmall, 0, 0, 0, FW_NORMAL,   FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, iconFont);
    g_hFontIcons      = CreateFontW(sIcon,      0, 0, 0, FW_NORMAL,   FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, iconFont);
    g_hFontIconsLarge = CreateFontW(sIconLarge, 0, 0, 0, FW_NORMAL,   FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, iconFont);
}

// =============================================================================
// ULTRA HIZLI 64-BIT / SDF PİKSEL GRAFİK MOTORU (SIFIR GEREKSİZ GDIFLUSH!)
// =============================================================================
static inline void FlushGdiIfNeeded(void) {
    if (g_gdiDirty) {
        GdiFlush();
        g_gdiDirty = FALSE;
    }
}

static inline void DrawUiText(HDC hdc, const wchar_t *str, RECT *rc, UINT fmt) {
    g_gdiDirty = TRUE;
    DrawTextW(hdc, str, -1, rc, fmt);
}

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

static inline void FillSpanSolid32(uint32_t *dst, int count, uint32_t pxVal) {
    uint64_t pair = ((uint64_t)pxVal << 32) | (uint64_t)pxVal;
    uint64_t *dst64 = (uint64_t*)dst;
    int pairs = count >> 1;
    for (int p = 0; p < pairs; p++) {
        dst64[p] = pair;
    }
    int i = pairs << 1;
    if (i < count) {
        dst[i] = pxVal;
    }
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

static void FillVerticalGradientPx(int x, int y, int w, int h, COLORREF topCol, COLORREF botCol) {
    if (!g_backPixels || w <= 0 || h <= 0) return;
    FlushGdiIfNeeded();
    int x0 = (x < g_pxClip.left) ? g_pxClip.left : x;
    int y0 = (y < g_pxClip.top)  ? g_pxClip.top  : y;
    int x1 = (x + w > g_pxClip.right)  ? g_pxClip.right  : (x + w);
    int y1 = (y + h > g_pxClip.bottom) ? g_pxClip.bottom : (y + h);
    if (x0 >= x1 || y0 >= y1) return;

    int span = x1 - x0;
    if (topCol == botCol) {
        uint32_t solid = ColorToBGRA(topCol);
        for (int py = y0; py < y1; py++) {
            FillSpanSolid32(g_backPixels + py * g_backW + x0, span, solid);
        }
        return;
    }

    int r0 = GetRValue(topCol), g0 = GetGValue(topCol), b0 = GetBValue(topCol);
    int r1 = GetRValue(botCol), g1 = GetGValue(botCol), b1 = GetBValue(botCol);
    int denom = (h > 1) ? (h - 1) : 1;

    for (int py = y0; py < y1; py++) {
        int t = py - y;
        uint32_t cr = (uint32_t)(r0 + ((r1 - r0) * t) / denom);
        uint32_t cg = (uint32_t)(g0 + ((g1 - g0) * t) / denom);
        uint32_t cb = (uint32_t)(b0 + ((b1 - b0) * t) / denom);
        uint32_t pxVal = cb | (cg << 8) | (cr << 16) | 0xFF000000u;
        FillSpanSolid32(g_backPixels + py * g_backW + x0, span, pxVal);
    }
}

// Apple Derinlik Katmanı: Ultra hızlı SWAR arka plan karartma (<0.08ms)
static void DimRectFastPx(int x, int y, int w, int h, BYTE dimAlpha) {
    if (!g_backPixels || dimAlpha == 0 || w <= 0 || h <= 0) return;
    FlushGdiIfNeeded();
    int x0 = (x < 0) ? 0 : x;
    int y0 = (y < 0) ? 0 : y;
    int x1 = (x + w > g_backW) ? g_backW : (x + w);
    int y1 = (y + h > g_backH) ? g_backH : (y + h);
    if (x0 >= x1 || y0 >= y1) return;

    uint32_t keep = 256u - (uint32_t)dimAlpha;
    for (int py = y0; py < y1; py++) {
        uint32_t *row = g_backPixels + py * g_backW;
        for (int px = x0; px < x1; px++) {
            uint32_t d = row[px];
            uint32_t rb = (((d & 0x00FF00FFu) * keep) >> 8) & 0x00FF00FFu;
            uint32_t g  = (((d & 0x0000FF00u) * keep) >> 8) & 0x0000FF00u;
            row[px] = rb | g | 0xFF000000u;
        }
    }
}

// Apple Buzlu Cam (Frosted Glass): O(1) Kayan Toplamlı 2 Geçişli Hızlı Bulanıklaştırma (<0.25ms)
static void BlurFrostedGlassPx(int x, int y, int w, int h) {
    if (!g_backPixels || w <= 2 || h <= 2) return;
    FlushGdiIfNeeded();
    int x0 = (x < 0) ? 0 : x;
    int y0 = (y < 0) ? 0 : y;
    int x1 = (x + w > g_backW) ? g_backW : (x + w);
    int y1 = (y + h > g_backH) ? g_backH : (y + h);
    int bw = x1 - x0;
    int bh = y1 - y0;
    if (bw <= 2 || bh <= 2 || bw > 2048 || bh > 2048) return;

    enum { R = 10, KERN = 21, INV21 = 3121 }; // (3121 * sum) >> 16 ~= sum / 21
    uint32_t tmp[2048];

    // 1. Yatay Geçiş
    for (int py = y0; py < y1; py++) {
        uint32_t *row = g_backPixels + py * g_backW + x0;
        memcpy(tmp, row, (size_t)bw * sizeof(uint32_t));
        uint32_t f = tmp[0];
        uint32_t sr = ((f >> 16) & 0xFFu) * (R + 1);
        uint32_t sg = ((f >> 8)  & 0xFFu) * (R + 1);
        uint32_t sb = ( f        & 0xFFu) * (R + 1);
        for (int i = 1; i <= R; i++) {
            uint32_t p = tmp[(i < bw) ? i : (bw - 1)];
            sr += (p >> 16) & 0xFFu; sg += (p >> 8) & 0xFFu; sb += p & 0xFFu;
        }
        for (int px = 0; px < bw; px++) {
            uint32_t or_ = (sr * INV21) >> 16;
            uint32_t og  = (sg * INV21) >> 16;
            uint32_t ob  = (sb * INV21) >> 16;
            row[px] = ob | (og << 8) | (or_ << 16) | 0xFF000000u;

            int iOut = px - R;     if (iOut < 0) iOut = 0;
            int iIn  = px + R + 1; if (iIn >= bw) iIn = bw - 1;
            uint32_t pOut = tmp[iOut], pIn = tmp[iIn];
            sr += ((pIn >> 16) & 0xFFu) - ((pOut >> 16) & 0xFFu);
            sg += ((pIn >> 8)  & 0xFFu) - ((pOut >> 8)  & 0xFFu);
            sb += ( pIn        & 0xFFu) - ( pOut        & 0xFFu);
        }
    }

    // 2. Dikey Geçiş
    for (int px = x0; px < x1; px++) {
        for (int i = 0; i < bh; i++) {
            tmp[i] = g_backPixels[(y0 + i) * g_backW + px];
        }
        uint32_t f = tmp[0];
        uint32_t sr = ((f >> 16) & 0xFFu) * (R + 1);
        uint32_t sg = ((f >> 8)  & 0xFFu) * (R + 1);
        uint32_t sb = ( f        & 0xFFu) * (R + 1);
        for (int i = 1; i <= R; i++) {
            uint32_t p = tmp[(i < bh) ? i : (bh - 1)];
            sr += (p >> 16) & 0xFFu; sg += (p >> 8) & 0xFFu; sb += p & 0xFFu;
        }
        for (int i = 0; i < bh; i++) {
            uint32_t or_ = (sr * INV21) >> 16;
            uint32_t og  = (sg * INV21) >> 16;
            uint32_t ob  = (sb * INV21) >> 16;
            g_backPixels[(y0 + i) * g_backW + px] = ob | (og << 8) | (or_ << 16) | 0xFF000000u;

            int iOut = i - R;     if (iOut < 0) iOut = 0;
            int iIn  = i + R + 1; if (iIn >= bh) iIn = bh - 1;
            uint32_t pOut = tmp[iOut], pIn = tmp[iIn];
            sr += ((pIn >> 16) & 0xFFu) - ((pOut >> 16) & 0xFFu);
            sg += ((pIn >> 8)  & 0xFFu) - ((pOut >> 8)  & 0xFFu);
            sb += ( pIn        & 0xFFu) - ( pOut        & 0xFFu);
        }
    }
}

static inline void EvalRoundRectEdgePixel(uint32_t *dst, float dx, float dy, float rad,
                                          uint32_t fr, uint32_t fg, uint32_t fb, uint32_t fA256,
                                          uint32_t br, uint32_t bg, uint32_t bb, uint32_t bA256, BYTE borderAlpha) {
    float dist;
    if (dx <= 0.0f && dy <= 0.0f) {
        dist = ((dx > dy) ? dx : dy) - rad;
    } else {
        float ox = (dx > 0.0f) ? dx : 0.0f;
        float oy = (dy > 0.0f) ? dy : 0.0f;
        dist = sqrtf(ox * ox + oy * oy) - rad;
    }
    if (dist >= 0.65f) return;

    float outerCov = 0.5f - dist;
    if (outerCov < 0.0f) outerCov = 0.0f;
    if (outerCov > 1.0f) outerCov = 1.0f;

    if (borderAlpha > 0) {
        float innerCov = 0.5f - (dist + 1.1f);
        if (innerCov < 0.0f) innerCov = 0.0f;
        if (innerCov > 1.0f) innerCov = 1.0f;

        float borderWeight = outerCov - innerCov;
        if (innerCov > 0.0f && fA256 > 0) {
            BlendPixelBGRA(dst, fr, fg, fb, (uint32_t)(innerCov * (float)fA256));
        }
        if (borderWeight > 0.0f) {
            BlendPixelBGRA(dst, br, bg, bb, (uint32_t)(borderWeight * (float)bA256));
        }
    } else if (fA256 > 0) {
        BlendPixelBGRA(dst, fr, fg, fb, (uint32_t)(outerCov * (float)fA256));
    }
}

// Pürüzsüz SDF Yuvarlatılmış Dikdörtgen (64-bit İç Şerit Hızlandırmalı: ~15x Daha Hızlı!)
static void DrawSmoothRoundRect(const RECT *rc, int radiusPx, COLORREF fillCol, BYTE fillAlpha, COLORREF borderCol, BYTE borderAlpha) {
    if (!g_backPixels || !rc) return;
    int w = rc->right - rc->left;
    int h = rc->bottom - rc->top;
    if (w <= 0 || h <= 0) return;
    FlushGdiIfNeeded();

    int x0 = (rc->left < g_pxClip.left) ? g_pxClip.left : rc->left;
    int y0 = (rc->top  < g_pxClip.top)  ? g_pxClip.top  : rc->top;
    int x1 = (rc->right  > g_pxClip.right)  ? g_pxClip.right  : rc->right;
    int y1 = (rc->bottom > g_pxClip.bottom) ? g_pxClip.bottom : rc->bottom;
    if (x0 >= x1 || y0 >= y1) return;

    if (radiusPx <= 0 && borderAlpha == 0 && fillAlpha == 255) {
        uint32_t solid = ColorToBGRA(fillCol);
        int span = x1 - x0;
        for (int py = y0; py < y1; py++) {
            FillSpanSolid32(g_backPixels + py * g_backW + x0, span, solid);
        }
        return;
    }

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

    int innerLeft  = rc->left + 3;
    int innerRight = rc->right - 3;
    if (innerLeft < x0)  innerLeft = x0;
    if (innerRight > x1) innerRight = x1;

    for (int py = y0; py < y1; py++) {
        float dy = fabsf(((float)py + 0.5f) - cy) - by;
        uint32_t *row = g_backPixels + py * g_backW;

        if (dy <= -1.8f && innerRight > innerLeft) {
            for (int px = x0; px < innerLeft; px++) {
                float dx = fabsf(((float)px + 0.5f) - cx) - bx;
                EvalRoundRectEdgePixel(&row[px], dx, dy, rad, fr, fg, fb, fA256, br, bg, bb, bA256, borderAlpha);
            }
            if (fillAlpha == 255) {
                FillSpanSolid32(row + innerLeft, innerRight - innerLeft, solidPx);
            } else if (fA256 > 0) {
                for (int px = innerLeft; px < innerRight; px++) {
                    BlendPixelBGRA(&row[px], fr, fg, fb, fA256);
                }
            }
            for (int px = innerRight; px < x1; px++) {
                float dx = fabsf(((float)px + 0.5f) - cx) - bx;
                EvalRoundRectEdgePixel(&row[px], dx, dy, rad, fr, fg, fb, fA256, br, bg, bb, bA256, borderAlpha);
            }
            continue;
        }

        for (int px = x0; px < x1; px++) {
            float dx = fabsf(((float)px + 0.5f) - cx) - bx;
            EvalRoundRectEdgePixel(&row[px], dx, dy, rad, fr, fg, fb, fA256, br, bg, bb, bA256, borderAlpha);
        }
    }
}

// Yatay 3 Renkli Gradyan Yuvarlatılmış Dikdörtgen (Önbellekli LUT ile Ultra Hızlı)
static void DrawGradientRoundRectH(const RECT *rc, int radiusPx, COLORREF c0, COLORREF c1, COLORREF c2) {
    if (!g_backPixels || !rc) return;
    int w = rc->right - rc->left;
    int h = rc->bottom - rc->top;
    if (w <= 0 || h <= 0) return;
    FlushGdiIfNeeded();

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
    float totalW = (w > 1) ? (float)(w - 1) : 1.0f;

    float r0 = (float)GetRValue(c0), g0 = (float)GetGValue(c0), b0 = (float)GetBValue(c0);
    float r1 = (float)GetRValue(c1), g1 = (float)GetGValue(c1), b1 = (float)GetBValue(c1);
    float r2 = (float)GetRValue(c2), g2 = (float)GetGValue(c2), b2 = (float)GetBValue(c2);

    uint32_t lutR[2048], lutG[2048], lutB[2048], lutSolid[2048];
    int spanW = x1 - x0;
    BOOL useLut = (spanW <= 2048);
    if (useLut) {
        for (int px = x0; px < x1; px++) {
            float t = (float)(px - rc->left) / totalW;
            if (t < 0.0f) t = 0.0f;
            if (t > 1.0f) t = 1.0f;
            uint32_t cr, cg, cb;
            if (t <= 0.5f) {
                float u = t * 2.0f;
                cr = (uint32_t)(r0 + (r1 - r0) * u);
                cg = (uint32_t)(g0 + (g1 - g0) * u);
                cb = (uint32_t)(b0 + (b1 - b0) * u);
            } else {
                float u = (t - 0.5f) * 2.0f;
                cr = (uint32_t)(r1 + (r2 - r1) * u);
                cg = (uint32_t)(g1 + (g2 - g1) * u);
                cb = (uint32_t)(b1 + (b2 - b1) * u);
            }
            int k = px - x0;
            lutR[k] = cr; lutG[k] = cg; lutB[k] = cb;
            lutSolid[k] = cb | (cg << 8) | (cr << 16) | 0xFF000000u;
        }
    }

    for (int py = y0; py < y1; py++) {
        float dy = fabsf(((float)py + 0.5f) - cy) - by;
        uint32_t *row = g_backPixels + py * g_backW;

        for (int px = x0; px < x1; px++) {
            float dx = fabsf(((float)px + 0.5f) - cx) - bx;
            if (useLut && dx <= rad - 0.6f && dy <= -0.6f) {
                row[px] = lutSolid[px - x0];
                continue;
            }
            float dist;
            if (dx <= 0.0f && dy <= 0.0f) {
                dist = ((dx > dy) ? dx : dy) - rad;
            } else {
                float ox = (dx > 0.0f) ? dx : 0.0f;
                float oy = (dy > 0.0f) ? dy : 0.0f;
                dist = sqrtf(ox * ox + oy * oy) - rad;
            }

            float cov = 0.5f - dist;
            if (cov <= 0.0f) continue;
            if (cov > 1.0f) cov = 1.0f;

            if (useLut) {
                int k = px - x0;
                BlendPixelBGRA(&row[px], lutR[k], lutG[k], lutB[k], (uint32_t)(cov * 256.0f));
            } else {
                float t = (float)(px - rc->left) / totalW;
                if (t < 0.0f) t = 0.0f;
                if (t > 1.0f) t = 1.0f;
                float fr = (t <= 0.5f) ? (r0 + (r1 - r0) * (t * 2.0f)) : (r1 + (r2 - r1) * ((t - 0.5f) * 2.0f));
                float fg = (t <= 0.5f) ? (g0 + (g1 - g0) * (t * 2.0f)) : (g1 + (g2 - g1) * ((t - 0.5f) * 2.0f));
                float fb = (t <= 0.5f) ? (b0 + (b1 - b0) * (t * 2.0f)) : (b1 + (b2 - b1) * ((t - 0.5f) * 2.0f));
                BlendPixelBGRA(&row[px], (uint32_t)fr, (uint32_t)fg, (uint32_t)fb, (uint32_t)(cov * 256.0f));
            }
        }
    }
}

static void DrawSmoothCircle(float cx, float cy, float radius, COLORREF col, BYTE alpha) {
    if (!g_backPixels || radius <= 0.0f || alpha == 0) return;
    FlushGdiIfNeeded();
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
    FlushGdiIfNeeded();
    if (x1 < g_pxClip.left)  x1 = g_pxClip.left;
    if (x2 > g_pxClip.right) x2 = g_pxClip.right;
    if (x1 >= x2) return;
    uint32_t sr = GetRValue(col), sg = GetGValue(col), sb = GetBValue(col);
    uint32_t a256 = ((uint32_t)alpha * 256u + 127u) / 255u;
    uint32_t *row = g_backPixels + y * g_backW;
    if (a256 >= 256) {
        FillSpanSolid32(row + x1, x2 - x1, ColorToBGRA(col));
    } else {
        for (int px = x1; px < x2; px++) {
            BlendPixelBGRA(&row[px], sr, sg, sb, a256);
        }
    }
}

static void DrawHairlineV(int x, int y1, int y2, COLORREF col, BYTE alpha) {
    if (!g_backPixels || x < g_pxClip.left || x >= g_pxClip.right) return;
    FlushGdiIfNeeded();
    if (y1 < g_pxClip.top)    y1 = g_pxClip.top;
    if (y2 > g_pxClip.bottom) y2 = g_pxClip.bottom;
    if (y1 >= y2) return;
    uint32_t sr = GetRValue(col), sg = GetGValue(col), sb = GetBValue(col);
    uint32_t a256 = ((uint32_t)alpha * 256u + 127u) / 255u;
    for (int py = y1; py < y2; py++) {
        BlendPixelBGRA(&g_backPixels[py * g_backW + x], sr, sg, sb, a256);
    }
}

// =============================================================================
// DÜZEN, FİZİK MOTORU VE JAM SENKRONİZASYON YARDIMCILARI
// =============================================================================
static inline int GetJamStageHeight(void) {
    int baseH = Jam_IsDownloadingTrack() ? S(64) : S(54);
    return (int)(g_jamBannerAnim * (float)baseH + 0.5f);
}

static inline int GetDlStageHeight(void) {
    return (int)(g_dlBannerAnim * (float)S(60) + 0.5f);
}

static inline int GetTracklistTop(void) {
    return GetHeaderH() + GetJamStageHeight() + GetDlStageHeight();
}

static inline int GetCurrentSidebarRight(void) {
    float a = (g_sidebarAnim < 0.0f) ? 0.0f : ((g_sidebarAnim > 1.0f) ? 1.0f : g_sidebarAnim);
    return (int)(a * (float)GetSidebarW() + 0.5f);
}

static BOOL IsSocialRefreshActive(void) {
    return ((g_sidebarOpen && g_sidebarView == SIDEBAR_VIEW_FRIENDS) || Jam_GetState() != JAM_STATE_NONE);
}

static void TriggerImmediateFriendsRefresh(void) {
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
    int listTop = GetTracklistTop() + S(26);
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
    BOOL shouldShow = (g_sidebarAnim >= 0.96f && !g_isDraggingSidebar && g_sidebarView == SIDEBAR_VIEW_FRIENDS && g_friendsTab == FRIENDS_TAB_ADD);
    if (!shouldShow) {
        if (IsWindowVisible(g_hFriendAddEdit)) ShowWindow(g_hFriendAddEdit, SW_HIDE);
        return;
    }
    int sbW = GetSidebarW();
    int inputTop = GetHeaderH() + S(204);
    MoveWindow(g_hFriendAddEdit, S(22), inputTop + S(8), sbW - S(44), S(22), TRUE);
    if (!IsWindowVisible(g_hFriendAddEdit)) ShowWindow(g_hFriendAddEdit, SW_SHOW);
}

static BOOL g_networkStarted = FALSE;
static void EnsureSocialNetworkStarted(void) {
    if (g_networkStarted || !g_hWnd) return;
    g_networkStarted = TRUE;
    wchar_t myInitTag[32] = {0};
    Friends_GetMyTag(myInitTag, 32);
    char myInitKey[64] = {0};
    Network_TagToTopicKeyW(myInitTag, myInitKey, sizeof(myInitKey));
    Network_Init(g_hWnd, myInitKey);

    char myInboxTopic[160];
    snprintf(myInboxTopic, sizeof(myInboxTopic), "seloplayer/u/%s", myInitKey);
    Network_Subscribe(myInboxTopic);
}

static void UpdateTimerSchedule(HWND hWnd) {
    if (!hWnd) return;
    if (!g_isWindowActive || g_isMinimized) {
        if (g_animTimerActive) {
            KillTimer(hWnd, IDT_ANIM_TIMER);
            g_animTimerActive = FALSE;
            g_sidebarAnim   = g_sidebarOpen ? 1.0f : 0.0f;
            g_jamBannerAnim = (Jam_GetState() != JAM_STATE_NONE) ? 1.0f : 0.0f;
            g_dlBannerAnim  = (g_dlMode != DL_BANNER_NONE) ? 1.0f : 0.0f;
            g_scrollPosPx   = g_scrollTargetPx;
        }
        if (Jam_GetState() == JAM_STATE_NONE && g_dlFinishHideTick == 0) {
            KillTimer(hWnd, IDT_UPDATE_TIMER);
        } else {
            SetTimer(hWnd, IDT_UPDATE_TIMER, 1500, NULL);
        }
        return;
    }

    if (Player_IsPlaying() || g_dlFinishHideTick != 0) {
        SetTimer(hWnd, IDT_UPDATE_TIMER, 1000, NULL);
    } else if (Jam_GetState() != JAM_STATE_NONE || (g_sidebarOpen && g_sidebarView == SIDEBAR_VIEW_FRIENDS)) {
        SetTimer(hWnd, IDT_UPDATE_TIMER, 1500, NULL);
    } else {
        KillTimer(hWnd, IDT_UPDATE_TIMER);
    }
}

static void StartAnimLoop(HWND hWnd) {
    if (!g_animTimerActive && hWnd && g_isWindowActive && !g_isMinimized) {
        g_lastAnimTick = GetTickCount();
        SetTimer(hWnd, IDT_ANIM_TIMER, 10, NULL);
        g_animTimerActive = TRUE;
    }
}

static BOOL StepSpring(float *pos, float *vel, float target, float omega, float damping, float dt, float eps) {
    float disp = *pos - target;
    if (fabsf(disp) <= eps && fabsf(*vel) <= eps * 4.0f) {
        *pos = target;
        *vel = 0.0f;
        return FALSE;
    }
    // Apple Akışkan Fizik: 4 alt adımlı (sub-stepping) yarı-kapalı Euler entegrasyonu.
    // Windows timer dalgalanmalarından bağımsız %100 kararlı, sıfır titreme (zero jitter).
    float subDt = dt * 0.25f;
    for (int step = 0; step < 4; step++) {
        disp = *pos - target;
        float accel = -omega * omega * disp - 2.0f * damping * omega * (*vel);
        *vel += accel * subDt;
        *pos += (*vel) * subDt;
    }
    if (fabsf(*pos - target) <= eps && fabsf(*vel) <= eps * 4.0f) {
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
    if (g_hWnd) {
        InvalidateRect(g_hWnd, NULL, FALSE);
        UpdateTimerSchedule(g_hWnd);
    }
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
    int viewH = (rc.bottom - GetDockH()) - (GetTracklistTop() + S(26));
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

    static const wchar_t *exts[] = { L".jpg", L".jpeg", L".png", L".webp" };
    for (int i = 0; i < 4; i++) {
        wchar_t candidate[MAX_PATH];
        _snwprintf(candidate, MAX_PATH - 1, L"%s%s", base, exts[i]);
        candidate[MAX_PATH - 1] = L'\0';
        if (GetFileAttributesW(candidate) != INVALID_FILE_ATTRIBUTES) {
            OleLoadPicturePath(candidate, NULL, 0, 0, &IID_IPicture, (void**)&g_pMainThumb);
            if (g_pMainThumb) break;
        }
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
    LoadTrackThumbnail(item->filePath);
    Player_OpenFile(item->filePath);
    SyncLocalTrackToFriends();

    if (Jam_IsActive() && !g_isSyncingFromRemote && item->title[0] != 0) {
        wchar_t ytId[16] = {0};
        Playlist_ExtractYouTubeId(item->filePath, ytId, 16);
        Jam_SetSyncStatus(L"✓ Canlı senkronize", FALSE, NULL);
        Jam_BroadcastState(TRUE, item->title, ytId, 0);
    }
    if (g_hWnd) {
        InvalidateRect(g_hWnd, NULL, FALSE);
        UpdateTimerSchedule(g_hWnd);
    }
}

// =============================================================================
// JAM OTOMATİK EKSİK ŞARKI İNDİRİCİ (ARKA PLAN YT-DLP ENTEGRASYONU)
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

static void OnJamAutoDlProgress(int pct, void *userData) {
    (void)userData;
    g_jamAutoDlPct = pct;
    if (g_hWnd && IsWindow(g_hWnd)) {
        PostMessageW(g_hWnd, WM_APP_JAM_AUTODL_PROG, (WPARAM)pct, 0);
    }
}

static DWORD WINAPI JamAutoDownloadThread(LPVOID param) {
    JamAutoDlTask *task = (JamAutoDlTask*)param;

    wchar_t targetSpec[320];
    if (task->ytId[0] && wcslen(task->ytId) == 11) {
        _snwprintf(targetSpec, 319, L"https://www.youtube.com/watch?v=%s", task->ytId);
    } else {
        _snwprintf(targetSpec, 319, L"ytsearch1:%s", task->title);
    }
    targetSpec[319] = L'\0';

    BOOL success = Search_RunYtDlpDownload(targetSpec, task->folder, NULL, OnJamAutoDlProgress, NULL);

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

    g_jamAutoDlPct = 0;

    wchar_t statusMsg[128];
    _snwprintf(statusMsg, 127, L"⚡ \"%s\" sende yok • İndiriliyor (%%0)...", trackTitle);
    statusMsg[127] = L'\0';
    Jam_SetSyncStatus(statusMsg, TRUE, NULL);
    if (g_hWnd) StartAnimLoop(g_hWnd);

    HANDLE hThread = CreateThread(NULL, 64 * 1024, JamAutoDownloadThread, task, STACK_SIZE_PARAM_IS_A_RESERVATION, NULL);
    if (hThread) {
        CloseHandle(hThread);
    } else {
        free(task);
        InterlockedExchange(&g_jamAutoDlBusy, 0);
    }
}

// =============================================================================
// PAYLAŞILAN OYNATMA LİSTESİ OTOMATİK ARKA PLAN İNDİRME KUYRUĞU
// =============================================================================
static void OnSharedPlaylistProgress(int pct, void *userData) {
    (void)userData;
    if (g_hWnd && IsWindow(g_hWnd)) {
        PostMessageW(g_hWnd, WM_APP_PL_DL_PROGRESS, (WPARAM)pct, 0);
    }
}

static DWORD WINAPI SharedPlaylistDlThread(LPVOID param) {
    (void)param;
    wchar_t destFolder[MAX_PATH];
    EnsureDefaultMusicFolder(destFolder, MAX_PATH);

    int successTotal = 0;
    while (InterlockedCompareExchange(&g_plDlCancel, 0, 0) == 0) {
        MissingPlaylistTrack cur = {0};
        int curOneBased = 0;
        int totalSnap = 0;
        BOOL hasItem = FALSE;

        EnterCriticalSection(&g_plDlCs);
        if (g_plDlHead < g_plDlTail) {
            cur = g_plDlQueue[g_plDlHead++];
            curOneBased = g_plDlHead;
            totalSnap   = g_plDlTail;
            hasItem     = TRUE;
        }
        LeaveCriticalSection(&g_plDlCs);

        if (!hasItem) break;

        MissingPlaylistTrack *notifyStart = (MissingPlaylistTrack*)calloc(1, sizeof(MissingPlaylistTrack));
        if (notifyStart) {
            *notifyStart = cur;
            if (g_hWnd && IsWindow(g_hWnd)) {
                PostMessageW(g_hWnd, WM_APP_PL_DL_PROGRESS, (WPARAM)0, (LPARAM)notifyStart);
            } else {
                free(notifyStart);
            }
        }
        (void)curOneBased; (void)totalSnap;

        wchar_t targetSpec[320];
        if (cur.ytId[0] && wcslen(cur.ytId) == 11) {
            _snwprintf(targetSpec, 319, L"https://www.youtube.com/watch?v=%s", cur.ytId);
        } else {
            _snwprintf(targetSpec, 319, L"ytsearch1:%s", cur.title);
        }
        targetSpec[319] = L'\0';

        BOOL ok = Search_RunYtDlpDownload(targetSpec, destFolder, &g_plDlCancel, OnSharedPlaylistProgress, NULL);
        if (InterlockedCompareExchange(&g_plDlCancel, 0, 0) != 0) break;

        if (ok) successTotal++;
        if (g_hWnd && IsWindow(g_hWnd)) {
            PostMessageW(g_hWnd, WM_APP_PL_DL_TRACK_DONE, (WPARAM)ok, 0);
        }
    }

    BOOL wasCancelled = (InterlockedCompareExchange(&g_plDlCancel, 0, 0) != 0);
    InterlockedExchange(&g_plDlRunning, 0);
    if (g_hWnd && IsWindow(g_hWnd)) {
        PostMessageW(g_hWnd, WM_APP_PL_DL_ALL_DONE, (WPARAM)successTotal, (LPARAM)wasCancelled);
    }
    return 0;
}

static void EnqueueSharedPlaylistDownloads(const MissingPlaylistTrack *items, int count) {
    if (!items || count <= 0 || !g_plDlCsReady) return;

    EnterCriticalSection(&g_plDlCs);
    if (InterlockedCompareExchange(&g_plDlRunning, 0, 0) == 0) {
        g_plDlHead = 0;
        g_plDlTail = 0;
        g_dlDoneCount = 0;
        g_dlTotalCount = 0;
    }
    for (int i = 0; i < count && g_plDlTail < MAX_IMPORT_MISSING; i++) {
        BOOL dup = FALSE;
        for (int k = g_plDlHead; k < g_plDlTail; k++) {
            if ((items[i].ytId[0] && wcscmp(g_plDlQueue[k].ytId, items[i].ytId) == 0) ||
                (items[i].title[0] && _wcsicmp(g_plDlQueue[k].title, items[i].title) == 0)) {
                dup = TRUE;
                break;
            }
        }
        if (!dup) {
            g_plDlQueue[g_plDlTail++] = items[i];
            g_dlTotalCount = g_plDlTail;
        }
    }
    int pendingNow = g_plDlTail - g_plDlHead;
    LeaveCriticalSection(&g_plDlCs);

    if (pendingNow <= 0) return;

    g_dlMode = DL_BANNER_PLAYLIST;
    g_dlFinishHideTick = 0;
    g_dlPercent = 0;
    _snwprintf(g_dlHeadline, 159, L"PAYLAŞILAN LİSTE YÜKLENİYOR  •  1 / %d Parça", g_dlTotalCount);
    g_dlHeadline[159] = L'\0';
    wcsncpy(g_dlSubline, items[0].title, 255);
    g_dlSubline[255] = L'\0';

    if (g_hWnd) {
        StartAnimLoop(g_hWnd);
        UpdateTimerSchedule(g_hWnd);
        InvalidateRect(g_hWnd, NULL, FALSE);
    }

    if (InterlockedCompareExchange(&g_plDlRunning, 1, 0) == 0) {
        InterlockedExchange(&g_plDlCancel, 0);
        HANDLE hT = CreateThread(NULL, 64 * 1024, SharedPlaylistDlThread, NULL, STACK_SIZE_PARAM_IS_A_RESERVATION, NULL);
        if (hT) {
            CloseHandle(hT);
        } else {
            InterlockedExchange(&g_plDlRunning, 0);
        }
    }
}

static void HandleSmartPlaylistImport(const wchar_t *m3uPath) {
    if (!m3uPath || !m3uPath[0]) return;

    // Liste boşsa önce varsayılan müzik klasörünü tara ki bilgisayarda zaten olan şarkılar tekrar indirilmesin
    if (Playlist_Count() == 0) {
        wchar_t defFolder[MAX_PATH];
        EnsureDefaultMusicFolder(defFolder, MAX_PATH);
        Playlist_ScanFolder(defFolder);
    }

    MissingPlaylistTrack *missing = (MissingPlaylistTrack*)calloc(MAX_IMPORT_MISSING, sizeof(MissingPlaylistTrack));
    int missingCount = 0;
    int addedLocal = Playlist_ImportSmart(m3uPath, missing, MAX_IMPORT_MISSING, &missingCount);

    if (Playlist_Count() > 0 && g_selectedTrack < 0) {
        g_selectedTrack = 0;
    }
    ApplyFilter(g_searchQuery);

    if (missing && missingCount > 0) {
        EnqueueSharedPlaylistDownloads(missing, missingCount);
    } else if (addedLocal > 0) {
        g_dlMode = DL_BANNER_PLAYLIST;
        g_dlPercent = 100;
        wcscpy(g_dlHeadline, L"✓ OYNATMA LİSTESİ İÇE AKTARILDI");
        _snwprintf(g_dlSubline, 255, L"%d parça yerel kitaplığınızdan anında yüklendi", addedLocal);
        g_dlSubline[255] = L'\0';
        g_dlFinishHideTick = GetTickCount() + 3000;
        if (g_hWnd) {
            StartAnimLoop(g_hWnd);
            UpdateTimerSchedule(g_hWnd);
        }
    }
    if (missing) free(missing);
    if (g_hWnd) InvalidateRect(g_hWnd, NULL, FALSE);
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
    g_sidebarPressedItem = -1;
    if (!g_isWindowActive || g_isMinimized) {
        g_sidebarAnim = open ? 1.0f : 0.0f;
        g_sidebarVel = 0.0f;
    }
    if (!open && g_hFriendAddEdit) {
        ShowWindow(g_hFriendAddEdit, SW_HIDE);
    }
    if (open && g_sidebarView == SIDEBAR_VIEW_FRIENDS) {
        EnsureSocialNetworkStarted();
        TriggerImmediateFriendsRefresh();
    }
    if (g_hWnd) {
        StartAnimLoop(g_hWnd);
        InvalidateRect(g_hWnd, NULL, FALSE);
    }
}

static void OpenFriendsDrawer(void) {
    EnsureSocialNetworkStarted();
    g_sidebarView = SIDEBAR_VIEW_FRIENDS;
    g_sidebarOpen = TRUE;
    g_sidebarPressedItem = -1;
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

static void ImportPlaylistDialog(void) {
    wchar_t szFile[MAX_PATH] = {0};
    OPENFILENAMEW ofn;
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hWnd;
    ofn.lpstrTitle = L"Oynatma Listesini İçe Aktar";
    ofn.lpstrFilter = L"Oynatma Listesi (*.m3u8;*.m3u)\0*.m3u8;*.m3u\0Tüm Dosyalar (*.*)\0*.*\0";
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (GetOpenFileNameW(&ofn)) {
        HandleSmartPlaylistImport(szFile);
    }
}

static void ExportPlaylistDialog(void) {
    if (Playlist_Count() == 0) {
        MessageBoxW(g_hWnd, L"Dışa aktarılacak oynatma listesinde henüz şarkı bulunmuyor.", L"Oynatma Listesini Dışa Aktar", MB_OK | MB_ICONINFORMATION);
        return;
    }
    wchar_t szFile[MAX_PATH] = L"oynatma_listem.m3u8";
    OPENFILENAMEW ofn;
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hWnd;
    ofn.lpstrTitle = L"Oynatma Listesini Dışa Aktar";
    ofn.lpstrFilter = L"Paylaşılabilir Oynatma Listesi (*.m3u8)\0*.m3u8\0Standart Oynatma Listesi (*.m3u)\0*.m3u\0";
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT;
    ofn.lpstrDefExt = L"m3u8";

    if (GetSaveFileNameW(&ofn)) {
        if (Playlist_ExportSmart(szFile)) {
            g_dlMode = DL_BANNER_PLAYLIST;
            g_dlPercent = 100;
            wcscpy(g_dlHeadline, L"✓ OYNATMA LİSTESİ DIŞA AKTARILDI");
            wcscpy(g_dlSubline, L"Arkadaşın bu dosyayı içe aktardığında şarkılar otomatik indirilecek");
            g_dlFinishHideTick = GetTickCount() + 3500;
            if (g_hWnd) {
                StartAnimLoop(g_hWnd);
                UpdateTimerSchedule(g_hWnd);
                InvalidateRect(g_hWnd, NULL, FALSE);
            }
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
        wcscpy(g_friendAddMsg, L"✓ Arkadaşlık isteği gönderildi!");
        SetWindowTextW(g_hFriendAddEdit, L"");
    } else {
        wcscpy(g_friendAddMsg, L"İstek gönderilemedi. Etiketi veya interneti kontrol edin.");
    }
    InvalidateRect(hWnd, NULL, FALSE);
}

// =============================================================================
// MODERN ARAYÜZ ÇİZİM MOTORU (BAŞTAN AŞAĞI YENİLENMİŞ PREMIUM TASARIM)
// =============================================================================
static COLORREF GetAvatarJewelColor(const wchar_t *text) {
    static const COLORREF palette[] = {
        RGB(38, 20, 52),   // Zarif Derin Mor
        RGB(48, 18, 40),   // Koyu Pembe / Magenta
        RGB(44, 26, 20),   // Sıcak Koyu Amber
        RGB(26, 22, 40),   // Gece Menekşesi
        RGB(22, 26, 36),   // Koyu Gece Mavisi
        RGB(40, 22, 32)    // Koyu Bordo / Şarap
    };
    unsigned int hash = 2166136261u;
    if (text) {
        for (int i = 0; text[i]; i++) {
            hash ^= (unsigned int)(text[i] | 0x20);
            hash *= 16777619u;
        }
    }
    return palette[hash % 6];
}

static wchar_t GetInitialChar(const wchar_t *text) {
    if (!text) return L'♪';
    for (int i = 0; text[i]; i++) {
        if (text[i] > L' ' && text[i] != L'(' && text[i] != L'[') {
            wchar_t ch = text[i];
            if (ch == L'i') return 0x0130;
            if (ch == 0x0131) return L'I';
            CharUpperBuffW(&ch, 1);
            return ch;
        }
    }
    return L'♪';
}

static void DrawIconCircleBtn(HDC hdc, const RECT *rc, UiZone zone, const wchar_t *glyph, COLORREF activeColor, BOOL showDot) {
    BOOL isHot = (g_hotZone == zone);
    BOOL isPress = (g_pressedZone == zone);
    float cx = (float)(rc->left + rc->right) * 0.5f;
    float cy = (float)(rc->top + rc->bottom) * 0.5f;
    float rad = (float)(rc->right - rc->left) * 0.5f;

    if (isHot || isPress) {
        DrawSmoothCircle(cx, cy, isPress ? (rad - 1.0f) : rad, isPress ? COLOR_CTRL_PRESS : COLOR_CTRL_HOVER, 255);
    }

    SetBkMode(hdc, TRANSPARENT);
    SelectObject(hdc, g_hFontIcons);
    SetTextColor(hdc, activeColor != 0 ? activeColor : (isHot ? COLOR_TEXT_WHITE : COLOR_TEXT_MUTED));
    RECT rcG = *rc;
    if (isPress) OffsetRect(&rcG, 0, 1);
    DrawUiText(hdc, glyph, &rcG, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    if (showDot) {
        DrawSmoothCircle(cx, (float)rc->bottom - (float)S(2), (float)S(2), COLOR_ACCENT_ORANGE, 255);
    }
}

static void RenderUI(HDC hdc, const RECT *clientRc) {
    int w = clientRc->right - clientRc->left;
    int h = clientRc->bottom - clientRc->top;
    int headerH = GetHeaderH();
    int dockH   = GetDockH();
    int itemH   = GetItemH();
    JamState jamState = Jam_GetState();

    SetBkMode(hdc, TRANSPARENT);
    ResetPixelClip();

    // 0. Ana Arka Plan (#050505 Minimal Düz Siyah)
    FillVerticalGradientPx(0, 0, w, h, (jamState == JAM_STATE_ACTIVE) ? COLOR_BG_JAM_TOP : COLOR_BG_TOP, COLOR_BG_BOT);

    // =========================================================================
    // 1. ÜST BÖLGE: MODERN CAM ÜST ÇUBUK (HEADER)
    // =========================================================================
    RECT rcHeader = { 0, 0, w, headerH };
    DrawSmoothRoundRect(&rcHeader, 0, COLOR_HEADER_SURF, 255, 0, 0);
    DrawHairlineH(0, w, headerH - 1, COLOR_BORDER, 255);

    // Sol Menü Düğmesi
    rcHamburger = (RECT){ S(12), (headerH - S(36)) / 2, S(48), (headerH + S(36)) / 2 };
    {
        BOOL hHot = (g_hotZone == ZONE_HAMBURGER);
        BOOL hPrs = (g_pressedZone == ZONE_HAMBURGER);
        if (hHot || hPrs || g_sidebarOpen) {
            RECT rcHDraw = rcHamburger;
            if (hPrs) InflateRect(&rcHDraw, -1, -1);
            DrawSmoothRoundRect(&rcHDraw, S(10), hPrs ? COLOR_CTRL_PRESS : COLOR_CTRL_HOVER, 255, COLOR_BORDER, 140);
        }
        SetBkMode(hdc, TRANSPARENT);
        SelectObject(hdc, g_hFontIcons);
        SetTextColor(hdc, g_sidebarOpen ? COLOR_ACCENT_ORANGE : COLOR_TEXT_WHITE);
        DrawUiText(hdc, L"\uE700", &rcHamburger, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        if (Friends_GetPendingCount() > 0 && !g_sidebarOpen) {
            DrawSmoothCircle((float)(rcHamburger.right - S(6)), (float)(rcHamburger.top + S(6)), (float)S(4), COLOR_ACCENT_ORANGE, 255);
        }
    }

    // Logo İkonu ve Marka İsmi ("Λ U R N S")
    int logoSz = S(26);
    int logoX  = S(56);
    int logoY  = (headerH - logoSz) / 2;
    if (g_hAppIcon) {
        DrawIconEx(hdc, logoX, logoY, g_hAppIcon, logoSz, logoSz, 0, NULL, DI_NORMAL);
        g_gdiDirty = TRUE;
    }
    RECT rcBrand = { logoX + logoSz + S(9), 0, logoX + logoSz + S(140), headerH };
    SetBkMode(hdc, TRANSPARENT);
    SelectObject(hdc, g_hFontBrand);
    SetTextColor(hdc, COLOR_TEXT_WHITE);
    DrawUiText(hdc, L"\u039B U R N S", &rcBrand, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // Sağ Arama Kutusu Hapı
    int searchW = (w >= S(560)) ? S(190) : S(148);
    int searchH = S(34);
    int searchX = w - searchW - S(12);
    int searchY = (headerH - searchH) / 2;
    rcSearchBox = (RECT){ searchX, searchY, searchX + searchW, searchY + searchH };

    BOOL searchFocused = (GetFocus() == g_hSearchEdit);
    DrawSmoothRoundRect(&rcSearchBox, searchH / 2, COLOR_INPUT_BG, 255,
                        searchFocused ? COLOR_ACCENT_ORANGE : COLOR_BORDER,
                        searchFocused ? 240 : 180);

    RECT rcSearchIcon = { searchX + S(10), searchY, searchX + S(28), searchY + searchH };
    SelectObject(hdc, g_hFontIconsSmall);
    SetTextColor(hdc, searchFocused ? COLOR_ACCENT_ORANGE : COLOR_TEXT_DIM);
    DrawUiText(hdc, L"\uE721", &rcSearchIcon, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    if (g_searchQuery[0]) {
        rcSearchClear = (RECT){ rcSearchBox.right - S(28), searchY + S(3), rcSearchBox.right - S(4), searchY + searchH - S(3) };
        SelectObject(hdc, g_hFontIconsSmall);
        SetTextColor(hdc, (g_hotZone == ZONE_SEARCH_CLEAR) ? COLOR_TEXT_WHITE : COLOR_TEXT_MUTED);
        DrawUiText(hdc, L"\uE711", &rcSearchClear, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    } else {
        rcSearchClear = (RECT){0};
    }

    // Hızlı "+ İndir" Hap Düğmesi
    int dlW = S(78);
    int dlRight = searchX - S(8);
    int dlLeft  = dlRight - dlW;
    if (dlLeft > S(116)) {
        rcQuickDownload = (RECT){ dlLeft, searchY, dlRight, searchY + searchH };
        BOOL qHot = (g_hotZone == ZONE_QUICK_DOWNLOAD);
        BOOL qPrs = (g_pressedZone == ZONE_QUICK_DOWNLOAD);
        RECT rcQDraw = rcQuickDownload;
        if (qPrs) InflateRect(&rcQDraw, -1, -1);
        DrawSmoothRoundRect(&rcQDraw, searchH / 2,
                            qPrs ? COLOR_CTRL_PRESS : (qHot ? COLOR_CARD_HOVER : COLOR_CARD_SURF), 255,
                            qHot ? COLOR_ACCENT_ORANGE : COLOR_BORDER, qHot ? 200 : 150);
        SelectObject(hdc, g_hFontSmallBold);
        SetTextColor(hdc, qHot ? COLOR_ACCENT_ORANGE : COLOR_TEXT_WHITE);
        DrawUiText(hdc, L"+ İndir", &rcQuickDownload, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    } else {
        rcQuickDownload = (RECT){0};
    }

    // Hızlı "Arkadaşlar / Jam" Hap Düğmesi (Üst Çubuktan Tek Tıkla Erişim!)
    int frW = S(116);
    int frRight = (rcQuickDownload.right > 0 ? rcQuickDownload.left : searchX) - S(8);
    int frLeft  = frRight - frW;
    if (frLeft > S(112)) {
        rcHeaderFriends = (RECT){ frLeft, searchY, frRight, searchY + searchH };
        BOOL fHot = (g_hotZone == ZONE_HEADER_FRIENDS);
        BOOL fPrs = (g_pressedZone == ZONE_HEADER_FRIENDS);
        BOOL fActive = ((g_sidebarOpen && g_sidebarView == SIDEBAR_VIEW_FRIENDS) || jamState == JAM_STATE_ACTIVE);

        COLORREF fFill = fActive ? RGB(28, 16, 32) : (fPrs ? COLOR_CTRL_PRESS : (fHot ? COLOR_CARD_HOVER : COLOR_CARD_SURF));
        COLORREF fBord = fActive ? COLOR_ACCENT_PINK : COLOR_BORDER;
        RECT rcFDraw = rcHeaderFriends;
        if (fPrs) InflateRect(&rcFDraw, -1, -1);
        DrawSmoothRoundRect(&rcFDraw, searchH / 2, fFill, 255, fBord, fActive ? 220 : 160);

        RECT rcFIcon = { frLeft + S(14), searchY, frLeft + S(32), searchY + searchH };
        SelectObject(hdc, g_hFontIconsSmall);
        SetTextColor(hdc, fActive ? COLOR_ACCENT_PINK : COLOR_TEXT_MUTED);
        DrawUiText(hdc, L"\uE716", &rcFIcon, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        RECT rcFTxt = { frLeft + S(34), searchY, frRight - S(10), searchY + searchH };
        SelectObject(hdc, g_hFontSmallBold);
        SetTextColor(hdc, fActive ? COLOR_ACCENT_PINK : COLOR_TEXT_WHITE);
        DrawUiText(hdc, (jamState == JAM_STATE_ACTIVE) ? L"Jam Aktif" : L"Arkadaşlar", &rcFTxt, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        if (Friends_GetPendingCount() > 0) {
            DrawSmoothCircle((float)(rcHeaderFriends.right - S(8)), (float)(rcHeaderFriends.top + S(7)), (float)S(4), COLOR_ACCENT_ORANGE, 255);
        }
    } else {
        rcHeaderFriends = (RECT){0};
    }

    // =========================================================================
    // 2. CANLI JAM SAHNE ÇUBUĞU (JAM İNDİRME YÜZDESİ VE DURUM ÇUBUĞU DAHİL)
    // =========================================================================
    int jamStageH = GetJamStageHeight();
    rcJamStage = (RECT){0};
    rcJamLeave = (RECT){0};
    rcJamMissingBtn = (RECT){0};

    if (jamStageH > S(8) && jamState != JAM_STATE_NONE) {
        int stageClipBot = headerH + jamStageH;
        int savedJam = SaveDC(hdc);
        IntersectClipRect(hdc, 0, headerH, w, stageClipBot);
        SetPixelClip(0, headerH, w, stageClipBot);

        BOOL isJamDl    = Jam_IsDownloadingTrack();
        int cardBottom  = headerH + (isJamDl ? S(60) : S(50));
        rcJamStage = (RECT){ S(12), headerH + S(6), w - S(12), cardBottom };
        BOOL isInviting = (jamState == JAM_STATE_INVITING);

        COLORREF stageBg   = isInviting ? RGB(32, 20, 16) : RGB(26, 14, 30);
        COLORREF stageBord = isInviting ? COLOR_ACCENT_ORANGE : COLOR_ACCENT_PINK;
        DrawSmoothRoundRect(&rcJamStage, S(12), stageBg, 255, stageBord, 200);

        float dotX = (float)(rcJamStage.left + S(20));
        float dotY = (float)(rcJamStage.top + S(22));
        COLORREF dotCol = isInviting ? COLOR_ACCENT_ORANGE : COLOR_ACCENT_PINK;
        DrawSmoothCircle(dotX, dotY, (float)S(8), dotCol, 55);
        DrawSmoothCircle(dotX, dotY, (float)S(4), dotCol, 255);

        rcJamLeave = (RECT){ rcJamStage.right - S(74), rcJamStage.top + S(8), rcJamStage.right - S(10), rcJamStage.top + S(36) };
        BOOL lvHot = (g_hotZone == ZONE_JAM_LEAVE);
        DrawSmoothRoundRect(&rcJamLeave, S(8), lvHot ? RGB(56, 18, 28) : RGB(24, 24, 24), 255,
                            lvHot ? COLOR_DANGER : COLOR_BORDER, 160);
        SelectObject(hdc, g_hFontSmallBold);
        SetTextColor(hdc, lvHot ? COLOR_ACCENT_PINK : COLOR_TEXT_WHITE);
        DrawUiText(hdc, isInviting ? L"İptal" : L"Ayrıl", &rcJamLeave, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        wchar_t missingTitle[256] = {0};
        Jam_GetMissingTrack(missingTitle, 256);
        int textRightLimit = rcJamLeave.left - S(10);

        if (isJamDl) {
            int jPct = g_jamAutoDlPct;
            if (jPct < 0) jPct = 0;
            if (jPct > 100) jPct = 100;
            wchar_t jPctTxt[16];
            _snwprintf(jPctTxt, 15, L"%%%d", jPct);
            jPctTxt[15] = L'\0';

            RECT rcJPct = { rcJamLeave.left - S(58), rcJamStage.top + S(8), rcJamLeave.left - S(8), rcJamStage.top + S(36) };
            SelectObject(hdc, g_hFontBold);
            SetTextColor(hdc, COLOR_ACCENT_ORANGE);
            DrawUiText(hdc, jPctTxt, &rcJPct, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
            textRightLimit = rcJPct.left - S(6);

            RECT rcJBarBg = { rcJamStage.left + S(14), rcJamStage.bottom - S(10), rcJamStage.right - S(14), rcJamStage.bottom - S(5) };
            DrawSmoothRoundRect(&rcJBarBg, S(3), COLOR_BAR_BG, 255, 0, 0);
            int jbW = rcJBarBg.right - rcJBarBg.left;
            int jbFill = (jbW * jPct) / 100;
            if (jbFill > 0 && jbFill < S(6)) jbFill = S(6);
            if (jbFill > 0) {
                RECT rcJBarFill = { rcJBarBg.left, rcJBarBg.top, rcJBarBg.left + jbFill, rcJBarBg.bottom };
                DrawGradientRoundRectH(&rcJBarFill, S(3), COLOR_ACCENT_ORANGE, COLOR_ACCENT_PINK, COLOR_ACCENT_PURPLE);
            }
        } else if (!isInviting && missingTitle[0]) {
            rcJamMissingBtn = (RECT){ rcJamLeave.left - S(82), rcJamStage.top + S(8), rcJamLeave.left - S(6), rcJamStage.top + S(36) };
            DrawGradientRoundRectH(&rcJamMissingBtn, S(8), COLOR_ACCENT_ORANGE, COLOR_ACCENT_PINK, COLOR_ACCENT_PURPLE);
            SelectObject(hdc, g_hFontSmallBold);
            SetTextColor(hdc, RGB(5, 5, 5));
            DrawUiText(hdc, L"İndir", &rcJamMissingBtn, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            textRightLimit = rcJamMissingBtn.left - S(8);
        }

        wchar_t partnerName[64] = {0};
        Jam_GetPartnerName(partnerName, 64);
        wchar_t jamHeadline[160];
        if (isInviting) {
            _snwprintf(jamHeadline, 159, L"JAM DAVETİ  •  %s bekleniyor", partnerName[0] ? partnerName : L"Arkadaşın");
        } else {
            _snwprintf(jamHeadline, 159, L"CANLI JAM  •  %s ile birlikte dinliyorsun", partnerName[0] ? partnerName : L"Arkadaşın");
        }
        jamHeadline[159] = L'\0';

        wchar_t syncSub[128] = {0};
        Jam_GetSyncStatus(syncSub, 128);

        RECT rcJTop = { rcJamStage.left + S(36), rcJamStage.top + S(5),  textRightLimit, rcJamStage.top + S(23) };
        RECT rcJSub = { rcJamStage.left + S(36), rcJamStage.top + S(22), textRightLimit, rcJamStage.top + S(41) };

        SelectObject(hdc, g_hFontSmallBold);
        SetTextColor(hdc, isInviting ? COLOR_ACCENT_ORANGE : COLOR_ACCENT_PINK);
        DrawUiText(hdc, jamHeadline, &rcJTop, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        SelectObject(hdc, g_hFontSmall);
        SetTextColor(hdc, COLOR_TEXT_WHITE);
        DrawUiText(hdc, syncSub, &rcJSub, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        RestoreDC(hdc, savedJam);
        SetBkMode(hdc, TRANSPARENT);
        ResetPixelClip();
    }

    // =========================================================================
    // 2.5 EVRENSEL CANLI İNDİRME YÜZDESİ VE DURUM ÇUBUĞU (APPLE FLUID BANNER)
    // =========================================================================
    int dlStageH = GetDlStageHeight();
    rcDlStage  = (RECT){0};
    rcDlCancel = (RECT){0};

    if (dlStageH > S(8) && (g_dlMode != DL_BANNER_NONE || g_dlBannerAnim > 0.02f)) {
        int dlClipTop = headerH + jamStageH;
        int dlClipBot = dlClipTop + dlStageH;
        int savedDl = SaveDC(hdc);
        IntersectClipRect(hdc, 0, dlClipTop, w, dlClipBot);
        SetPixelClip(0, dlClipTop, w, dlClipBot);

        rcDlStage = (RECT){ S(12), dlClipTop + S(4), w - S(12), dlClipTop + S(56) };
        BOOL isDone = (g_dlFinishHideTick != 0);
        COLORREF dlBord = isDone ? COLOR_ACCENT_ORANGE : COLOR_ACCENT_PINK;
        DrawSmoothRoundRect(&rcDlStage, S(12), RGB(20, 14, 24), 245, dlBord, 195);

        float dDotX = (float)(rcDlStage.left + S(18));
        float dDotY = (float)(rcDlStage.top + S(20));
        COLORREF dDotCol = isDone ? COLOR_ACCENT_ORANGE : COLOR_ACCENT_PINK;
        DrawSmoothCircle(dDotX, dDotY, (float)S(7), dDotCol, 55);
        DrawSmoothCircle(dDotX, dDotY, SF(3.5f), dDotCol, 255);

        int pctVal = g_dlPercent;
        if (pctVal < 0) pctVal = 0;
        if (pctVal > 100) pctVal = 100;

        int rightCursor = rcDlStage.right - S(12);
        if (g_dlMode == DL_BANNER_PLAYLIST && !isDone && InterlockedCompareExchange(&g_plDlRunning, 0, 0) != 0) {
            rcDlCancel = (RECT){ rightCursor - S(58), rcDlStage.top + S(7), rightCursor, rcDlStage.top + S(31) };
            BOOL cHot = (g_hotZone == ZONE_DL_CANCEL);
            BOOL cPrs = (g_pressedZone == ZONE_DL_CANCEL);
            DrawSmoothRoundRect(&rcDlCancel, S(7), cPrs ? COLOR_CTRL_PRESS : (cHot ? RGB(52, 18, 28) : RGB(28, 22, 32)),
                                255, cHot ? COLOR_DANGER : COLOR_BORDER, 160);
            SelectObject(hdc, g_hFontMicro);
            SetTextColor(hdc, cHot ? COLOR_ACCENT_PINK : COLOR_TEXT_MUTED);
            DrawUiText(hdc, L"İptal", &rcDlCancel, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            rightCursor = rcDlCancel.left - S(8);
        }

        wchar_t pctBadge[16];
        _snwprintf(pctBadge, 15, L"%%%d", pctVal);
        pctBadge[15] = L'\0';
        RECT rcPctTxt = { rightCursor - S(52), rcDlStage.top + S(5), rightCursor, rcDlStage.top + S(33) };
        SelectObject(hdc, g_hFontBold);
        SetTextColor(hdc, COLOR_ACCENT_ORANGE);
        DrawUiText(hdc, pctBadge, &rcPctTxt, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

        int dlTextRight = rcPctTxt.left - S(8);
        RECT rcDlHead = { rcDlStage.left + S(32), rcDlStage.top + S(4),  dlTextRight, rcDlStage.top + S(20) };
        RECT rcDlSub  = { rcDlStage.left + S(32), rcDlStage.top + S(19), dlTextRight, rcDlStage.top + S(37) };

        SelectObject(hdc, g_hFontMicro);
        SetTextColor(hdc, isDone ? COLOR_ACCENT_ORANGE : COLOR_ACCENT_PINK);
        DrawUiText(hdc, g_dlHeadline, &rcDlHead, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        SelectObject(hdc, g_hFontSmallBold);
        SetTextColor(hdc, COLOR_TEXT_WHITE);
        DrawUiText(hdc, g_dlSubline, &rcDlSub, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        // Alt Tam Genişlikli Sunset-Neon İlerleme Durum Çubuğu
        RECT rcDlBarBg = { rcDlStage.left + S(12), rcDlStage.bottom - S(10), rcDlStage.right - S(12), rcDlStage.bottom - S(5) };
        DrawSmoothRoundRect(&rcDlBarBg, S(3), COLOR_BAR_BG, 255, 0, 0);
        int barTotalW = rcDlBarBg.right - rcDlBarBg.left;
        int barFillW  = (barTotalW * pctVal) / 100;
        if (barFillW > 0 && barFillW < S(6)) barFillW = S(6);
        if (barFillW > 0) {
            RECT rcDlBarFill = { rcDlBarBg.left, rcDlBarBg.top, rcDlBarBg.left + barFillW, rcDlBarBg.bottom };
            DrawGradientRoundRectH(&rcDlBarFill, S(3), COLOR_ACCENT_ORANGE, COLOR_ACCENT_PINK, COLOR_ACCENT_PURPLE);
        }

        RestoreDC(hdc, savedDl);
        SetBkMode(hdc, TRANSPARENT);
        ResetPixelClip();
    }

    // =========================================================================
    // 3. ORTA BÖLGE: MODERN ÇİFT SATIRLI ŞARKI LİSTESİ
    // =========================================================================
    int sectionTop = headerH + jamStageH + dlStageH;
    int colHeaderH = S(26);
    int listTop    = sectionTop + colHeaderH;
    int listBottom = h - dockH - S(4);
    int totalCount = g_isFiltered ? g_filterCount : Playlist_Count();

    // Sütun Mikro Başlığı ("#   PARÇA VE SANATÇI               TOPLAM")
    if (listBottom > listTop) {
        RECT rcColNum  = { S(18), sectionTop + S(2), S(48), listTop };
        RECT rcColName = { S(68), sectionTop + S(2), w - S(120), listTop };
        RECT rcColCnt  = { w - S(120), sectionTop + S(2), w - S(20), listTop };

        SelectObject(hdc, g_hFontMicro);
        SetTextColor(hdc, COLOR_TEXT_DIM);
        DrawUiText(hdc, L"#", &rcColNum, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        DrawUiText(hdc, L"PARÇA VE SANATÇI", &rcColName, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        wchar_t cntStr[48];
        _snwprintf(cntStr, 47, L"%d PARÇA", totalCount);
        cntStr[47] = L'\0';
        DrawUiText(hdc, cntStr, &rcColCnt, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

        DrawHairlineH(S(14), w - S(14), listTop - 1, COLOR_BORDER, 180);
    }

    int savedList = SaveDC(hdc);
    IntersectClipRect(hdc, 0, listTop, w, listBottom);
    SetPixelClip(0, listTop, w, listBottom);

    if (totalCount == 0) {
        int midY = (listTop + listBottom) / 2;
        RECT rcEmptyCard = { S(28), midY - S(58), w - S(28), midY + S(58) };
        DrawSmoothRoundRect(&rcEmptyCard, S(14), COLOR_CARD_SURF, 180, COLOR_BORDER, 140);

        SetBkMode(hdc, TRANSPARENT);
        if (g_isFiltered) {
            SelectObject(hdc, g_hFontBold);
            SetTextColor(hdc, COLOR_TEXT_WHITE);
            DrawUiText(hdc, L"Eşleşen şarkı bulunamadı", &rcEmptyCard, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        } else {
            RECT rcET = { rcEmptyCard.left + S(16), midY - S(42), rcEmptyCard.right - S(16), midY - S(12) };
            RECT rcES = { rcEmptyCard.left + S(16), midY - S(8),  rcEmptyCard.right - S(16), midY + S(44) };
            SelectObject(hdc, g_hFontTitle);
            SetTextColor(hdc, COLOR_TEXT_WHITE);
            DrawUiText(hdc, L"Müzik Kitaplığınız Hazır", &rcET, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(hdc, g_hFontSmall);
            SetTextColor(hdc, COLOR_TEXT_MUTED);
            DrawUiText(hdc, L"Üstteki '+ İndir' düğmesiyle şarkı indirebilir, menüden oynatma listesi içe aktarabilir\nveya ses dosyalarını doğrudan bu pencereye sürükleyebilirsiniz.", &rcES, DT_CENTER | DT_TOP | DT_WORDBREAK);
        }
    } else {
        int scrollPx = (int)(g_scrollPosPx + 0.5f);
        int firstIdx = scrollPx / itemH;
        if (firstIdx < 0) firstIdx = 0;
        int padX = S(12);
        int rowGap = S(4);
        DWORD tickNow = GetTickCount();

        for (int displayIdx = firstIdx; displayIdx < totalCount; displayIdx++) {
            int itemY = listTop + S(6) + displayIdx * itemH - scrollPx;
            if (itemY >= listBottom) break;
            if (itemY + itemH < listTop) continue;

            int actualIdx = g_isFiltered ? g_filterIndices[displayIdx] : displayIdx;
            RECT rcRow = { padX, itemY, w - S(18), itemY + itemH - rowGap };

            BOOL isPlayingTrack = (actualIdx == g_currentTrack);
            BOOL isSelected     = (actualIdx == g_selectedTrack);
            BOOL isPressedRow   = (actualIdx == g_pressedTrack && !g_isDraggingList);
            BOOL isHovered      = (actualIdx == g_hoverTrack && g_sidebarAnim < 0.08f);

            RECT rcRowDraw = rcRow;
            if (isPressedRow) InflateRect(&rcRowDraw, -2, -1);

            if (isPlayingTrack) {
                DrawSmoothRoundRect(&rcRowDraw, S(11), isPressedRow ? RGB(34, 20, 38) : COLOR_ROW_ACTIVE, 255, COLOR_ACCENT_PINK, 150);
            } else if (isPressedRow) {
                DrawSmoothRoundRect(&rcRowDraw, S(11), COLOR_CTRL_PRESS, 255, COLOR_ACCENT_ORANGE, 150);
            } else if (isHovered) {
                DrawSmoothRoundRect(&rcRowDraw, S(11), COLOR_ROW_HOVER, 255, COLOR_BORDER, 120);
            } else if (isSelected) {
                DrawSmoothRoundRect(&rcRowDraw, S(11), COLOR_ROW_SEL, 200, 0, 0);
            }

            if (g_isDraggingList && !g_isFiltered && displayIdx == g_dragTargetIndex) {
                DrawHairlineH(rcRow.left + S(8), rcRow.right - S(8), rcRow.top, COLOR_ACCENT_ORANGE, 255);
                DrawHairlineH(rcRow.left + S(8), rcRow.right - S(8), rcRow.top + 1, COLOR_ACCENT_ORANGE, 255);
            }

            const PlaylistItem *item = Playlist_Get(actualIdx);
            if (!item) continue;

            wchar_t artistStr[128] = {0};
            wchar_t songStr[160]   = {0};
            Playlist_SplitArtistSong(item->title, artistStr, 128, songStr, 160);
            if (!artistStr[0]) wcscpy(artistStr, L"Yerel Parça");

            // Sol Numara / Oynat Sütunu
            RECT rcNum = { rcRow.left + S(6), rcRow.top, rcRow.left + S(32), rcRow.bottom };
            if (isHovered || isPressedRow) {
                SetBkMode(hdc, TRANSPARENT);
                SelectObject(hdc, g_hFontIconsSmall);
                if (isPlayingTrack && Player_IsPlaying()) {
                    SetTextColor(hdc, COLOR_ACCENT_PINK);
                    DrawUiText(hdc, L"\uE769", &rcNum, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                } else {
                    SetTextColor(hdc, COLOR_TEXT_WHITE);
                    DrawUiText(hdc, L"\uE768", &rcNum, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                }
            } else {
                wchar_t numBuf[16];
                _snwprintf(numBuf, 15, L"%d", displayIdx + 1);
                numBuf[15] = L'\0';
                SetBkMode(hdc, TRANSPARENT);
                SelectObject(hdc, isPlayingTrack ? g_hFontBold : g_hFontSmall);
                SetTextColor(hdc, isPlayingTrack ? COLOR_ACCENT_ORANGE : COLOR_TEXT_DIM);
                DrawUiText(hdc, numBuf, &rcNum, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }

            // İnteraktif Durum Noktası (Çalan şarkıda dinamik neon haleli, fareyle parlayan, beklemede minimal)
            int midRowY  = (rcRow.top + rcRow.bottom) / 2;
            float dotCx  = (float)(rcRow.left + S(42));
            float dotCy  = (float)midRowY;

            if (isPlayingTrack) {
                float phase = fmodf((float)tickNow * 0.002f, 3.0f);
                COLORREF liveDotCol;
                if (phase < 1.0f) {
                    float t = phase;
                    liveDotCol = RGB(
                        (BYTE)(255 * (1.0f - t) + 240 * t),
                        (BYTE)(146 * (1.0f - t) +  68 * t),
                        (BYTE)( 92 * (1.0f - t) + 184 * t));
                } else if (phase < 2.0f) {
                    float t = phase - 1.0f;
                    liveDotCol = RGB(
                        (BYTE)(240 * (1.0f - t) + 139 * t),
                        (BYTE)( 68 * (1.0f - t) +  53 * t),
                        (BYTE)(184 * (1.0f - t) + 255 * t));
                } else {
                    float t = phase - 2.0f;
                    liveDotCol = RGB(
                        (BYTE)(139 * (1.0f - t) + 255 * t),
                        (BYTE)( 53 * (1.0f - t) + 146 * t),
                        (BYTE)(255 * (1.0f - t) +  92 * t));
                }

                if (Player_IsPlaying()) {
                    float pulse = 0.5f + 0.5f * sinf((float)tickNow * 0.007f);
                    float haloR = SF(6.5f) + pulse * SF(2.5f);
                    BYTE haloA = (BYTE)(70 + 45 * pulse);
                    DrawSmoothCircle(dotCx, dotCy, haloR, liveDotCol, haloA);
                    DrawSmoothCircle(dotCx, dotCy, SF(4.0f), liveDotCol, 255);
                    DrawSmoothCircle(dotCx, dotCy, SF(1.8f), RGB(255, 255, 255), 240);
                } else {
                    DrawSmoothCircle(dotCx, dotCy, SF(5.5f), COLOR_ACCENT_PINK, 65);
                    DrawSmoothCircle(dotCx, dotCy, SF(3.5f), COLOR_ACCENT_ORANGE, 255);
                    DrawSmoothCircle(dotCx, dotCy, SF(1.5f), RGB(255, 255, 255), 200);
                }
            } else if (isHovered || isPressedRow) {
                DrawSmoothCircle(dotCx, dotCy, SF(5.5f), COLOR_ACCENT_ORANGE, 80);
                DrawSmoothCircle(dotCx, dotCy, SF(3.5f), COLOR_TEXT_WHITE, 245);
            } else {
                DrawSmoothCircle(dotCx, dotCy, SF(2.5f), RGB(60, 60, 70), 180);
            }

            // Sağ Durum Rozeti (Çalan satırda)
            int textRight = rcRow.right - S(12);
            if (isPlayingTrack && w >= S(460)) {
                RECT rcPill = { rcRow.right - S(76), midRowY - S(11), rcRow.right - S(10), midRowY + S(11) };
                DrawSmoothRoundRect(&rcPill, S(11), RGB(32, 18, 30), 255, COLOR_ACCENT_PINK, 160);
                SelectObject(hdc, g_hFontMicro);
                SetTextColor(hdc, COLOR_ACCENT_ORANGE);
                DrawUiText(hdc, Player_IsPlaying() ? L"ÇALIYOR" : L"DURAKLAT", &rcPill, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                textRight = rcPill.left - S(10);
            }

            // İki Satırlı Modern Tipografi: Üstte Şarkı (Kalın), Altta Sanatçı (Yumuşak Gri)
            int textLeft = rcRow.left + S(56);
            RECT rcSong   = { textLeft, midRowY - S(19), textRight, midRowY + S(1) };
            RECT rcArtist = { textLeft, midRowY + S(1),  textRight, midRowY + S(19) };

            SelectObject(hdc, g_hFontBold);
            SetTextColor(hdc, isPlayingTrack ? COLOR_ACCENT_ORANGE : COLOR_TEXT_WHITE);
            DrawUiText(hdc, songStr, &rcSong, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

            SelectObject(hdc, g_hFontSmall);
            SetTextColor(hdc, isHovered ? COLOR_TEXT_WHITE : COLOR_TEXT_MUTED);
            DrawUiText(hdc, artistStr, &rcArtist, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        }

        // İnce Modern Kaydırma Çubuğu (Scrollbar)
        int maxScrollPx = GetMaxScrollPx(g_hWnd);
        if (maxScrollPx > 0) {
            int sbW = (g_scrollbarHover || g_isDraggingScrollbar) ? S(7) : S(4);
            int sbRight = w - S(4);
            int sbTop = listTop + S(6);
            int sbBottom = listBottom - S(6);
            int trackH = sbBottom - sbTop;

            rcScrollbarTrack = (RECT){ w - S(14), sbTop, w, sbBottom };

            if (trackH > S(24)) {
                int viewH = listBottom - listTop;
                int contentH = totalCount * itemH + S(12);
                int thumbH = (int)(((float)viewH / (float)contentH) * (float)trackH);
                if (thumbH < S(30)) thumbH = S(30);
                if (thumbH > trackH) thumbH = trackH;

                float ratio = g_scrollPosPx / (float)maxScrollPx;
                if (ratio < 0.0f) ratio = 0.0f;
                if (ratio > 1.0f) ratio = 1.0f;
                int thumbTop = sbTop + (int)(ratio * (float)(trackH - thumbH));

                rcScrollbarThumb = (RECT){ sbRight - sbW, thumbTop, sbRight, thumbTop + thumbH };
                COLORREF sbCol = g_isDraggingScrollbar ? COLOR_ACCENT_ORANGE : (g_scrollbarHover ? RGB(90, 90, 100) : RGB(46, 46, 52));
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
    // 4. ALT BÖLGE: YÜZEN CAM OYNATICI KARTI (FLOATING PLAYER DOCK)
    // =========================================================================
    rcDock = (RECT){ S(10), h - dockH + S(4), w - S(10), h - S(8) };
    BOOL jamGlow = (jamState == JAM_STATE_ACTIVE);
    DrawSmoothRoundRect(&rcDock, S(16), COLOR_DOCK_SURF, 255,
                        jamGlow ? COLOR_ACCENT_PINK : COLOR_BORDER,
                        jamGlow ? 180 : 160);

    int dockMidY     = (rcDock.top + rcDock.bottom) / 2;
    int ctrlRowY     = rcDock.top + S(31);
    int seekRowY     = rcDock.bottom - S(22);

    // --- ORTA BÖLGE: 2 Katmanlı Oynatma Düğmeleri + İlerleme Çubuğu ---
    int centerX  = w / 2;
    int playRad  = S(19);
    int iconBox  = S(32);

    rcPlay    = (RECT){ centerX - playRad, ctrlRowY - playRad, centerX + playRad, ctrlRowY + playRad };
    rcPrev    = (RECT){ rcPlay.left - S(8) - iconBox,  ctrlRowY - iconBox / 2, rcPlay.left - S(8),  ctrlRowY + iconBox / 2 };
    rcShuffle = (RECT){ rcPrev.left - S(4) - iconBox,  ctrlRowY - iconBox / 2, rcPrev.left - S(4),  ctrlRowY + iconBox / 2 };
    rcNext    = (RECT){ rcPlay.right + S(8),           ctrlRowY - iconBox / 2, rcPlay.right + S(8) + iconBox, ctrlRowY + iconBox / 2 };
    rcRepeat  = (RECT){ rcNext.right + S(4),           ctrlRowY - iconBox / 2, rcNext.right + S(4) + iconBox, ctrlRowY + iconBox / 2 };

    DrawIconCircleBtn(hdc, &rcShuffle, ZONE_SHUFFLE, L"\uE8B1", g_cfg.shuffle ? COLOR_ACCENT_ORANGE : 0, g_cfg.shuffle != 0);
    DrawIconCircleBtn(hdc, &rcPrev,    ZONE_PREV,    L"\uE892", 0, FALSE);

    // Pürüzsüz Anti-Aliased Ana Oynat/Duraklat Dairesi
    {
        BOOL pHot = (g_hotZone == ZONE_PLAY);
        BOOL pPrs = (g_pressedZone == ZONE_PLAY);
        float pRad = (float)(pPrs ? (playRad - 1) : (pHot ? (playRad + 1) : playRad));
        COLORREF pBg = pPrs ? RGB(225, 225, 225) : COLOR_TEXT_WHITE;
        if (pHot) {
            DrawSmoothCircle((float)centerX, (float)ctrlRowY, pRad + 3.0f, COLOR_ACCENT_ORANGE, 70);
        }
        DrawSmoothCircle((float)centerX, (float)ctrlRowY, pRad, pBg, 255);

        SetBkMode(hdc, TRANSPARENT);
        SelectObject(hdc, g_hFontIconsLarge);
        SetTextColor(hdc, RGB(5, 5, 5));
        RECT rcPG = rcPlay;
        if (!Player_IsPlaying()) OffsetRect(&rcPG, S(1), 0);
        DrawUiText(hdc, Player_IsPlaying() ? L"\uE769" : L"\uE768", &rcPG, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    DrawIconCircleBtn(hdc, &rcNext, ZONE_NEXT, L"\uE893", 0, FALSE);
    const wchar_t *repGlyph = (g_cfg.repeat == 2) ? L"\uE8ED" : L"\uE8EE";
    DrawIconCircleBtn(hdc, &rcRepeat, ZONE_REPEAT, repGlyph, (g_cfg.repeat > 0) ? COLOR_ACCENT_ORANGE : 0, g_cfg.repeat > 0);
    if (g_cfg.repeat == 2) {
        RECT rcOne = { rcRepeat.right - S(12), rcRepeat.top + S(1), rcRepeat.right, rcRepeat.top + S(13) };
        SelectObject(hdc, g_hFontMicro);
        SetTextColor(hdc, COLOR_ACCENT_ORANGE);
        DrawUiText(hdc, L"1", &rcOne, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    // Orta Alt: Zaman Damgalı Pürüzsüz İlerleme Çubuğu
    int duration = Player_GetDuration();
    int position = g_isDraggingSeek ? g_seekDragSec : Player_GetPosition();
    if (duration > 0 && position > duration) position = duration;

    int seekHalfW = (w >= S(760)) ? S(140) : S(104);
    int maxSeekHalf = (w / 2) - S(240);
    if (maxSeekHalf < S(56)) maxSeekHalf = S(56);
    if (seekHalfW > maxSeekHalf) seekHalfW = maxSeekHalf;

    BOOL seekActive = (g_hotZone == ZONE_SEEK || g_isDraggingSeek);
    int barH = seekActive ? S(6) : S(4);
    if (barH < 4) barH = 4;

    rcSeekBar = (RECT){ centerX - seekHalfW, seekRowY - barH / 2, centerX + seekHalfW, seekRowY + (barH - barH / 2) };
    rcSeekHit = (RECT){ rcSeekBar.left - S(8), seekRowY - S(11), rcSeekBar.right + S(8), seekRowY + S(11) };

    DrawSmoothRoundRect(&rcSeekBar, barH / 2, seekActive ? COLOR_BAR_HOVER : COLOR_BAR_BG, 255, 0, 0);

    float progRatio = (duration > 0) ? ((float)position / (float)duration) : 0.0f;
    if (progRatio < 0.0f) progRatio = 0.0f;
    if (progRatio > 1.0f) progRatio = 1.0f;
    int fillW = (int)((float)(rcSeekBar.right - rcSeekBar.left) * progRatio);
    if (fillW > 0) {
        RECT rcFill = { rcSeekBar.left, rcSeekBar.top, rcSeekBar.left + fillW, rcSeekBar.bottom };
        DrawGradientRoundRectH(&rcFill, barH / 2, COLOR_ACCENT_ORANGE, COLOR_ACCENT_PINK, COLOR_ACCENT_PURPLE);
    }
    if (seekActive && duration > 0) {
        DrawSmoothCircle((float)(rcSeekBar.left + fillW), (float)seekRowY, (float)S(6), COLOR_TEXT_WHITE, 255);
    }

    wchar_t tPos[16], tDur[16];
    FormatTime(position, tPos);
    FormatTime(duration, tDur);

    RECT rcTimeL = { rcSeekBar.left - S(42), seekRowY - S(9), rcSeekBar.left - S(6), seekRowY + S(9) };
    RECT rcTimeR = { rcSeekBar.right + S(6), seekRowY - S(9), rcSeekBar.right + S(44), seekRowY + S(9) };
    SelectObject(hdc, g_hFontMicro);
    SetTextColor(hdc, COLOR_TEXT_DIM);
    DrawUiText(hdc, tPos, &rcTimeL, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    DrawUiText(hdc, tDur, &rcTimeR, DT_LEFT  | DT_VCENTER | DT_SINGLELINE);

    // --- SOL BÖLGE: Kapak Görseli + Şarkı & Sanatçı ---
    int artSize = S(56);
    int artX    = rcDock.left + S(14);
    int artY    = dockMidY - artSize / 2;
    RECT rcArt  = { artX, artY, artX + artSize, artY + artSize };

    DrawSmoothRoundRect(&rcArt, S(10), RGB(24, 24, 24), 255, COLOR_BORDER, 160);
    if (g_pMainThumb) {
        OLE_XSIZE_HIMETRIC hmW = 0;
        OLE_YSIZE_HIMETRIC hmH = 0;
        g_pMainThumb->lpVtbl->get_Width(g_pMainThumb, &hmW);
        g_pMainThumb->lpVtbl->get_Height(g_pMainThumb, &hmH);
        if (hmW > 0 && hmH > 0) {
            int savedArt = SaveDC(hdc);
            HRGN hRgn = CreateRoundRectRgn(artX + 1, artY + 1, artX + artSize, artY + artSize, S(9), S(9));
            SelectClipRgn(hdc, hRgn);
            int oldStretch = SetStretchBltMode(hdc, HALFTONE);
            SetBrushOrgEx(hdc, 0, 0, NULL);
            g_pMainThumb->lpVtbl->Render(g_pMainThumb, hdc, artX, artY, artSize, artSize, 0, hmH, hmW, -hmH, NULL);
            g_gdiDirty = TRUE;
            SetStretchBltMode(hdc, oldStretch);
            RestoreDC(hdc, savedArt);
            DeleteObject(hRgn);
        }
    } else {
        DrawSmoothCircle((float)(artX + artSize / 2), (float)(artY + artSize / 2), (float)S(16), RGB(32, 32, 32), 255);
        SetBkMode(hdc, TRANSPARENT);
        SelectObject(hdc, g_hFontIcons);
        SetTextColor(hdc, g_currentTrack >= 0 ? COLOR_ACCENT_ORANGE : COLOR_TEXT_DIM);
        DrawUiText(hdc, L"\uEC4F", &rcArt, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    int metaLeft    = rcArt.right + S(12);
    int titleRight  = rcShuffle.left - S(10);
    int artistRight = (rcShuffle.left < rcTimeL.left ? rcShuffle.left : rcTimeL.left) - S(6);
    if (titleRight > metaLeft + S(28)) {
        RECT rcDTitle  = { metaLeft, dockMidY - S(19), titleRight,  dockMidY + S(1) };
        RECT rcDArtist = { metaLeft, dockMidY + S(1),  artistRight, dockMidY + S(19) };

        if (g_currentTrack >= 0 && g_currentTrack < Playlist_Count()) {
            const PlaylistItem *cur = Playlist_Get(g_currentTrack);
            wchar_t dArt[128] = {0}, dSong[160] = {0};
            Playlist_SplitArtistSong(cur->title, dArt, 128, dSong, 160);
            if (!dArt[0]) wcscpy(dArt, Player_IsPlaying() ? L"Şu an çalıyor" : L"Duraklatıldı");

            SelectObject(hdc, g_hFontBold);
            SetTextColor(hdc, COLOR_TEXT_WHITE);
            DrawUiText(hdc, dSong, &rcDTitle, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

            SelectObject(hdc, g_hFontSmall);
            SetTextColor(hdc, COLOR_TEXT_MUTED);
            DrawUiText(hdc, dArt, &rcDArtist, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        } else {
            SelectObject(hdc, g_hFontBold);
            SetTextColor(hdc, COLOR_TEXT_WHITE);
            DrawUiText(hdc, L"Hazır", &rcDTitle, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            SelectObject(hdc, g_hFontSmall);
            SetTextColor(hdc, COLOR_TEXT_DIM);
            DrawUiText(hdc, L"Bir parçaya tıklayın", &rcDArtist, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        }
    }

    // --- SAĞ BÖLGE: Hoparlör (Mute) + Ses Çubuğu ---
    int volW     = S(72);
    int volRight = rcDock.right - S(16);
    int volLeft  = volRight - volW;
    int minRightStart = (rcRepeat.right > rcTimeR.right ? rcRepeat.right : rcTimeR.right) + S(36);
    if (volLeft > minRightStart) {
        BOOL volActive = (g_hotZone == ZONE_VOL || g_isDraggingVol);
        int vBarH = volActive ? S(6) : S(4);
        if (vBarH < 4) vBarH = 4;

        rcVolBar  = (RECT){ volLeft, dockMidY - vBarH / 2, volRight, dockMidY + (vBarH - vBarH / 2) };
        rcVolHit  = (RECT){ volLeft - S(6), dockMidY - S(12), volRight + S(6), dockMidY + S(12) };
        rcSpeaker = (RECT){ volLeft - S(34), dockMidY - S(15), volLeft - S(4), dockMidY + S(15) };

        int curVol = Player_GetVolume();
        const wchar_t *spkGlyph = (curVol == 0) ? L"\uE74F" : (curVol < 50 ? L"\uE993" : L"\uE767");
        DrawIconCircleBtn(hdc, &rcSpeaker, ZONE_SPEAKER, spkGlyph, (curVol == 0) ? COLOR_DANGER : 0, FALSE);

        DrawSmoothRoundRect(&rcVolBar, vBarH / 2, volActive ? COLOR_BAR_HOVER : COLOR_BAR_BG, 255, 0, 0);
        int vFillW = (int)((float)(rcVolBar.right - rcVolBar.left) * ((float)curVol / 100.0f));
        if (vFillW > 0) {
            RECT rcVFill = { rcVolBar.left, rcVolBar.top, rcVolBar.left + vFillW, rcVolBar.bottom };
            DrawGradientRoundRectH(&rcVFill, vBarH / 2, COLOR_ACCENT_ORANGE, COLOR_ACCENT_PINK, COLOR_ACCENT_PURPLE);
        }
        if (volActive) {
            DrawSmoothCircle((float)(rcVolBar.left + vFillW), (float)dockMidY, (float)S(5), COLOR_TEXT_WHITE, 255);
        }
    } else {
        rcVolBar = rcVolHit = rcSpeaker = (RECT){0};
    }

    // =========================================================================
    // 5. AKIŞKAN CAM YAN ÇEKMECE & ARKADAŞLAR / JAM MERKEZİ (APPLE DERİNLİK & YAY FİZİĞİ)
    // =========================================================================
    if (g_sidebarAnim > 0.002f) {
        int sbW  = GetSidebarW();
        float animNorm = (g_sidebarAnim < 0.0f) ? 0.0f : ((g_sidebarAnim > 1.0f) ? 1.0f : g_sidebarAnim);
        int offX = (int)((animNorm - 1.0f) * (float)sbW);
        int sideRight = offX + sbW;

        // Apple Malzeme Derinliği: Arka planı odak için yumuşakça karart (<0.08ms SWAR)
        int scrimLeft = (sideRight > 0) ? sideRight : 0;
        if (scrimLeft < w) {
            DimRectFastPx(scrimLeft, headerH, w - scrimLeft, h - headerH, (BYTE)(animNorm * 118.0f));
        }

        int clipRight = sideRight;
        if (clipRight > w) clipRight = w;
        int savedSide = SaveDC(hdc);
        IntersectClipRect(hdc, 0, headerH, clipRight, h);
        SetPixelClip(0, headerH, clipRight, h);

        // Apple Buzlu Cam Katmanı: Altındaki neon ve metinleri yumuşakça bulanıklaştırıp yarı saydam obsidyen ile kapla
        if (clipRight > 2) {
            BlurFrostedGlassPx(0, headerH, clipRight, h - headerH);
        }
        RECT rcSideGlass = { offX, headerH, sideRight, h };
        DrawSmoothRoundRect(&rcSideGlass, 0, COLOR_SIDEBAR_SURF, 226, 0, 0);
        DrawHairlineV(sideRight - 1, headerH, h, COLOR_BORDER, 235);

        if (g_sidebarView == SIDEBAR_VIEW_MENU) {
            static const struct { const wchar_t *icon; const wchar_t *label; const wchar_t *sub; BOOL accent; } items[] = {
                { L"\uE8B7", L"Müzik Klasörü Seç",             L"Yerel arşivi tara",                 FALSE },
                { L"\uE896", L"Müzik Ara ve İndir",            L"YouTube'dan yüksek kalite",         TRUE  },
                { L"\uE716", L"Arkadaşlar ve Jam",             L"Birlikte canlı dinle",              TRUE  },
                { L"\uE8E5", L"Oynatma Listesini İçe Aktar",   L"Paylaşılan listeyi yükle ve indir", FALSE },
                { L"\uE74E", L"Oynatma Listesini Dışa Aktar",  L"Arkadaşlarınla paylaşmak için",     FALSE },
                { L"\uE74D", L"Listeyi Temizle",               L"Ekranı sıfırla",                    FALSE },
                { L"\uE72C", L"Güncellemeleri Denetle",        L"v" APP_VERSION_STR L" sürümü",       FALSE }
            };

            for (int m = 0; m < 7; m++) {
                int itemTop = headerH + S(14) + m * S(52);
                RECT rcMenu = { offX + S(10), itemTop, offX + sbW - S(10), itemTop + S(46) };
                BOOL isHov = (m == g_sidebarHoverItem);
                BOOL isPrs = (m == g_sidebarPressedItem && !g_isDraggingSidebar);

                if (isPrs) InflateRect(&rcMenu, -2, -1);

                if (isPrs || isHov || items[m].accent) {
                    COLORREF mFill = isPrs ? COLOR_CTRL_PRESS : (isHov ? COLOR_CARD_HOVER : RGB(26, 18, 28));
                    COLORREF mBord = items[m].accent ? COLOR_ACCENT_PINK : (isPrs ? COLOR_ACCENT_ORANGE : COLOR_BORDER);
                    DrawSmoothRoundRect(&rcMenu, S(10), mFill, (isPrs || isHov) ? 255 : 155, mBord, (isPrs || isHov) ? 175 : 85);
                }

                RECT rcIc = { rcMenu.left + S(8), rcMenu.top, rcMenu.left + S(36), rcMenu.bottom };
                SetBkMode(hdc, TRANSPARENT);
                SelectObject(hdc, g_hFontIcons);
                SetTextColor(hdc, items[m].accent ? COLOR_ACCENT_ORANGE : ((isHov || isPrs) ? COLOR_TEXT_WHITE : COLOR_TEXT_MUTED));
                DrawUiText(hdc, items[m].icon, &rcIc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

                int badgeReserve = (m == 2 && Friends_GetPendingCount() > 0) ? S(34) : S(8);
                RECT rcLbl = { rcMenu.left + S(38), rcMenu.top + S(6),  rcMenu.right - badgeReserve, rcMenu.top + S(25) };
                RECT rcSub = { rcMenu.left + S(38), rcMenu.top + S(24), rcMenu.right - badgeReserve, rcMenu.bottom - S(5) };

                SelectObject(hdc, g_hFontBold);
                SetTextColor(hdc, items[m].accent ? COLOR_ACCENT_ORANGE : COLOR_TEXT_WHITE);
                DrawUiText(hdc, items[m].label, &rcLbl, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

                SelectObject(hdc, g_hFontMicro);
                SetTextColor(hdc, COLOR_TEXT_DIM);
                DrawUiText(hdc, items[m].sub, &rcSub, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

                if (m == 2 && Friends_GetPendingCount() > 0) {
                    wchar_t bTxt[16];
                    _snwprintf(bTxt, 15, L"%d", Friends_GetPendingCount());
                    bTxt[15] = L'\0';
                    RECT rcBadge = { rcMenu.right - S(30), rcMenu.top + S(13), rcMenu.right - S(10), rcMenu.bottom - S(13) };
                    DrawSmoothRoundRect(&rcBadge, S(10), COLOR_ACCENT_ORANGE, 255, 0, 0);
                    SelectObject(hdc, g_hFontMicro);
                    SetTextColor(hdc, RGB(5, 5, 5));
                    DrawUiText(hdc, bTxt, &rcBadge, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                }
            }
        }
        else if (g_sidebarView == SIDEBAR_VIEW_FRIENDS) {
            rcFriendsBack = (RECT){ offX + S(12), headerH + S(10), offX + S(42), headerH + S(40) };
            DrawIconCircleBtn(hdc, &rcFriendsBack, ZONE_FRIENDS_BACK, L"\uE72B", 0, FALSE);

            rcFriendsRefresh = (RECT){ offX + sbW - S(42), headerH + S(10), offX + sbW - S(12), headerH + S(40) };
            DrawIconCircleBtn(hdc, &rcFriendsRefresh, ZONE_FRIENDS_REFRESH, L"\uE72C", 0, FALSE);

            RECT rcFHead = { offX + S(46), headerH + S(10), offX + sbW - S(46), headerH + S(40) };
            SelectObject(hdc, g_hFontTitle);
            SetTextColor(hdc, COLOR_TEXT_WHITE);
            DrawUiText(hdc, L"Arkadaşlar & Jam", &rcFHead, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

            RECT rcMyCard = { offX + S(12), headerH + S(46), offX + sbW - S(12), headerH + S(98) };
            DrawSmoothRoundRect(&rcMyCard, S(12), COLOR_CARD_SURF, 255, COLOR_BORDER, 160);

            wchar_t myName[64] = {0}, myTag[32] = {0};
            Friends_GetMyName(myName, 64);
            Friends_GetMyTag(myTag, 32);

            float myAvX = (float)(rcMyCard.left + S(26));
            float myAvY = (float)(rcMyCard.top + rcMyCard.bottom) * 0.5f;
            DrawSmoothCircle(myAvX, myAvY, (float)S(16), RGB(42, 20, 46), 255);
            DrawSmoothCircle(myAvX + (float)S(11), myAvY + (float)S(11), SF(5.0f), COLOR_CARD_SURF, 255);
            DrawSmoothCircle(myAvX + (float)S(11), myAvY + (float)S(11), SF(3.5f), COLOR_ACCENT_PINK, 255);

            RECT rcMyInit = { (int)(myAvX - S(16)), (int)(myAvY - S(16)), (int)(myAvX + S(16)), (int)(myAvY + S(16)) };
            wchar_t myInit[2] = { GetInitialChar(myName[0] ? myName : myTag), L'\0' };
            SelectObject(hdc, g_hFontBold);
            SetTextColor(hdc, COLOR_ACCENT_ORANGE);
            DrawUiText(hdc, myInit, &rcMyInit, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            BOOL recentlyCopied = (g_copiedTagTick != 0 && (GetTickCount() - g_copiedTagTick) < 2000);
            rcCopyTag = (RECT){ rcMyCard.right - S(76), rcMyCard.top + S(12), rcMyCard.right - S(10), rcMyCard.bottom - S(12) };
            DrawSmoothRoundRect(&rcCopyTag, S(8),
                                recentlyCopied ? COLOR_ACCENT_ORANGE : ((g_hotZone == ZONE_COPY_TAG) ? COLOR_CARD_HOVER : RGB(24, 24, 24)),
                                255, COLOR_BORDER, 160);
            SelectObject(hdc, g_hFontMicro);
            SetTextColor(hdc, recentlyCopied ? RGB(5, 5, 5) : COLOR_TEXT_WHITE);
            DrawUiText(hdc, recentlyCopied ? L"✓ Kopyalandı" : L"Kopyala", &rcCopyTag, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            RECT rcMyLbl = { rcMyCard.left + S(50), rcMyCard.top + S(8),  rcCopyTag.left - S(6), rcMyCard.top + S(26) };
            RECT rcMyTag = { rcMyCard.left + S(50), rcMyCard.top + S(25), rcCopyTag.left - S(6), rcMyCard.bottom - S(6) };
            SelectObject(hdc, g_hFontMicro);
            SetTextColor(hdc, COLOR_TEXT_DIM);
            DrawUiText(hdc, L"SENİN ETİKETİN", &rcMyLbl, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            SelectObject(hdc, g_hFontBold);
            SetTextColor(hdc, COLOR_TEXT_WHITE);
            DrawUiText(hdc, myTag, &rcMyTag, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

            RECT rcTabTray = { offX + S(12), headerH + S(106), offX + sbW - S(12), headerH + S(140) };
            DrawSmoothRoundRect(&rcTabTray, S(10), RGB(16, 16, 16), 255, COLOR_BORDER, 180);

            int trayW = (rcTabTray.right - rcTabTray.left - S(8)) / 3;
            rcFriendsTab0 = (RECT){ rcTabTray.left + S(4),              rcTabTray.top + S(3), rcTabTray.left + S(4) + trayW,     rcTabTray.bottom - S(3) };
            rcFriendsTab1 = (RECT){ rcFriendsTab0.right,                rcTabTray.top + S(3), rcFriendsTab0.right + trayW,       rcTabTray.bottom - S(3) };
            rcFriendsTab2 = (RECT){ rcFriendsTab1.right,                rcTabTray.top + S(3), rcTabTray.right - S(4),            rcTabTray.bottom - S(3) };

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
                    DrawSmoothRoundRect(tabRects[t], S(7), COLOR_CARD_HOVER, 255, COLOR_ACCENT_PINK, 150);
                }
                SelectObject(hdc, sel ? g_hFontSmallBold : g_hFontSmall);
                SetTextColor(hdc, sel ? COLOR_ACCENT_ORANGE : COLOR_TEXT_MUTED);
                DrawUiText(hdc, tabLabels[t], (RECT*)tabRects[t], DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }

            int contentTop = headerH + S(150);

            if (g_friendsTab == FRIENDS_TAB_LIST) {
                int fCount = Friends_GetFriendCount();
                for (int k = 0; k < MAX_FRIENDS; k++) {
                    rcFriendActionBtns[k] = (RECT){0};
                    rcFriendRemoveBtns[k] = (RECT){0};
                }

                if (fCount == 0) {
                    RECT rcEmptyF = { offX + S(18), contentTop + S(20), offX + sbW - S(18), contentTop + S(110) };
                    SelectObject(hdc, g_hFontSmall);
                    SetTextColor(hdc, COLOR_TEXT_MUTED);
                    DrawUiText(hdc, L"Henüz ekli arkadaşın yok.\n\n'+ Ekle' sekmesinden arkadaşının etiketini girerek davet gönderebilirsin.", &rcEmptyF, DT_CENTER | DT_TOP | DT_WORDBREAK);
                } else {
                    wchar_t jamPartner[32] = {0};
                    Jam_GetPartnerTag(jamPartner, 32);

                    for (int i = 0; i < fCount; i++) {
                        int cardY = contentTop + i * S(60);
                        if (cardY + S(54) > h - S(8)) break;

                        const FriendItem *fi = Friends_GetFriend(i);
                        if (!fi) continue;

                        BOOL inJamWithFriend = (jamState != JAM_STATE_NONE && _wcsicmp(jamPartner, fi->tag) == 0);
                        RECT rcFC = { offX + S(12), cardY, offX + sbW - S(12), cardY + S(54) };
                        DrawSmoothRoundRect(&rcFC, S(11),
                                            inJamWithFriend ? RGB(28, 16, 32) : COLOR_CARD_SURF, 255,
                                            inJamWithFriend ? COLOR_ACCENT_PINK : COLOR_BORDER, 150);

                        float avX = (float)(rcFC.left + S(24));
                        float avY = (float)(rcFC.top + rcFC.bottom) * 0.5f;
                        DrawSmoothCircle(avX, avY, (float)S(15), GetAvatarJewelColor(fi->name), 255);

                        RECT rcAvTxt = { (int)(avX - S(15)), (int)(avY - S(15)), (int)(avX + S(15)), (int)(avY + S(15)) };
                        wchar_t fInit[2] = { GetInitialChar(fi->name), L'\0' };
                        SelectObject(hdc, g_hFontSmallBold);
                        SetTextColor(hdc, COLOR_TEXT_WHITE);
                        DrawUiText(hdc, fInit, &rcAvTxt, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

                        DrawSmoothCircle(avX + (float)S(11), avY + (float)S(11), SF(5.2f), COLOR_CARD_SURF, 255);
                        DrawSmoothCircle(avX + (float)S(11), avY + (float)S(11), SF(3.8f),
                                         fi->isOnline ? COLOR_ACCENT_ORANGE : RGB(70, 70, 75), 255);

                        rcFriendRemoveBtns[i] = (RECT){ rcFC.right - S(28), cardY + S(15), rcFC.right - S(6), cardY + S(39) };
                        SelectObject(hdc, g_hFontIconsSmall);
                        SetTextColor(hdc, COLOR_TEXT_DIM);
                        DrawUiText(hdc, L"\uE711", &rcFriendRemoveBtns[i], DT_CENTER | DT_VCENTER | DT_SINGLELINE);

                        rcFriendActionBtns[i] = (RECT){ rcFC.right - S(78), cardY + S(13), rcFC.right - S(30), cardY + S(41) };
                        COLORREF actBg = inJamWithFriend ? RGB(56, 18, 28) : (fi->isOnline ? COLOR_ACCENT_ORANGE : RGB(24, 24, 24));
                        DrawSmoothRoundRect(&rcFriendActionBtns[i], S(8), actBg, 255, 0, 0);
                        SelectObject(hdc, g_hFontMicro);
                        SetTextColor(hdc, inJamWithFriend ? RGB(255, 165, 165) : (fi->isOnline ? RGB(5, 5, 5) : COLOR_TEXT_DIM));
                        DrawUiText(hdc, inJamWithFriend ? L"Ayrıl" : L"Jam", &rcFriendActionBtns[i], DT_CENTER | DT_VCENTER | DT_SINGLELINE);

                        RECT rcFN = { rcFC.left + S(46), cardY + S(8),  rcFriendActionBtns[i].left - S(6), cardY + S(27) };
                        RECT rcFS = { rcFC.left + S(46), cardY + S(26), rcFriendActionBtns[i].left - S(6), cardY + S(46) };

                        SelectObject(hdc, g_hFontBold);
                        SetTextColor(hdc, COLOR_TEXT_WHITE);
                        DrawUiText(hdc, fi->name, &rcFN, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

                        wchar_t subLine[160];
                        if (!fi->isOnline) {
                            wcscpy(subLine, L"Çevrimdışı");
                        } else if (fi->isPlaying && fi->currentTrack[0]) {
                            _snwprintf(subLine, 159, L"♫ %s", fi->currentTrack);
                            subLine[159] = L'\0';
                        } else {
                            wcscpy(subLine, L"Çevrimiçi • Boşta");
                        }

                        SelectObject(hdc, g_hFontMicro);
                        SetTextColor(hdc, !fi->isOnline ? COLOR_TEXT_DIM : (fi->isPlaying ? COLOR_ACCENT_PINK : COLOR_TEXT_MUTED));
                        DrawUiText(hdc, subLine, &rcFS, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                    }
                }
            }
            else if (g_friendsTab == FRIENDS_TAB_REQUESTS) {
                int pCount = Friends_GetPendingCount();
                for (int k = 0; k < MAX_REQUESTS; k++) {
                    rcReqAcceptBtns[k] = (RECT){0};
                    rcReqRejectBtns[k] = (RECT){0};
                }

                if (pCount == 0) {
                    RECT rcEmptyR = { offX + S(18), contentTop + S(24), offX + sbW - S(18), contentTop + S(90) };
                    SelectObject(hdc, g_hFontSmall);
                    SetTextColor(hdc, COLOR_TEXT_MUTED);
                    DrawUiText(hdc, L"Bekleyen arkadaşlık isteği yok.", &rcEmptyR, DT_CENTER | DT_TOP | DT_WORDBREAK);
                } else {
                    for (int i = 0; i < pCount; i++) {
                        int cardY = contentTop + i * S(58);
                        if (cardY + S(52) > h - S(8)) break;

                        const FriendRequest *rq = Friends_GetPending(i);
                        if (!rq) continue;

                        RECT rcCard = { offX + S(12), cardY, offX + sbW - S(12), cardY + S(52) };
                        DrawSmoothRoundRect(&rcCard, S(11), COLOR_CARD_SURF, 255, COLOR_BORDER, 140);

                        RECT rcName = { rcCard.left + S(12), cardY + S(7),  rcCard.right - S(74), cardY + S(26) };
                        RECT rcTag  = { rcCard.left + S(12), cardY + S(25), rcCard.right - S(74), cardY + S(44) };

                        SelectObject(hdc, g_hFontBold);
                        SetTextColor(hdc, COLOR_TEXT_WHITE);
                        DrawUiText(hdc, rq->name, &rcName, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

                        SelectObject(hdc, g_hFontMicro);
                        SetTextColor(hdc, COLOR_TEXT_MUTED);
                        DrawUiText(hdc, rq->tag, &rcTag, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

                        rcReqAcceptBtns[i] = (RECT){ rcCard.right - S(68), cardY + S(11), rcCard.right - S(38), cardY + S(41) };
                        DrawSmoothRoundRect(&rcReqAcceptBtns[i], S(8), COLOR_ACCENT_ORANGE, 255, 0, 0);
                        SelectObject(hdc, g_hFontIconsSmall);
                        SetTextColor(hdc, RGB(5, 5, 5));
                        DrawUiText(hdc, L"\uE73E", &rcReqAcceptBtns[i], DT_CENTER | DT_VCENTER | DT_SINGLELINE);

                        rcReqRejectBtns[i] = (RECT){ rcCard.right - S(34), cardY + S(11), rcCard.right - S(6), cardY + S(41) };
                        DrawSmoothRoundRect(&rcReqRejectBtns[i], S(8), COLOR_CTRL_HOVER, 255, COLOR_BORDER, 140);
                        SelectObject(hdc, g_hFontIconsSmall);
                        SetTextColor(hdc, COLOR_TEXT_MUTED);
                        DrawUiText(hdc, L"\uE711", &rcReqRejectBtns[i], DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                    }
                }
            }
            else if (g_friendsTab == FRIENDS_TAB_ADD) {
                RECT rcLbl = { offX + S(14), contentTop + S(2),  offX + sbW - S(14), contentTop + S(22) };
                RECT rcSub = { offX + S(14), contentTop + S(22), offX + sbW - S(14), contentTop + S(42) };
                SelectObject(hdc, g_hFontBold);
                SetTextColor(hdc, COLOR_TEXT_WHITE);
                DrawUiText(hdc, L"Etiket ile Arkadaş Ekle", &rcLbl, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

                SelectObject(hdc, g_hFontSmall);
                SetTextColor(hdc, COLOR_TEXT_MUTED);
                DrawUiText(hdc, L"Arkadaşının etiketini gir (Örn: Selim#8421)", &rcSub, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

                RECT rcInputBox = { offX + S(12), headerH + S(204), offX + sbW - S(12), headerH + S(242) };
                BOOL addFocused = (GetFocus() == g_hFriendAddEdit);
                DrawSmoothRoundRect(&rcInputBox, S(10), COLOR_INPUT_BG, 255,
                                    addFocused ? COLOR_ACCENT_ORANGE : COLOR_BORDER, addFocused ? 220 : 150);

                rcFriendSendBtn = (RECT){ offX + S(12), headerH + S(252), offX + sbW - S(12), headerH + S(290) };
                COLORREF btnCol = (g_pressedZone == ZONE_FRIEND_SEND) ? COLOR_ACCENT_PURPLE :
                                  ((g_hotZone == ZONE_FRIEND_SEND) ? COLOR_ACCENT_PINK : COLOR_ACCENT_ORANGE);
                DrawSmoothRoundRect(&rcFriendSendBtn, S(10), btnCol, 255, 0, 0);
                SelectObject(hdc, g_hFontBold);
                SetTextColor(hdc, RGB(5, 5, 5));
                DrawUiText(hdc, L"Arkadaşlık İsteği Gönder", &rcFriendSendBtn, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

                if (g_friendAddMsg[0]) {
                    RECT rcMsg = { offX + S(14), headerH + S(300), offX + sbW - S(14), headerH + S(356) };
                    SelectObject(hdc, g_hFontSmall);
                    SetTextColor(hdc, wcsstr(g_friendAddMsg, L"gönderildi") ? COLOR_ACCENT_ORANGE : COLOR_DANGER);
                    DrawUiText(hdc, g_friendAddMsg, &rcMsg, DT_LEFT | DT_WORDBREAK);
                }
            }
        }

        RestoreDC(hdc, savedSide);
        SetBkMode(hdc, TRANSPARENT);
        ResetPixelClip();
    }

    // =========================================================================
    // 6. GELEN CANLI JAM DAVETİ YÜZEN BİLDİRİMİ (FLOATING INVITE BANNER)
    // =========================================================================
    if (Jam_HasPendingInvite()) {
        wchar_t invName[64] = {0}, invTag[32] = {0};
        Jam_GetPendingInviteInfo(invName, invTag);

        int bannerW = S(390);
        if (bannerW > w - S(24)) bannerW = w - S(24);
        int topY = headerH + S(10);
        rcToastBox = (RECT){ (w - bannerW) / 2, topY, (w + bannerW) / 2, topY + S(54) };
        DrawSmoothRoundRect(&rcToastBox, S(14), RGB(26, 14, 30), 250, COLOR_ACCENT_PINK, 230);

        DrawSmoothCircle((float)(rcToastBox.left + S(20)), (float)(topY + S(27)), (float)S(5), COLOR_ACCENT_ORANGE, 255);

        wchar_t toastTitle[128];
        _snwprintf(toastTitle, 127, L"%s seni Canlı Jam'e davet etti!", invName[0] ? invName : invTag);
        toastTitle[127] = L'\0';

        rcToastAccept = (RECT){ rcToastBox.right - S(104), rcToastBox.top + S(11), rcToastBox.right - S(42), rcToastBox.bottom - S(11) };
        DrawGradientRoundRectH(&rcToastAccept, S(9), COLOR_ACCENT_ORANGE, COLOR_ACCENT_PINK, COLOR_ACCENT_PURPLE);
        SelectObject(hdc, g_hFontSmallBold);
        SetTextColor(hdc, RGB(5, 5, 5));
        DrawUiText(hdc, L"Katıl", &rcToastAccept, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        rcToastReject = (RECT){ rcToastBox.right - S(36), rcToastBox.top + S(11), rcToastBox.right - S(8), rcToastBox.bottom - S(11) };
        DrawSmoothRoundRect(&rcToastReject, S(9), COLOR_CTRL_HOVER, 255, COLOR_BORDER, 140);
        SelectObject(hdc, g_hFontIconsSmall);
        SetTextColor(hdc, COLOR_TEXT_MUTED);
        DrawUiText(hdc, L"\uE711", &rcToastReject, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        RECT rcToastTxt = { rcToastBox.left + S(34), rcToastBox.top, rcToastAccept.left - S(8), rcToastBox.bottom };
        SelectObject(hdc, g_hFontBold);
        SetTextColor(hdc, COLOR_TEXT_WHITE);
        DrawUiText(hdc, toastTitle, &rcToastTxt, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    } else {
        rcToastBox = rcToastAccept = rcToastReject = (RECT){0};
    }
}

static UiZone HitTestZones(POINT pt) {
    if (Jam_HasPendingInvite()) {
        if (PtInRect(&rcToastAccept, pt)) return ZONE_TOAST_ACCEPT;
        if (PtInRect(&rcToastReject, pt)) return ZONE_TOAST_REJECT;
    }
    if (PtInRect(&rcHamburger, pt)) return ZONE_HAMBURGER;
    if (rcHeaderFriends.right > 0 && PtInRect(&rcHeaderFriends, pt)) return ZONE_HEADER_FRIENDS;
    if (g_searchQuery[0] && PtInRect(&rcSearchClear, pt)) return ZONE_SEARCH_CLEAR;
    if (rcQuickDownload.right > 0 && PtInRect(&rcQuickDownload, pt)) return ZONE_QUICK_DOWNLOAD;

    int sideRight = GetCurrentSidebarRight();
    if (sideRight > S(12) && pt.y >= GetHeaderH()) {
        if (pt.x < sideRight) {
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
        if (g_sidebarOpen || g_sidebarAnim > 0.25f) return ZONE_NONE;
    }

    if (Jam_GetState() != JAM_STATE_NONE) {
        if (rcJamLeave.right > 0 && PtInRect(&rcJamLeave, pt)) return ZONE_JAM_LEAVE;
        if (rcJamMissingBtn.right > 0 && PtInRect(&rcJamMissingBtn, pt)) return ZONE_JAM_MISSING_DL;
    }
    if ((g_dlMode != DL_BANNER_NONE || g_dlBannerAnim > 0.1f) && rcDlCancel.right > 0 && PtInRect(&rcDlCancel, pt)) {
        return ZONE_DL_CANCEL;
    }
    if (PtInRect(&rcPlay, pt))    return ZONE_PLAY;
    if (PtInRect(&rcPrev, pt))    return ZONE_PREV;
    if (PtInRect(&rcNext, pt))    return ZONE_NEXT;
    if (PtInRect(&rcShuffle, pt)) return ZONE_SHUFFLE;
    if (PtInRect(&rcRepeat, pt))  return ZONE_REPEAT;
    if (rcSpeaker.right > 0 && PtInRect(&rcSpeaker, pt)) return ZONE_SPEAKER;
    if (PtInRect(&rcSeekHit, pt)) return ZONE_SEEK;
    if (rcVolHit.right > 0 && PtInRect(&rcVolHit, pt)) return ZONE_VOL;
    return ZONE_NONE;
}

// =============================================================================
// GİRİŞ KUTUSU ALT SINIFLAMA VE PENCERE OLAY DÖNGÜSÜ
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
                DrawTextW(hdc, L"Parça ara...", -1, &rc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
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
        if (g_hWnd) {
        InvalidateRect(g_hWnd, NULL, FALSE);
        UpdateTimerSchedule(g_hWnd);
    }
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
    int searchW  = (clientW >= S(560)) ? S(190) : S(148);
    int searchH  = S(34);
    int searchX  = clientW - searchW - S(12);
    int searchY  = (headerH - searchH) / 2;
    int rightPad = g_searchQuery[0] ? S(30) : S(12);
    if (g_hSearchEdit) {
        MoveWindow(g_hSearchEdit, searchX + S(28), searchY + S(7), searchW - S(28) - rightPad, searchH - S(14), TRUE);
    }
    UpdateSidebarEditPosition();
}

static int HitTestTrackRowIndex(HWND hWnd, POINT pt, int *outDisplayIdx) {
    RECT rcClient;
    GetClientRect(hWnd, &rcClient);
    int listTop    = GetTracklistTop() + S(26);
    int listBottom = rcClient.bottom - GetDockH();
    if (pt.y < listTop || pt.y >= listBottom || pt.x < S(12) || pt.x > rcClient.right - S(18)) {
        return -1;
    }
    int displayIdx = (int)(((float)(pt.y - (listTop + S(6))) + g_scrollPosPx) / (float)GetItemH());
    int totalCount = g_isFiltered ? g_filterCount : Playlist_Count();
    if (displayIdx < 0 || displayIdx >= totalCount) return -1;
    if (outDisplayIdx) *outDisplayIdx = displayIdx;
    return g_isFiltered ? g_filterIndices[displayIdx] : displayIdx;
}

static LRESULT CALLBACK MainWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            g_hWnd = hWnd;
            InitializeCriticalSection(&g_plDlCs);
            g_plDlCsReady = TRUE;

            // Koyu Başlık Çubuğu & Windows 11 Yuvarlatılmış Köşe Desteği
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
            SendMessageW(g_hSearchEdit, EM_SETCUEBANNER, TRUE, (LPARAM)L"Parça ara...");
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
            Jam_Init();

            Config_Load(&g_cfg, L"config.json");
            Player_SetVolume(g_cfg.volume);
            if (g_cfg.volume > 0) g_preMuteVol = g_cfg.volume;

            DragAcceptFiles(hWnd, TRUE);

            // Pencere anında (<5ms) ekrana çizilsin; klasör tarama ve ağ başlatma hemen ardından çalışsın!
            PostMessageW(hWnd, WM_APP_POST_INIT, 0, 0);
            return 0;
        }

        case WM_APP_POST_INIT: {
            Friends_Init();
            if (Friends_GetFriendCount() > 0) {
                EnsureSocialNetworkStarted();
            }

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

            // Komut satırından paylaşılan bir oynatma listesi (.m3u8/.m3u) veya ses dosyası ile açıldıysa yükle
            int argc = 0;
            LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
            if (argv) {
                for (int i = 1; i < argc; i++) {
                    DWORD attr = GetFileAttributesW(argv[i]);
                    if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
                        const wchar_t *ext = PathFindExtensionW(argv[i]);
                        if (_wcsicmp(ext, L".m3u") == 0 || _wcsicmp(ext, L".m3u8") == 0) {
                            HandleSmartPlaylistImport(argv[i]);
                        } else {
                            Playlist_Add(argv[i]);
                        }
                    }
                }
                LocalFree(argv);
            }

            UpdateTimerSchedule(hWnd);
            InvalidateRect(hWnd, NULL, FALSE);

            Updater_Init();
            Updater_CheckAsync(hWnd, FALSE);

            // Tek seferlik başlangıç bellek temizliği (soğuk açılış DLL kalıntılarını bırakır, menüleri asla yavaşlatmaz)
            SetTimer(hWnd, 1003, 1800, NULL);
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
            g_isWindowActive = (LOWORD(wParam) != WA_INACTIVE);
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
                UpdateTimerSchedule(hWnd);
                int w = LOWORD(lParam);
                ClampScrollTargets(hWnd);
                LayoutHeaderInputs(w);
                InvalidateRect(hWnd, NULL, FALSE);
            }
            break;
        }

        case WM_TIMER: {
            if (wParam == 1003) {
                if (!g_animTimerActive && !g_isDraggingSidebar) {
                    KillTimer(hWnd, 1003);
                    SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
                }
                return 0;
            }
            if (wParam == IDT_ANIM_TIMER) {
                DWORD now = GetTickCount();
                float dt = (float)(now - g_lastAnimTick) * 0.001f;
                g_lastAnimTick = now;
                if (dt <= 0.0f || dt > 0.05f) dt = 0.012f;

                float jamTarget = (Jam_GetState() != JAM_STATE_NONE) ? 1.0f : 0.0f;
                float dlTarget  = (g_dlMode != DL_BANNER_NONE) ? 1.0f : 0.0f;

                BOOL mScroll  = StepSpring(&g_scrollPosPx,   &g_scrollVelPx,   g_scrollTargetPx, 26.0f, 1.0f, dt, 0.25f);
                BOOL mSidebar = FALSE;
                if (!g_isDraggingSidebar) {
                    mSidebar = StepSpring(&g_sidebarAnim, &g_sidebarVel, g_sidebarOpen ? 1.0f : 0.0f, 34.0f, 1.0f, dt, 0.003f);
                    if (g_sidebarAnim >= 1.0f) {
                        g_sidebarAnim = 1.0f;
                        if (g_sidebarOpen) { g_sidebarVel = 0.0f; mSidebar = FALSE; }
                    } else if (g_sidebarAnim <= 0.0f) {
                        g_sidebarAnim = 0.0f;
                        if (!g_sidebarOpen) { g_sidebarVel = 0.0f; mSidebar = FALSE; }
                    }
                }
                BOOL mJamBar = StepSpring(&g_jamBannerAnim, &g_jamBannerVel, jamTarget, 28.0f, 1.0f, dt, 0.002f);
                BOOL mDlBar  = StepSpring(&g_dlBannerAnim,  &g_dlBannerVel,  dlTarget,  28.0f, 1.0f, dt, 0.002f);

                if (jamTarget >= 0.99f && g_jamBannerAnim >= 1.0f) { g_jamBannerAnim = 1.0f; g_jamBannerVel = 0.0f; mJamBar = FALSE; }
                else if (jamTarget <= 0.01f && g_jamBannerAnim <= 0.0f) { g_jamBannerAnim = 0.0f; g_jamBannerVel = 0.0f; mJamBar = FALSE; }

                if (dlTarget >= 0.99f && g_dlBannerAnim >= 1.0f) { g_dlBannerAnim = 1.0f; g_dlBannerVel = 0.0f; mDlBar = FALSE; }
                else if (dlTarget <= 0.01f && g_dlBannerAnim <= 0.0f) { g_dlBannerAnim = 0.0f; g_dlBannerVel = 0.0f; mDlBar = FALSE; }

                UpdateSidebarEditPosition();

                RECT rcClient;
                GetClientRect(hWnd, &rcClient);
                RECT rcBody = { 0, GetHeaderH(), rcClient.right, rcClient.bottom };
                InvalidateRect(hWnd, &rcBody, FALSE);

                if (!mScroll && !mSidebar && !mJamBar && !mDlBar && !g_isDraggingSidebar) {
                    KillTimer(hWnd, IDT_ANIM_TIMER);
                    g_animTimerActive = FALSE;
                }
                return 0;
            }

            if (wParam == IDT_UPDATE_TIMER) {
                DWORD now = GetTickCount();

                if (g_dlFinishHideTick != 0 && (int)(now - g_dlFinishHideTick) >= 0) {
                    g_dlFinishHideTick = 0;
                    g_dlMode = DL_BANNER_NONE;
                    StartAnimLoop(hWnd);
                    UpdateTimerSchedule(hWnd);
                }

                // SADECE Arkadaşlar sekmesi açıkken VEYA Jam aktifken ağ sorgusu yap! Boşta %0 ağ ve işlemci yükü!
                if (IsSocialRefreshActive()) {
                    Jam_CheckTimeouts();
                    Friends_CheckTimeouts();

                    if (now - g_lastFriendsRefreshTick >= 4000) {
                        g_lastFriendsRefreshTick = now;
                        Friends_RequestAllStatuses();
                    }

                    if (Jam_GetState() == JAM_STATE_NONE && g_jamBannerAnim > 0.01f) {
                        StartAnimLoop(hWnd);
                    }
                    if (g_isWindowActive && !g_isMinimized) {
                        InvalidateRect(hWnd, NULL, FALSE);
                    }
                } else if (Jam_HasPendingInvite()) {
                    if (g_isWindowActive && !g_isMinimized && rcToastBox.right > 0) {
                        InvalidateRect(hWnd, &rcToastBox, FALSE);
                    }
                }

                // Şarkı çalıyorsa alt oynatıcı kartını yenile
                if (g_isWindowActive && !g_isMinimized) {
                    if (Player_IsPlaying() && !g_isDraggingSeek && rcDock.right > 0) {
                        InvalidateRect(hWnd, &rcDock, FALSE);
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
                InvalidateRect(hWnd, NULL, FALSE);
            }
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
                    JamState beforeState = Jam_GetState();

                    Friends_HandleNetworkMessage(evt->topic, evt->payload);
                    Jam_HandleNetworkMessage(evt->topic, evt->payload);

                    if (Jam_PollNeedSendCurrentState()) {
                        BroadcastCurrentJamTrack(Player_GetPosition());
                    }

                    JamRemoteSync rs;
                    if (Jam_PollRemoteSync(&rs)) {
                        ApplyRemoteJamSync(&rs);
                    }

                    if (Jam_GetState() != beforeState) {
                        StartAnimLoop(hWnd);
                    }
                }
                free(evt);
                InvalidateRect(hWnd, NULL, FALSE);
            }
            return 0;
        }

        case WM_APP_JAM_AUTODL_PROG: {
            int pct = (int)wParam;
            if (pct < 0) pct = 0;
            if (pct > 100) pct = 100;
            g_jamAutoDlPct = pct;
            if (Jam_IsActive() && Jam_IsDownloadingTrack()) {
                wchar_t msgBuf[128];
                _snwprintf(msgBuf, 127, L"⚡ Ortak şarkı otomatik indiriliyor (%%%d)...", pct);
                msgBuf[127] = L'\0';
                Jam_SetSyncStatus(msgBuf, TRUE, NULL);
                if (rcJamStage.bottom > rcJamStage.top) {
                    InvalidateRect(hWnd, &rcJamStage, FALSE);
                } else {
                    InvalidateRect(hWnd, NULL, FALSE);
                }
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
                    Jam_SetSyncStatus(L"⚠ Otomatik indirilemedi • Sağdaki 'İndir' düğmesini kullan", FALSE, task->title);
                }
                free(task);
            }
            StartAnimLoop(hWnd);
            InvalidateRect(hWnd, NULL, FALSE);
            return 0;
        }

        case WM_APP_SINGLE_DL_PROGRESS: {
            int pct = (int)wParam;
            if (pct < 0) {
                if (g_dlMode == DL_BANNER_SINGLE && g_dlFinishHideTick == 0) {
                    g_dlMode = DL_BANNER_NONE;
                    StartAnimLoop(hWnd);
                }
                return 0;
            }
            if (g_dlMode != DL_BANNER_PLAYLIST && g_dlMode != DL_BANNER_UPDATE) {
                BOOL wasHidden = (g_dlMode == DL_BANNER_NONE);
                g_dlMode = DL_BANNER_SINGLE;
                g_dlFinishHideTick = 0;
                g_dlPercent = pct;
                wcscpy(g_dlHeadline, L"MÜZİK İNDİRİLİYOR");
                Search_GetActiveDownloadTitle(g_dlSubline, 256);
                if (!g_dlSubline[0]) wcscpy(g_dlSubline, L"Yüksek kaliteli ses dosyası hazırlanıyor...");
                if (wasHidden) StartAnimLoop(hWnd);
                if (rcDlStage.bottom > rcDlStage.top) {
                    InvalidateRect(hWnd, &rcDlStage, FALSE);
                } else {
                    InvalidateRect(hWnd, NULL, FALSE);
                }
            }
            return 0;
        }

        case WM_APP_PL_DL_PROGRESS: {
            int pct = (int)wParam;
            MissingPlaylistTrack *started = (MissingPlaylistTrack*)lParam;
            if (started) {
                wcsncpy(g_dlSubline, started->title, 255);
                g_dlSubline[255] = L'\0';
                free(started);
            }
            int curNum = g_dlDoneCount + 1;
            if (curNum > g_dlTotalCount && g_dlTotalCount > 0) curNum = g_dlTotalCount;
            g_dlMode = DL_BANNER_PLAYLIST;
            g_dlFinishHideTick = 0;
            g_dlPercent = pct;
            _snwprintf(g_dlHeadline, 159, L"PAYLAŞILAN LİSTE YÜKLENİYOR  •  %d / %d Parça", curNum, g_dlTotalCount);
            g_dlHeadline[159] = L'\0';
            StartAnimLoop(hWnd);
            if (rcDlStage.bottom > rcDlStage.top) {
                InvalidateRect(hWnd, &rcDlStage, FALSE);
            } else {
                InvalidateRect(hWnd, NULL, FALSE);
            }
            return 0;
        }

        case WM_APP_PL_DL_TRACK_DONE: {
            BOOL ok = (BOOL)wParam;
            if (ok) {
                g_dlDoneCount++;
                if (g_cfg.lastFolder[0]) {
                    Playlist_ScanFolder(g_cfg.lastFolder);
                    if (g_selectedTrack < 0 && Playlist_Count() > 0) g_selectedTrack = 0;
                    ApplyFilter(g_searchQuery);
                    ClampScrollTargets(hWnd);
                    InvalidateRect(hWnd, NULL, FALSE);
                }
            }
            return 0;
        }

        case WM_APP_PL_DL_ALL_DONE: {
            int okCount = (int)wParam;
            BOOL cancelled = (BOOL)lParam;
            if (g_cfg.lastFolder[0]) {
                Playlist_ScanFolder(g_cfg.lastFolder);
                if (g_selectedTrack < 0 && Playlist_Count() > 0) g_selectedTrack = 0;
                ApplyFilter(g_searchQuery);
            }
            g_dlMode = DL_BANNER_PLAYLIST;
            if (cancelled) {
                wcscpy(g_dlHeadline, L"PAYLAŞILAN LİSTE İNDİRMESİ DURDURULDU");
                _snwprintf(g_dlSubline, 255, L"%d parça kitaplığınıza eklendi", okCount);
            } else {
                g_dlPercent = 100;
                wcscpy(g_dlHeadline, L"✓ PAYLAŞILAN OYNATMA LİSTESİ HAZIR (%100)");
                _snwprintf(g_dlSubline, 255, L"%d eksik parça otomatik indirildi ve listeye eklendi", okCount);
            }
            g_dlSubline[255] = L'\0';
            g_dlFinishHideTick = GetTickCount() + 4000;
            StartAnimLoop(hWnd);
            UpdateTimerSchedule(hWnd);
            InvalidateRect(hWnd, NULL, FALSE);
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

                    if (g_dlMode == DL_BANNER_SINGLE || g_dlMode == DL_BANNER_NONE) {
                        g_dlMode = DL_BANNER_SINGLE;
                        g_dlPercent = 100;
                        wcscpy(g_dlHeadline, L"✓ MÜZİK İNDİRİLDİ (%100)");
                        const PlaylistItem *topIt = Playlist_Get(0);
                        if (topIt) {
                            wcsncpy(g_dlSubline, topIt->title, 255);
                            g_dlSubline[255] = L'\0';
                        }
                        g_dlFinishHideTick = GetTickCount() + 2600;
                        UpdateTimerSchedule(hWnd);
                    }
                    StartAnimLoop(hWnd);

                    // Jam sırasında eksik şarkıyı manuel indirdiyse hemen senkronla!
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

        case WM_APP_UPDATE_PROGRESS: {
            int pct = (int)wParam;
            if (pct < 0) pct = 0;
            if (pct > 100) pct = 100;
            g_dlMode = DL_BANNER_UPDATE;
            g_dlFinishHideTick = 0;
            g_dlPercent = pct;
            wcscpy(g_dlHeadline, L"\u039BURNS GÜNCELLENİYOR");
            wcscpy(g_dlSubline, L"Yeni sürüm paketi indiriliyor, lütfen bekleyin...");
            StartAnimLoop(hWnd);
            if (rcDlStage.bottom > rcDlStage.top) {
                InvalidateRect(hWnd, &rcDlStage, FALSE);
            } else {
                InvalidateRect(hWnd, NULL, FALSE);
            }
            return 0;
        }

        case WM_APP_UPDATE_DONE: {
            if (!wParam) {
                g_dlMode = DL_BANNER_NONE;
                g_dlFinishHideTick = 0;
                StartAnimLoop(hWnd);
                MessageBoxW(hWnd,
                            L"Güncelleme dosyası indirilemedi.\nLütfen internet bağlantınızı kontrol edip tekrar deneyin.",
                            L"Güncelleme Hatası", MB_OK | MB_ICONERROR);
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
                                   L"\u039BURNS için yeni bir güncelleme mevcut!\n\n"
                                   L"Mevcut Sürüm: v%s\nYeni Sürüm: v%s\n\nYenilikler:\n%s\n\n"
                                   L"Şimdi otomatik olarak indirip güncellemek istiyor musunuz?",
                                   APP_VERSION_STR, info->newVersion, info->changelog);
                    } else {
                        _snwprintf(prompt, 2047,
                                   L"\u039BURNS için yeni bir güncelleme mevcut!\n\n"
                                   L"Mevcut Sürüm: v%s\nYeni Sürüm: v%s\n\n"
                                   L"Şimdi otomatik olarak indirip güncellemek istiyor musunuz?",
                                   APP_VERSION_STR, info->newVersion);
                    }
                    prompt[2047] = L'\0';

                    if (MessageBoxW(hWnd, prompt, L"Güncelleme Mevcut", MB_YESNO | MB_ICONINFORMATION) == IDYES) {
                        g_dlMode = DL_BANNER_UPDATE;
                        g_dlFinishHideTick = 0;
                        g_dlPercent = 0;
                        wcscpy(g_dlHeadline, L"\u039BURNS GÜNCELLENİYOR");
                        wcscpy(g_dlSubline, L"Yeni sürüm paketi indiriliyor...");
                        StartAnimLoop(hWnd);
                        Updater_StartUpdateAsync(hWnd, info->downloadUrl);
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
                            HandleSmartPlaylistImport(filePath);
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
                InvalidateRect(hWnd, &rcDock, FALSE);
            } else if (pt.x >= GetCurrentSidebarRight()) {
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

            // Apple 1:1 Doğrudan Çekmece Sürükleme (Kesintiye Uğratılabilir & Hız Aktarımlı)
            if ((wParam & MK_LBUTTON) && (g_isDraggingSidebar || (g_sidebarDragStartX >= 0 && abs(pt.x - g_sidebarDragStartX) > S(7)))) {
                if (!g_isDraggingSidebar) {
                    g_isDraggingSidebar = TRUE;
                    g_sidebarPressedItem = -1;
                    g_pressedTrack = -1;
                }
                int sbW = GetSidebarW();
                if (sbW > 0) {
                    float newAnim = g_sidebarDragStartAnim + ((float)(pt.x - g_sidebarDragStartX) / (float)sbW);
                    if (newAnim < 0.0f) newAnim = 0.0f;
                    if (newAnim > 1.0f) newAnim = 1.0f;

                    DWORD now = GetTickCount();
                    DWORD dtMs = now - g_sidebarDragLastTick;
                    if (dtMs > 0) {
                        float instVel = ((float)(pt.x - g_sidebarDragLastX) / (float)sbW) / ((float)dtMs * 0.001f);
                        g_sidebarVel = g_sidebarVel * 0.35f + instVel * 0.65f;
                        g_sidebarDragLastTick = now;
                        g_sidebarDragLastX = pt.x;
                    }

                    g_sidebarAnim = newAnim;
                    UpdateSidebarEditPosition();
                    InvalidateRect(hWnd, NULL, FALSE);
                }
                return 0;
            }

            if (g_isDraggingScrollbar) {
                RECT rcClient;
                GetClientRect(hWnd, &rcClient);
                int sbTop = GetTracklistTop() + S(26) + S(6);
                int sbBottom = rcClient.bottom - GetDockH() - S(6);
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
                    InvalidateRect(hWnd, &rcDock, FALSE);
                }
                return 0;
            }

            if ((wParam & MK_LBUTTON) && g_dragFromIndex >= 0 && !g_isFiltered) {
                if (!g_isDraggingList && abs(pt.y - g_mouseDownPt.y) > S(7)) {
                    g_isDraggingList = TRUE;
                    g_pressedTrack = -1;
                }
                if (g_isDraggingList) {
                    int listTop = GetTracklistTop() + S(26) + S(6);
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

            int sideRight = GetCurrentSidebarRight();
            BOOL drawerBlocking = (sideRight > S(12) && (g_sidebarOpen || g_sidebarAnim > 0.2f));

            BOOL sbHov = (!drawerBlocking && rcScrollbarTrack.right > 0 && PtInRect(&rcScrollbarTrack, pt));
            if (sbHov != g_scrollbarHover) {
                g_scrollbarHover = sbHov;
                needRepaint = TRUE;
            }

            int newSideHover = -1;
            if (sideRight > S(24) && g_sidebarView == SIDEBAR_VIEW_MENU && pt.x < sideRight && pt.y >= GetHeaderH() + S(14)) {
                int m = (pt.y - (GetHeaderH() + S(14))) / S(52);
                if (m >= 0 && m < 7) newSideHover = m;
            }
            if (newSideHover != g_sidebarHoverItem) {
                g_sidebarHoverItem = newSideHover;
                needRepaint = TRUE;
            }

            int newHoverTrack = (!drawerBlocking && !sbHov) ? HitTestTrackRowIndex(hWnd, pt, NULL) : -1;
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
            if (!g_sidebarOpen && g_sidebarAnim < 0.1f) {
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

            g_sidebarDragStartX = -1;
            g_isDraggingSidebar = FALSE;
            g_sidebarPressedItem = -1;
            g_pressedTrack = -1;

            UiZone hit = HitTestZones(pt);
            g_pressedZone = hit;

            // 0. Jam Davet Bildirimi
            if (hit == ZONE_TOAST_ACCEPT) {
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

            // 1. Üst Çubuk Düğmeleri
            if (hit == ZONE_HAMBURGER) {
                if (!g_sidebarOpen) g_sidebarView = SIDEBAR_VIEW_MENU;
                ToggleSidebar(!g_sidebarOpen);
                g_pressedZone = ZONE_NONE;
                ReleaseCapture();
                InvalidateRect(hWnd, NULL, FALSE);
                return 0;
            }
            if (hit == ZONE_HEADER_FRIENDS) {
                if (g_sidebarOpen && g_sidebarView == SIDEBAR_VIEW_FRIENDS) {
                    ToggleSidebar(FALSE);
                } else {
                    OpenFriendsDrawer();
                }
                g_pressedZone = ZONE_NONE;
                ReleaseCapture();
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
                g_pressedZone = ZONE_NONE;
                ReleaseCapture();
                InvalidateRect(hWnd, NULL, FALSE);
                Search_ShowDialog(hWnd, g_cfg.lastFolder);
                return 0;
            }

            // 2. Yan Çekmece Etkileşimleri (Apple Pointer-Down + 1:1 Sürükleme Desteği)
            int sideRight = GetCurrentSidebarRight();
            if ((g_sidebarOpen || sideRight > S(14)) && pt.y >= GetHeaderH()) {
                if (pt.x >= sideRight) {
                    ToggleSidebar(FALSE);
                    g_pressedZone = ZONE_NONE;
                    ReleaseCapture();
                    return 0;
                }

                g_sidebarDragStartX = pt.x;
                g_sidebarDragStartAnim = g_sidebarAnim;
                g_sidebarDragLastTick = GetTickCount();
                g_sidebarDragLastX = pt.x;
                g_sidebarVel = 0.0f;

                if (g_sidebarView == SIDEBAR_VIEW_MENU) {
                    if (pt.y >= GetHeaderH() + S(14)) {
                        int item = (pt.y - (GetHeaderH() + S(14))) / S(52);
                        if (item >= 0 && item < 7) {
                            g_sidebarPressedItem = item;
                            InvalidateRect(hWnd, NULL, FALSE);
                        }
                    }
                    return 0;
                }
                else if (g_sidebarView == SIDEBAR_VIEW_FRIENDS) {
                    if (hit == ZONE_FRIENDS_BACK) {
                        g_sidebarView = SIDEBAR_VIEW_MENU;
                        g_sidebarDragStartX = -1;
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
                        g_sidebarDragStartX = -1;
                        ReleaseCapture();
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
                                        Jam_InviteFriend(fItem->tag, fItem->name);
                                        ToggleSidebar(FALSE);
                                        StartAnimLoop(hWnd);
                                    }
                                }
                                g_sidebarDragStartX = -1;
                                g_pressedZone = ZONE_NONE;
                                ReleaseCapture();
                                InvalidateRect(hWnd, NULL, FALSE);
                                return 0;
                            }
                            if (PtInRect(&rcFriendRemoveBtns[fi], pt)) {
                                const FriendItem *fItem = Friends_GetFriend(fi);
                                if (fItem) {
                                    wchar_t msg[128];
                                    _snwprintf(msg, 127, L"%s arkadaş listenden çıkarılsın mı?", fItem->name);
                                    msg[127] = L'\0';
                                    g_sidebarDragStartX = -1;
                                    g_pressedZone = ZONE_NONE;
                                    ReleaseCapture();
                                    if (MessageBoxW(hWnd, msg, L"Arkadaşı Kaldır", MB_YESNO | MB_ICONQUESTION) == IDYES) {
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

            // Sol Kenardan Çekmeceyi Sürükleyerek Açma (iOS Edge Swipe)
            if (!g_sidebarOpen && pt.x <= S(14) && pt.y >= GetHeaderH()) {
                g_sidebarView = SIDEBAR_VIEW_MENU;
                g_sidebarDragStartX = pt.x;
                g_sidebarDragStartAnim = g_sidebarAnim;
                g_sidebarDragLastTick = GetTickCount();
                g_sidebarDragLastX = pt.x;
                g_sidebarVel = 0.0f;
            }

            // 3. Canlı Jam & Evrensel İndirme Çubuğu Düğmeleri
            if (hit == ZONE_DL_CANCEL) {
                InterlockedExchange(&g_plDlCancel, 1);
                g_pressedZone = ZONE_NONE;
                ReleaseCapture();
                InvalidateRect(hWnd, NULL, FALSE);
                return 0;
            }
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

            // 4. Kaydırma Çubuğu
            if (GetMaxScrollPx(hWnd) > 0 && rcScrollbarTrack.right > 0 && PtInRect(&rcScrollbarTrack, pt)) {
                g_sidebarDragStartX = -1;
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

            // 5. Alt Oynatıcı Kontrolleri (Apple Pointer-Down Anında Tepki)
            if (hit == ZONE_SHUFFLE) {
                g_cfg.shuffle = !g_cfg.shuffle;
                Config_Save(&g_cfg, L"config.json");
                InvalidateRect(hWnd, &rcDock, FALSE);
                return 0;
            }
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
                InvalidateRect(hWnd, &rcDock, FALSE);
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
                InvalidateRect(hWnd, &rcDock, FALSE);
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
                    InvalidateRect(hWnd, &rcDock, FALSE);
                }
                return 0;
            }
            if (hit == ZONE_VOL) {
                g_isDraggingVol = TRUE;
                SendMessageW(hWnd, WM_MOUSEMOVE, wParam, lParam);
                return 0;
            }

            // 6. Şarkı Satırına Basma (Pointer-Down Anında Vurgu, Bırakınca Çalma / Sürükleyince Sıralama)
            int actualIdx = HitTestTrackRowIndex(hWnd, pt, NULL);
            if (actualIdx >= 0) {
                g_pressedTrack = actualIdx;
                g_selectedTrack = actualIdx;
                g_dragFromIndex = actualIdx;
                g_dragTargetIndex = actualIdx;
                g_mouseDownPt = pt;
                g_isDraggingList = FALSE;
                InvalidateRect(hWnd, NULL, FALSE);
            }
            return 0;
        }

        case WM_LBUTTONUP: {
            POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
            ReleaseCapture();

            BOOL needRepaint = (g_pressedZone != ZONE_NONE || g_pressedTrack != -1 || g_sidebarPressedItem != -1);
            g_pressedZone = ZONE_NONE;

            int releasedMenu = g_sidebarPressedItem;
            int releasedTrack = g_pressedTrack;
            g_sidebarPressedItem = -1;
            g_pressedTrack = -1;
            g_sidebarDragStartX = -1;

            if (g_isDraggingSidebar) {
                g_isDraggingSidebar = FALSE;
                float projected = g_sidebarAnim + g_sidebarVel * 0.14f;
                ToggleSidebar(projected >= 0.45f);
                return 0;
            }

            if (releasedMenu >= 0 && g_sidebarOpen && g_sidebarView == SIDEBAR_VIEW_MENU && pt.x < GetCurrentSidebarRight() && pt.y >= GetHeaderH() + S(14)) {
                int upItem = (pt.y - (GetHeaderH() + S(14))) / S(52);
                if (upItem == releasedMenu) {
                    if (upItem == 0) { ToggleSidebar(FALSE); SelectFolderDialog(); }
                    else if (upItem == 1) { ToggleSidebar(FALSE); Search_ShowDialog(hWnd, g_cfg.lastFolder); }
                    else if (upItem == 2) { OpenFriendsDrawer(); }
                    else if (upItem == 3) { ToggleSidebar(FALSE); ImportPlaylistDialog(); }
                    else if (upItem == 4) { ToggleSidebar(FALSE); ExportPlaylistDialog(); }
                    else if (upItem == 5) {
                        ToggleSidebar(FALSE);
                        Playlist_Clear();
                        Player_Stop();
                        g_currentTrack = -1;
                        g_selectedTrack = -1;
                        SyncLocalTrackToFriends();
                        ApplyFilter(L"");
                    }
                    else if (upItem == 6) { ToggleSidebar(FALSE); Updater_CheckAsync(hWnd, TRUE); }
                    return 0;
                }
            }

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
            } else if (!g_isDraggingList && releasedTrack >= 0) {
                int upTrack = HitTestTrackRowIndex(hWnd, pt, NULL);
                if (upTrack == releasedTrack) {
                    PlayTrack(releasedTrack);
                    needRepaint = TRUE;
                }
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
                    InvalidateRect(hWnd, &rcDock, FALSE);
                    return 0;
                }
                if (wParam == 'S' || wParam == 's') {
                    g_cfg.shuffle = !g_cfg.shuffle;
                    Config_Save(&g_cfg, L"config.json");
                    InvalidateRect(hWnd, &rcDock, FALSE);
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
                    InvalidateRect(hWnd, NULL, FALSE);
                    return 0;
                }
                case VK_LEFT: {
                    int pos = Player_GetPosition() - 5;
                    if (pos < 0) pos = 0;
                    Player_SetPosition(pos);
                    if (Jam_IsActive()) Jam_BroadcastSeek(pos);
                    InvalidateRect(hWnd, &rcDock, FALSE);
                    return 0;
                }
                case VK_RIGHT: {
                    int pos = Player_GetPosition() + 5;
                    int dur = Player_GetDuration();
                    if (dur > 0 && pos > dur) pos = dur;
                    Player_SetPosition(pos);
                    if (Jam_IsActive()) Jam_BroadcastSeek(pos);
                    InvalidateRect(hWnd, &rcDock, FALSE);
                    return 0;
                }
                case VK_UP: {
                    int vol = Player_GetVolume() + 5;
                    Player_SetVolume(vol);
                    if (Player_GetVolume() > 0) g_preMuteVol = Player_GetVolume();
                    g_cfg.volume = Player_GetVolume();
                    Config_Save(&g_cfg, L"config.json");
                    InvalidateRect(hWnd, &rcDock, FALSE);
                    return 0;
                }
                case VK_DOWN: {
                    int vol = Player_GetVolume() - 5;
                    Player_SetVolume(vol);
                    if (Player_GetVolume() > 0) g_preMuteVol = Player_GetVolume();
                    g_cfg.volume = Player_GetVolume();
                    Config_Save(&g_cfg, L"config.json");
                    InvalidateRect(hWnd, &rcDock, FALSE);
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
            mmi->ptMinTrackSize.x = S(500);
            mmi->ptMinTrackSize.y = S(520);
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
                g_gdiDirty = FALSE;
                RenderUI(g_backDC, &rcClient);
                FlushGdiIfNeeded();
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
                g_gdiDirty = FALSE;
                RenderUI(g_backDC, &rcClient);
                FlushGdiIfNeeded();
                BitBlt(hdc, 0, 0, rcClient.right, rcClient.bottom, g_backDC, 0, 0, SRCCOPY);
            } else {
                RenderUI(hdc, &rcClient);
            }
            return 0;
        }

        case WM_APPCOMMAND: {
            int cmd = GET_APPCOMMAND_LPARAM(lParam);
            if (cmd == APPCOMMAND_MEDIA_PLAY_PAUSE) {
                if (Player_IsPlaying()) {
                    Player_Pause();
                    SyncLocalTrackToFriends();
                    if (Jam_IsActive()) Jam_BroadcastPause(Player_GetPosition());
                } else if (g_currentTrack >= 0) {
                    Player_Play();
                    SyncLocalTrackToFriends();
                    BroadcastCurrentJamTrack(Player_GetPosition());
                } else if (Playlist_Count() > 0) {
                    PlayTrack(0);
                }
                InvalidateRect(hWnd, &rcDock, FALSE);
                UpdateTimerSchedule(hWnd);
                return TRUE;
            }
            if (cmd == APPCOMMAND_MEDIA_NEXTTRACK) {
                int next = Playlist_FindNext(g_currentTrack, g_cfg.shuffle, g_cfg.repeat == 2 ? 1 : g_cfg.repeat);
                if (next >= 0) { PlayTrack(next); ScrollTrackIntoView(next); }
                return TRUE;
            }
            if (cmd == APPCOMMAND_MEDIA_PREVIOUSTRACK) {
                int prev = Playlist_FindPrev(g_currentTrack);
                if (prev >= 0) { PlayTrack(prev); ScrollTrackIntoView(prev); }
                return TRUE;
            }
            if (cmd == APPCOMMAND_MEDIA_STOP) {
                Player_Stop();
                InvalidateRect(hWnd, &rcDock, FALSE);
                UpdateTimerSchedule(hWnd);
                return TRUE;
            }
            break;
        }

        case WM_ERASEBKGND:
            return 1;

        case WM_CLOSE:
            DestroyWindow(hWnd);
            return 0;

        case WM_DESTROY: {
            KillTimer(hWnd, IDT_UPDATE_TIMER);
            if (g_animTimerActive) KillTimer(hWnd, IDT_ANIM_TIMER);
            InterlockedExchange(&g_plDlCancel, 1);

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
            if (g_plDlCsReady && InterlockedCompareExchange(&g_plDlRunning, 0, 0) == 0) {
                DeleteCriticalSection(&g_plDlCs);
                g_plDlCsReady = FALSE;
            }

            if (g_hFontBrand)      DeleteObject(g_hFontBrand);
            if (g_hFontTitle)      DeleteObject(g_hFontTitle);
            if (g_hFontBold)       DeleteObject(g_hFontBold);
            if (g_hFontMain)       DeleteObject(g_hFontMain);
            if (g_hFontSmall)      DeleteObject(g_hFontSmall);
            if (g_hFontSmallBold)  DeleteObject(g_hFontSmallBold);
            if (g_hFontMicro)      DeleteObject(g_hFontMicro);
            if (g_hFontIcons)      DeleteObject(g_hFontIcons);
            if (g_hFontIconsSmall) DeleteObject(g_hFontIconsSmall);
            if (g_hFontIconsLarge) DeleteObject(g_hFontIconsLarge);
            if (g_hBrInputBg)      DeleteObject(g_hBrInputBg);
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
    (void)hInstance; (void)hPrevInstance; (void)pCmdLine;

    // Windows Görev Çubuğu ve Pinned Shortcut Modern Uygulama Tanımlayıcısı (AppUserModelID)
    typedef HRESULT(WINAPI *SetAppIDProc)(PCWSTR);
    HMODULE hShell = LoadLibraryW(L"shell32.dll");
    if (hShell) {
        SetAppIDProc pSetAppID = (SetAppIDProc)GetProcAddress(hShell, "SetCurrentProcessExplicitAppUserModelID");
        if (pSetAppID) {
            pSetAppID(L"Aurns.MusicPlayer.Modern.App");
        }
    }

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
    wc.lpszClassName = L"AurnsNativeClass";

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
        L"AurnsNativeClass",
        L"\u039BURNS",
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
