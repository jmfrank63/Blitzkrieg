$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath $args[0]
$p = Start-Process -FilePath '.\Game.exe' -PassThru -Wait -NoNewWindow
$code = $p.ExitCode
Write-Host "ExitCode: $code (0x$($code.ToString('X8')))"