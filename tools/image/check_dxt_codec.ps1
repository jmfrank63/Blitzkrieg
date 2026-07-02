$ErrorActionPreference = 'Stop'

$root = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$testExe = Join-Path $env:TEMP 'BlitzkriegDxtCodecTest.exe'

Push-Location $root
try {
	& cl /nologo /EHsc /W4 /WX /DDXT_CODEC_STANDALONE /I Sources\src\Image tools\image\DxtCodecTest.cpp Sources\src\Image\DxtCodec.cpp /Fe:$testExe
	if ($LASTEXITCODE -ne 0) {
		exit $LASTEXITCODE
	}
	& $testExe
	exit $LASTEXITCODE
}
finally {
	Pop-Location
}
