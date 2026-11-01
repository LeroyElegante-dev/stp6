#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
mkdir -p processed
rm -f processed/*
printf './images\n./processed\n1\n2\n4\n' | ./build/image_processor.exe
echo
echo "Готово. Результаты в ./processed :"
ls processed
