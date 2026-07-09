$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
python -m py_compile (Join-Path $scriptDir "blitzkrieg_export.py")
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
python -m unittest discover -s $scriptDir -p "test_*.py"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
