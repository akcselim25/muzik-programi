#ifndef PLAYER_H
#define PLAYER_H

#include <windows.h>

#define WM_APP_TRACK_ENDED (WM_APP + 101)

int  Player_Init(HWND hWndNotify);
void Player_Shutdown(void);
int  Player_OpenFile(const wchar_t *path);
void Player_Play(void);
void Player_Pause(void);
void Player_Stop(void);
int  Player_IsPlaying(void);
int  Player_IsPaused(void);
int  Player_GetDuration(void);
int  Player_GetPosition(void);
void Player_SetPosition(int seconds);
void Player_SetVolume(int volumePercent); // 0 - 100
int  Player_GetVolume(void);

#endif // PLAYER_H
