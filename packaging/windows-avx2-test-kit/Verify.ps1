# CI-only tests of runner parsing, pairing guards and a short end-to-end run.
param([Parameter(Mandatory=$true)][string]$Package, [Parameter(Mandatory=$true)][string]$Results)
Set-StrictMode -Version 2.0
$ErrorActionPreference='Stop'
. (Join-Path $Package 'Run.ps1') -HelpersOnly
# Standard SHA-256 vectors, a literal bracket/space path, and handle disposal.
$fixture=Join-Path ([IO.Path]::GetDirectoryName($Results)) ('hash [fixture] '+[Guid]::NewGuid().ToString('N')+'.tmp')
try {
    [IO.File]::WriteAllBytes($fixture,[Text.Encoding]::ASCII.GetBytes('abc'))
    if ((Get-FileSha256 $fixture) -cne 'ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad') { throw 'SHA-256 abc fixture failed.' }
    [IO.File]::WriteAllBytes($fixture,[byte[]]@())
    if ((Get-FileSha256 $fixture) -cne 'e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855') { throw 'SHA-256 empty fixture failed.' }
} finally { if ([IO.File]::Exists($fixture)) { [IO.File]::Delete($fixture) } }
Write-Host 'PASS .NET SHA-256 vectors and released file handles; no hash cmdlet required.'
if ((Median @(0.8,0.9,1.2)) -ne 0.9 -or (Median @(0.8,1.0)) -ne 0.9) { throw 'Median fixture failed.' }
$header='version,benchmark,size,sample,batch,init_time,run_time'
$good="$header`n2.1.0,Test,10,1,4,0.1,2.0`n"
$a=@(Parse-Timings $good @('Test'))
if ($a.Count -ne 1 -or $a[0].Time -ne 2.0) { throw 'CSV parser positive fixture failed.' }
$badCases=@(
    "$header`n2.1.0,Test,10,1,4,0.1,NaN`n",
    "$header`n2.1.0,Test,10,1,4,0.1,0`n",
    "$header`n2.1.0,Test,10,1,4,0.1,Infinity`n",
    "$header`n2.1.0,Test,10,1,4,0.1,2`n2.1.0,Test,10,1,4,0.1,2`n",
    "$header`n2.1.0,Wrong,10,1,4,0.1,2`n",
    "$header`n2.1.0,Test,10,0,4,0.1,2`n"
)
foreach ($text in $badCases) { $rejected=$false; try { $null=Parse-Timings $text @('Test') } catch { $rejected=$true }; if (-not $rejected) { throw 'Malformed CSV was accepted.' } }
$b=@(Parse-Timings ($good.Replace(',4,',',5,')) @('Test'))
$rejected=$false;try { Check-Pair $a $b } catch { $rejected=$true };if (-not $rejected) { throw 'Pair metadata mismatch was accepted.' }
# Sweden-style comma-decimal locale must not corrupt C-locale CSV parsing.
$oldCulture=[Threading.Thread]::CurrentThread.CurrentCulture
try { [Threading.Thread]::CurrentThread.CurrentCulture=[Globalization.CultureInfo]::GetCultureInfo('sv-SE');$a=@(Parse-Timings $good @('Test'));if ($a[0].Time -ne 2) { throw 'Locale fixture failed.' } } finally { [Threading.Thread]::CurrentThread.CurrentCulture=$oldCulture }
Write-Host 'PASS parser, NaN/zero/infinity/duplicate/protocol guards, median and Swedish locale fixtures.'
# Force any accidental Get-FileHash dependency to fail, including final checks.
$quotedRunner=(Join-Path $Package 'Run.ps1').Replace("'","''")
$quotedResults=$Results.Replace("'","''")
$command="function global:Get-FileHash { throw 'Get-FileHash deliberately unavailable in compatibility test' }; & '$quotedRunner' -Mode CI -OutputDirectory '$quotedResults'; exit `$LASTEXITCODE"
& powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -Command $command
if ($LASTEXITCODE -ne 0) { throw 'End-to-end native Windows runner without Get-FileHash failed.' }
$meta=Get-Content (Join-Path $Results 'metadata.json') -Raw | ConvertFrom-Json
# Windows PowerShell 5.1 emits a JSON root array as ONE pipeline object.
# Assign directly rather than wrapping that pipeline in an extra @(...).
$rows=Get-Content (Join-Path $Results 'timing-pairs.json') -Raw | ConvertFrom-Json
$valid=Get-Content (Join-Path $Results 'validation.json') -Raw | ConvertFrom-Json
if ($meta.Status -ne 'PASS' -or $rows.Count -ne 24 -or $valid.Count -ne 4 -or -not (Test-Path ($Results+'.zip'))) { throw "Wrong self-test output: status=$($meta.Status), rows=$($rows.Count), validations=$($valid.Count), zip=$(Test-Path ($Results+'.zip'))." }
Write-Host 'PASS native Windows end-to-end self-test: all 26-scene exact checks plus paired CSV/control pipeline.'
