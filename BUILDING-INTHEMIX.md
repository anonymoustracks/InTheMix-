# Building InTheMix (Windows x64)

Tested on Windows 10, Visual Studio 2026 (MSVC 14.52, Windows SDK 10.0.26100), CMake 4 + Ninja, Qt 6.10.3 (msvc2022_64).

## Dependencies (not in the repository)

| What | Where it goes | Source |
|---|---|---|
| muse_deps @ 06d49c62dd6d4522e04fa05de3919311b4976806 | `muse_deps/` | https://github.com/musescore/muse_deps |
| ONNX Runtime 1.18.0 (win-x64, NuGet Microsoft.ML.OnnxRuntime) | `mixhouse_deps/onnxruntime-win-x64-1.18.0/` (`include/`, `lib/`) | https://www.nuget.org/packages/Microsoft.ML.OnnxRuntime/1.18.0 |
| beat_this.onnx (sha256 c5c1466e08abdb03...) | `mixhouse_deps/beat_this/beat_this.onnx` | https://github.com/mosynthkey/beat_this_cpp @07ab790a9ec2 |
| Rubber Band | fetched by CMake (`buildscripts/cmake/GlideMixRubberBand.cmake`) | https://breakfastquay.com/rubberband/ |
| FFmpeg 8.1 LGPL shared (installer only) | `bin/ffmpeg/` in the installer | https://github.com/BtbN/FFmpeg-Builds (win64-lgpl-shared) |

## Build

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_PREFIX_PATH=<Qt>\6.10.3\msvc2022_64 -DCMAKE_INSTALL_PREFIX=install
cmake --build build --parallel 3
cmake --install build
```

The app is `install\bin\InTheMix.exe`. Tests do not build with MSVC 14.52: configure with the `*_TESTS=OFF` options.

## Installer

Inno Setup 6, then:

```
powershell -ExecutionPolicy Bypass -File buildscripts\packaging\Windows\make_installer.ps1 -Version 1.0.0 -Label 1.0
```

(paths of the install prefix, FFmpeg and artwork are parameters of the script).

## Third-party components

| Component | Licence |
|---|---|
| Audacity 4, MuseScore framework, GlideMix | GPL v3 (many files GPL v2+) |
| Mixxx effects | GPL v2+ |
| Rubber Band | GPL v2+ |
| Ableton Link | GPL v2+ |
| qm-dsp | GPL v2 |
| mp3guessenc | LGPL 2.1 |
| beat_this model, beat_this_cpp | MIT |
| ONNX Runtime | MIT |
| FFmpeg (LGPL build) | LGPL 2.1+ |
| Manrope, Unbounded fonts | SIL OFL 1.1 |
