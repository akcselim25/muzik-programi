@echo off
zig rc /fo resource.res resource.rc
zig cc -Os -ffunction-sections -fdata-sections "-Wl,--gc-sections" -s src/main.c src/player.c src/playlist.c src/config.c src/search.c src/updater.c resource.res -o SeloPlayer.exe -luser32 -lgdi32 -lcomctl32 -lcomdlg32 -lshell32 -lshlwapi -lole32 -loleaut32 -lmfplay -ldwmapi -lurlmon -lmsimg32 "-Wl,--subsystem,windows"
if %errorlevel% equ 0 (
    echo Derleme basarili! SeloPlayer.exe olusturuldu.
) else (
    echo Derleme sirasinda hata olustu!
)
