# DeadPixelQC OFX Plugin

OpenFX plugin for DaVinci Resolve that detects and optionally fixes stuck/hot pixels in finished/non-RAW footage.

## Overview

DeadPixelQC is a quality control tool designed to identify pixels (or micro-clusters) that are abnormally bright (near-white) and repeat at the same image coordinates over time. The plugin uses a multi-stage detection pipeline to avoid false positives from highlights, subtitles, edges, compression artifacts, and noise.

## Features

### Phase 1: Single-frame detection (MVP)
- **Brightness gate**: Detects pixels with luma near maximum (normalized threshold)
- **Whiteness/neutral chroma gate**: Filters colored pixels using low saturation or chroma distance
- **Robust local contrast gate**: Uses neighborhood median + MAD for robust z-score thresholding
- **Connected components filtering**: Keeps only clusters with area ≤ 4 pixels (configurable)
- **Output modes**: Pass-through, candidates overlay, confirmed overlay, mask-only

### Phase 2: Temporal confirmation (QC mode)
- **Rolling window tracking**: Confirms defects that persist across frames
- **Sequential-only mode**: Safe handling of scrubbing/out-of-order frame requests
- **Configurable thresholds**: Window size, minimum fraction, maximum gap

### Phase 3: Optional repair (nice-to-have)
- **Neighbor median**: Replace defective pixels with median of surrounding neighbors
- **Directional median**: Edge-preserving repair using directional consistency

## Requirements

- **Host**: DaVinci Resolve (via OpenFX)
- **Language**: C++17
- **Build**: CMake 3.15+
- **Platform**: Windows, macOS, Linux

## Building

### Prerequisites
- CMake 3.15 or higher
- C++17 compatible compiler
- OpenFX SDK (included in `ofx-sdk/`)

### Build Steps

```bash
# Create build directory
mkdir build
cd build

# Configure with CMake
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build . --config Release

# Install plugin (optional)
cmake --install . --config Release
```

### Build Options
- `BUILD_PLUGIN`: Build OFX plugin (default: ON)
- `BUILD_TESTS`: Build unit tests (default: ON)
- `BUILD_TOOLS`: Build offline runner tool (default: ON)

## Installation

### Windows
Copy the generated `.ofx` file to:
```
C:\ProgramData\Blackmagic Design\DaVinci Resolve\Support\OFX\Plugins\
```

### macOS
Copy the `.ofx` bundle to:
```
~/Library/OFX/Plugins/
```

### Linux
Copy the `.ofx` file to:
```
/usr/OFX/Plugins/
```

## Usage

### In DaVinci Resolve
1. Add the plugin to your clip from the Effects library
2. Configure parameters in the Inspector panel
3. Use ViewMode to visualize detection results:
   - **Output**: Pass-through or repaired image
   - **CandidatesOverlay**: Highlight candidate pixels
   - **ConfirmedOverlay**: Highlight confirmed stuck pixels
   - **MaskOnly**: Output detection mask

### Offline Runner
The `offline_runner` tool processes image sequences offline:

```bash
# Generate synthetic test sequence
offline_runner --synthetic -w 1920 -h 1080 -e 100 -o output

# Process with temporal tracking and repair
offline_runner --synthetic --repair --no-temporal -o output

# Show help
offline_runner --help
```

## Parameters

### Detection
- **Enable**: Toggle plugin on/off
- **ViewMode**: Output visualization mode
- **LumaThreshold**: Minimum luma for candidate pixels (0.0-1.0)
- **WhitenessThreshold**: Maximum whiteness/saturation (0.0-0.3)
- **Neighborhood**: 3×3 or 5×5 for local contrast analysis
- **RobustZ**: Minimum z-score for contrast gate (8.0-20.0)
- **MaxClusterArea**: Maximum area for connected components (1-10)

### Temporal
- **TemporalMode**: Off / SequentialOnly
- **StuckWindowFrames**: Rolling window size (10-120)
- **StuckMinFraction**: Minimum hit fraction for confirmation (0.5-1.0)
- **MaxGapFrames**: Maximum allowed gap between detections (0-5)

### Repair (Phase 3)
- **RepairEnable**: Enable defect repair
- **RepairMethod**: NeighborMedian / DirectionalMedian
- **RepairKernelSize**: 3×3 or 5×5 repair kernel

## Algorithm Details

### Luma + Whiteness
- Uses Rec.709 luma coefficients: `Y = 0.2126R + 0.7152G + 0.0722B`
- Whiteness metric: `abs(R-G) + abs(G-B) + abs(B-R)`
- Candidate must satisfy: `Y > Y_hi` AND `whiteness < W_max`

### Robust Local Contrast
- Neighborhood: 3×3 (8 samples) or 5×5 (24 samples)
- Compute median and MAD (Median Absolute Deviation)
- Robust z-score: `z = (Y_center - median) / (1.4826 * MAD + eps)`
- Require `z >= z_min` (default: 10 for 3×3, 8 for 5×5)

### Connected Components
- 8-connected labeling on candidate mask
- Filter by area ≤ Amax (default: 4)
- Output: pixel list, bounding box, centroid, area

### Temporal Confirmation
- Maintain track per coordinate (centroid rounded)
- Confirm stuck if: `hits/frames >= minFraction` AND `maxGap <= allowedGap`
- Handle scrubbing: Reset temporal state on non-monotonic frame indices

## Testing

### Unit Tests
```bash
# Build and run tests
cd build
cmake --build . --target DeadPixelQC_tests
./bin/DeadPixelQC_tests
```

### Synthetic Tests
The test suite includes:
- Single stuck pixel detection
- 2×2 cluster detection
- False-positive stress tests (moving highlights, subtitles)
- Temporal confirmation validation

## Performance

- **Phase 1**: Real-time friendly at 1080p
- **Memory**: No heap allocations in per-pixel loops
- **Thread-safe**: All temporal state protected with mutexes
- **Deterministic**: Same input produces same output

## Limitations

- Designed for finished/non-RAW footage
- May have reduced effectiveness on heavily compressed video
- Temporal tracking requires sequential frame access for best results
- Repair may cause slight blurring in textured areas

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make changes with appropriate tests
4. Submit a pull request

See `CONTRIBUTING.md` for detailed guidelines.

## License

[Specify license here - e.g., MIT, GPL, etc.]

## Acknowledgments

- OpenFX community for the SDK and documentation
- DaVinci Resolve team for the host implementation
- Contributors and testers

## Support

For issues and feature requests, please use the GitHub issue tracker.

---

*DeadPixelQC - Professional stuck pixel detection for video quality control*