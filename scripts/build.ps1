param(
    [ValidateSet('fast', 'dev', 'release')]
    [string]$Mode = 'fast'
)

$ErrorActionPreference = 'Stop'

# build.bat owns MSVC discovery/environment setup. Keeping this wrapper thin
# avoids doing the same Visual Studio probing twice.
& "$PSScriptRoot\build.bat" $Mode
if ($LASTEXITCODE -ne 0) {
    throw "Build failed with exit code $LASTEXITCODE"
}
