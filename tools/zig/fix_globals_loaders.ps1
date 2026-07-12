param(
    [string]$Root = "Sources\src"
)

$files = Get-ChildItem -Recurse -Filter "GlobalsLoader.cpp" $Root
$fixed = 0

foreach ($f in $files) {
    $content = Get-Content $f.FullName -Raw
    $original = $content

    # Skip if already fixed (has null check)
    if ($content -match 'if \( pStreamIO != 0 \)') {
        Write-Host "Already fixed: $($f.FullName)"
        continue
    }

    # Skip if no StreamIO loading
    if ($content -notmatch '_DONT_LOAD_STREAMIO') {
        Write-Host "No StreamIO load: $($f.FullName)"
        continue
    }

    # Pattern: NWin32Helper::CDLLHandle *pStreamIO = LoadModule( "streamio.dll" );
    #          if ( GETSLS_HOOK pfnGetSLS_Hook = pStreamIO->GetProcAddress
    # Replace with null check wrapper

    $content = $content -replace '(NWin32Helper::CDLLHandle \*pStreamIO = LoadModule\( "streamio\.dll" \);)\r?\n\t\tif \( GETSLS_HOOK', '$1
		if ( pStreamIO != 0 )
		{
			if ( GETSLS_HOOK'

    $content = $content -replace '(dllhandles\.push_back\( pStreamIO \);)\r?\n#endif // _DONT_LOAD_STREAMIO', '$1
		}
#endif // _DONT_LOAD_STREAMIO'

    if ($content -ne $original) {
        Set-Content $f.FullName -Value $content -NoNewline
        Write-Host "Fixed: $($f.FullName)"
        $fixed++
    } else {
        Write-Host "No change: $($f.FullName)"
    }
}

Write-Host "`nFixed $fixed files"