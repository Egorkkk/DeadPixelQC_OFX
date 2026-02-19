# Структура параметров плагина DeadPixelQC

Параметры разбиты по логическим группам в соответствии с этапами работы плагина:

## Группа 1: Основные параметры (Basic)
- Enable (boolean) - включение/выключение плагина
- ViewMode (choice) - режим отображения

## Группа 2: Детектирование (Detection)
- LumaThreshold (double) - порог яркости
- WhitenessThreshold (double) - порог белизны
- Neighborhood (choice) - размер окрестности (3x3 или 5x5)
- RobustZ (double) - порог Z-оценки
- MaxClusterArea (integer) - максимальная площадь кластера

## Группа 3: Временной анализ (Temporal)
- TemporalMode (choice) - режим временного анализа
- StuckWindowFrames (integer) - размер окна для анализа
- StuckMinFraction (double) - минимальная доля кадров
- MaxGapFrames (integer) - максимальный разрыв между детекциями

## Группа 4: Восстановление (Repair) - опционально
- RepairEnable (boolean) - включение восстановления
- RepairMethod (choice) - метод восстановления

## Файловая структура:
1. PluginParamsBasic.cpp - основные параметры
2. PluginParamsDetection.cpp - параметры детектирования
3. PluginParamsTemporal.cpp - параметры временного анализа
4. PluginParamsRepair.cpp - параметры восстановления
5. PluginParamsGetters.cpp - функции получения значений параметров
6. PluginParamsConstants.cpp - константы и строки