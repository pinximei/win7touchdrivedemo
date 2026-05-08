@echo off
rem deploy.bat — host-side. Pushes payload to target Win7 box and triggers reinstall.
rem
rem Usage:
rem   deploy.bat                        : iterative push+reinstall, default target (old x86 VM)
rem   deploy.bat first                  : initial setup (UAC off, root cert, testsigning, reboot)
rem   deploy.bat --target newx64        : use named target (see TARGETS below)
rem   deploy.bat --target newx64 first  : first-run on named target
rem
setlocal EnableExtensions EnableDelayedExpansion

set ROOT=%~dp0..
set PAYLOAD=%ROOT%\payload
set CERTDIR=%ROOT%\tools\cert
set PLINK=C:\Program Files\PuTTY\plink.exe
set PSCP=C:\Program Files\PuTTY\pscp.exe

rem ---- parse args ----
set TARGET_NAME=oldvm
set FIRST=0
:parse_args
if "%~1"=="" goto args_done
if /I "%~1"=="--target" (
  set TARGET_NAME=%~2
  shift
  shift
  goto parse_args
)
if /I "%~1"=="first" (
  set FIRST=1
  shift
  goto parse_args
)
echo [deploy] unknown arg: %~1
exit /b 1
:args_done

rem ---- resolve target ----
if /I "%TARGET_NAME%"=="oldvm" (
  set HOSTUSER=lebo@192.168.254.129
  set PASS=1
  set FP=SHA256:BPnMk5SfqqdUnhUWFxCoD2mZ3K9ZOF4KTBqB+B5W/5Q
  set ARCH=x86
) else if /I "%TARGET_NAME%"=="newx64" (
  set HOSTUSER=hello@192.168.0.110
  set PASS=123
  set FP=SHA256:kuMfCngE+I+p74T01k2cBhCEToXK06o6yVx2XKbB/m0
  set ARCH=x64
) else (
  echo [deploy] unknown target name: %TARGET_NAME%
  echo [deploy] valid: oldvm, newx64
  exit /b 1
)
set TARGET=C:/hidtouch/

echo [deploy] target=%TARGET_NAME% (%HOSTUSER%, arch=%ARCH%) FIRST=%FIRST%

if not exist "%PLINK%" ( echo [deploy] plink not found at %PLINK% & exit /b 1 )
if not exist "%PAYLOAD%\%ARCH%\hidtouch.sys" (
  echo [deploy] payload\%ARCH%\hidtouch.sys not found — run build.bat + sign.bat first
  exit /b 1
)

echo [deploy] mkdir on target
"%PLINK%" -batch -ssh %HOSTUSER% -pw %PASS% -hostkey %FP% "if not exist C:\hidtouch mkdir C:\hidtouch"
if errorlevel 1 ( echo [deploy] mkdir FAILED & exit /b 1 )

echo [deploy] pushing arch-specific driver package (%ARCH%)
"%PSCP%" -batch -pw %PASS% -hostkey %FP% "%PAYLOAD%\%ARCH%\hidtouch.sys"            %HOSTUSER%:%TARGET% || exit /b 1
"%PSCP%" -batch -pw %PASS% -hostkey %FP% "%PAYLOAD%\%ARCH%\hidtouch.inf"            %HOSTUSER%:%TARGET% || exit /b 1
"%PSCP%" -batch -pw %PASS% -hostkey %FP% "%PAYLOAD%\%ARCH%\hidtouch.cat"            %HOSTUSER%:%TARGET% || exit /b 1
"%PSCP%" -batch -pw %PASS% -hostkey %FP% "%PAYLOAD%\%ARCH%\WdfCoInstaller01009.dll" %HOSTUSER%:%TARGET% || exit /b 1
"%PSCP%" -batch -pw %PASS% -hostkey %FP% "%PAYLOAD%\%ARCH%\devcon.exe"              %HOSTUSER%:%TARGET% || exit /b 1

echo [deploy] pushing user-mode binaries
"%PSCP%" -batch -pw %PASS% -hostkey %FP% "%PAYLOAD%\app.exe"        %HOSTUSER%:%TARGET% || exit /b 1
"%PSCP%" -batch -pw %PASS% -hostkey %FP% "%PAYLOAD%\stats.exe"      %HOSTUSER%:%TARGET% || exit /b 1
"%PSCP%" -batch -pw %PASS% -hostkey %FP% "%PAYLOAD%\testinj.exe"    %HOSTUSER%:%TARGET% || exit /b 1
"%PSCP%" -batch -pw %PASS% -hostkey %FP% "%PAYLOAD%\gesture.exe"    %HOSTUSER%:%TARGET% || exit /b 1
"%PSCP%" -batch -pw %PASS% -hostkey %FP% "%PAYLOAD%\sysprobe.exe"   %HOSTUSER%:%TARGET% || exit /b 1
"%PSCP%" -batch -pw %PASS% -hostkey %FP% "%PAYLOAD%\touchsink.exe"  %HOSTUSER%:%TARGET% || exit /b 1
"%PSCP%" -batch -pw %PASS% -hostkey %FP% "%PAYLOAD%\hidcaps.exe"    %HOSTUSER%:%TARGET% || exit /b 1

echo [deploy] pushing CA + signer certs and helper scripts
"%PSCP%" -batch -pw %PASS% -hostkey %FP% "%CERTDIR%\HidTouchTestCA.cer"      %HOSTUSER%:%TARGET% || exit /b 1
"%PSCP%" -batch -pw %PASS% -hostkey %FP% "%CERTDIR%\HidTouchTestSigner.cer"  %HOSTUSER%:%TARGET% || exit /b 1
"%PSCP%" -batch -pw %PASS% -hostkey %FP% "%ROOT%\tools\setup_vm.bat"         %HOSTUSER%:%TARGET% || exit /b 1
"%PSCP%" -batch -pw %PASS% -hostkey %FP% "%ROOT%\tools\reinstall.bat"        %HOSTUSER%:%TARGET% || exit /b 1

if "%FIRST%"=="1" (
  echo [deploy] FIRST RUN — running setup_vm.bat (will reboot the box)
  "%PLINK%" -batch -ssh %HOSTUSER% -pw %PASS% -hostkey %FP% "C:\hidtouch\setup_vm.bat"
  echo [deploy] target rebooting. Re-run deploy without "first" after ~90s.
  exit /b 0
)

echo [deploy] running reinstall.bat
"%PLINK%" -batch -ssh %HOSTUSER% -pw %PASS% -hostkey %FP% "C:\hidtouch\reinstall.bat"
if errorlevel 1 ( echo [deploy] reinstall FAILED & exit /b 1 )

echo [deploy] OK — run "C:\hidtouch\app.exe" on the target (RDP/console) to test.
exit /b 0
