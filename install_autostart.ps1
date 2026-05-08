$startup = [Environment]::GetFolderPath('Startup')
Write-Host "StartupFolder=$startup"
$lnk = Join-Path $startup 'TouchDirve.lnk'
$ws = New-Object -ComObject WScript.Shell
$sc = $ws.CreateShortcut($lnk)
$sc.TargetPath = 'C:\hidtouch\app.exe'
$sc.WorkingDirectory = 'C:\hidtouch'
$sc.WindowStyle = 7
$sc.Description = 'TouchDirve global hotkey'
$sc.Save()
if (Test-Path $lnk) { Write-Host "CREATED=$lnk" } else { Write-Host "FAILED" }
