param(
    [ValidateSet('compile', 'gpu-smoke')]
    [string]$Mode = 'compile',
    [ValidateSet('quick', 'targeted', 'full')]
    [string]$TestTier = 'targeted',
    [string]$OutputDir = 'flashbench/results',
    [switch]$VisualReplay,
    [switch]$TuneMatrix
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
$trailMetricsPath = Join-Path $OutputDir 'trail-metrics.json'
$perceptualSweepPath = Join-Path $OutputDir 'perceptual-sweep.json'
$matrixReportPath = Join-Path $OutputDir 'matrix/matrix.json'
$visualDir = Join-Path $OutputDir 'visual'

$summary = [ordered]@{
    schema = 'FLASHBENCH/6'
    mode = $Mode
    test_tier = $TestTier
    commit = $env:GITHUB_SHA
    machine = $env:COMPUTERNAME
    os = [Environment]::OSVersion.VersionString
    build_status = 'NOT_RUN'
    build_ms = 0
    shader_validation_status = 'NOT_RUN'
    shader_validation_ms = 0
    risk_integrator_validation_status = 'NOT_RUN'
    risk_integrator_validation_ms = 0
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
    replay_protocol = $null
    replay_static_mae = $null
    replay_flash_reduction = $null
    flash_sweep_status = 'NOT_RUN'
    flash_sweep_protocol = $null
    flash_sweep_wcag_profile = $null
    flash_sweep_min_reduction = $null
    flash_sweep_max_output_flashes_per_second = $null
    flash_sweep_max_output_red_flashes_per_second = $null
    flash_sweep_max_output_g19_flashes_per_second = $null
    flash_sweep_max_internal_r16_epsilon_flashes_per_second = $null
    flash_sweep_all_sc_2_3_1_pass = $null
    flash_sweep_all_sc_2_3_2_pass = $null
    replay_moving_ghost_mae = $null
    replay_moving_inside_mae = $null
    replay_moving_edge_mae = $null
    replay_small_moving_ghost_mae = $null
    replay_pan_mae = $null
    replay_smooth_subpixel_scroll_mae = $null
    replay_integer_snapped_scroll_mae = $null
    replay_scroll_stop_recovery_ms = $null
    replay_saturated_red_motion_ghost_mae = $null
    replay_moving_flash_reduction = $null
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
    "Test tier: $TestTier" | Add-Content -Encoding utf8 $logPath
    "Commit: $($summary.commit)" | Add-Content -Encoding utf8 $logPath

    # Targeted/quick replay validates behavior, not optimized CPU codegen. Use
    # the lighter dev build to reduce compile time and commit pressure on the
    # persistent GPU runner; full and compile validation remain release builds.
    $buildMode = if ($Mode -eq 'gpu-smoke' -and $TestTier -ne 'full') { 'dev' } else { 'release' }
    "Build configuration: $buildMode" | Add-Content -Encoding utf8 $logPath
    $sw = [Diagnostics.Stopwatch]::StartNew()
    & cmd.exe /d /c "scripts\build.bat $buildMode" 2>&1 | Tee-Object -FilePath $logPath -Append
    $buildExit = $LASTEXITCODE
    $sw.Stop()
    $summary.build_ms = $sw.ElapsedMilliseconds
    if ($buildExit -ne 0) {
        throw "$buildMode build failed with exit code $buildExit"
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

    $sw.Restart()
    & .\FlashGuard.exe --validate-risk-integrator
    $riskIntegratorExit = $LASTEXITCODE
    $sw.Stop()
    $summary.risk_integrator_validation_ms = $sw.ElapsedMilliseconds
    if ($riskIntegratorExit -ne 0) {
        throw "risk integrator invariance validation failed with exit code $riskIntegratorExit"
    }
    $summary.risk_integrator_validation_status = 'SUCCESS'

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

        if ($TestTier -eq 'quick') {
            $summary.replay_status = 'SKIPPED'
            $summary.flash_sweep_status = 'SKIPPED'
            $summary['trail_metrics_status'] = 'SKIPPED'
            $summary['perceptual_sweep_status'] = 'SKIPPED'
            $summary.status = 'SUCCESS'
            return
        }

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
            $summary.replay_status = $replay.status
            $summary.replay_protocol = $replay.schema
            $summary.replay_static_mae = $replay.static_mae
            $summary.replay_flash_reduction = $replay.flash_reduction
            $summary.replay_moving_ghost_mae = $replay.moving_square_ghost_mae
            $summary.replay_moving_inside_mae = $replay.moving_square_inside_mae
            $summary.replay_moving_edge_mae = $replay.moving_square_edge_mae
            $summary.replay_small_moving_ghost_mae = $replay.small_moving_square_ghost_mae
            $summary.replay_pan_mae = $replay.pan_mae
            $summary.replay_smooth_subpixel_scroll_mae = $replay.smooth_subpixel_scroll_mae
            $summary.replay_integer_snapped_scroll_mae = $replay.integer_snapped_scroll_mae
            $summary.replay_scroll_stop_recovery_ms = $replay.scroll_stop_recovery_ms
            $summary.replay_saturated_red_motion_ghost_mae = $replay.saturated_red_motion_ghost_mae
            $summary.replay_moving_flash_reduction = $replay.moving_flash_reduction
            $summary.replay_nvof_flow_frames = $replay.nvof_flow_frames
            $summary.replay_pan_camera_motion = $replay.pan_camera_motion_mean
            $summary.replay_pan_affected_area = $replay.pan_affected_area_mean
            $summary.replay_pan_flow_frames = $replay.pan_flow_frames
        }
        if (Test-Path $flashSweepPath) {
            $flashSweep = Get-Content -Raw $flashSweepPath | ConvertFrom-Json
            $summary.flash_sweep_status = $flashSweep.status
            $summary.flash_sweep_protocol = $flashSweep.schema
            $summary.flash_sweep_wcag_profile = $flashSweep.wcag_profile
            if ($flashSweep.cases.Count -gt 0) {
                $summary.flash_sweep_min_reduction =
                    ($flashSweep.cases | Measure-Object -Property reduction -Minimum).Minimum
                $summary.flash_sweep_max_output_flashes_per_second =
                    ($flashSweep.cases |
                        Measure-Object -Property output_general_flashes_per_second -Maximum).Maximum
                $summary.flash_sweep_max_output_red_flashes_per_second =
                    ($flashSweep.cases |
                        Measure-Object -Property output_red_flashes_per_second -Maximum).Maximum
                $summary.flash_sweep_max_output_g19_flashes_per_second =
                    ($flashSweep.cases |
                        Measure-Object -Property output_g19_display_flashes_per_second -Maximum).Maximum
                $summary.flash_sweep_max_internal_r16_epsilon_flashes_per_second =
                    ($flashSweep.cases |
                        Measure-Object -Property output_internal_r16_epsilon_flashes_per_second -Maximum).Maximum
                $summary.flash_sweep_all_sc_2_3_1_pass =
                    @($flashSweep.cases | Where-Object { $_.wcag_sc_2_3_1_pass -ne $true }).Count -eq 0
                $summary.flash_sweep_all_sc_2_3_2_pass =
                    @($flashSweep.cases | Where-Object { $_.wcag_sc_2_3_2_pass -ne $true }).Count -eq 0
                $table = $flashSweep.cases |
                    Select-Object case, frequency_hz, reduction, peak_output_delta,
                        output_general_flashes_per_second, output_red_flashes_per_second,
                        output_g19_display_flashes_per_second,
                        output_internal_r16_epsilon_flashes_per_second,
                        wcag_sc_2_3_1_pass, wcag_sc_2_3_2_pass |
                    Format-Table -AutoSize | Out-String
                "Flash frequency sweep:`n$table" | Add-Content -Encoding utf8 $logPath
                Write-Host "Flash frequency sweep:"
                Write-Host $table
            }
        }
        if (Test-Path $trailMetricsPath) {
            $trail = Get-Content -Raw $trailMetricsPath | ConvertFrom-Json
            $summary['trail_metrics_status'] = $trail.status
            $summary['trail_metrics_protocol'] = $trail.schema
            $summary['replay_moving_vacated_mean_mae'] = $trail.moving_square_vacated_mean_mae
            $summary['replay_moving_vacated_p95_max'] = $trail.moving_square_vacated_p95_max
            $summary['replay_moving_vacated_p99_max'] = $trail.moving_square_vacated_p99_max
            $summary['replay_moving_vacated_peak'] = $trail.moving_square_vacated_peak
            $summary['replay_moving_vacated_area_02_max'] = $trail.moving_square_vacated_area_above_0_02_max
            $summary['replay_moving_vacated_area_05_max'] = $trail.moving_square_vacated_area_above_0_05_max
            $summary['replay_moving_trail_p99_frame_p95'] = $trail.moving_square_trail_p99_frame_p95
            $summary['replay_moving_trail_area_05_frame_mean'] = $trail.moving_square_trail_area_above_0_05_frame_mean
            $summary['replay_moving_recovery_p99_auc'] = $trail.moving_square_recovery_p99_auc
            $summary['replay_moving_recovery_area_05_auc'] = $trail.moving_square_recovery_area_above_0_05_auc
            $summary['replay_moving_recovery_p99_final'] = $trail.moving_square_recovery_p99_final
            $summary['replay_moving_clear_01_observed'] = $trail.moving_square_clear_to_0_01_observed
            $summary['replay_moving_clear_02_observed'] = $trail.moving_square_clear_to_0_02_observed
            $summary['replay_moving_clear_05_observed'] = $trail.moving_square_clear_to_0_05_observed
            $summary['replay_moving_clear_01_ms'] = $trail.moving_square_clear_to_0_01_ms
            $summary['replay_moving_clear_02_ms'] = $trail.moving_square_clear_to_0_02_ms
            $summary['replay_moving_clear_05_ms'] = $trail.moving_square_clear_to_0_05_ms
            $summary['replay_moving_clear_05_lower_bound_ms'] = $trail.moving_square_clear_to_0_05_lower_bound_ms
            $summary['replay_small_moving_vacated_p99_max'] = $trail.small_moving_square_vacated_p99_max
            $summary['replay_small_moving_vacated_peak'] = $trail.small_moving_square_vacated_peak
        }

        if (Test-Path $perceptualSweepPath) {
            $perceptual = Get-Content -Raw $perceptualSweepPath | ConvertFrom-Json
            $summary['perceptual_sweep_status'] = $perceptual.status
            $summary['perceptual_sweep_protocol'] = $perceptual.schema
            $summary['perceptual_sweep_case_count'] = @($perceptual.cases).Count
            if (@($perceptual.cases).Count -gt 0) {
                $summary['perceptual_sweep_min_reduction'] =
                    ($perceptual.cases | Measure-Object -Property reduction -Minimum).Minimum
                $summary['perceptual_sweep_max_output_delta'] =
                    ($perceptual.cases | Measure-Object -Property peak_output_delta -Maximum).Maximum
            }
        }

        # Canonical full-replay validation of stationary qualified-state
        # continuity after Matrix 26 isolated false disocclusion at 5-7.5 Hz.
        if ($TuneMatrix -or $TestTier -eq 'targeted' -or $TestTier -eq 'full') {
            $matrixDir = Join-Path $OutputDir 'matrix'
            & (Join-Path $PSScriptRoot 'matrix-v27.ps1') `
                -Executable (Join-Path $root 'FlashGuard.exe') `
                -OutputDir $matrixDir
            $matrixExit = $LASTEXITCODE
            if ($matrixExit -ne 0) {
                $summary['matrix_status'] = 'FAILED'
                "WARN: tuning matrix failed with exit code $matrixExit" |
                    Add-Content -Encoding utf8 $logPath
            } else {
                $summary['matrix_status'] = 'SUCCESS'
                "Tuning matrix: $matrixDir" | Add-Content -Encoding utf8 $logPath
                if (Test-Path $matrixReportPath) {
                    $matrix = Get-Content -Raw $matrixReportPath | ConvertFrom-Json
                    $summary['matrix_protocol'] = $matrix.schema
                    $summary['matrix_candidate_count'] = @($matrix.screen_candidates).Count
                    if ($matrix.selected) { $summary['matrix_selected'] = $matrix.selected.name }
                }
            }
        }

        $replayFailure = $null
        if ($replayExit -ne 0 -and -not $replay) {
            $summary.replay_status = 'FAILED'
        }
        if ($replayExit -ne 0) {
            $replayStage = if ($replay) { $replay.status } else { 'no-report' }
            $replayFailure =
                "synthetic replay failed ($replayStage) with exit code $replayExit"
        } elseif (-not $replay -or $replay.status -ne 'SUCCESS') {
            $replayFailure = 'synthetic replay did not produce a SUCCESS report'
        } elseif (-not (Test-Path $flashSweepPath) -or
                  $summary.flash_sweep_status -ne 'SUCCESS') {
            $replayFailure = '5-30 Hz flash sweep did not produce a SUCCESS report'
        } else {
            $summary.replay_status = 'SUCCESS'
        }

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

        if ($replayFailure) {
            throw $replayFailure
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
