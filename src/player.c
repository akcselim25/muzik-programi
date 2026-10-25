#define INITGUID
#include <windows.h>
#include <mfplay.h>
#include "player.h"

static const GUID MY_GUID_NULL = { 0x00000000, 0x0000, 0x0000, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } };
#define MFP_POSITION_UNIT_100NS MY_GUID_NULL

static const GUID MY_IID_IMFPMediaPlayerCallback = { 0x766c8ffb, 0x5d7d, 0x41be, { 0x9e, 0x50, 0x58, 0x3e, 0xaa, 0xb4, 0xf6, 0x69 } };
static const GUID MY_IID_IUnknown = { 0x00000000, 0x0000, 0x0000, { 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 } };

typedef struct {
    IMFPMediaPlayerCallback cb;
    LONG refCount;
} PlayerCallback;

static HWND g_hWndNotify = NULL;
static HMODULE g_hMfPlayDll = NULL;
static IMFPMediaPlayer *g_pPlayer = NULL;

typedef HRESULT (STDAPICALLTYPE *PFN_MFPCreateMediaPlayer)(
    LPCWSTR pwszURL,
    BOOL fStartPlayback,
    MFP_CREATION_OPTIONS creationOptions,
    IMFPMediaPlayerCallback *pCallback,
    HWND hWnd,
    IMFPMediaPlayer **ppMediaPlayer
);
static PFN_MFPCreateMediaPlayer g_pfnMFPCreate = NULL;
static PlayerCallback *g_pCallback = NULL;
static volatile LONG g_openGen = 0;
static volatile LONG g_mediaReady = 0;
static volatile LONG g_pendingSeekSec = -1;
static volatile int g_isPlaying = 0;
static volatile int g_isPaused = 0;
static int g_volume = 80; // 0 - 100
static volatile int g_durationCache = 0;

static HRESULT STDMETHODCALLTYPE Cb_QueryInterface(IMFPMediaPlayerCallback *this, REFIID riid, void **ppv) {
    if (IsEqualGUID(riid, &MY_IID_IUnknown) || IsEqualGUID(riid, &MY_IID_IMFPMediaPlayerCallback)) {
        *ppv = this;
        this->lpVtbl->AddRef(this);
        return S_OK;
    }
    *ppv = NULL;
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE Cb_AddRef(IMFPMediaPlayerCallback *this) {
    PlayerCallback *c = (PlayerCallback*)this;
    return InterlockedIncrement(&c->refCount);
}

static ULONG STDMETHODCALLTYPE Cb_Release(IMFPMediaPlayerCallback *this) {
    PlayerCallback *c = (PlayerCallback*)this;
    ULONG count = InterlockedDecrement(&c->refCount);
    if (count == 0) {
        free(c);
    }
    return count;
}

static void STDMETHODCALLTYPE Cb_OnMediaPlayerEvent(IMFPMediaPlayerCallback *this, MFP_EVENT_HEADER *pEventHeader) {
    (void)this;
    if (!pEventHeader) return;

    if (pEventHeader->eEventType == MFP_EVENT_TYPE_MEDIAITEM_CREATED) {
        MFP_MEDIAITEM_CREATED_EVENT *ev = MFP_GET_MEDIAITEM_CREATED_EVENT(pEventHeader);
        if (ev && ev->pMediaItem && SUCCEEDED(pEventHeader->hrEvent)) {
            if (ev->dwUserData == (DWORD_PTR)g_openGen && g_pPlayer) {
                g_pPlayer->lpVtbl->SetMediaItem(g_pPlayer, ev->pMediaItem);
            }
        }
    }
    else if (pEventHeader->eEventType == MFP_EVENT_TYPE_MEDIAITEM_SET) {
        if (g_pPlayer && SUCCEEDED(pEventHeader->hrEvent)) {
            InterlockedExchange(&g_mediaReady, 1);
            g_pPlayer->lpVtbl->SetVolume(g_pPlayer, (float)g_volume / 100.0f);

            PROPVARIANT var;
            PropVariantInit(&var);
            if (SUCCEEDED(g_pPlayer->lpVtbl->GetDuration(g_pPlayer, &MFP_POSITION_UNIT_100NS, &var))) {
                g_durationCache = (int)(var.hVal.QuadPart / 10000000LL);
                PropVariantClear(&var);
            }

            LONG seekTarget = InterlockedExchange(&g_pendingSeekSec, -1);
            if (seekTarget >= 0) {
                if (g_durationCache > 0 && seekTarget > g_durationCache) {
                    seekTarget = g_durationCache;
                }
                PROPVARIANT posVar;
                PropVariantInit(&posVar);
                posVar.vt = VT_I8;
                posVar.hVal.QuadPart = (LONGLONG)seekTarget * 10000000LL;
                g_pPlayer->lpVtbl->SetPosition(g_pPlayer, &MFP_POSITION_UNIT_100NS, &posVar);
                PropVariantClear(&posVar);
            }

            if (g_isPlaying && !g_isPaused) {
                g_pPlayer->lpVtbl->Play(g_pPlayer);
            }
        }
        if (g_hWndNotify) {
            InvalidateRect(g_hWndNotify, NULL, FALSE);
        }
    }
    else if (pEventHeader->eEventType == MFP_EVENT_TYPE_PLAYBACK_ENDED) {
        if (g_hWndNotify) {
            PostMessageW(g_hWndNotify, WM_APP_TRACK_ENDED, 0, 0);
        }
    }
}

static IMFPMediaPlayerCallbackVtbl g_cbVtbl = {
    Cb_QueryInterface,
    Cb_AddRef,
    Cb_Release,
    Cb_OnMediaPlayerEvent
};

int Player_Init(HWND hWndNotify) {
    g_hWndNotify = hWndNotify;

    g_pCallback = (PlayerCallback*)malloc(sizeof(PlayerCallback));
    if (!g_pCallback) return 0;
    g_pCallback->cb.lpVtbl = &g_cbVtbl;
    g_pCallback->refCount = 1;

    return 1;
}

void Player_Shutdown(void) {
    InterlockedIncrement(&g_openGen);
    if (g_pPlayer) {
        g_pPlayer->lpVtbl->Stop(g_pPlayer);
        g_pPlayer->lpVtbl->Shutdown(g_pPlayer);
        g_pPlayer->lpVtbl->Release(g_pPlayer);
        g_pPlayer = NULL;
    }
    g_isPlaying = 0;
    g_isPaused = 0;
    InterlockedExchange(&g_mediaReady, 0);
    g_durationCache = 0;
    if (g_pCallback) {
        g_pCallback->cb.lpVtbl->Release((IMFPMediaPlayerCallback*)g_pCallback);
        g_pCallback = NULL;
    }
    if (g_hMfPlayDll) {
        FreeLibrary(g_hMfPlayDll);
        g_hMfPlayDll = NULL;
        g_pfnMFPCreate = NULL;
    }
}

int Player_OpenFile(const wchar_t *path) {
    if (!path || !path[0]) return 0;

    // Motor henüz oluşturulmadıysa mfplay.dll'i ilk şarkı açılışında yükle (açılışta sıfır DLL/RAM yükü!)
    if (!g_pPlayer) {
        if (!g_pfnMFPCreate) {
            g_hMfPlayDll = LoadLibraryW(L"mfplay.dll");
            if (!g_hMfPlayDll) return 0;
            g_pfnMFPCreate = (PFN_MFPCreateMediaPlayer)GetProcAddress(g_hMfPlayDll, "MFPCreateMediaPlayer");
            if (!g_pfnMFPCreate) return 0;
        }
        HRESULT hr = g_pfnMFPCreate(NULL, FALSE, 0, (IMFPMediaPlayerCallback*)g_pCallback, NULL, &g_pPlayer);
        if (FAILED(hr) || !g_pPlayer) {
            g_pPlayer = NULL;
            return 0;
        }
        g_pPlayer->lpVtbl->SetVolume(g_pPlayer, (float)g_volume / 100.0f);
    }

    LONG gen = InterlockedIncrement(&g_openGen);
    InterlockedExchange(&g_mediaReady, 0);
    InterlockedExchange(&g_pendingSeekSec, -1);
    g_durationCache = 0;
    g_isPlaying = 1;
    g_isPaused = 0;

    // Asenkron yükleme: Arayüzü (UI Thread) 0.0 ms bile dondurmaz!
    HRESULT hr = g_pPlayer->lpVtbl->CreateMediaItemFromURL(g_pPlayer, path, FALSE, (DWORD_PTR)gen, NULL);
    if (FAILED(hr)) {
        g_isPlaying = 0;
        return 0;
    }

    return 1;
}

void Player_Play(void) {
    if (!g_pPlayer) return;
    g_isPlaying = 1;
    g_isPaused = 0;
    if (g_mediaReady) {
        g_pPlayer->lpVtbl->Play(g_pPlayer);
    }
}

void Player_Pause(void) {
    if (!g_pPlayer) return;
    if (g_isPlaying && !g_isPaused) {
        if (g_mediaReady) {
            g_pPlayer->lpVtbl->Pause(g_pPlayer);
        }
        g_isPaused = 1;
    } else if (g_isPaused) {
        g_isPaused = 0;
        if (g_mediaReady) {
            g_pPlayer->lpVtbl->Play(g_pPlayer);
        }
    }
}

void Player_Stop(void) {
    InterlockedIncrement(&g_openGen);
    InterlockedExchange(&g_mediaReady, 0);
    InterlockedExchange(&g_pendingSeekSec, -1);
    if (g_pPlayer) {
        g_pPlayer->lpVtbl->Stop(g_pPlayer);
    }
    g_isPlaying = 0;
    g_isPaused = 0;
    g_durationCache = 0;
}

int Player_IsPlaying(void) {
    return g_isPlaying && !g_isPaused;
}

int Player_IsPaused(void) {
    return g_isPaused;
}

int Player_GetDuration(void) {
    if (!g_pPlayer || !g_mediaReady) return 0;
    if (g_durationCache > 0) return g_durationCache;

    PROPVARIANT var;
    PropVariantInit(&var);
    if (SUCCEEDED(g_pPlayer->lpVtbl->GetDuration(g_pPlayer, &MFP_POSITION_UNIT_100NS, &var))) {
        g_durationCache = (int)(var.hVal.QuadPart / 10000000LL);
        PropVariantClear(&var);
        return g_durationCache;
    }
    return 0;
}

int Player_GetPosition(void) {
    if (!g_pPlayer || !g_mediaReady) {
        LONG pending = g_pendingSeekSec;
        return (pending >= 0) ? (int)pending : 0;
    }
    PROPVARIANT var;
    PropVariantInit(&var);
    int posSec = 0;
    if (SUCCEEDED(g_pPlayer->lpVtbl->GetPosition(g_pPlayer, &MFP_POSITION_UNIT_100NS, &var))) {
        posSec = (int)(var.hVal.QuadPart / 10000000LL);
        PropVariantClear(&var);
    }
    return posSec;
}

void Player_SetPosition(int seconds) {
    if (!g_pPlayer || seconds < 0) return;
    if (!g_mediaReady) {
        InterlockedExchange(&g_pendingSeekSec, (LONG)seconds);
        return;
    }
    int dur = Player_GetDuration();
    if (dur > 0 && seconds > dur) seconds = dur;
    PROPVARIANT var;
    PropVariantInit(&var);
    var.vt = VT_I8;
    var.hVal.QuadPart = (LONGLONG)seconds * 10000000LL;
    g_pPlayer->lpVtbl->SetPosition(g_pPlayer, &MFP_POSITION_UNIT_100NS, &var);
    PropVariantClear(&var);
}

void Player_SetVolume(int volumePercent) {
    if (volumePercent < 0) volumePercent = 0;
    if (volumePercent > 100) volumePercent = 100;
    g_volume = volumePercent;
    if (g_pPlayer) {
        float fVol = (float)g_volume / 100.0f;
        g_pPlayer->lpVtbl->SetVolume(g_pPlayer, fVol);
    }
}

int Player_GetVolume(void) {
    return g_volume;
}
