@echo off
setlocal EnableExtensions EnableDelayedExpansion

rem ---- Locate VS2022 (v143) ----
set VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat
if not exist "%VCVARS%" (
  echo [build] vcvarsall.bat not found at %VCVARS%
  exit /b 1
)
call "%VCVARS%" x86
if errorlevel 1 (
  echo [build] vcvarsall.bat failed
  exit /b 1
)

set ROOT=%~dp0..

echo [build] === driver (x86, Win7) ===
msbuild "%ROOT%\driver\hidtouch.vcxproj" /p:Configuration=Release /p:Platform=Win32 /m /v:minimal /nologo
if errorlevel 1 (
  echo [build] driver x86 build FAILED
  exit /b 1
)

echo [build] === driver (x64, Win7) ===
msbuild "%ROOT%\driver\hidtouch.vcxproj" /p:Configuration=Release /p:Platform=x64 /m /v:minimal /nologo
if errorlevel 1 (
  echo [build] driver x64 build FAILED
  exit /b 1
)

echo [build] === vmulti reference driver (x86) ===
msbuild "%ROOT%\vmulti\vmulti.vcxproj" /p:Configuration=Release /p:Platform=Win32 /m /v:minimal /nologo
if errorlevel 1 (
  echo [build] vmulti x86 build FAILED
  exit /b 1
)

echo [build] === vmulti reference driver (x64) ===
msbuild "%ROOT%\vmulti\vmulti.vcxproj" /p:Configuration=Release /p:Platform=x64 /m /v:minimal /nologo
if errorlevel 1 (
  echo [build] vmulti x64 build FAILED
  exit /b 1
)

echo [build] === app ===
msbuild "%ROOT%\app\app.vcxproj" /p:Configuration=Release /p:Platform=Win32 /m /v:minimal /nologo
if errorlevel 1 (
  echo [build] app build FAILED
  exit /b 1
)

echo [build] === stats.exe + testinj.exe (cl direct) ===
pushd "%ROOT%\build\app" 2>nul || (mkdir "%ROOT%\build\app" & pushd "%ROOT%\build\app")
cl /nologo /EHsc /MT /O2 /W3 "%ROOT%\app\stats.cpp" /Fe:stats.exe /link user32.lib >nul
if errorlevel 1 (
  echo [build] stats.exe build FAILED
  popd
  exit /b 1
)
cl /nologo /EHsc /MT /O2 /W3 "%ROOT%\app\testinj.cpp" /Fe:testinj.exe /link user32.lib setupapi.lib >nul
if errorlevel 1 (
  echo [build] testinj.exe build FAILED
  popd
  exit /b 1
)
cl /nologo /EHsc /MT /O2 /W3 "%ROOT%\app\gesture.cpp" /Fe:gesture.exe /link user32.lib >nul
if errorlevel 1 (
  echo [build] gesture.exe build FAILED
  popd
  exit /b 1
)
cl /nologo /EHsc /MT /O2 /W3 "%ROOT%\app\sysprobe.cpp" /Fe:sysprobe.exe /link user32.lib >nul
if errorlevel 1 (
  echo [build] sysprobe.exe build FAILED
  popd
  exit /b 1
)
cl /nologo /EHsc /MT /O2 /W3 "%ROOT%\app\vmulticli.cpp" /Fe:vmulticli.exe /link user32.lib setupapi.lib hid.lib >nul
if errorlevel 1 (
  echo [build] vmulticli.exe build FAILED
  popd
  exit /b 1
)

REM testvmulti is the original vmulti reference test program. Verifies vmulti
REM can dispatch WM_TOUCH using only vmulti-project code (no hidtouch deps).
cl /nologo /MT /O2 /W3 /I"%ROOT%\vmulti" "%ROOT%\vmulti\client.c" "%ROOT%\vmulti\testvmulti.c" /Fe:testvmulti.exe /link user32.lib setupapi.lib hid.lib >nul
if errorlevel 1 (
  echo [build] testvmulti.exe build FAILED
  popd
  exit /b 1
)
cl /nologo /EHsc /MT /O2 /W3 "%ROOT%\app\hidcaps.cpp" /Fe:hidcaps.exe /link user32.lib setupapi.lib hid.lib >nul
if errorlevel 1 (
  echo [build] hidcaps.exe build FAILED
  popd
  exit /b 1
)
cl /nologo /EHsc /MT /O2 /W3 "%ROOT%\app\touchsink.cpp" /Fe:touchsink.exe /link user32.lib gdi32.lib >nul
if errorlevel 1 (
  echo [build] touchsink.exe build FAILED
  popd
  exit /b 1
)
cl /nologo /EHsc /MT /O2 /W3 "%ROOT%\app\injtest.cpp" /Fe:injtest.exe /link user32.lib >nul
if errorlevel 1 (
  echo [build] injtest.exe build FAILED
  popd
  exit /b 1
)
popd

echo [build] === payload ===
if not exist "%ROOT%\payload"     mkdir "%ROOT%\payload"
if not exist "%ROOT%\payload\x86" mkdir "%ROOT%\payload\x86"
if not exist "%ROOT%\payload\x64" mkdir "%ROOT%\payload\x64"
if not exist "%ROOT%\payload\x86\vmulti" mkdir "%ROOT%\payload\x86\vmulti"
if not exist "%ROOT%\payload\x64\vmulti" mkdir "%ROOT%\payload\x64\vmulti"

REM ---- x86 driver package ----
copy /Y "%ROOT%\build\driver\Win32\hidtouch\hidtouch.sys"            "%ROOT%\payload\x86\" >nul
copy /Y "%ROOT%\build\driver\Win32\hidtouch\hidtouch.inf"            "%ROOT%\payload\x86\" >nul
copy /Y "%ROOT%\build\driver\Win32\hidtouch\hidtouch.cat"            "%ROOT%\payload\x86\" >nul
copy /Y "%ROOT%\build\driver\Win32\hidtouch\WdfCoInstaller01009.dll" "%ROOT%\payload\x86\" >nul
copy /Y "%ROOT%\build\vmulti\Win32\vmulti\vmulti.sys"                 "%ROOT%\payload\x86\vmulti\" >nul
copy /Y "%ROOT%\build\vmulti\Win32\vmulti\vmulti.cat"                 "%ROOT%\payload\x86\vmulti\" >nul
copy /Y "%ROOT%\build\vmulti\Win32\vmulti\vmulti.inf"                 "%ROOT%\payload\x86\vmulti\" >nul
copy /Y "%ROOT%\build\vmulti\Win32\vmulti\WdfCoInstaller01009.dll"    "%ROOT%\payload\x86\vmulti\" >nul

REM ---- x64 driver package ----
copy /Y "%ROOT%\build\driver\x64\hidtouch\hidtouch.sys"               "%ROOT%\payload\x64\" >nul
copy /Y "%ROOT%\build\driver\x64\hidtouch\hidtouch.inf"               "%ROOT%\payload\x64\" >nul
copy /Y "%ROOT%\build\driver\x64\hidtouch\hidtouch.cat"               "%ROOT%\payload\x64\" >nul
copy /Y "%ROOT%\build\driver\x64\hidtouch\WdfCoInstaller01009.dll"    "%ROOT%\payload\x64\" >nul
copy /Y "%ROOT%\build\vmulti\x64\vmulti\vmulti.sys"                   "%ROOT%\payload\x64\vmulti\" >nul
copy /Y "%ROOT%\build\vmulti\x64\vmulti\vmulti.cat"                   "%ROOT%\payload\x64\vmulti\" >nul
copy /Y "%ROOT%\build\vmulti\x64\vmulti\vmulti.inf"                   "%ROOT%\payload\x64\vmulti\" >nul
copy /Y "%ROOT%\build\vmulti\x64\vmulti\WdfCoInstaller01009.dll"      "%ROOT%\payload\x64\vmulti\" >nul

REM ---- user-mode binaries (x86 build, runs on x86 and x64 Win7 via WoW64) ----
copy /Y "%ROOT%\build\app\app.exe"        "%ROOT%\payload\" >nul
copy /Y "%ROOT%\build\app\stats.exe"      "%ROOT%\payload\" >nul
copy /Y "%ROOT%\build\app\testinj.exe"    "%ROOT%\payload\" >nul
copy /Y "%ROOT%\build\app\gesture.exe"    "%ROOT%\payload\" >nul
copy /Y "%ROOT%\build\app\sysprobe.exe"   "%ROOT%\payload\" >nul
copy /Y "%ROOT%\build\app\touchsink.exe"  "%ROOT%\payload\" >nul
copy /Y "%ROOT%\build\app\vmulticli.exe"  "%ROOT%\payload\" >nul
copy /Y "%ROOT%\build\app\testvmulti.exe" "%ROOT%\payload\" >nul
copy /Y "%ROOT%\build\app\hidcaps.exe"    "%ROOT%\payload\" >nul
copy /Y "%ROOT%\build\app\injtest.exe"    "%ROOT%\payload\" >nul

rem devcon: ship both x86 and x64 (target VM picks based on its own arch)
for %%P in (
  "C:\Program Files (x86)\Windows Kits\8.1\Tools\x86\devcon.exe"
  "C:\Program Files (x86)\Windows Kits\10\Tools\10.0.22621.0\x86\devcon.exe"
) do (
  if exist %%P (
    copy /Y %%P "%ROOT%\payload\x86\devcon.exe" >nul
    goto :devcon_x86_ok
  )
)
echo [build] x86 devcon.exe NOT found
exit /b 1
:devcon_x86_ok

for %%P in (
  "C:\Program Files (x86)\Windows Kits\8.1\Tools\x64\devcon.exe"
  "C:\Program Files (x86)\Windows Kits\10\Tools\10.0.22621.0\x64\devcon.exe"
) do (
  if exist %%P (
    copy /Y %%P "%ROOT%\payload\x64\devcon.exe" >nul
    goto :devcon_x64_ok
  )
)
echo [build] x64 devcon.exe NOT found
exit /b 1
:devcon_x64_ok

echo [build] OK
exit /b 0
