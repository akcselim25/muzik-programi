#include "updater.h"
#include <winhttp.h>
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

static HINTERNET OpenHttpRequest(const wchar_t *url, HINTERNET *pSession, HINTERNET *pConnect) {
    *pSession = NULL;
    *pConnect = NULL;

    URL_COMPONENTS uc;
    memset(&uc, 0, sizeof(uc));
    uc.dwStructSize = sizeof(uc);

    wchar_t host[256] = {0};
    wchar_t path[1024] = {0};
    uc.lpszHostName = host;
    uc.dwHostNameLength = 255;
    uc.lpszUrlPath = path;
    uc.dwUrlPathLength = 1023;

    if (!WinHttpCrackUrl(url, 0, 0, &uc)) return NULL;

    HINTERNET hSession = WinHttpOpen(L"SeloPlayer/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                     WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return NULL;

    WinHttpSetTimeouts(hSession, 4000, 4000, 5000, 8000);

    HINTERNET hConnect = WinHttpConnect(hSession, host, uc.nPort, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return NULL;
    }

    DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path, NULL,
                                            WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return NULL;
    }

    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(hRequest, NULL)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return NULL;
    }

    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);
    if (statusCode != 200) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return NULL;
    }

    *pSession = hSession;
    *pConnect = hConnect;
    return hRequest;
}

static BOOL HttpDownloadTextUtf8(const wchar_t *url, char *outBuf, DWORD maxBytes) {
    HINTERNET hSession = NULL, hConnect = NULL;
    HINTERNET hRequest = OpenHttpRequest(url, &hSession, &hConnect);
    if (!hRequest) return FALSE;

    DWORD total = 0;
    DWORD avail = 0;
    while (WinHttpQueryDataAvailable(hRequest, &avail) && avail > 0 && total + 1 < maxBytes) {
        DWORD toRead = avail;
        if (total + toRead >= maxBytes) toRead = maxBytes - 1 - total;
        DWORD got = 0;
        if (!WinHttpReadData(hRequest, outBuf + total, toRead, &got) || got == 0) break;
        total += got;
    }
    outBuf[total] = '\0';

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return (total > 0);
}

static BOOL HttpDownloadToFile(const wchar_t *url, const wchar_t *destPath) {
    HINTERNET hSession = NULL, hConnect = NULL;
    HINTERNET hRequest = OpenHttpRequest(url, &hSession, &hConnect);
    if (!hRequest) return FALSE;

    HANDLE hFile = CreateFileW(destPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return FALSE;
    }

    BYTE chunk[16384];
    DWORD avail = 0;
    BOOL ok = TRUE;
    while (WinHttpQueryDataAvailable(hRequest, &avail) && avail > 0) {
        while (avail > 0) {
            DWORD toRead = (avail > sizeof(chunk)) ? sizeof(chunk) : avail;
            DWORD got = 0;
            if (!WinHttpReadData(hRequest, chunk, toRead, &got) || got == 0) {
                ok = FALSE;
                break;
            }
            DWORD written = 0;
            if (!WriteFile(hFile, chunk, got, &written, NULL) || written != got) {
                ok = FALSE;
                break;
            }
            avail -= got;
        }
        if (!ok) break;
    }

    CloseHandle(hFile);
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return ok;
}

static DWORD WINAPI CheckUpdateThread(LPVOID param) {
    CheckParam *cp = (CheckParam*)param;
    HWND hWnd = cp->hWnd;
    BOOL isManual = cp->isManual;
    free(cp);

    UpdateInfo *info = (UpdateInfo*)calloc(1, sizeof(UpdateInfo));
    if (!info) return 0;
    info->isManual = isManual;

    wchar_t urlWithTimestamp[1200];
    _snwprintf(urlWithTimestamp, 1199, L"%s?t=%lu", GITHUB_VERSION_URL, (unsigned long)GetTickCount());
    urlWithTimestamp[1199] = L'\0';

    char utf8Buf[4096];
    if (HttpDownloadTextUtf8(urlWithTimestamp, utf8Buf, sizeof(utf8Buf))) {
        const char *src = utf8Buf;
        // UTF-8 BOM varsa atla
        if ((unsigned char)src[0] == 0xEF && (unsigned char)src[1] == 0xBB && (unsigned char)src[2] == 0xBF) {
            src += 3;
        }

        wchar_t wText[4096] = {0};
        MultiByteToWideChar(CP_UTF8, 0, src, -1, wText, 4095);

        wchar_t *ctx = NULL;
        wchar_t *line1 = wcstok(wText, L"\r\n", &ctx);
        if (line1) {
            StrTrimW(line1, L" \t");
            wcsncpy(info->newVersion, line1, 63);

            wchar_t *line2 = wcstok(NULL, L"\r\n", &ctx);
            if (line2) {
                StrTrimW(line2, L" \t");
                if (wcsncmp(line2, L"http", 4) == 0) {
                    wcsncpy(info->downloadUrl, line2, 1023);
                } else {
                    wcsncpy(info->changelog, line2, 1020);
                    wcscat(info->changelog, L"\n");
                }
            }

            wchar_t *noteLine;
            while ((noteLine = wcstok(NULL, L"\r\n", &ctx)) != NULL) {
                if (wcslen(info->changelog) + wcslen(noteLine) + 2 < 1020) {
                    wcscat(info->changelog, noteLine);
                    wcscat(info->changelog, L"\n");
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
    _snwprintf(szOldExe, MAX_PATH - 1, L"%s\\SeloPlayer_old.tmp", szCurrentExe);
    szOldExe[MAX_PATH - 1] = L'\0';
    DeleteFileW(szOldExe);

    wchar_t szNewExe[MAX_PATH];
    _snwprintf(szNewExe, MAX_PATH - 1, L"%s\\SeloPlayer_new.tmp", szCurrentExe);
    szNewExe[MAX_PATH - 1] = L'\0';
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
    _snwprintf(szNewExe, MAX_PATH - 1, L"%s\\SeloPlayer_new.tmp", szDir);
    szNewExe[MAX_PATH - 1] = L'\0';

    wchar_t szOldExe[MAX_PATH];
    _snwprintf(szOldExe, MAX_PATH - 1, L"%s\\SeloPlayer_old.tmp", szDir);
    szOldExe[MAX_PATH - 1] = L'\0';

    DeleteFileW(szNewExe);

    wchar_t urlWithTs[1200];
    _snwprintf(urlWithTs, 1199, L"%s?t=%lu", downloadUrl, (unsigned long)GetTickCount());
    urlWithTs[1199] = L'\0';

    if (!HttpDownloadToFile(urlWithTs, szNewExe)) {
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

    HINSTANCE hInst = ShellExecuteW(NULL, L"open", szCurrentExe, NULL, szDir, SW_SHOWNORMAL);
    if ((INT_PTR)hInst > 32) {
        ExitProcess(0);
    }
    return TRUE;
}
