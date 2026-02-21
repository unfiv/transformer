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
./out/build/release/transformer <meshFile.obj> <boneWeightFile.json> <inverseBindPoseFile.json> <newPoseFile.json> <resultFile.obj> <statsFile.json>
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
В `statsFile.json` записывается время в миллисекундах:
- каждого модуля (чтение mesh/weights/poses, skinning, запись mesh);
- суммарное время `total` (на верхнем уровне).

# Окружение
Проект использует CMake/CMakePresets/clang-format, чтобы сохранять единый стиль кодирования, а также обеспечивать единообразность и простоту сборки/запуска на разных платформах. Также обеспечивается консистентность тулчейна, чтобы избежать проблем с различными версиями компилятора, линкера и т.д.

# Стресс-тестирование
Организовано отдельный стенд для запуска тестов: cmake target `stress`. Его задача запускать утилиту извне на базовом тесте 100 раз и выдавать статистику, включающую:
    * аппаратные особенности конкретного стенда
    * минимальное/максимальное/среднее время выполнения каждого этапа (чтение, skinning, запись)
Позволяет эффективно сравнивать оптимизации и изменения в коде в некоторой степени консистентно, а также отслеживать производительность на разных машинах.