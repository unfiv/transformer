# Transformer (CPU Mesh Skinning)
Консольное приложение для CPU skinning'а OBJ-меша на основе:
- весов/индексов костей на вершину (до 4 влияний);
- inverse bind pose матриц;
- матриц новой позы.

## Структура исходников
- `src/app` — оркестрация сценария выполнения приложения.
- `src/io` — парсинг входных данных и запись выходных файлов (OBJ/JSON/stats).
- `src/skinning` — CPU skinning.
- `src/core` — базовые типы, математика, профилировщик.

## Сборка
Проект собирается через `CMakePresets.json`.

### Linux/macOS (Ninja)
```bash
cmake --preset release
cmake --build --preset build
```

Для других типов сборки:
- Debug: `cmake --preset debug && cmake --build --preset build-debug`
- RelWithDebInfo: `cmake --preset relwithdeb && cmake --build --preset build-relwithdeb`

### Windows (новое устройство / MSVC + Ninja)
```powershell
cmake --preset windows-msvc-release
cmake --build --preset build-win-release
```

Для отладки на Windows используйте `windows-msvc-debug` + `build-win-debug`.

## Запуск
```bash
./out/build/release/transformer --mesh <meshFile.obj> --bones-weights <boneWeightFile.json> --inverse-bind-pose <inverseBindPoseFile.json> --new-pose <newPoseFile.json> --output <resultFile.obj> --stats <statsFile.json> [--bench <N>]
```

## Формат `boneWeightFile.json`
```json
{
  "vertices": [
    {
      "bone_indices": [0, 4, 9, 15],
      "weights": [0.6, 0.2, 0.2, 0.0]
    }
  ]
}
```

## Формат pose json (`inverseBindPoseFile.json` и `newPoseFile.json`)
```json
{
  "bones": [
    {
      "matrix": [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]
    }
  ]
}
```
Матрица передаётся в **column-major** порядке.

## Профилирование
В `statsFile.json` записывается время в микросекундах (с точностью до 3 знаков после запятой):
- каждого модуля (чтение mesh/weights/poses, skinning, запись mesh);
- суммарное время `total` (на верхнем уровне).

Если передан `--bench <N>`, операция `cpu_skinning` выполняется `N` раз, а в `statsFile.json` дополнительно пишется сводка: `min/max/mean/median/stddev` по этим `N` прогонам.

# Окружение
Проект использует CMake/CMakePresets/clang-format, чтобы сохранять единый стиль кодирования, а также обеспечивать единообразность и простоту сборки/запуска на разных платформах. Также обеспечивается консистентность тулчейна, чтобы избежать проблем с различными версиями компилятора, линкера и т.д.

# Стресс-тестирование
Организован отдельный стенд для запуска тестов: cmake target `stress`.
Теперь это **один** запуск утилиты с параметром `--bench 100` (число настраивается через `STRESS_RUNS`), без внешнего цикла в CMake.

Агрегированная статистика берётся из `statsFile.json` приложения и включает:
- `mean`, `median`, `stddev`, `min`, `max` (в микросекундах) для `cpu_skinning` по `N` прогонам.

## Ошибки CLI
- При неизвестном аргументе, пропущенном значении флага или отсутствии обязательных флагов приложение завершится с кодом `1` и выведет usage.
- `--bench` принимает только положительное целое число.
