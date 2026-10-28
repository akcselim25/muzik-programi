#ifndef SEARCH_H
#define SEARCH_H

#include <windows.h>

#define WM_APP_FILE_DOWNLOADED    (WM_APP + 102)
#define WM_APP_SINGLE_DL_PROGRESS (WM_APP + 103) // wParam: 0..100 yuzde, -1 hata/bitti

typedef void (*YtDlpProgressFn)(int percent, void *userData);

void Search_ShowDialog(HWND hParentWnd, const wchar_t *downloadFolder);
void Search_ShowDialogWithQuery(HWND hParentWnd, const wchar_t *downloadFolder, const wchar_t *initialQuery);
const wchar_t* Search_GetDownloadFolder(void);
void Search_GetActiveDownloadTitle(wchar_t *outTitle, int maxLen);

// Ortak canli yuzde okuyuculu yt-dlp indirme motoru (Tekil indirme, Paylasilan Liste ve Canli Jam tarafindan kullanilir)
BOOL Search_RunYtDlpDownload(const wchar_t *targetSpec,
                             const wchar_t *destFolder,
                             volatile LONG *pCancelFlag,
                             YtDlpProgressFn onProgress,
                             void *userData);

#endif // SEARCH_H
