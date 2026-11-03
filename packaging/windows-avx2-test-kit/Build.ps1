# CI packaging only. Build pinned engines natively, test, then release just two EXEs.
[CmdletBinding()]
param([string]$Root=$env:GITHUB_WORKSPACE)
Set-StrictMode -Version 2.0
$ErrorActionPreference='Stop'
$Utf8=New-Object Text.UTF8Encoding($false)
$Tools=Join-Path $Root 'packaging/windows-avx2-test-kit'
$Out=Join-Path $Root 'out'
$Package=Join-Path $Out 'munk2d-avx2-windows-x64'
[IO.Directory]::CreateDirectory($Package) | Out-Null
function Native([string]$File,[string[]]$Arguments) {
    & $File @Arguments
    if ($LASTEXITCODE -ne 0) { throw "Native command failed ($LASTEXITCODE): $File $($Arguments -join ' ')" }
}
function Patch-Timer([string]$Source) {
    $path=Join-Path $Source 'benchmarks/munkbench.c'
    $text=[IO.File]::ReadAllText($path)
    $include=@'
#include <time.h>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
'@
    if ([regex]::Matches($text,'#include <time\.h>').Count -ne 1) { throw 'Timer include did not match exactly once.' }
    $text=$text.Replace('#include <time.h>',$include)
    $pattern='static double\s+now_seconds\(void\)\s*\{\s*return \(double\)clock\(\)/\(double\)CLOCKS_PER_SEC;\s*\}'
    if ([regex]::Matches($text,$pattern).Count -ne 1) { throw 'Timer function did not match exactly once.' }
    $replacement=@'
static double
now_seconds(void)
{
#ifdef _WIN32
    LARGE_INTEGER counter, frequency;
    if(!QueryPerformanceFrequency(&frequency) || !QueryPerformanceCounter(&counter)){
        fputs("Windows performance counter unavailable.\n", stderr);
        exit(EXIT_FAILURE);
    }
    return (double)counter.QuadPart/(double)frequency.QuadPart;
#else
    return (double)clock()/(double)CLOCKS_PER_SEC;
#endif
}
'@
    $text=[regex]::Replace($text,$pattern,$replacement)
    [IO.File]::WriteAllText($path,$text,$Utf8)
    $dirty=@(& git -C $Source diff --name-only HEAD)
    if ($LASTEXITCODE -ne 0 -or $dirty.Count -ne 1 -or $dirty[0] -ne 'benchmarks/munkbench.c') { throw 'Unexpected engine-source modification.' }
}
$refs=@(
    [pscustomobject]@{Label='baseline';Ref='3d2070976911b7e4b659a5365b0313f99dc98788'},
    [pscustomobject]@{Label='candidate';Ref='5784f8f48728e4fdd71638f0dc11aa26fa07f8ce'}
)
$exeInfo=@()
foreach ($r in $refs) {
    $source=Join-Path $Root $r.Label
    $head=(& git -C $source rev-parse HEAD | Out-String).Trim()
    if ($LASTEXITCODE -ne 0 -or $head -cne $r.Ref) { throw 'Source revision does not match the pinned engine.' }
    Patch-Timer $source
    $build=Join-Path $Root ('build-'+$r.Label)
    Native cmake @('-S',$source,'-B',$build,'-G','Visual Studio 17 2022','-A','x64','-DCMAKE_BUILD_TYPE=Release','-DBUILD_DEMOS=OFF','-DBUILD_SHARED=OFF','-DBUILD_STATIC=ON','-DBUILD_TESTING=ON','-DBUILD_BENCHMARKS=ON','-DMUNK2D_ENABLE_LTO=ON','-DCMAKE_POLICY_DEFAULT_CMP0091=NEW','-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded')
    Native cmake @('--build',$build,'--config','Release','--parallel','2')
    Push-Location $build
    try {
        & ctest -C Release --output-on-failure 2>&1 | Tee-Object -FilePath (Join-Path $Out ('tests-'+$r.Label+'.log'))
        if ($LASTEXITCODE -ne 0) { throw "CTest failed: $($r.Label)" }
    } finally { Pop-Location }
    $target=Join-Path $Package ('munkbench-'+$r.Label+'.exe')
    Copy-Item -LiteralPath (Join-Path $build 'benchmarks/Release/munkbench.exe') -Destination $target
    $exeInfo += [pscustomobject]@{name=[IO.Path]::GetFileName($target);sha256=(Get-FileHash $target -Algorithm SHA256).Hash.ToLowerInvariant();source=$r.Ref;patchedBenchmarkSHA256=(Get-FileHash (Join-Path $source 'benchmarks/munkbench.c') -Algorithm SHA256).Hash.ToLowerInvariant()}
}
if ($exeInfo[0].patchedBenchmarkSHA256 -cne $exeInfo[1].patchedBenchmarkSHA256) { throw 'The two benchmark harnesses are not byte-identical.' }
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vs=(& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Out-String).Trim()
$tool=Get-ChildItem (Join-Path $vs 'VC/Tools/MSVC') -Directory | Sort-Object Name -Descending | Select-Object -First 1
$dumpbin=Join-Path $tool.FullName 'bin/Hostx64/x64/dumpbin.exe'
$cl=Join-Path $tool.FullName 'bin/Hostx64/x64/cl.exe'
$dependencies=''
foreach ($r in $refs) {
    $exe=Join-Path $Package ('munkbench-'+$r.Label+'.exe')
    $info=(& $dumpbin /DEPENDENTS $exe | Out-String)
    if ($LASTEXITCODE -ne 0 -or $info -match '(?i)(VCRUNTIME\d*|MSVCP\d*|ucrtbase|chipmunk)\.dll') { throw 'Unexpected CRT/library DLL dependency; /MT was not effective.' }
    $dependencies += $info
}
[IO.File]::WriteAllText((Join-Path $Out 'dependencies.txt'),$dependencies,$Utf8)
foreach ($file in @('Run.cmd','Run.ps1','README.txt')) { Copy-Item -LiteralPath (Join-Path $Tools $file) -Destination $Package }
$manifest=[ordered]@{
    kit='Munk2D PR #15 Windows x64 AVX2 comparison'; baseline=$refs[0].Ref; candidate=$refs[1].Ref;
    compiler=(Get-Item $cl).VersionInfo.FileVersion; generator='Visual Studio 17 2022 / x64'; configuration='Release / default LTO ON / static library / static CRT (/MT)';
    timer='Identical benchmark-only QueryPerformanceCounter patch on both sources; high-resolution wall time inside physics step';
    buildRun=('https://github.com/'+$env:GITHUB_REPOSITORY+'/actions/runs/'+$env:GITHUB_RUN_ID); packagingCommit=$env:GITHUB_SHA;
    executables=$exeInfo; dependencies=$dependencies; tests='Both complete CTest suites passed before packaging; exact-state/runner self-test gates artifact publication.'
}
[IO.File]::WriteAllText((Join-Path $Package 'manifest.json'),(ConvertTo-Json -InputObject $manifest -Depth 10),$Utf8)
$patch=(& git -C (Join-Path $Root 'baseline') diff -- benchmarks/munkbench.c | Out-String)
[IO.File]::WriteAllText((Join-Path $Package 'TIMER-PATCH.txt'),$patch,$Utf8)
& powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Tools 'Verify.ps1') -Package $Package -Results (Join-Path $Out 'self-test')
if ($LASTEXITCODE -ne 0) { throw 'Runner verification failed; kit will not be published.' }
$verification="PASS: both native MSVC /MT Release suites, exact all-26 smoke/reference original/AVX2 repeated checks, parser/adversarial/Swedish-locale fixtures, paired AB/BA A/A/original-control end-to-end self-test. No non-system runtime DLL required.`r`nBuild run: $($manifest.buildRun)`r`n"
[IO.File]::WriteAllText((Join-Path $Package 'VERIFIED.txt'),$verification,$Utf8)
Compress-Archive -Path $Package -DestinationPath (Join-Path $Out 'munk2d-avx2-windows-x64.zip') -CompressionLevel Optimal
Write-Host 'PASS. Native Windows two-executable comparison package built and verified.'
