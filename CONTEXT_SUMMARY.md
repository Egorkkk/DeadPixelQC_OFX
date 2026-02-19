# Контекст проекта DeadPixelQC_OFX

## Обзор проекта
OpenFX плагин для DaVinci Resolve, обнаруживающий и исправляющий "мертвые" (застрявшие) пиксели на видео. Плагин использует трехэтапный алгоритм: пространственное детектирование, временное подтверждение и восстановление пикселей.

## Текущее состояние
✅ **Плагин успешно компилируется** в файл `DeadPixelQC_OFX.ofx` (89.6 KB)
⚠️ **Плагин не загружается в DaVinci Resolve** с ошибкой "failed to load"

## Выполненная работа

### 1. Реорганизация кода параметров
**Проблема:** Исходный файл `PluginParams.cpp` был слишком большим и монолитным.

**Решение:** Разбит на логические модули:
- `PluginParamsConstants.cpp` - константы и строковые значения
- `PluginParamsBasic.cpp` - основные параметры (Enable, ViewMode)
- `PluginParamsDetection.cpp` - параметры детектирования (LumaThreshold, WhitenessThreshold, Neighborhood, RobustZ, MaxClusterArea)
- `PluginParamsTemporal.cpp` - параметры временного анализа (TemporalMode, StuckWindowFrames, StuckMinFraction, MaxGapFrames)
- `PluginParamsRepair.cpp` - параметры восстановления (RepairEnable, RepairMethod)
- `PluginParamsGetters.cpp` - функции получения значений параметров
- `PluginParams.cpp` - главная функция объединения всех групп

**Результат:** Каждый файл ≤ 150 строк, код стал модульным и поддерживаемым.

### 2. Исправление ошибок компиляции
**Проблема:** Файл `OfxUtil.cpp` содержал устаревший код с ошибками:
- Неправильное использование OpenFX API
- Отсутствие заголовка `<mutex>`
- Несоответствие с заголовочным файлом `OfxUtil.h`

**Решение:**
- Удален старый `OfxUtil.cpp`
- Создан новый `OfxUtil.cpp` с определением глобальных переменных:
  ```cpp
  const OfxImageEffectSuiteV1* gEffectSuite = nullptr;
  const OfxPropertySuiteV1* gPropertySuite = nullptr;
  const OfxParameterSuiteV1* gParamSuite = nullptr;
  const OfxMemorySuiteV1* gMemorySuite = nullptr;
  const OfxMultiThreadSuiteV1* gMultiThreadSuite = nullptr;
  const OfxMessageSuiteV1* gMessageSuite = nullptr;
  ```

### 3. Обновление системы сборки
**Изменения в `CMakeLists.txt`:**
- Добавлены все новые файлы параметров в список исходных кодов
- Удалена ссылка на старый `OfxUtil.cpp`
- Добавлена ссылка на новый `OfxUtil.cpp`

### 4. Ключевые компоненты реализации

#### Ядро алгоритмов (core/)
- `SpatialDetector.h` - пространственное детектирование
- `TemporalTracker.h` - временное подтверждение
- `Fixer.h` - восстановление пикселей
- `ColorMetrics.h`, `ConnectedComponents.h` - вспомогательные алгоритмы

#### Плагин (plugin/)
- `PluginMain.cpp` - основные точки входа OpenFX
- `PluginParams.h` - заголовок системы параметров
- `OfxUtil.h`/`.cpp` - утилиты для работы с OpenFX API
- `DebugLog.h` - система логирования для отладки

#### Параметры плагина
- **Основные:** Enable (boolean), ViewMode (choice: Output, CandidatesOverlay, ConfirmedOverlay, MaskOnly)
- **Детектирование:** LumaThreshold (0.98), WhitenessThreshold (0.05), Neighborhood (3x3/5x5), RobustZ (10.0/8.0), MaxClusterArea (4)
- **Временной анализ:** TemporalMode (Off/SequentialOnly), StuckWindowFrames (60), StuckMinFraction (0.95), MaxGapFrames (2)
- **Восстановление:** RepairEnable (false), RepairMethod (NeighborMedian/DirectionalMedian)

### 5. Архитектурные решения
1. **Header-only реализация ядра** для производительности
2. **Потокобезопасность** без глобального состояния
3. **Приоритет точности над полнотой** (минимизация ложных срабатываний)
4. **Реальное время** для видео 1080p
5. **Логическое разделение параметров** по этапам работы алгоритма

## Проблемы для дальнейшей отладки

### Возможные причины ошибки загрузки:
1. **Неправильные точки входа OpenFX** в `PluginMain.cpp`
2. **Отсутствие обязательных действий** (kOfxActionLoad, kOfxActionDescribe, kOfxActionCreateInstance)
3. **Некорректные свойства плагина** для DaVinci Resolve
4. **Проблемы с инициализацией** в DllMain (Windows)
5. **Отсутствие зависимостей времени выполнения** (MSVCP140.dll, VCRUNTIME140_1.dll)

### Необходимые шаги для отладки:
1. Добавить расширенное логирование с самого начала выполнения
2. Проверить корректность экспортируемых функций
3. Убедиться в правильности дескриптора плагина
4. Проверить инициализацию глобальных переменных
5. Добавить обработку ошибок в каждой точке входа

## Файлы для анализа
- `plugin/PluginMain.cpp` - главные точки входа OpenFX
- `plugin/PluginParams.h`/`.cpp` - система параметров
- `ofx-sdk/include/` - заголовки OpenFX SDK
- `CMakeLists.txt` - конфигурация сборки

## Следующие шаги
1. Добавить детальное логирование для отслеживания выполнения
2. Проверить корректность OpenFX entry points
3. Проанализировать требования DaVinci Resolve к плагинам
4. Исправить найденные проблемы с загрузкой