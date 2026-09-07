param(
    [string]$RunnerDir = 'C:\actions-runner',
    [string]$Token = '',
    [switch]$NoStart
)

$ErrorActionPreference = 'Stop'
$repo = 'Luci0n/FlashGuard'
$repoUrl = "https://github.com/$repo"

if (-not [Environment]::Is64BitOperatingSystem) {
    throw 'FlashGuard GPU runner requires 64-bit Windows.'
}

New-Item -ItemType Directory -Force -Path $RunnerDir | Out-Null
$runnerConfig = Join-Path $RunnerDir '.runner'
$runCmd = Join-Path $RunnerDir 'run.cmd'

if (-not (Test-Path $runnerConfig)) {
    if (-not $Token) {
        $gh = Get-Command gh.exe -ErrorAction SilentlyContinue
        if (-not $gh) {
            throw 'GitHub CLI (gh.exe) is required for automatic runner registration. Install it and run "gh auth login", or pass a current repository runner registration token with -Token.'
        }
        $Token = (& $gh.Source api --method POST "repos/$repo/actions/runners/registration-token" --jq .token 2>$null).Trim()
        if ($LASTEXITCODE -ne 0 -or -not $Token) {
            throw 'Could not obtain a self-hosted runner registration token. Run "gh auth login" as the repository owner, then retry.'
        }
    }

    $headers = @{ 'User-Agent' = 'FlashGuard-FlashBench' }
    $release = Invoke-RestMethod -Headers $headers -Uri 'https://api.github.com/repos/actions/runner/releases/latest'
    $asset = $release.assets | Where-Object { $_.name -like 'actions-runner-win-x64-*.zip' } | Select-Object -First 1
    if (-not $asset) {
        throw 'Could not locate the current Windows x64 GitHub Actions runner package.'
    }

    $zipPath = Join-Path $env:TEMP $asset.name
    Write-Host "Downloading $($asset.name)..."
    Invoke-WebRequest -Headers $headers -Uri $asset.browser_download_url -OutFile $zipPath
    Expand-Archive -Path $zipPath -DestinationPath $RunnerDir -Force
    Remove-Item $zipPath -Force -ErrorAction SilentlyContinue

    Push-Location $RunnerDir
    try {
        & .\config.cmd --unattended --url $repoUrl --token $Token `
            --name "flashguard-$env:COMPUTERNAME" --labels 'flashguard-gpu' `
            --work '_work' --replace
        if ($LASTEXITCODE -ne 0) {
            throw "GitHub runner configuration failed with exit code $LASTEXITCODE"
        }
    }
    finally {
        Pop-Location
    }
}

if (-not (Test-Path $runCmd)) {
    throw "Runner executable was not found at $runCmd"
}

Write-Host "FlashGuard runner is configured at $RunnerDir."
Write-Host 'It must run in this logged-in desktop session; do not install it as a Windows service.'

if (-not $NoStart) {
    Push-Location $RunnerDir
    try {
        & .\run.cmd
    }
    finally {
        Pop-Location
    }
}
