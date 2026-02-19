# DeadPixelQC Architecture

This document describes the architecture, modules, threading model, and temporal behavior of the DeadPixelQC OFX plugin.

## System Overview

DeadPixelQC is a multi-stage pipeline for detecting and repairing stuck/hot pixels in video footage. The system is designed to be thread-safe, deterministic, and real-time friendly.

```
┌─────────────────────────────────────────────────────────────┐
│                     OFX Plugin Interface                    │
├─────────────────────────────────────────────────────────────┤
│                    Plugin Parameters                        │
├─────────────────────────────────────────────────────────────┤
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐   │
│  │  Phase 1 │  │  Phase 2 │  │  Phase 3 │  │  Output  │   │
│  │  Spatial │  │ Temporal │  │  Repair  │  │  Render  │   │
│  │ Detection│  │ Tracking │  │          │  │          │   │
│  └──────────┘  └──────────┘  └──────────┘  └──────────┘   │
└─────────────────────────────────────────────────────────────┘
```

## Module Structure

### 1. Core Library (`/core/`)

#### Types.h
- **Purpose**: Common type definitions and constants
- **Contents**:
  - Integer types (i8, i16, i32, i64, u8, u16, u32, u64)
  - Floating point types (f32, f64)
  - Pixel coordinate structure (`PixelCoord`)
  - Component structure (`Component`) for connected components
  - Frame detection result structure (`FrameDetection`)
  - Constants (MAD_SCALE = 1.4826, EPSILON = 1e-6)

#### PixelFormatAdapter.h/.cpp
- **Purpose**: Abstract pixel format handling
- **Key Classes**:
  - `PixelFormat`: Enum for supported formats (RGB8, RGBA8, RGB_F32, RGBA_F32)
  - `ImageBuffer`: Wrapper for image data with format conversion
- **Features**:
  - Normalized access to pixel values [0..1]
  - Automatic format conversion
  - Bounds checking
  - Thread-safe read-only operations

#### ColorMetrics.h/.cpp
- **Purpose**: Color space calculations and metrics
- **Key Functions**:
  - `computeLuma()`: Rec.709 luma calculation
  - `computeWhitenessFast()`: Fast whiteness metric
  - `computeSaturationFast()`: Fast saturation approximation
  - `computeRobustZScore()`: Robust local contrast analysis
- **Configuration Structures**:
  - `ColorGateParams`: Color-based detection thresholds
  - `RobustContrastParams`: Local contrast analysis parameters

#### SpatialDetector.h/.cpp
- **Purpose**: Single-frame candidate detection (Phase 1)
- **Key Classes**:
  - `SpatialDetector`: Main detection class
  - `SpatialDetectorParams`: Configuration parameters
- **Algorithm**:
  1. Color gate (luma + whiteness)
  2. Robust contrast gate (median + MAD)
  3. Connected components analysis
  4. Area filtering
- **Performance**: Optimized for real-time 1080p processing

#### ConnectedComponents.h/.cpp
- **Purpose**: Connected components labeling and analysis
- **Algorithms**:
  - Union-find (disjoint set) for 8-connected labeling
  - Flood fill alternative
- **Features**:
  - Area computation
  - Bounding box calculation
  - Centroid computation
  - Area filtering

#### TemporalTracker.h
- **Purpose**: Multi-frame tracking and confirmation (Phase 2)
- **Key Classes**:
  - `TemporalTracker`: Main tracking class
  - `TemporalTrackerParams`: Configuration parameters
  - `Track`: Individual pixel track
- **Algorithm**:
  1. Maintain tracks per coordinate
  2. Update hits/frames counters
  3. Confirm if hits/frames ≥ minFraction AND maxGap ≤ allowedGap
  4. Handle non-sequential frame access
- **Thread Safety**: Protected by mutex for shared temporal state

#### Fixer.h
- **Purpose**: Defect repair (Phase 3)
- **Key Classes**:
  - `Fixer`: Repair class
  - `RepairParams`: Configuration parameters
- **Algorithms**:
  - `NeighborMedian`: Replace with median of surrounding pixels
  - `DirectionalMedian`: Edge-preserving directional median
- **Features**:
  - Configurable kernel size (3×3 or 5×5)
  - Border handling
  - In-place repair

### 2. Plugin Interface (`/plugin/`)

#### PluginMain.cpp
- **Purpose**: OpenFX plugin entry points and host integration
- **Key Functions**:
  - `OfxPluginMain()`: Plugin entry point
  - `describe()`: Plugin description
  - `describeInContext()`: Context-specific description
  - `createInstance()`: Plugin instance creation
- **Features**:
  - OpenFX 1.4 compliance
  - Multi-thread rendering support
  - Clip and parameter management

#### PluginParams.h/.cpp
- **Purpose**: Parameter definition and management
- **Key Classes**:
  - `DeadPixelQCPlugin`: Main plugin class
  - Parameter handlers for all configurable settings
- **Parameters**: See PARAMS.md for complete list

#### OfxUtil.h/.cpp
- **Purpose**: OpenFX utility functions
- **Features**:
  - Error handling
  - Memory management
  - Thread synchronization
  - Parameter validation

### 3. Testing (`/tests/`)

#### SyntheticGenerator.h/.cpp
- **Purpose**: Generate synthetic test images with known defects
- **Features**:
  - Configurable stuck pixel positions
  - Moving highlights (false positive tests)
  - Subtitle-like edges (false positive tests)
  - Ground truth generation
  - Validation metrics (precision, recall, F1)

#### UnitTests.cpp
- **Purpose**: Unit test suite
- **Tests**:
  - Color metrics validation
  - Spatial detection accuracy
  - Temporal tracking correctness
  - Repair functionality
  - False positive rejection

### 4. Tools (`/tools/`)

#### offline_runner.cpp
- **Purpose**: Command-line tool for offline processing
- **Features**:
  - Image sequence processing
  - Synthetic test generation
  - Mask and report output
  - Performance benchmarking
  - OpenCV integration (optional)

## Threading Model

### Thread Safety Guarantees

1. **Core Library**:
   - All core classes are thread-safe for concurrent read operations
   - `TemporalTracker` uses mutex protection for shared state
   - No global mutable state

2. **OFX Plugin**:
   - OpenFX host may call render from multiple threads
   - Each plugin instance has its own state
   - Temporal state protected by mutex in `TemporalTracker`

3. **Memory Allocation**:
   - No heap allocations in per-pixel loops
   - Fixed-size stack arrays for neighborhood sampling
   - Pre-allocated buffers for intermediate results

### Performance Considerations

- **Phase 1 (Spatial)**: Designed for real-time 1080p (≈16ms per frame)
- **Phase 2 (Temporal)**: Minimal overhead, mainly counter updates
- **Phase 3 (Repair)**: Additional processing for confirmed defects only
- **Memory**: ≈4× width×height bytes for intermediate masks

## Temporal Behavior

### Sequential Mode

When `TemporalMode` is set to `SequentialOnly`:

1. **Normal Operation**:
   - Tracks are maintained across sequential frames
   - Counters updated for each detection
   - Confirmation after sufficient hits

2. **Non-Sequential Access**:
   - Detected by checking frame index monotonicity
   - Temporal state is reset
   - Warning counter incremented
   - Falls back to single-frame detection for that segment

3. **Scrubbing Safety**:
   - No corruption of temporal state
   - Graceful degradation to Phase 1 behavior
   - Automatic recovery when sequential access resumes

### Track Management

Each track maintains:
- `hits`: Number of frames where pixel was detected
- `frames`: Number of frames in current window
- `lastSeen`: Frame index of last detection
- `maxGap`: Maximum gap between detections
- `score`: hits/frames ratio

Confirmation criteria:
```
confirmed = (hits / frames >= minFraction) && (maxGap <= allowedGap)
```

### Window Management

- **Rolling window**: Fixed size (StuckWindowFrames)
- **Aging**: Old frames automatically dropped
- **Gap tolerance**: Configurable via MaxGapFrames

## Pixel Format Support

### Supported Formats
- RGB8 (8-bit per channel)
- RGBA8 (8-bit per channel with alpha)
- RGB_F32 (32-bit float per channel)
- RGBA_F32 (32-bit float per channel with alpha)

### Internal Representation
- All processing uses normalized float values [0..1]
- Format conversion handled by `PixelFormatAdapter`
- Alpha channel preserved but not used in detection

## Error Handling

### Graceful Degradation

1. **Invalid Parameters**: Use safe defaults, log warning
2. **Memory Allocation Failure**: Return empty result, log error
3. **Invalid Image Data**: Skip processing, pass through input
4. **Thread Synchronization Errors**: Continue with single-threaded fallback

### Logging
- Error messages to OpenFX host log
- Warning counters for non-fatal issues
- Debug information in development builds

## Build Configuration

### CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `BUILD_PLUGIN` | ON | Build OFX plugin |
| `BUILD_TESTS` | ON | Build unit tests |
| `BUILD_TOOLS` | ON | Build offline runner |
| `CMAKE_BUILD_TYPE` | Release | Build type (Debug/Release) |

### Dependencies
- **Required**: OpenFX SDK (included)
- **Optional**: OpenCV (for offline_runner image I/O)
- **Compiler**: C++17 compatible

## Testing Strategy

### Unit Tests
- **Coverage**: Core algorithms and data structures
- **Validation**: Ground truth comparison
- **Performance**: Timing measurements

### Integration Tests
- **OFX Host**: DaVinci Resolve compatibility
- **Format Support**: All pixel formats
- **Thread Safety**: Multi-threaded rendering

### Synthetic Tests
- **True Positives**: Stuck pixels at known positions
- **False Positives**: Moving highlights, subtitles, edges
- **Edge Cases**: Border pixels, cluster boundaries

## Future Extensions

### Planned Features
1. **GPU Acceleration**: OpenCL/CUDA implementation
2. **Adaptive Thresholds**: Content-aware parameter adjustment
3. **Batch Processing**: Offline sequence optimization
4. **Export Formats**: Additional mask and report formats

### Architecture Considerations
- Modular design allows incremental enhancement
- Plugin interface supports parameter additions
- Core library independent of host specifics

## Conclusion

DeadPixelQC uses a layered architecture with clear separation between:
1. Core detection algorithms
2. Temporal tracking logic
3. Host integration
4. Testing and tooling

The system prioritizes precision over recall, thread safety, and deterministic behavior while maintaining real-time performance for 1080p video.