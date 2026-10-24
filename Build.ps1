[CmdletBinding()]
param([switch]$AddressSanitizer,[switch]$Screenshot,[string]$SdkDirectory)
$ErrorActionPreference='Stop'
$vs=& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(!$vs){throw 'Visual Studio 2022 Desktop development with C++ is required.'}
$cmake=Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$ctest=Join-Path (Split-Path $cmake) 'ctest.exe'
$build=Join-Path $PSScriptRoot $(if($AddressSanitizer){'obj\Asan'}else{'obj\Build'})
$args=@('-S',$PSScriptRoot,'-B',$build,'-G','Visual Studio 17 2022','-A','x64',('-DENABLE_ASAN='+$(if($AddressSanitizer){'ON'}else{'OFF'})))
if($SdkDirectory){$args+=('-DSDK_DIRECTORY='+[IO.Path]::GetFullPath($SdkDirectory))}
if($AddressSanitizer){
 $toolset=Get-ChildItem -LiteralPath (Join-Path $vs 'VC\Tools\MSVC') -Directory|Where-Object {[version]$_.Name -ge [version]'14.38.0' -and (Test-Path -LiteralPath (Join-Path $_.FullName 'bin\Hostx64\x64\clang_rt.asan_dynamic-x86_64.dll'))}|Sort-Object {[version]$_.Name} -Descending|Select-Object -First 1
 if(!$toolset){throw 'Install the current Visual Studio C++ AddressSanitizer component.'}
 $args+=@('-T',('version='+$toolset.Name))
 $savedPath=$env:PATH
 $env:PATH=(Join-Path $toolset.FullName 'bin\Hostx64\x64')+';'+$env:PATH
}
try {
& $cmake @args
if($LASTEXITCODE){throw 'CMake configuration failed.'}
$buildArgs=@()
if($AddressSanitizer){$buildArgs=@('--',('/p:VCToolsVersion='+$toolset.Name),'/p:PreferredToolArchitecture=x64')}
& $cmake --build $build --config Release --parallel @buildArgs
if($LASTEXITCODE){throw 'C++ build failed.'}
& $ctest --test-dir $build -C Release --output-on-failure
if($LASTEXITCODE){throw 'Offline tests failed.'}
if($Screenshot){$process=Start-Process -FilePath (Join-Path $build 'bin\InputReplayLab.exe') -ArgumentList @('--screenshot',('"'+(Join-Path $PSScriptRoot 'docs\app.png')+'"')) -PassThru -WindowStyle Hidden;try{if(!$process.WaitForExit(30000)){throw 'Offline screenshot timed out.'};if($process.ExitCode){throw 'Offline screenshot failed.'}}finally{$process.Dispose()}}
Write-Host 'Build and offline checks passed. No driver was installed, opened or activated; no input was sent.'
} finally {if($AddressSanitizer){$env:PATH=$savedPath}}
