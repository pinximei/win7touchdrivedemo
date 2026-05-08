@echo off
rem setup_vm.bat — runs ON THE VM (not host). Pushed via deploy script.
rem Disables UAC, adds user to Administrators (already there for lebo on test VM),
rem installs the test root + TrustedPublisher cert, enables testsigning, reboots.
setlocal EnableExtensions

set HERE=%~dp0
echo [setup_vm] disabling UAC (EnableLUA=0)
reg add HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\policies\system /v EnableLUA /t REG_DWORD /d 0 /f
reg add HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\policies\system /v ConsentPromptBehaviorAdmin /t REG_DWORD /d 0 /f

echo [setup_vm] installing test root certificate
certutil -addstore Root             "%HERE%HidTouchTestCA.cer"
certutil -addstore TrustedPublisher "%HERE%HidTouchTestCA.cer"
rem PnP driver-trust on Win7 also needs the SIGNER cert in TrustedPublisher,
rem not just the CA. Without this, cat verify returns 0xE0000242.
certutil -addstore TrustedPublisher "%HERE%HidTouchTestSigner.cer"

echo [setup_vm] enabling testsigning
bcdedit /set testsigning on

echo [setup_vm] DONE. VM will reboot in 10 seconds. Reconnect via SSH after ~60s.
shutdown /r /t 10 /c "TouchDirve: enabling testsigning"
exit /b 0
