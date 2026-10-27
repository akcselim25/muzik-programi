@echo off
zig rc /fo resource.res resource.rc
zig cc -O2 -ffunction-sections -fdata-sections "-Wl,--gc-sections" -s src/main.c src/player.c src/playlist.c src/config.c src/search.c src/updater.c src/network.c src/friends.c src/jam.c resource.res -o Aurns.exe -luser32 -lgdi32 -lcomctl32 -lcomdlg32 -lshell32 -lshlwapi -lole32 -loleaut32 -ldwmapi -lwinhttp -lws2_32 "-Wl,--subsystem,windows"
if %errorlevel% equ 0 (
    copy /Y Aurns.exe SeloPlayer.exe >nul
    echo Derleme basarili! Aurns.exe ve SeloPlayer.exe olusturuldu.
) else (
    echo Derleme sirasinda hata olustu!
)
