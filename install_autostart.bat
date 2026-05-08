@echo off
REM 把 app.exe 的快捷方式放到 Windows 启动文件夹，实现开机自启
REM 启动文件夹路径：%AppData%\Microsoft\Windows\Start Menu\Programs\Startup\
REM Windows 登录时 explorer 自动运行启动文件夹里所有 .lnk

setlocal
set STARTUP=%AppData%\Microsoft\Windows\Start Menu\Programs\Startup
set LNK=%STARTUP%\TouchDirve.lnk
set TARGET=C:\hidtouch\app.exe

echo [autostart] target startup folder: %STARTUP%
if not exist "%STARTUP%" (
  echo [autostart] startup folder not found
  pause
  exit /b 1
)

echo [autostart] creating shortcut %LNK%
REM Use PowerShell to create .lnk via WScript.Shell COM. This avoids needing
REM external utilities like shortcut.exe. Works on Win7 PowerShell 2.0.
powershell -Command "$ws = New-Object -ComObject WScript.Shell; $sc = $ws.CreateShortcut('%LNK%'); $sc.TargetPath = '%TARGET%'; $sc.WorkingDirectory = 'C:\hidtouch'; $sc.WindowStyle = 7; $sc.Description = 'TouchDirve global hotkey'; $sc.Save()"
if errorlevel 1 (
  echo [autostart] FAILED to create shortcut
  pause
  exit /b 1
)

if exist "%LNK%" (
  echo [autostart] OK -- shortcut created
  echo [autostart] TouchDirve will start automatically next time you log in.
) else (
  echo [autostart] FAILED -- shortcut file not present after creation
  pause
  exit /b 1
)
echo.
echo To remove autostart: run uninstall_autostart.bat
echo.
pause
