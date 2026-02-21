#pragma once

#include "Types.h"
#include <vector>
#include <queue>
#include <cstring>
#include <algorithm>

namespace DeadPixelQC {

// Connected components labeling (8-connected)
class ConnectedComponents {
public:
    // Label mask and extract components
    static std::vector<Component> findComponents(const std::vector<bool>& mask, 
                                                 i32 width, i32 height,
                                                 i32 maxArea = 4,
                                                 i32 minArea = 1) {
        if (width <= 0 || height <= 0 || mask.size() != static_cast<size_t>(width * height)) {
            return {};
        }

        minArea = std::max<i32>(1, minArea);
        if (maxArea < minArea) {
            return {};
        }
        
        std::vector<i32> labels(width * height, 0);
        std::vector<Component> components;
        
        i32 nextLabel = 1;
        
        // First pass: label propagation
        for (i32 y = 0; y < height; ++y) {
            for (i32 x = 0; x < width; ++x) {
                i32 idx = y * width + x;
                
                if (!mask[idx]) {
                    continue;
                }
                
                // Check neighbors (8-connected)
                i32 leftLabel = (x > 0) ? labels[idx - 1] : 0;
                i32 upLabel = (y > 0) ? labels[idx - width] : 0;
                i32 upLeftLabel = (x > 0 && y > 0) ? labels[idx - width - 1] : 0;
                i32 upRightLabel = (x < width - 1 && y > 0) ? labels[idx - width + 1] : 0;
                
                i32 minNeighbor = 0;
                if (leftLabel > 0) minNeighbor = (minNeighbor == 0) ? leftLabel : std::min(minNeighbor, leftLabel);
                if (upLabel > 0) minNeighbor = (minNeighbor == 0) ? upLabel : std::min(minNeighbor, upLabel);
                if (upLeftLabel > 0) minNeighbor = (minNeighbor == 0) ? upLeftLabel : std::min(minNeighbor, upLeftLabel);
                if (upRightLabel > 0) minNeighbor = (minNeighbor == 0) ? upRightLabel : std::min(minNeighbor, upRightLabel);
                
                if (minNeighbor == 0) {
                    // New component
                    labels[idx] = nextLabel++;
                } else {
                    labels[idx] = minNeighbor;
                    
                    // Union find: if neighbors have different labels, they're actually the same component
                    // We'll handle this in second pass with union-find
                    if (leftLabel > 0 && leftLabel != minNeighbor) {
                        unionLabels(labels, leftLabel, minNeighbor);
                    }
                    if (upLabel > 0 && upLabel != minNeighbor) {
                        unionLabels(labels, upLabel, minNeighbor);
                    }
                    if (upLeftLabel > 0 && upLeftLabel != minNeighbor) {
                        unionLabels(labels, upLeftLabel, minNeighbor);
                    }
                    if (upRightLabel > 0 && upRightLabel != minNeighbor) {
                        unionLabels(labels, upRightLabel, minNeighbor);
                    }
                }
            }
        }
        
        // Second pass: resolve unions and collect components
        std::vector<i32> labelMap(nextLabel, 0);
        i32 componentCount = 0;
        
        // First, find root for each label
        for (i32 i = 1; i < nextLabel; ++i) {
            i32 root = findRoot(labels, i);
            if (labelMap[root] == 0) {
                labelMap[root] = ++componentCount;
            }
            labelMap[i] = labelMap[root];
        }
        
        // Initialize components
        components.resize(componentCount);
        for (auto& comp : components) {
            comp.pixels.reserve(maxArea * 2); // Reserve some space
        }
        
        // Collect pixels
        for (i32 y = 0; y < height; ++y) {
            for (i32 x = 0; x < width; ++x) {
                i32 idx = y * width + x;
                
                if (labels[idx] > 0) {
                    i32 compIdx = labelMap[labels[idx]] - 1;
                    components[compIdx].pixels.push_back({x, y});
                }
            }
        }
        
        // Compute properties and filter by area
        std::vector<Component> filtered;
        for (auto& comp : components) {
            comp.computeProperties();
            if (comp.area >= minArea && comp.area <= maxArea) {
                filtered.push_back(std::move(comp));
            }
        }
        
        return filtered;
    }
    
    // Simple flood fill implementation (alternative)
    static std::vector<Component> findComponentsFloodFill(const std::vector<bool>& mask,
                                                         i32 width, i32 height,
                                                         i32 maxArea = 4,
                                                         i32 minArea = 1) {
        if (width <= 0 || height <= 0 || mask.size() != static_cast<size_t>(width * height)) {
            return {};
        }

        minArea = std::max<i32>(1, minArea);
        if (maxArea < minArea) {
            return {};
        }
        
        std::vector<bool> visited(width * height, false);
        std::vector<Component> components;
        
        // 8-connected directions
        constexpr i32 dx8[] = {-1, 0, 1, -1, 1, -1, 0, 1};
        constexpr i32 dy8[] = {-1, -1, -1, 0, 0, 1, 1, 1};
        
        for (i32 y = 0; y < height; ++y) {
            for (i32 x = 0; x < width; ++x) {
                i32 idx = y * width + x;
                
                if (!mask[idx] || visited[idx]) {
                    continue;
                }
                
                // Start new component
                Component comp;
                std::queue<PixelCoord> queue;
                queue.push({x, y});
                visited[idx] = true;
                
                while (!queue.empty()) {
                    PixelCoord p = queue.front();
                    queue.pop();
                    
                    comp.pixels.push_back(p);
                    
                    // Check 8 neighbors
                    for (i32 d = 0; d < 8; ++d) {
                        i32 nx = p.x + dx8[d];
                        i32 ny = p.y + dy8[d];
                        
                        if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                            i32 nidx = ny * width + nx;
                            if (mask[nidx] && !visited[nidx]) {
                                visited[nidx] = true;
                                queue.push({nx, ny});
                            }
                        }
                    }
                }
                
                comp.computeProperties();
                if (comp.area >= minArea && comp.area <= maxArea) {
                    components.push_back(std::move(comp));
                }
            }
        }
        
        return components;
    }
    
private:
    // Simple union-find helpers
    static i32 findRoot(std::vector<i32>& labels, i32 label) {
        while (labels[label] != label) {
            label = labels[label];
        }
        return label;
    }
    
    static void unionLabels(std::vector<i32>& labels, i32 label1, i32 label2) {
        i32 root1 = findRoot(labels, label1);
        i32 root2 = findRoot(labels, label2);
        
        if (root1 != root2) {
            // Make root1 parent of root2
            labels[root2] = root1;
        }
    }
};

} // namespace DeadPixelQC
