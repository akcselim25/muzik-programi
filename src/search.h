#ifndef SEARCH_H
#define SEARCH_H

#include <windows.h>

#define WM_APP_FILE_DOWNLOADED (WM_APP + 102)

void Search_ShowDialog(HWND hParentWnd, const wchar_t *downloadFolder);
const wchar_t* Search_GetDownloadFolder(void);

#endif // SEARCH_H
