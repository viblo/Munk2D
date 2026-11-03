# Native Windows AVX2 comparison. No installation, network access or admin rights.
[CmdletBinding()]
param(
    [ValidateSet('Full','Quick','CI')][string]$Mode = 'Full',
    [int]$Cpu = -1,
    [string]$OutputDirectory = '',
    [switch]$HelpersOnly
)
Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'
$Invariant = [Globalization.CultureInfo]::InvariantCulture
$Utf8 = New-Object Text.UTF8Encoding($false)
$script:ActiveProcess = $null

function Save-Text([string]$Path, [string]$Text) {
    [IO.File]::WriteAllText($Path, $Text, $Utf8)
}
function Save-Json([string]$Path, $Value) {
    Save-Text $Path (ConvertTo-Json -InputObject $Value -Depth 30)
}
function Median([double[]]$Values) {
    if ($Values.Count -eq 0) { throw 'Cannot take median of empty data.' }
    $s = @($Values | Sort-Object)
    if ($s.Count % 2) { return [double]$s[[int][Math]::Floor($s.Count / 2)] }
    return ([double]$s[$s.Count / 2 - 1] + [double]$s[$s.Count / 2]) / 2
}
function Parse-Timings([string]$Text, [string[]]$ExpectedScenes) {
    $lines = @($Text.Trim() -split '\r?\n')
    if ($lines.Count -lt 2 -or $lines[0] -cne 'version,benchmark,size,sample,batch,init_time,run_time') { throw 'Unexpected MunkBench CSV header.' }
    $rows = @($Text | ConvertFrom-Csv)
    if ($rows.Count -ne $ExpectedScenes.Count) { throw "Expected $($ExpectedScenes.Count) benchmark rows, got $($rows.Count)." }
    $seen = @{}
    $parsed = New-Object 'Collections.Generic.List[object]'
    foreach ($r in $rows) {
        if ($seen.ContainsKey($r.benchmark) -or $ExpectedScenes -cnotcontains $r.benchmark) { throw 'Missing, duplicate or unexpected benchmark row.' }
        $seen[$r.benchmark] = $true
        $time = [double]::Parse($r.run_time, $Invariant)
        $size = [int]::Parse($r.size, $Invariant)
        $batch = [int]::Parse($r.batch, $Invariant)
        if ([double]::IsNaN($time) -or [double]::IsInfinity($time) -or $time -le 0 -or $size -lt 1 -or $batch -lt 1 -or $r.sample -ne '1') { throw "Invalid timing/metadata for $($r.benchmark): time=$time, size=$size, batch=$batch, sample=$($r.sample) (expected sample 1)." }
        $parsed.Add([pscustomobject]@{ Benchmark=$r.benchmark; Size=$size; Batch=$batch; Time=$time })
    }
    return $parsed.ToArray()
}
function Check-Pair($Baseline, $Candidate) {
    if ($Baseline.Count -ne $Candidate.Count) { throw 'Pair row-count mismatch.' }
    for ($i=0; $i -lt $Baseline.Count; $i++) {
        if ($Baseline[$i].Benchmark -cne $Candidate[$i].Benchmark -or $Baseline[$i].Size -ne $Candidate[$i].Size -or $Baseline[$i].Batch -ne $Candidate[$i].Batch) { throw 'Baseline/candidate benchmark protocol mismatch.' }
    }
}
function Invoke-Engine([string]$Executable, [string[]]$Arguments) {
    # Arguments are internal fixed benchmark names/options; no shell is used.
    $info = New-Object Diagnostics.ProcessStartInfo
    $info.FileName = $Executable
    $info.WorkingDirectory = Split-Path -Parent $Executable
    $info.Arguments = $Arguments -join ' '
    $info.UseShellExecute = $false
    $info.CreateNoWindow = $true
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    $p = New-Object Diagnostics.Process
    $p.StartInfo = $info
    $script:ActiveProcess = $p
    try {
        if (-not $p.Start()) { throw 'Cannot start benchmark.' }
        $stdout = $p.StandardOutput.ReadToEndAsync()
        $stderr = $p.StandardError.ReadToEndAsync()
        if (-not $p.WaitForExit(900000)) { $p.Kill(); throw 'One benchmark invocation exceeded 15 minutes.' }
        $text = $stdout.Result
        $errors = $stderr.Result
        if ($p.ExitCode -ne 0) { throw "Benchmark failed (exit $($p.ExitCode)). AVX2 may be unavailable.`r`n$errors`r`n$text" }
        return $text
    } finally {
        if (-not $p.HasExited) { $p.Kill(); $p.WaitForExit() }
        $p.Dispose()
        $script:ActiveProcess = $null
    }
}
function Check-Summary([string]$Text) {
    $j = $Text | ConvertFrom-Json
    if (@($j.benchmarks).Count -ne 26) { throw 'Expected all 26 validation scenes.' }
    $seen = @{}
    foreach ($b in $j.benchmarks) {
        if ($seen.ContainsKey($b.benchmark)) { throw 'Duplicate validation scene.' }
        $seen[$b.benchmark] = $true
        foreach ($c in $b.checkpoints) { if ($c.invalid_values -ne 0) { throw "Non-finite physics state: $($b.benchmark)." } }
    }
    return $j
}
if ($HelpersOnly) { return }

$Package = Split-Path -Parent $MyInvocation.MyCommand.Path
$BaselineExe = Join-Path $Package 'munkbench-baseline.exe'
$CandidateExe = Join-Path $Package 'munkbench-candidate.exe'
if ($OutputDirectory -eq '') {
    $OutputDirectory = Join-Path $Package ('results\windows-avx2-' + $Mode + '-' + [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ') + '-' + $PID)
}
if (Test-Path -LiteralPath $OutputDirectory) { throw 'Results directory already exists; refusing to overwrite it.' }
[IO.Directory]::CreateDirectory($OutputDirectory) | Out-Null
$OutputDirectory = (Resolve-Path -LiteralPath $OutputDirectory).Path
$raw = New-Object 'Collections.Generic.List[object]'
$validation = New-Object 'Collections.Generic.List[object]'
$exitCode = 0
$self = [Diagnostics.Process]::GetCurrentProcess()
$oldAffinity = $self.ProcessorAffinity
$metadata = [ordered]@{ Status='Running'; StartedUtc=[DateTime]::UtcNow.ToString('o'); Mode=$Mode; Timer='QueryPerformanceCounter: high-resolution wall time inside MunkBench, physics stepping only'; AffinityCpu=$null; PowerShell=$PSVersionTable.PSVersion.ToString(); Executables=@(); Source=$null; Processor=$null; OS=$null; System=$null; PowerPlan=$null }

try {
    Write-Host 'Munk2D native Windows AVX2 comparison' -ForegroundColor Cyan
    Write-Host 'Plug into AC power; leave power settings unchanged between runs; close CPU-heavy apps.'
    Write-Host 'No installation, network access, security changes or administrator rights are used.'
    $manifest = Get-Content -LiteralPath (Join-Path $Package 'manifest.json') -Raw | ConvertFrom-Json
    $metadata.Source = $manifest
    foreach ($item in $manifest.executables) {
        $file = Join-Path $Package $item.name
        $hash = (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($hash -cne $item.sha256) { throw "Executable checksum mismatch: $($item.name). Re-extract the original package." }
        $metadata.Executables += [pscustomobject]@{ Name=$item.name; SHA256=$hash }
    }
    if ($metadata.Executables.Count -ne 2) { throw 'Expected exactly two executable checksums.' }
    $allowed = $oldAffinity.ToInt64()
    if ($Cpu -lt 0) {
        if (($allowed -band ([long]1 -shl 2)) -ne 0) { $Cpu = 2 }
        else { for ($n=0; $n -lt 64; $n++) { if (($allowed -band ([long]1 -shl $n)) -ne 0) { $Cpu=$n; break } } }
    }
    if ($Cpu -lt 0 -or $Cpu -ge 64 -or ($allowed -band ([long]1 -shl $Cpu)) -eq 0) { throw 'Requested logical CPU is not in this process affinity mask.' }
    # Children inherit this affinity before executing; no start/affinity race.
    $self.ProcessorAffinity = [IntPtr]([long]1 -shl $Cpu)
    $metadata.AffinityCpu = $Cpu
    try { $metadata.Processor = @(Get-CimInstance Win32_Processor | Select-Object Name,NumberOfCores,NumberOfLogicalProcessors,MaxClockSpeed) } catch {}
    try { $metadata.OS = Get-CimInstance Win32_OperatingSystem | Select-Object Caption,Version,BuildNumber,OSArchitecture } catch {}
    try { $metadata.System = Get-CimInstance Win32_ComputerSystem | Select-Object Manufacturer,Model,HypervisorPresent } catch {}
    try { $metadata.PowerPlan = (& powercfg.exe /GETACTIVESCHEME | Out-String).Trim() } catch {}
    Save-Json (Join-Path $OutputDirectory 'metadata.json') $metadata
    Write-Host "Mode: $Mode. Logical CPU: $Cpu. Results: $OutputDirectory"
    if ($Mode -eq 'Quick') { Write-Host 'QUICK is a selected-scene screen, NOT a full-suite conclusion.' -ForegroundColor Yellow }

    $allScenes = $null
    foreach ($profile in @('smoke','reference')) {
        foreach ($solver in @('original','avx2')) {
            Write-Host "Exact validation: $profile / $solver"
            $args = @('--profile',$profile,'--contact-solver',$solver,'--summary-json','--checkpoints','0,1,10,50,180,181,final')
            $a = Invoke-Engine $BaselineExe $args
            $b = Invoke-Engine $BaselineExe $args
            $c = Invoke-Engine $CandidateExe $args
            $e = Invoke-Engine $CandidateExe $args
            $parsed = Check-Summary $a
            $null = Check-Summary $c
            if ($allScenes -eq $null) { $allScenes = @($parsed.benchmarks | ForEach-Object { $_.benchmark }) }
            Save-Text (Join-Path $OutputDirectory "baseline-$profile-$solver.json") $a
            Save-Text (Join-Path $OutputDirectory "candidate-$profile-$solver.json") $c
            $record = [pscustomobject]@{ Profile=$profile; Solver=$solver; Exact=($a -ceq $c); BaselineDeterministic=($a -ceq $b); CandidateDeterministic=($c -ceq $e) }
            $validation.Add($record)
            Save-Json (Join-Path $OutputDirectory 'validation.json') ($validation.ToArray())
            if (-not $record.Exact -or -not $record.BaselineDeterministic -or -not $record.CandidateDeterministic) { throw 'Numerical validation mismatch; timings intentionally not trusted.' }
        }
    }
    if ($Mode -eq 'Full') { $scenes=$allScenes; $rounds=7; $profiles=@('smoke','reference') }
    elseif ($Mode -eq 'Quick') { $scenes=@('FallingSquares','FallingCircles','N2','MostlyStaticSingleBody','MostlyStaticMultiBody','FrictionalPyramid','CollisionCallbacks','SleepWake'); $rounds=3; $profiles=@('smoke','reference') }
    else { $scenes=@('FallingSquares','N2'); $rounds=2; $profiles=@('smoke','reference') }
    $cases = @(
        [pscustomobject]@{ Label='candidate-avx2'; Baseline=$BaselineExe; Candidate=$CandidateExe; Solver='avx2' },
        [pscustomobject]@{ Label='AA-control'; Baseline=$BaselineExe; Candidate=$BaselineExe; Solver='avx2' },
        [pscustomobject]@{ Label='original-control'; Baseline=$BaselineExe; Candidate=$CandidateExe; Solver='original' }
    )
    foreach ($profile in $profiles) {
        $common = @('--profile',$profile,'--batch','auto','--samples','1','-b') + $scenes
        Write-Host "Untimed warmups: $profile"
        foreach ($exe in @($BaselineExe,$CandidateExe)) { foreach ($solver in @('original','avx2')) { $null = Invoke-Engine $exe (@('--contact-solver',$solver) + $common) } }
        for ($round=0; $round -lt $rounds; $round++) {
            $order = @($cases)
            if ($round % 2) { [Array]::Reverse($order) }
            foreach ($case in $order) {
                Write-Host ("Paired batch: {0}, round {1}/{2}, {3}" -f $profile,($round+1),$rounds,$case.Label)
                $args = @('--contact-solver',$case.Solver) + $common
                if ($round % 2) {
                    $b = @(Parse-Timings (Invoke-Engine $case.Candidate $args) $scenes)
                    $a = @(Parse-Timings (Invoke-Engine $case.Baseline $args) $scenes)
                } else {
                    $a = @(Parse-Timings (Invoke-Engine $case.Baseline $args) $scenes)
                    $b = @(Parse-Timings (Invoke-Engine $case.Candidate $args) $scenes)
                }
                Check-Pair $a $b
                for ($i=0; $i -lt $a.Count; $i++) {
                    $raw.Add([pscustomobject]@{ Profile=$profile; Label=$case.Label; Round=$round; Benchmark=$a[$i].Benchmark; Size=$a[$i].Size; Batch=$a[$i].Batch; BaselineSeconds=$a[$i].Time; CandidateSeconds=$b[$i].Time; Ratio=($b[$i].Time / $a[$i].Time) })
                }
                Save-Json (Join-Path $OutputDirectory 'timing-pairs.json') ($raw.ToArray())
            }
        }
    }
    $summary = New-Object 'Collections.Generic.List[object]'
    $totals = New-Object 'Collections.Generic.List[object]'
    foreach ($profile in $profiles) { foreach ($case in $cases) {
        $logSum=0.0
        foreach ($scene in $scenes) {
            $r = @($raw.ToArray() | Where-Object { $_.Profile -eq $profile -and $_.Label -eq $case.Label -and $_.Benchmark -eq $scene })
            if ($r.Count -ne $rounds -or @($r | Select-Object -ExpandProperty Round -Unique).Count -ne $rounds) { throw 'Missing or duplicate timing pair.' }
            $ratios = @($r | ForEach-Object { [double]$_.Ratio })
            $m = Median $ratios
            $logSum += [Math]::Log($m)
            $summary.Add([pscustomobject]@{ Profile=$profile; Label=$case.Label; Benchmark=$scene; Percent=(100*($m-1)); FasterPairs=@($ratios | Where-Object { $_ -lt 1 }).Count; Pairs=$rounds; MinPercent=(100*(($ratios | Measure-Object -Minimum).Minimum-1)); MaxPercent=(100*(($ratios | Measure-Object -Maximum).Maximum-1)) })
        }
        $totals.Add([pscustomobject]@{ Profile=$profile; Label=$case.Label; Scenes=$scenes.Count; PairsPerScene=$rounds; GeomeanPercent=(100*([Math]::Exp($logSum/$scenes.Count)-1)) })
    } }
    Save-Json (Join-Path $OutputDirectory 'summary.json') ([pscustomobject]@{ Totals=$totals.ToArray(); Scenes=$summary.ToArray() })
    $csv = @($summary.ToArray() | Select-Object Profile,Label,Benchmark,@{n='Percent';e={$_.Percent.ToString('F4',$Invariant)}},FasterPairs,Pairs,@{n='MinPercent';e={$_.MinPercent.ToString('F4',$Invariant)}},@{n='MaxPercent';e={$_.MaxPercent.ToString('F4',$Invariant)}} | ConvertTo-Csv -NoTypeInformation)
    Save-Text (Join-Path $OutputDirectory 'summary.csv') ($csv -join "`r`n")
    $lines = @('Munk2D Windows AVX2 comparison',"Mode: $Mode; logical CPU: $Cpu; exact checks: PASS",'Negative = faster. candidate-avx2 compares PR #15 to existing AVX2, NOT to the original default.','AA-control is baseline versus itself; original-control compares candidate original versus baseline original.','Clock: QueryPerformanceCounter (high-resolution wall time), complete physics step; initialization/cleanup/process start excluded.','Do not draw conclusions from differences comparable to the AA control or from a single scene.')
    if ($Mode -eq 'Quick') { $lines += 'QUICK: selected 8 scenes / 3 paired rounds only; run Full before drawing a suite-wide conclusion.' }
    if ($Mode -eq 'CI') { $lines += 'CI SELF-TEST ONLY. These timings are not performance evidence.' }
    foreach ($t in $totals.ToArray()) { $lines += ('{0,-10} {1,-19} {2,8}%  ({3} scenes, {4} pairs/scene)' -f $t.Profile,$t.Label,$t.GeomeanPercent.ToString('+0.00;-0.00;0.00',$Invariant),$t.Scenes,$t.PairsPerScene) }
    $lines += ''; $lines += 'All per-scene results and raw timing pairs are retained, including regressions.'
    $text = $lines -join "`r`n"
    Save-Text (Join-Path $OutputDirectory 'SUMMARY.txt') $text
    Write-Host "`r`n$text"
    $metadata.Status='PASS'
} catch {
    $exitCode=1
    $metadata.Status='FAIL'
    Save-Text (Join-Path $OutputDirectory 'ERROR.txt') ($_.Exception.ToString())
    Write-Host "FAILED: $($_.Exception.Message)" -ForegroundColor Red
    Write-Host 'Please send the results ZIP even if this failed; it contains diagnostics.'
} finally {
    if ($script:ActiveProcess -ne $null) { try { if (-not $script:ActiveProcess.HasExited) { $script:ActiveProcess.Kill() } } catch {} }
    try { $self.ProcessorAffinity = $oldAffinity } catch {}
    if ($metadata.Status -eq 'Running') { $metadata.Status='Interrupted'; $exitCode=1 }
    $metadata['EndedUtc']=[DateTime]::UtcNow.ToString('o')
    Save-Json (Join-Path $OutputDirectory 'metadata.json') $metadata
    foreach ($item in $metadata.Executables) {
        try { if ((Get-FileHash -LiteralPath (Join-Path $Package $item.Name) -Algorithm SHA256).Hash.ToLowerInvariant() -cne $item.SHA256) { throw 'Executable changed while testing.' } } catch { $metadata.Status='FAIL'; $exitCode=1; Save-Text (Join-Path $OutputDirectory 'HASH-ERROR.txt') ($_.Exception.ToString()) }
    }
    Save-Json (Join-Path $OutputDirectory 'metadata.json') $metadata
    Compress-Archive -Path (Join-Path $OutputDirectory '*') -DestinationPath ($OutputDirectory + '.zip') -CompressionLevel Optimal
    Write-Host "`r`nPlease send this file back: $OutputDirectory.zip" -ForegroundColor Cyan
}
exit $exitCode
