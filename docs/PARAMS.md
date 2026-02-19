# DeadPixelQC Parameters Reference

This document provides detailed descriptions of all plugin parameters, their ranges, default values, and recommended settings.

## Overview

DeadPixelQC uses a multi-stage pipeline with configurable thresholds at each stage. Parameters are organized into logical groups: Detection, Temporal, and Repair.

## Parameter Groups

### 1. Enable (Boolean)
- **Description**: Master enable/disable switch for the plugin
- **Default**: `true` (enabled)
- **Range**: `true` / `false`
- **Usage**: Use to temporarily disable processing without removing the effect

### 2. ViewMode (Enum)
- **Description**: Controls what is displayed in the output
- **Default**: `Output`
- **Options**:
  - **Output**: Pass-through image (or repaired if repair enabled)
  - **CandidatesOverlay**: Highlight candidate pixels (yellow)
  - **ConfirmedOverlay**: Highlight confirmed stuck pixels (red)
  - **MaskOnly**: Output binary mask (white = defect, black = background)
- **Usage**: Use overlay modes for debugging, mask for compositing

## Detection Parameters

### 3. LumaThreshold (Float)
- **Description**: Minimum luma (brightness) for candidate pixels
- **Default**: `0.98` (98% of maximum brightness)
- **Range**: `0.0` - `1.0`
- **Unit**: Normalized [0..1]
- **Effect**: Higher values detect only brighter pixels, reducing false positives
- **Recommended**: `0.95` - `0.99` depending on content brightness

### 4. WhitenessThreshold (Float)
- **Description**: Maximum whiteness metric for candidate pixels
- **Default**: `0.05`
- **Range**: `0.0` - `0.3`
- **Unit**: Sum of absolute RGB differences
- **Formula**: `abs(R-G) + abs(G-B) + abs(B-R)`
- **Effect**: Lower values require pixels to be more neutral/white
- **Recommended**: `0.03` - `0.08` for precision-focused QC

### 5. UseSaturation (Boolean)
- **Description**: Use saturation instead of whiteness metric
- **Default**: `false`
- **Range**: `true` / `false`
- **Effect**: When true, uses HSV saturation; when false, uses whiteness metric
- **Recommended**: `false` for performance, `true` for more accurate color filtering

### 6. SaturationThreshold (Float)
- **Description**: Maximum saturation when UseSaturation is enabled
- **Default**: `0.05`
- **Range**: `0.0` - `1.0`
- **Unit**: Normalized HSV saturation [0..1]
- **Effect**: Lower values require pixels to be less saturated
- **Recommended**: `0.03` - `0.10`

### 7. Neighborhood (Enum)
- **Description**: Neighborhood size for local contrast analysis
- **Default**: `ThreeByThree`
- **Options**:
  - **ThreeByThree**: 3×3 neighborhood (8 samples)
  - **FiveByFive**: 5×5 neighborhood (24 samples)
- **Effect**: Larger neighborhoods are more robust but slower
- **Recommended**: `ThreeByThree` for real-time, `FiveByFive` for offline QC

### 8. RobustZ (Float)
- **Description**: Minimum robust z-score for contrast gate
- **Default**: `10.0` (for 3×3), `8.0` (for 5×5)
- **Range**: `5.0` - `20.0`
- **Unit**: Standard deviations from neighborhood median
- **Formula**: `z = (Y_center - median) / (1.4826 * MAD + eps)`
- **Effect**: Higher values require pixels to be more extreme outliers
- **Recommended**: `8.0` - `12.0` depending on neighborhood size

### 9. MaxClusterArea (Integer)
- **Description**: Maximum area for connected components
- **Default**: `4`
- **Range**: `1` - `10`
- **Unit**: Pixels
- **Effect**: Filters out larger clusters (likely not stuck pixels)
- **Recommended**: `2` - `4` for single pixels and micro-clusters

## Temporal Parameters

### 10. TemporalMode (Enum)
- **Description**: Temporal tracking mode
- **Default**: `SequentialOnly`
- **Options**:
  - **Off**: No temporal tracking (single-frame detection only)
  - **SequentialOnly**: Temporal tracking with scrubbing safety
- **Effect**: Controls whether temporal confirmation is applied
- **Recommended**: `SequentialOnly` for QC workflows

### 11. StuckWindowFrames (Integer)
- **Description**: Rolling window size for temporal confirmation
- **Default**: `60` (2 seconds at 30fps)
- **Range**: `10` - `120`
- **Unit**: Frames
- **Effect**: Larger windows are more stable but slower to confirm
- **Recommended**: `30` - `90` depending on frame rate

### 12. StuckMinFraction (Float)
- **Description**: Minimum hit fraction for defect confirmation
- **Default**: `0.95` (95%)
- **Range**: `0.5` - `1.0`
- **Unit**: Fraction of frames in window
- **Formula**: `hits / window_size >= min_fraction`
- **Effect**: Higher values require more consistent detection
- **Recommended**: `0.9` - `0.98` for precision-focused QC

### 13. MaxGapFrames (Integer)
- **Description**: Maximum allowed gap between detections
- **Default**: `2`
- **Range**: `0` - `5`
- **Unit**: Frames
- **Effect**: Allows brief interruptions in detection (e.g., scene cuts)
- **Recommended**: `1` - `3` depending on content stability

## Repair Parameters (Phase 3)

### 14. RepairEnable (Boolean)
- **Description**: Enable defect repair
- **Default**: `false`
- **Range**: `true` / `false`
- **Effect**: When enabled, confirmed defects are repaired in output
- **Recommended**: `false` for detection-only, `true` for automatic correction

### 15. RepairMethod (Enum)
- **Description**: Repair algorithm to use
- **Default**: `NeighborMedian`
- **Options**:
  - **NeighborMedian**: Replace with median of surrounding neighbors
  - **DirectionalMedian**: Edge-preserving directional median
- **Effect**: Directional median preserves edges better but is slower
- **Recommended**: `NeighborMedian` for speed, `DirectionalMedian` for quality

### 16. RepairKernelSize (Integer)
- **Description**: Kernel size for repair operations
- **Default**: `3` (3×3)
- **Range**: `3` / `5`
- **Unit**: Pixels (kernel width/height)
- **Effect**: Larger kernels sample more neighbors but may cause blurring
- **Recommended**: `3` for most cases, `5` for larger defects

## Parameter Interactions

### Precision vs Recall Trade-off
- **High precision (low FP)**: Increase LumaThreshold, decrease WhitenessThreshold, increase RobustZ
- **High recall (low FN)**: Decrease LumaThreshold, increase WhitenessThreshold, decrease RobustZ

### Temporal Stability
- **More stable**: Increase StuckWindowFrames, increase StuckMinFraction
- **More responsive**: Decrease StuckWindowFrames, decrease StuckMinFraction

### Performance Considerations
- **Faster**: Use 3×3 neighborhood, disable temporal tracking, disable repair
- **Slower but better**: Use 5×5 neighborhood, enable temporal tracking, enable directional median repair

## Recommended Presets

### QC Mode (High Precision)
```
LumaThreshold: 0.98
WhitenessThreshold: 0.03
Neighborhood: FiveByFive
RobustZ: 12.0
MaxClusterArea: 2
TemporalMode: SequentialOnly
StuckWindowFrames: 60
StuckMinFraction: 0.98
MaxGapFrames: 1
RepairEnable: false
```

### Real-time Mode
```
LumaThreshold: 0.96
WhitenessThreshold: 0.05
Neighborhood: ThreeByThree
RobustZ: 8.0
MaxClusterArea: 4
TemporalMode: Off
RepairEnable: false
```

### Repair Mode
```
LumaThreshold: 0.97
WhitenessThreshold: 0.04
Neighborhood: ThreeByThree
RobustZ: 10.0
MaxClusterArea: 3
TemporalMode: SequentialOnly
StuckWindowFrames: 30
StuckMinFraction: 0.95
MaxGapFrames: 2
RepairEnable: true
RepairMethod: DirectionalMedian
RepairKernelSize: 3
```

## Default Values Summary

| Parameter | Default Value | Range |
|-----------|---------------|-------|
| Enable | true | true/false |
| ViewMode | Output | Output/CandidatesOverlay/ConfirmedOverlay/MaskOnly |
| LumaThreshold | 0.98 | 0.0-1.0 |
| WhitenessThreshold | 0.05 | 0.0-0.3 |
| UseSaturation | false | true/false |
| SaturationThreshold | 0.05 | 0.0-1.0 |
| Neighborhood | ThreeByThree | ThreeByThree/FiveByFive |
| RobustZ | 10.0 | 5.0-20.0 |
| MaxClusterArea | 4 | 1-10 |
| TemporalMode | SequentialOnly | Off/SequentialOnly |
| StuckWindowFrames | 60 | 10-120 |
| StuckMinFraction | 0.95 | 0.5-1.0 |
| MaxGapFrames | 2 | 0-5 |
| RepairEnable | false | true/false |
| RepairMethod | NeighborMedian | NeighborMedian/DirectionalMedian |
| RepairKernelSize | 3 | 3/5 |

## Notes

- All thresholds are normalized to [0..1] range regardless of bit depth
- Temporal parameters only affect confirmed defects, not candidate detection
- Repair only applies to confirmed defects when RepairEnable is true
- The plugin is thread-safe and deterministic: same input produces same output