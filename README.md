# DataConverter — учебный проект для лабораторной по CI/CD

DataConverter — CLI-утилита на C++17 для конвертации табличных данных между CSV, JSON и YAML.

## Сборка и тесты

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Запуск приложения:

```bash
./build/dataconv examples/people.csv people.json
```

## CI

Конвейер GitHub Actions: `.github/workflows/ci.yml`

- триггеры: `push`, `pull_request`
- матрица ОС: Ubuntu, macOS, Windows (`fail-fast: false`)
- кэш FetchContent-зависимостей
- артефакты: JUnit-отчёт тестов и бинарник

Подробный отчёт по лабораторной: [LAB.md](LAB.md)
