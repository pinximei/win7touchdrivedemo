@echo off
REM 从 Windows 启动文件夹移除 TouchDirve

setlocal
set STARTUP=%AppData%\Microsoft\Windows\Start Menu\Programs\Startup
set LNK=%STARTUP%\TouchDirve.lnk

echo [autostart] checking %LNK%
if exist "%LNK%" (
  del "%LNK%"
  if exist "%LNK%" (
    echo [autostart] FAILED -- could not delete shortcut
  ) else (
    echo [autostart] OK -- autostart shortcut removed
  )
) else (
  echo [autostart] not found (already removed or never installed)
)

REM Also clean up any leftover registry Run entry from old install method.
reg delete "HKCU\Software\Microsoft\Windows\CurrentVersion\Run" /v TouchDirve /f >nul 2>&1

echo.
pause
