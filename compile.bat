@echo off
C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe /target:winexe /r:System.dll /r:System.Windows.Forms.dll /r:System.Drawing.dll /r:WindowsBase.dll /r:PresentationCore.dll /r:PresentationFramework.dll /out:MYTPlayer.exe Program.cs
if %errorlevel% equ 0 (
    echo Derleme basarili! MYTPlayer.exe olusturuldu.
) else (
    echo Derleme sirasinda hata olustu!
)
pause
