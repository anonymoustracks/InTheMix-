# InTheMix: builds the Windows setup from the CMake install prefix.
#   powershell -ExecutionPolicy Bypass -File make_installer.ps1 [-Version 1.0.0] [-Label 1.0]
# Stages install\ (without GlideMix.exe / *.pdb), adds FFmpeg (LGPL shared
# build, avformat/avcodec/avutil/swresample only) and every licence, then runs ISCC.
param(
    [string]$Version = "1.0.0",
    [string]$Label = "1.0",
    [string]$Install = "D:\InTheMix\install",
    [string]$Stage = "D:\InTheMix\stage",
    [string]$FFmpeg = "D:\InTheMix\downloads\ffmpeg\ffmpeg-n8.1-latest-win64-lgpl-shared-8.1",
    [string]$Art = "D:\InTheMix\tools\installer",
    [string]$Out = "D:\InTheMix\release",
    [string]$Iscc = "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe"
)
$ErrorActionPreference = "Stop"
$src = Resolve-Path "$PSScriptRoot\..\..\.."

if (Test-Path $Stage) { Remove-Item $Stage -Recurse -Force }
robocopy $Install $Stage /E /XF GlideMix.exe *.pdb /NFL /NDL /NJH /NJS /NP | Out-Null
if ($LASTEXITCODE -ge 8) { throw "robocopy failed ($LASTEXITCODE)" }

# FFmpeg: only what Audacity's loader needs.
$ff = New-Item -ItemType Directory -Force "$Stage\bin\ffmpeg"
foreach ($p in "avformat-*.dll", "avcodec-*.dll", "avutil-*.dll", "swresample-*.dll") {
    Copy-Item "$FFmpeg\bin\$p" $ff
}

# Licences: InTheMix's own (GPL) and every bundled component's.
$lic = "$Stage\licenses"
function Add-Licence($name, $file) {
    $d = New-Item -ItemType Directory -Force "$lic\$name"
    Copy-Item $file $d
}
Copy-Item "$src\LICENSE.txt" "$Stage\LICENSE.txt"
Add-Licence "ffmpeg (LGPL)" "$FFmpeg\LICENSE.txt"
Add-Licence "rubberband" "D:\InTheMix\build\_deps\rubberband-src\COPYING"
Add-Licence "beat_this model" "$src\src\mixhouse\thirdparty\beat_this_model\LICENSE"
Add-Licence "beat_this_cpp" "$src\src\mixhouse\thirdparty\beat_this_cpp\LICENSE"
Add-Licence "qm-dsp" "$src\src\mixhouse\thirdparty\qm-dsp\COPYING"
Add-Licence "mp3guessenc" "$src\src\mixhouse\thirdparty\mp3guessenc\COPYING"
Add-Licence "ableton link" "$src\thirdparty\link\LICENSE.md"
Add-Licence "onnxruntime" "D:\InTheMix\downloads\ort_nuget\LICENSE"
Add-Licence "onnxruntime" "D:\InTheMix\downloads\ort_nuget\ThirdPartyNotices.txt"
Add-Licence "font manrope (OFL)" "$src\muse\framework\ui\data\manrope\OFL.txt"
Add-Licence "font unbounded (OFL)" "$src\muse\framework\ui\data\unbounded\OFL.txt"
@"
InTheMix by Anonymous Tracks
Copyright (C) 2026 Anonymous Tracks

InTheMix is free software, released under the GNU General Public License
version 3 (see LICENSE.txt). It is based on GlideMix (GPL), which is built on
Audacity (GPL v3), the MuseScore framework (GPL v3) and Mixxx effects (GPL v2+).
The source code is available from the InTheMix repository on GitHub.
Bundled components and their licences are in this folder.
"@ | Set-Content "$lic\README.txt" -Encoding UTF8

New-Item -ItemType Directory -Force $Out | Out-Null
& $Iscc /Q "/DStageDir=$Stage" "/DAppVersion=$Version" "/DAppVersionLabel=$Label" "/DArtDir=$Art" "/DOutDir=$Out" "$PSScriptRoot\inthemix.iss"
if ($LASTEXITCODE -ne 0) { throw "ISCC failed ($LASTEXITCODE)" }
Get-ChildItem $Out -Filter "InTheMix-*setup.exe" | Sort LastWriteTime | Select -Last 1 | % { "$($_.FullName)  $([math]::Round($_.Length/1MB)) MB" }
