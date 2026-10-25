#ifndef PLAYLIST_H
#define PLAYLIST_H

#include <windows.h>

typedef struct {
    wchar_t filePath[MAX_PATH];
    wchar_t title[MAX_PATH];
} PlaylistItem;

void Playlist_Init(void);
void Playlist_Free(void);
int  Playlist_Count(void);
const PlaylistItem* Playlist_Get(int index);
int  Playlist_Add(const wchar_t *filePath);
void Playlist_Remove(int index);
void Playlist_Move(int fromIndex, int toIndex);
void Playlist_Clear(void);
int  Playlist_FindNext(int currentIndex, int shuffle, int repeat);
int  Playlist_FindPrev(int currentIndex);
int  Playlist_ScanFolder(const wchar_t *folderPath);
int  Playlist_ImportM3U(const wchar_t *filePath);
int  Playlist_ExportM3U(const wchar_t *filePath);
int  Playlist_FindByPrefix(wchar_t prefixChar, int startIndex);
void Playlist_CleanTitle(const wchar_t *path, wchar_t *outTitle, int maxLen);

void Playlist_SplitArtistSong(const wchar_t *fullTitle, wchar_t *outArtist, int artistMax, wchar_t *outSong, int songMax);
void Playlist_ExtractYouTubeId(const wchar_t *filePath, wchar_t *outId, int maxLen);
int  Playlist_FindBestMatch(const wchar_t *targetTitle, const wchar_t *targetYtId);
BOOL Playlist_IsSameTrack(int index, const wchar_t *targetTitle, const wchar_t *targetYtId);

#endif // PLAYLIST_H
