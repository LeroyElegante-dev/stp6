# Лабораторные 7 и 8 (одна ветка)

Репозиторий: ветка `lab7-8`.

```
lab7/   — обработка изображений (threads / mutex / queue)
lab8/   — патч «Стоп-кадр» + service_lab8.cpp
```

## Лаба 7 — запуск одной командой

```bash
cd lab7
./run.sh
```

Или вручную:
```bash
cd lab7
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/image_processor.exe
# input: ./images   output: ./processed
```

## Лаба 8 — патч к Stopkadr

В `lab8/` лежит патч (UI + `service_lab8.cpp` с реализованными TODO).  
Чтобы проверить GUI, скопируйте содержимое `lab8/` **поверх** своего проекта Stopkadr после лабы 7 (ODB/FLTK), затем пересоберите тот проект.

Полного Stopkadr в этом репозитории нет — только патч и реализация методов лабы 8.
