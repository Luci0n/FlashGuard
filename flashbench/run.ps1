param(
    [ValidateSet('compile', 'gpu-smoke')]
    [string]$Mode = 'compile',
    [string]$OutputDir = 'flashbench/results',
    [switch]$VisualReplay
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
$outputPath = Join-Path $OutputDir 'summary.json'
$logPath = Join-Path $OutputDir 'flashbench.log'
$smokeReportPath = Join-Path $OutputDir 'nvof-smoke.json'
$replayReportPath = Join-Path $OutputDir 'synthetic-replay.json'
$flashSweepPath = Join-Path $OutputDir 'flash-sweep.json'
$visualDir = Join-Path $OutputDir 'visual'

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
    replay_status = 'NOT_RUN'
    replay_static_mae = $null
    replay_flash_reduction = $null
    flash_sweep_status = 'NOT_RUN'
    flash_sweep_min_reduction = $null
    flash_sweep_max_output_flashes_per_second = $null
    replay_moving_ghost_mae = $null
    replay_moving_inside_mae = $null
    replay_moving_edge_mae = $null
    replay_small_moving_ghost_mae = $null
    replay_pan_mae = $null
    replay_nvof_flow_frames = $null
    replay_pan_camera_motion = $null
    replay_pan_affected_area = $null
    replay_pan_flow_frames = $null
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

        Remove-Item $replayReportPath -ErrorAction SilentlyContinue
        if ($VisualReplay) {
            Remove-Item $visualDir -Recurse -Force -ErrorAction SilentlyContinue
            New-Item -ItemType Directory -Force -Path $visualDir | Out-Null
            & .\FlashGuard.exe --synthetic-replay $replayReportPath `
                --synthetic-replay-visual $visualDir 2>&1 |
                Tee-Object -FilePath $logPath -Append
        } else {
            & .\FlashGuard.exe --synthetic-replay $replayReportPath 2>&1 |
                Tee-Object -FilePath $logPath -Append
        }
        $replayExit = $LASTEXITCODE
        $replay = $null
        if (Test-Path $replayReportPath) {
            $replay = Get-Content -Raw $replayReportPath | ConvertFrom-Json
            $summary.replay_static_mae = $replay.static_mae
            $summary.replay_flash_reduction = $replay.flash_reduction
            $summary.replay_moving_ghost_mae = $replay.moving_square_ghost_mae
            $summary.replay_moving_inside_mae = $replay.moving_square_inside_mae
            $summary.replay_moving_edge_mae = $replay.moving_square_edge_mae
            $summary.replay_small_moving_ghost_mae = $replay.small_moving_square_ghost_mae
            $summary.replay_pan_mae = $replay.pan_mae
            $summary.replay_nvof_flow_frames = $replay.nvof_flow_frames
            $summary.replay_pan_camera_motion = $replay.pan_camera_motion_mean
            $summary.replay_pan_affected_area = $replay.pan_affected_area_mean
            $summary.replay_pan_flow_frames = $replay.pan_flow_frames
        }
        if (Test-Path $flashSweepPath) {
            $flashSweep = Get-Content -Raw $flashSweepPath | ConvertFrom-Json
            $summary.flash_sweep_status = $flashSweep.status
            if ($flashSweep.cases.Count -gt 0) {
                $summary.flash_sweep_min_reduction =
                    ($flashSweep.cases | Measure-Object -Property reduction -Minimum).Minimum
                $summary.flash_sweep_max_output_flashes_per_second =
                    ($flashSweep.cases |
                        Measure-Object -Property output_general_flashes_per_second -Maximum).Maximum
                $table = $flashSweep.cases |
                    Select-Object case, frequency_hz, reduction, peak_output_delta,
                        output_general_flashes_per_second, output_red_flashes_per_second |
                    Format-Table -AutoSize | Out-String
                "Flash frequency sweep:`n$table" | Add-Content -Encoding utf8 $logPath
                Write-Host "Flash frequency sweep:"
                Write-Host $table
            }
        }
        if ($replayExit -ne 0) {
            $replayStage = if ($replay) { $replay.status } else { 'no-report' }
            throw "synthetic replay failed ($replayStage) with exit code $replayExit"
        }
        if (-not $replay -or $replay.status -ne 'SUCCESS') {
            throw 'synthetic replay did not produce a SUCCESS report'
        }
        if (-not (Test-Path $flashSweepPath) -or $summary.flash_sweep_status -ne 'SUCCESS') {
            throw '5-30 Hz flash sweep did not produce a SUCCESS report'
        }
        $summary.replay_status = 'SUCCESS'

        if ($VisualReplay -and (Test-Path $visualDir)) {
            $caseMap = [ordered]@{}
            Get-ChildItem -Path $visualDir -Directory | Sort-Object Name | ForEach-Object {
                $caseMap[$_.Name] = @(
                    Get-ChildItem -Path $_.FullName -Filter '*.bmp' |
                        Sort-Object Name | ForEach-Object { $_.Name }
                )
            }
            $caseJson = $caseMap | ConvertTo-Json -Depth 4 -Compress
            $viewerPath = Join-Path $visualDir 'index.html'
            $html = @"
<!doctype html>
<html>
<head>
<meta charset="utf-8">
<title>FlashGuard Visual Replay</title>
<style>
body { margin:0; background:#111; color:#eee; font:14px Segoe UI, sans-serif; }
header { position:sticky; top:0; background:#1b1b1b; padding:12px 16px; z-index:2; }
.controls { display:flex; gap:12px; align-items:center; flex-wrap:wrap; }
select, button, input { font:inherit; }
input[type=range] { width:min(520px,60vw); }
.labels { display:grid; grid-template-columns:repeat(3,1fr); text-align:center; padding:8px 0; font-weight:600; }
main { padding:0 16px 18px; }
img { display:block; width:100%; height:auto; border:1px solid #333; background:#000; }
#meta { opacity:.75; min-width:110px; }
</style>
</head>
<body>
<header>
  <div class="controls">
    <select id="case"></select>
    <button id="play">Play</button>
    <input id="frame" type="range" min="0" max="0" value="0">
    <span id="meta"></span>
  </div>
</header>
<main>
  <div class="labels"><span>SOURCE</span><span>FILTERED</span><span>6x DIFFERENCE</span></div>
  <img id="image" alt="FlashGuard replay frame">
</main>
<script>
const framesByCase = $caseJson;
const caseSelect = document.getElementById('case');
const slider = document.getElementById('frame');
const image = document.getElementById('image');
const meta = document.getElementById('meta');
const play = document.getElementById('play');
let timer = null;
for (const name of Object.keys(framesByCase)) {
  const option = document.createElement('option');
  option.value = name; option.textContent = name; caseSelect.appendChild(option);
}
function refresh() {
  const caseName = caseSelect.value;
  const frames = framesByCase[caseName] || [];
  slider.max = Math.max(0, frames.length - 1);
  const index = Math.min(Number(slider.value) || 0, Math.max(0, frames.length - 1));
  slider.value = index;
  if (frames.length) image.src = encodeURIComponent(caseName) + '/' + encodeURIComponent(frames[index]);
  meta.textContent = frames.length ? (String(index + 1) + ' / ' + String(frames.length)) : 'no frames';
}
caseSelect.addEventListener('change', () => { slider.value = 0; refresh(); });
slider.addEventListener('input', refresh);
play.addEventListener('click', () => {
  if (timer) { clearInterval(timer); timer = null; play.textContent = 'Play'; return; }
  play.textContent = 'Pause';
  timer = setInterval(() => {
    const max = Number(slider.max) || 0;
    slider.value = Number(slider.value) >= max ? 0 : Number(slider.value) + 1;
    refresh();
  }, 167);
});
refresh();
</script>
</body>
</html>
"@
            Set-Content -Path $viewerPath -Value $html -Encoding utf8
            "Visual replay viewer: $viewerPath" | Add-Content -Encoding utf8 $logPath
            Write-Host "Visual replay viewer: $viewerPath"
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
