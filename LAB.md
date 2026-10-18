# Отчёт по лабораторной работе: CI с GitHub Actions

## Часть 1. Подготовка репозитория

Проект DataConverter (C++17) собран локально:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Результат: **13/13** тестов пройдено.

Структура: `src/` (приложение), `tests/` (GoogleTest), `third_party/nlohmann/` (JSON), `yaml-cpp` и GoogleTest подтягиваются через `FetchContent` в `CMakeLists.txt`.

CI запускается при изменении кода через workflow `.github/workflows/ci.yml` (триггеры `push` и `pull_request`).

## Часть 2–3. Конвейер и матрица ОС

Workflow выполняет на каждой ОС из матрицы:

1. `actions/checkout` — получение исходников  
2. установка Ninja (Linux/macOS)  
3. кэш `build/_deps`  
4. `cmake` configure → build → `ctest`  
5. загрузка артефактов (JUnit-отчёт и бинарник)

Матрица: `ubuntu-latest`, `macos-latest`, `windows-latest`. Стандарт C++17 задан в `CMakeLists.txt` одинаково для всех платформ.

### Почему успех на одной ОС не гарантирует успех на остальных?

- разные компиляторы и стандартные библиотеки (GCC / Clang / MSVC);
- разный filesystem (пути, регистр имён, CRLF/LF);
- разное поведение API и заголовочных файлов;
- зависимость от системных утилит и способов линковки.

## Часть 4. Параметр `fail-fast`

| Значение | Поведение |
|----------|-----------|
| `fail-fast: false` | при падении одного задания матрицы остальные **продолжают** работу; видно полный срез по ОС |
| `fail-fast: true`  | при первом падении остальные задания матрицы **отменяются** (cancelled); экономит минуты CI, но скрывает платформенные отличия |

Фактический эксперимент (сломали `Converter.CsvToJson`):

| Run | fail-fast | Ubuntu | macOS | Windows |
|-----|-----------|--------|-------|---------|
| [#2](https://github.com/LeroyElegante-dev/stp6/actions/runs/39129677972) | `false` | failure | failure | failure |
| [#3](https://github.com/LeroyElegante-dev/stp6/actions/runs/39129905776) | `true` | **cancelled** | failure | **cancelled** |

С `false` все три ОС доработали и упали на тесте. С `true` после первого падения (macOS) остальные задания матрицы отменились.

После эксперимента тесты восстановлены, в workflow оставлен `fail-fast: false`.

## Часть 5. Кэширование

Кэшируется каталог `build/_deps` (yaml-cpp, GoogleTest из FetchContent).

Ключ: `${{ runner.os }}-deps-${{ hashFiles('CMakeLists.txt') }}` — отдельно для каждой ОС и с учётом изменений зависимостей в CMake.

| Ситуация | Ожидание |
|----------|----------|
| первый запуск | cache miss, зависимости скачиваются и собираются |
| повтор без изменений | cache hit по `_deps`, ускорение configure/сборки зависимостей |
| изменение только `src/*.cpp` | ключ тот же → hit по deps, пересобирается только проект |
| изменение `CMakeLists.txt` | новый ключ → miss |

Кэш не ускоряет каждый запуск: при промахе, при изменении ключа, при холодном старте runner’а или если узкое место — компиляция самого проекта, а не deps.

## Часть 6. Артефакты

Сохраняются:

- `test-results-<os>` — JUnit XML отчёт `ctest --output-junit`;
- `dataconv-<os>` — собранный бинарник.

Скачать можно на странице workflow run → Artifacts.

### Кэш vs артефакты

| | Кэш | Артефакты |
|--|-----|-----------|
| Назначение | ускорить **следующие** запуски CI | сохранить **результаты** текущего запуска для людей/других job |
| Срок | внутренний для Actions, по ключу | явно загружаются и скачиваются из UI |
| Пример | `_deps`, node_modules | отчёты тестов, бинарники, coverage |
