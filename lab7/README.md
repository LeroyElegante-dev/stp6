# Лабораторная 7 — обработка изображений

Консольная утилита: grayscale / invert, последовательный и параллельный режимы (`std::thread`, `std::mutex`, `std::queue`).

## Сборка

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

## Запуск

```bash
./build/image_processor
# Windows: build\image_processor.exe
```

Ввод: входная директория → выходная → операция (1–3) → режим (1–2) → число потоков (если parallel).

Выходные файлы: `<имя>.processed.png` в выходной директории.
