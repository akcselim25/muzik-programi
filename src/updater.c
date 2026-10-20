#include "updater.h"
#include <urlmon.h>
#include <shlwapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GITHUB_VERSION_URL L"https://raw.githubusercontent.com/akcselim25/muzik-programi/main/version.txt"
#define DEFAULT_EXE_URL    L"https://raw.githubusercontent.com/akcselim25/muzik-programi/main/SeloPlayer.exe"

typedef struct {
    HWND hWnd;
    BOOL isManual;
} CheckParam;

static int CompareVersions(const wchar_t *v1, const wchar_t *v2) {
    int maj1 = 0, min1 = 0, pat1 = 0;
    int maj2 = 0, min2 = 0, pat2 = 0;
    swscanf(v1, L"%d.%d.%d", &maj1, &min1, &pat1);
    swscanf(v2, L"%d.%d.%d", &maj2, &min2, &pat2);
    if (maj1 != maj2) return maj1 - maj2;
    if (min1 != min2) return min1 - min2;
    return pat1 - pat2;
}

static DWORD WINAPI CheckUpdateThread(LPVOID param) {
    CheckParam *cp = (CheckParam*)param;
    HWND hWnd = cp->hWnd;
    BOOL isManual = cp->isManual;
    free(cp);

    UpdateInfo *info = (UpdateInfo*)calloc(1, sizeof(UpdateInfo));
    if (!info) return 0;
    info->isManual = isManual;

    // Cache-busting: Tarayici veya Windows onbellegi eski dosyayi vermesin diye zaman damgasi ekle
    wchar_t urlWithTimestamp[1200];
    wsprintfW(urlWithTimestamp, L"%s?t=%lu", GITHUB_VERSION_URL, (unsigned long)GetTickCount());

    wchar_t tempPath[MAX_PATH];
    wchar_t tempFile[MAX_PATH];
    GetTempPathW(MAX_PATH, tempPath);
    GetTempFileNameW(tempPath, L"spv", 0, tempFile);

    HRESULT hr = URLDownloadToFileW(NULL, urlWithTimestamp, tempFile, 0, NULL);
    if (SUCCEEDED(hr)) {
        FILE *f = _wfopen(tempFile, L"r, ccs=UTF-8");
        if (!f) f = _wfopen(tempFile, L"r");

        if (f) {
            wchar_t lineVer[128] = {0};
            wchar_t lineUrl[1024] = {0};

            if (fgetws(lineVer, sizeof(lineVer)/sizeof(wchar_t), f)) {
                // Trim trailing newline & whitespace
                wchar_t *p = lineVer + wcslen(lineVer) - 1;
                while (p >= lineVer && (*p == L'\r' || *p == L'\n' || *p == L' ')) {
                    *p = L'\0';
                    p--;
                }
                wcsncpy(info->newVersion, lineVer, 63);

                // Satir 2: Indirme linki (opsiyonel)
                if (fgetws(lineUrl, sizeof(lineUrl)/sizeof(wchar_t), f)) {
                    p = lineUrl + wcslen(lineUrl) - 1;
                    while (p >= lineUrl && (*p == L'\r' || *p == L'\n' || *p == L' ')) {
                        *p = L'\0';
                        p--;
                    }
                    if (wcsncmp(lineUrl, L"http", 4) == 0) {
                        wcsncpy(info->downloadUrl, lineUrl, 1023);
                    }
                }

                // Satir 3+: Yenilik notlari
                wchar_t noteBuf[256];
                while (fgetws(noteBuf, sizeof(noteBuf)/sizeof(wchar_t), f)) {
                    if (wcslen(info->changelog) + wcslen(noteBuf) < 1020) {
                        wcscat(info->changelog, noteBuf);
                    }
                }

                if (info->downloadUrl[0] == L'\0') {
                    wcscpy(info->downloadUrl, DEFAULT_EXE_URL);
                }

                info->checkSuccess = TRUE;
                if (CompareVersions(info->newVersion, APP_VERSION_STR) > 0) {
                    info->hasUpdate = TRUE;
                }
            }
            fclose(f);
        }
        DeleteFileW(tempFile);
    }

    if (IsWindow(hWnd)) {
        PostMessageW(hWnd, WM_APP_UPDATE_CHECK_DONE, 0, (LPARAM)info);
    } else {
        free(info);
    }
    return 0;
}

void Updater_Init(void) {
    wchar_t szCurrentExe[MAX_PATH];
    GetModuleFileNameW(NULL, szCurrentExe, MAX_PATH);
    PathRemoveFileSpecW(szCurrentExe);

    wchar_t szOldExe[MAX_PATH];
    wsprintfW(szOldExe, L"%s\\SeloPlayer_old.tmp", szCurrentExe);
    DeleteFileW(szOldExe);

    wchar_t szNewExe[MAX_PATH];
    wsprintfW(szNewExe, L"%s\\SeloPlayer_new.tmp", szCurrentExe);
    DeleteFileW(szNewExe);
}

void Updater_CheckAsync(HWND hWnd, BOOL isManual) {
    CheckParam *cp = (CheckParam*)malloc(sizeof(CheckParam));
    if (!cp) return;
    cp->hWnd = hWnd;
    cp->isManual = isManual;

    HANDLE hThread = CreateThread(NULL, 0, CheckUpdateThread, cp, 0, NULL);
    if (hThread) {
        CloseHandle(hThread);
    } else {
        free(cp);
    }
}

BOOL Updater_PerformUpdate(HWND hWnd, const wchar_t *downloadUrl) {
    if (!downloadUrl || !downloadUrl[0]) return FALSE;

    wchar_t szCurrentExe[MAX_PATH];
    GetModuleFileNameW(NULL, szCurrentExe, MAX_PATH);

    wchar_t szDir[MAX_PATH];
    wcscpy(szDir, szCurrentExe);
    PathRemoveFileSpecW(szDir);

    wchar_t szNewExe[MAX_PATH];
    wsprintfW(szNewExe, L"%s\\SeloPlayer_new.tmp", szDir);

    wchar_t szOldExe[MAX_PATH];
    wsprintfW(szOldExe, L"%s\\SeloPlayer_old.tmp", szDir);

    DeleteFileW(szNewExe);

    // Cache-busting URL
    wchar_t urlWithTs[1200];
    wsprintfW(urlWithTs, L"%s?t=%lu", downloadUrl, (unsigned long)GetTickCount());

    HRESULT hr = URLDownloadToFileW(NULL, urlWithTs, szNewExe, 0, NULL);
    if (FAILED(hr)) {
        MessageBoxW(hWnd, L"Güncelleme dosyası indirilemedi!\nLütfen internet bağlantınızı kontrol edin.", L"Güncelleme Hatası", MB_OK | MB_ICONERROR);
        DeleteFileW(szNewExe);
        return FALSE;
    }

    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExW(szNewExe, GetFileExInfoStandard, &fad) || fad.nFileSizeLow < 50000) {
        MessageBoxW(hWnd, L"İndirilen güncelleme dosyası eksik veya geçersiz!", L"Güncelleme Hatası", MB_OK | MB_ICONERROR);
        DeleteFileW(szNewExe);
        return FALSE;
    }

    // Windows'ta calisan .exe silinemez ama adi degistirilebilir (rename).
    DeleteFileW(szOldExe);
    if (!MoveFileExW(szCurrentExe, szOldExe, MOVEFILE_REPLACE_EXISTING)) {
        MessageBoxW(hWnd, L"Mevcut program dosyası taşınamadı. Yönetici izinlerini kontrol edin.", L"Güncelleme Hatası", MB_OK | MB_ICONERROR);
        DeleteFileW(szNewExe);
        return FALSE;
    }

    if (!MoveFileExW(szNewExe, szCurrentExe, MOVEFILE_REPLACE_EXISTING)) {
        MoveFileExW(szOldExe, szCurrentExe, MOVEFILE_REPLACE_EXISTING);
        MessageBoxW(hWnd, L"Yeni sürüm dosyası yerine yerleştirilemedi!", L"Güncelleme Hatası", MB_OK | MB_ICONERROR);
        return FALSE;
    }

    // Yeni surumu ac ve mevcut sureci kapat
    HINSTANCE hInst = ShellExecuteW(NULL, L"open", szCurrentExe, NULL, szDir, SW_SHOWNORMAL);
    if ((INT_PTR)hInst > 32) {
        ExitProcess(0);
    }
    return TRUE;
}
