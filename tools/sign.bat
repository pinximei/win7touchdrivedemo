@echo off
setlocal EnableExtensions

set ROOT=%~dp0..
set CERTDIR=%ROOT%\tools\cert
set PAYLOAD=%ROOT%\payload

set MAKECERT="C:\Program Files (x86)\Windows Kits\8.1\bin\x86\makecert.exe"
set PVK2PFX="C:\Program Files (x86)\Windows Kits\8.1\bin\x86\pvk2pfx.exe"
set SIGNTOOL="C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x86\signtool.exe"
set INF2CAT="C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x86\Inf2Cat.exe"

if not exist "%CERTDIR%" mkdir "%CERTDIR%"

if not exist "%CERTDIR%\HidTouchTestSigner.pfx" (
  echo [sign] generating self-signed test CA + signing cert
  %MAKECERT% -n "CN=TouchDirve Test CA" -r -pe -a sha1 -cy authority -sky signature -sv "%CERTDIR%\HidTouchTestCA.pvk" "%CERTDIR%\HidTouchTestCA.cer"
  if errorlevel 1 ( echo [sign] makecert root FAILED & exit /b 1 )
  %MAKECERT% -n "CN=TouchDirve Test Signer" -pe -a sha1 -cy end -sky signature -eku 1.3.6.1.5.5.7.3.3 -ic "%CERTDIR%\HidTouchTestCA.cer" -iv "%CERTDIR%\HidTouchTestCA.pvk" -sv "%CERTDIR%\HidTouchTestSigner.pvk" "%CERTDIR%\HidTouchTestSigner.cer"
  if errorlevel 1 ( echo [sign] makecert signer FAILED & exit /b 1 )
  %PVK2PFX% -pvk "%CERTDIR%\HidTouchTestSigner.pvk" -spc "%CERTDIR%\HidTouchTestSigner.cer" -pfx "%CERTDIR%\HidTouchTestSigner.pfx" -po ""
  if errorlevel 1 ( echo [sign] pvk2pfx signer FAILED & exit /b 1 )
)

REM ============================================================
REM x86 hidtouch
REM ============================================================
echo [sign] [x86] signing hidtouch.sys
%SIGNTOOL% sign /v /fd sha1 /f "%CERTDIR%\HidTouchTestSigner.pfx" /ac "%CERTDIR%\HidTouchTestCA.cer" "%PAYLOAD%\x86\hidtouch.sys"
if errorlevel 1 ( echo [sign] [x86] signtool sys FAILED & exit /b 1 )

echo [sign] [x86] regenerating hidtouch.cat
%INF2CAT% /driver:"%PAYLOAD%\x86" /os:7_X86 /verbose
if errorlevel 1 ( echo [sign] [x86] inf2cat FAILED & exit /b 1 )

echo [sign] [x86] signing hidtouch.cat
%SIGNTOOL% sign /v /fd sha1 /f "%CERTDIR%\HidTouchTestSigner.pfx" /ac "%CERTDIR%\HidTouchTestCA.cer" "%PAYLOAD%\x86\hidtouch.cat"
if errorlevel 1 ( echo [sign] [x86] signtool cat FAILED & exit /b 1 )

REM ============================================================
REM x64 hidtouch
REM ============================================================
echo [sign] [x64] signing hidtouch.sys
%SIGNTOOL% sign /v /fd sha1 /f "%CERTDIR%\HidTouchTestSigner.pfx" /ac "%CERTDIR%\HidTouchTestCA.cer" "%PAYLOAD%\x64\hidtouch.sys"
if errorlevel 1 ( echo [sign] [x64] signtool sys FAILED & exit /b 1 )

echo [sign] [x64] regenerating hidtouch.cat
%INF2CAT% /driver:"%PAYLOAD%\x64" /os:7_X64 /verbose
if errorlevel 1 ( echo [sign] [x64] inf2cat FAILED & exit /b 1 )

echo [sign] [x64] signing hidtouch.cat
%SIGNTOOL% sign /v /fd sha1 /f "%CERTDIR%\HidTouchTestSigner.pfx" /ac "%CERTDIR%\HidTouchTestCA.cer" "%PAYLOAD%\x64\hidtouch.cat"
if errorlevel 1 ( echo [sign] [x64] signtool cat FAILED & exit /b 1 )

REM ============================================================
REM x86 vmulti reference
REM ============================================================
if exist "%PAYLOAD%\x86\vmulti\vmulti.sys" (
  echo [sign] [x86] signing vmulti.sys
  %SIGNTOOL% sign /v /fd sha1 /f "%CERTDIR%\HidTouchTestSigner.pfx" /ac "%CERTDIR%\HidTouchTestCA.cer" "%PAYLOAD%\x86\vmulti\vmulti.sys"
  if errorlevel 1 ( echo [sign] [x86] signtool vmulti sys FAILED & exit /b 1 )

  echo [sign] [x86] regenerating vmulti.cat
  %INF2CAT% /driver:"%PAYLOAD%\x86\vmulti" /os:7_X86 /verbose
  if errorlevel 1 ( echo [sign] [x86] inf2cat vmulti FAILED & exit /b 1 )

  echo [sign] [x86] signing vmulti.cat
  %SIGNTOOL% sign /v /fd sha1 /f "%CERTDIR%\HidTouchTestSigner.pfx" /ac "%CERTDIR%\HidTouchTestCA.cer" "%PAYLOAD%\x86\vmulti\vmulti.cat"
  if errorlevel 1 ( echo [sign] [x86] signtool vmulti cat FAILED & exit /b 1 )
)

REM ============================================================
REM x64 vmulti reference
REM ============================================================
if exist "%PAYLOAD%\x64\vmulti\vmulti.sys" (
  echo [sign] [x64] signing vmulti.sys
  %SIGNTOOL% sign /v /fd sha1 /f "%CERTDIR%\HidTouchTestSigner.pfx" /ac "%CERTDIR%\HidTouchTestCA.cer" "%PAYLOAD%\x64\vmulti\vmulti.sys"
  if errorlevel 1 ( echo [sign] [x64] signtool vmulti sys FAILED & exit /b 1 )

  echo [sign] [x64] regenerating vmulti.cat
  %INF2CAT% /driver:"%PAYLOAD%\x64\vmulti" /os:7_X64 /verbose
  if errorlevel 1 ( echo [sign] [x64] inf2cat vmulti FAILED & exit /b 1 )

  echo [sign] [x64] signing vmulti.cat
  %SIGNTOOL% sign /v /fd sha1 /f "%CERTDIR%\HidTouchTestSigner.pfx" /ac "%CERTDIR%\HidTouchTestCA.cer" "%PAYLOAD%\x64\vmulti\vmulti.cat"
  if errorlevel 1 ( echo [sign] [x64] signtool vmulti cat FAILED & exit /b 1 )
)

echo [sign] OK
exit /b 0
