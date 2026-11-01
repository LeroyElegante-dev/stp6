@echo off
cd /d %~dp0
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
if not exist processed mkdir processed
del /q processed\* 2>nul
(
echo ./images
echo ./processed
echo 1
echo 2
echo 4
) | build\image_processor.exe
echo.
echo Done. See .\processed
dir processed
