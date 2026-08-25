param(
    [ValidateSet('compile', 'gpu-smoke')]
    [string]$Mode = 'compile',
    [string]$OutputDir = 'flashbench/results'
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
$outputPath = Join-Path $OutputDir 'summary.json'
$logPath = Join-Path $OutputDir 'flashbench.log'

$summary = [ordered]@{
    schema = 'FLASHBENCH/1'
    mode = $Mode
    commit = $env:GITHUB_SHA
    machine = $env:COMPUTERNAME
    os = [Environment]::OSVersion.VersionString
    build_status = 'NOT_RUN'
    build_ms = 0
    shader_validation_status = 'NOT_RUN'
    shader_validation_ms = 0
    gpu = $null
    nvidia_driver = $null
    nvof_runtime_present = $false
    status = 'FAILED'
    error = $null
}

if (-not $summary.commit) {
    $summary.commit = (& git rev-parse HEAD 2>$null)
}

try {
    "FlashBench mode: $Mode" | Set-Content -Encoding utf8 $logPath
    "Commit: $($summary.commit)" | Add-Content -Encoding utf8 $logPath

    $sw = [Diagnostics.Stopwatch]::StartNew()
    & cmd.exe /d /c 'scripts\build.bat release' 2>&1 | Tee-Object -FilePath $logPath -Append
    $buildExit = $LASTEXITCODE
    $sw.Stop()
    $summary.build_ms = $sw.ElapsedMilliseconds
    if ($buildExit -ne 0) {
        throw "release build failed with exit code $buildExit"
    }
    $summary.build_status = 'SUCCESS'

    $sw.Restart()
    & .\FlashGuard.exe --validate-shaders
    $shaderExit = $LASTEXITCODE
    $sw.Stop()
    $summary.shader_validation_ms = $sw.ElapsedMilliseconds
    if ($shaderExit -ne 0) {
        throw "embedded HLSL validation failed with exit code $shaderExit"
    }
    $summary.shader_validation_status = 'SUCCESS'

    if ($Mode -eq 'gpu-smoke') {
        $smi = Get-Command nvidia-smi.exe -ErrorAction SilentlyContinue
        if (-not $smi) {
            throw 'nvidia-smi.exe was not found on the self-hosted GPU runner'
        }
        $gpuLine = (& $smi.Source --query-gpu=name,driver_version --format=csv,noheader 2>&1 | Select-Object -First 1)
        if ($LASTEXITCODE -ne 0 -or -not $gpuLine) {
            throw 'nvidia-smi failed to query the NVIDIA GPU'
        }
        $parts = $gpuLine -split ',', 2
        $summary.gpu = $parts[0].Trim()
        if ($parts.Count -gt 1) { $summary.nvidia_driver = $parts[1].Trim() }

        $nvofPath = Join-Path $env:SystemRoot 'System32\nvofapi64.dll'
        $summary.nvof_runtime_present = Test-Path $nvofPath
        if (-not $summary.nvof_runtime_present) {
            throw 'nvofapi64.dll was not found in Windows System32'
        }
    }

    $summary.status = 'SUCCESS'
}
catch {
    $summary.error = $_.Exception.Message
    "ERROR: $($summary.error)" | Add-Content -Encoding utf8 $logPath
}
finally {
    $summary | ConvertTo-Json -Depth 5 | Set-Content -Encoding utf8 $outputPath
}

if ($summary.status -ne 'SUCCESS') { exit 1 }
exit 0
