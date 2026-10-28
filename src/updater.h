#ifndef UPDATER_H
#define UPDATER_H

#include <windows.h>

#define APP_VERSION_STR L"1.1.0"

// Pencereye gonderilecek bildirim mesajlari
#define WM_APP_UPDATE_CHECK_DONE (WM_APP + 301)
#define WM_APP_UPDATE_PROGRESS   (WM_APP + 302) // wParam: 0..100 yuzde
#define WM_APP_UPDATE_DONE       (WM_APP + 303) // wParam: 1=basarili, 0=hata (lParam: hata kodu)

typedef struct {
    BOOL hasUpdate;
    BOOL isManual;
    BOOL checkSuccess;
    wchar_t newVersion[64];
    wchar_t downloadUrl[1024];
    wchar_t changelog[1024];
} UpdateInfo;

// Eski gecici dosyalari temizler
void Updater_Init(void);

// Arka planda guncelleme kontrolu baslatir (isManual: Menuden tiklandiysa TRUE, acilista FALSE)
void Updater_CheckAsync(HWND hWnd, BOOL isManual);

// Yeni surumu arka planda yuzde bildirimiyle indirip mevcut Aurns.exe ile degistirir ve yeniden baslatir
BOOL Updater_StartUpdateAsync(HWND hWnd, const wchar_t *downloadUrl);

#endif // UPDATER_H
