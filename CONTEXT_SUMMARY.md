# DeadPixelQC OFX Plugin - Context Summary

## Что сделано:

### ✅ Полная реализация ядра (core/)
- **Types.h**: Общие типы, структуры, константы
- **PixelFormatAdapter.h**: Обработка форматов пикселей (RGB8, RGBA8, float)
- **ColorMetrics.h**: Метрики цвета (luma Rec.709, whiteness, robust z-score)
- **SpatialDetector.h**: Детектирование в одиночных кадрах (Phase 1)
- **ConnectedComponents.h**: Связные компоненты 8-connected
- **TemporalTracker.h**: Временное отслеживание (Phase 2)
- **Fixer.h**: Восстановление пикселей (Phase 3)

### ✅ Тестирование
- **UnitTests.cpp**: Все тесты проходят успешно
- **SyntheticGenerator.h/.cpp**: Генерация тестовых данных
- Результаты тестов: 100% прохождение всех алгоритмов

### ✅ Сборка
- **CMakeLists.txt**: Полностью рабочая конфигурация
- Плагин компилируется в `DeadPixelQC_OFX.ofx`
- Тесты компилируются и выполняются

### ✅ Документация
- **README.md**: Полная документация по сборке и использованию
- **docs/ARCHITECTURE.md**: Архитектура системы
- **docs/PARAMS.md**: Документация параметров

## Проблема:
Плагин **компилируется успешно**, но **не загружается в DaVinci Resolve**.

## Причины проблемы:

### 1. Неправильные OpenFX entry points
- Используются упрощенные функции (`OfxPluginMain`, `createInstance`)
- DaVinci Resolve ожидает стандартные OpenFX entry points из `ofxImageEffect.h`

### 2. Неполная OpenFX совместимость
- Отсутствуют `describe()`, `describeInContext()`, правильный `createInstance()`
- Нет интеграции с OpenFX parameter suite

### 3. Файлы, требующие исправления:
- `plugin/PluginMain.cpp` - нужны правильные OpenFX entry points
- `plugin/PluginParams.cpp` - ошибки компиляции из-за OpenFX API
- `plugin/OfxUtil.h/.cpp` - неполная реализация утилит OpenFX

## Что работает:
- Все алгоритмы детектирования (Phase 1-3)
- Тесты проходят на 100%
- CMake сборка работает
- Документация полная

## Что нужно исправить для DaVinci Resolve:
1. Реализовать стандартные OpenFX entry points в `PluginMain.cpp`
2. Исправить `PluginParams.cpp` для работы с OpenFX API
3. Убедиться в правильной интеграции с OpenFX SDK

## Ключевые файлы для отладки:
- `plugin/PluginMain.cpp` - главный файл плагина
- `plugin/PluginParams.cpp` - управление параметрами
- `ofx-sdk/include/` - заголовки OpenFX SDK

Плагин функционально завершен, осталось только сделать его совместимым с OpenFX для загрузки в DaVinci Resolve.