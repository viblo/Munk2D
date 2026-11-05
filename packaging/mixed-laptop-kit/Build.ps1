$ErrorActionPreference='Stop'
$Root=$env:GITHUB_WORKSPACE
$Tools=Join-Path $Root 'packaging/mixed-laptop-kit'
$Out=Join-Path $Root 'out'
$Package=Join-Path $Out 'munk2d-mixed-windows-x64'
New-Item -ItemType Directory -Force $Package | Out-Null
function Native([string]$File,[string[]]$Arguments) { & $File @Arguments; if($LASTEXITCODE -ne 0) { throw "Failed: $File $Arguments" } }
$env:GOFLAGS='-buildvcs=false'
Set-Location $Tools
Native go @('test','./cmd/runner')
$env:CGO_ENABLED='0';$env:GOAMD64='v1'
Native go @('build','-trimpath','-ldflags=-s -w','-o',(Join-Path $Package 'runner.exe'),'./cmd/runner')
Native go @('run','./cmd/prepare','--stage','sources','--root',$Root)
foreach($Variant in @('baseline','candidate')) {
 $Source=Join-Path $Root $Variant;$Build=Join-Path $Root ('build-'+$Variant)
 Native cmake @('-S',$Source,'-B',$Build,'-G','Visual Studio 17 2022','-A','x64','-DBUILD_DEMOS=OFF','-DBUILD_SHARED=OFF','-DBUILD_STATIC=ON','-DBUILD_TESTING=ON','-DBUILD_BENCHMARKS=ON','-DMUNK2D_ENABLE_LTO=ON','-DCMAKE_POLICY_DEFAULT_CMP0091=NEW','-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded')
 Native cmake @('--build',$Build,'--config','Release','--parallel','2')
 & ctest --test-dir $Build -C Release --output-on-failure 2>&1 | Tee-Object -FilePath (Join-Path $Out ('tests-'+$Variant+'.log'))
 if($LASTEXITCODE -ne 0) {throw 'CTest failed'}
 foreach($Driver in @('munkbench','munkbench_mixed')) { Copy-Item (Join-Path $Build "benchmarks/Release/$Driver.exe") (Join-Path $Package "$Driver-$Variant.exe") }
}
$VsWhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$Vs=(& $VsWhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Out-String).Trim()
$Tool=Get-ChildItem (Join-Path $Vs 'VC/Tools/MSVC') -Directory | Sort-Object Name -Descending | Select-Object -First 1
$Dumpbin=Join-Path $Tool.FullName 'bin/Hostx64/x64/dumpbin.exe'
$Compiler=(Get-Item (Join-Path $Tool.FullName 'bin/Hostx64/x64/cl.exe')).VersionInfo.FileVersion
foreach($File in Get-ChildItem $Package -Filter '*.exe') {
 $Deps=(& $Dumpbin /DEPENDENTS $File.FullName | Out-String)
 if($LASTEXITCODE -ne 0 -or $Deps -match '(?i)(VCRUNTIME\d*|MSVCP\d*|ucrtbase|chipmunk)\.dll') {throw 'Unexpected non-system DLL dependency'}
 Add-Content (Join-Path $Out 'dependencies.txt') $Deps
}
Native go @('run','./cmd/prepare','--stage','manifest','--root',$Root,'--package',$Package,'--compiler',$Compiler)
Copy-Item (Join-Path $Tools 'Run.cmd') $Package
Native (Join-Path $Package 'runner.exe') @('--mode','ci','--output',(Join-Path $Out 'self-test'))
'PASS: both complete MSVC /MT CTests, Go unit tests, repeated exact all26 state checks, mixed checkpoint matrix, 64-row ABBA/control/telemetry/archive self-test; system DLLs only.' | Set-Content (Join-Path $Package 'VERIFIED.txt')
Compress-Archive -Path $Package -DestinationPath (Join-Path $Out 'munk2d-mixed-windows-x64.zip') -CompressionLevel Optimal
