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
$smokeReportPath = Join-Path $OutputDir 'nvof-smoke.json'

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
    nvof_smoke_build_status = 'NOT_RUN'
    nvof_execute_status = 'NOT_RUN'
    nvof_grid = $null
    nvof_nonzero_vectors = $null
    nvof_total_vectors = $null
    nvof_mean_abs_flow_pixels = $null
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

    & cmd.exe /d /c 'flashbench\build-nvof-smoke.bat' 2>&1 | Tee-Object -FilePath $logPath -Append
    $smokeBuildExit = $LASTEXITCODE
    if ($smokeBuildExit -ne 0) {
        throw "NVOFA smoke helper build failed with exit code $smokeBuildExit"
    }
    $summary.nvof_smoke_build_status = 'SUCCESS'

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
        if ($smi) {
            $gpuOutput = @(& $smi.Source --query-gpu=name,driver_version --format=csv,noheader 2>&1)
            $smiExit = $LASTEXITCODE
            if ($smiExit -eq 0 -and $gpuOutput.Count -gt 0) {
                $gpuLine = [string]$gpuOutput[0]
                $parts = $gpuLine -split ',', 2
                $summary.gpu = $parts[0].Trim()
                if ($parts.Count -gt 1) { $summary.nvidia_driver = $parts[1].Trim() }
            } else {
                "WARN: nvidia-smi query failed (exit $smiExit): $($gpuOutput -join ' ')" | Add-Content -Encoding utf8 $logPath
            }
        } else {
            'WARN: nvidia-smi.exe not found; continuing with the real NVOFA smoke test.' | Add-Content -Encoding utf8 $logPath
        }

        $nvofPath = Join-Path $env:SystemRoot 'System32\nvofapi64.dll'
        $summary.nvof_runtime_present = Test-Path $nvofPath
        if (-not $summary.nvof_runtime_present) {
            'WARN: nvofapi64.dll not found in System32; LoadLibrary in NvofSmoke is authoritative.' | Add-Content -Encoding utf8 $logPath
        }

        Remove-Item $smokeReportPath -ErrorAction SilentlyContinue
        & .\build\NvofSmoke.exe $smokeReportPath 2>&1 | Tee-Object -FilePath $logPath -Append
        $nvofExit = $LASTEXITCODE
        $smoke = $null
        if (Test-Path $smokeReportPath) {
            $smoke = Get-Content -Raw $smokeReportPath | ConvertFrom-Json
            $summary.nvof_grid = $smoke.grid
            $summary.nvof_nonzero_vectors = $smoke.nonzero_vectors
            $summary.nvof_total_vectors = $smoke.total_vectors
            $summary.nvof_mean_abs_flow_pixels = $smoke.mean_abs_flow_pixels
        }
        if ($nvofExit -ne 0) {
            $stage = if ($smoke) { $smoke.stage } else { 'unknown' }
            throw "NvOFExecute smoke failed at $stage with exit code $nvofExit"
        }
        $summary.nvof_execute_status = 'SUCCESS'
        $summary.nvof_runtime_present = $true
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
