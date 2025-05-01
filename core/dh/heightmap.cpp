
#include "heightmap.h"
#include "../ew/external/glad.h"
#include "../ew/external/stb_image.h"
#include "../ew/texture.h"
#include <algorithm>
#include <iostream>

namespace dh {

    bool getHeightmapDimensions(const char* filePath, int& width, int& height) {
        int channels;
        unsigned char* data = stbi_load(filePath, &width, &height, &channels, 0);

        if (data == nullptr) {
            std::printf("Failed to load heightmap image %s\n", filePath);
            return false;
        }

        stbi_image_free(data);
        return true;
    }

    std::vector<float> loadHeightmapData(const char* filePath, bool normalizeHeight) {
        int width, height, numComponents;
        unsigned char* data = stbi_load(filePath, &width, &height, &numComponents, 1);

        if (data == nullptr) {
            std::printf("Failed to load heightmap image %s\n", filePath);
            return std::vector<float>();
        }

        std::printf("Loaded heightmap: %s (%dx%d, %d components)\n",
            filePath, width, height, numComponents);

        // Create height data array
        std::vector<float> heights(width * height, 0.0f);

        // First pass - collect statistics about the data
        float minHeight = 255.0f;
        float maxHeight = 0.0f;

        for (int i = 0; i < width * height; i++) {
            float h = static_cast<float>(data[i]);
            heights[i] = h;

            if (normalizeHeight) {
                minHeight = std::min(minHeight, h);
                maxHeight = std::max(maxHeight, h);
            }
        }

        // Debug statistics
        std::printf("Raw height range: min=%f, max=%f\n", minHeight, maxHeight);

        // Apply normalization
        float range = maxHeight - minHeight;
        if (normalizeHeight && range > 0.001f) {
            for (int i = 0; i < width * height; i++) {
                heights[i] = (heights[i] - minHeight) / range;
            }
            std::printf("Normalized heights to 0-1 range\n");
        }
        else {
            // Just scale to 0-1
            for (int i = 0; i < width * height; i++) {
                heights[i] /= 255.0f;
            }
            std::printf("Scaled heights by 1/255\n");
        }

        // Detect and report any anomalies
        int zeroCount = 0;
        int oneCount = 0;
        for (int i = 0; i < std::min(10000, (int)heights.size()); i++) {
            if (heights[i] < 0.001f) zeroCount++;
            if (heights[i] > 0.999f) oneCount++;
        }

        float zeroPercent = (float)zeroCount / std::min(10000, (int)heights.size()) * 100.0f;
        float onePercent = (float)oneCount / std::min(10000, (int)heights.size()) * 100.0f;

        std::printf("First 10k samples: %.1f%% near zero, %.1f%% near one\n",
            zeroPercent, onePercent);

        // Release the image data
        stbi_image_free(data);
        return heights;
    }
    ew::Mesh createHeightmapMesh(const std::vector<float>& heightmapData, int width, int height, glm::vec3 scale) {
        if (heightmapData.size() != width * height) {
            std::printf("ERROR in createHeightmapMesh: Data size (%zu) doesn't match dimensions (%d x %d = %d)\n",
                heightmapData.size(), width, height, width * height);
            return ew::Mesh(ew::MeshData{}); // Return empty mesh
        }

        std::vector<ew::Vertex> vertices;
        std::vector<unsigned int> indices;

        vertices.reserve(width * height);
        indices.reserve((width - 1) * (height - 1) * 6);

        for(int z = 0; z < height; z++) {
            for (int x = 0; x < width; x++) {
                ew::Vertex vertex;

                size_t index = z * width + x;
                if (index >= heightmapData.size()) {
                    std::printf("ERROR: Index out of bounds in createHeightmapMesh: %zu >= %zu\n",
                        index, heightmapData.size());
                    continue;
                }

                float heightValue = heightmapData[index];
                // Ensure height is within a reasonable range
                if (std::isnan(heightValue) || std::isinf(heightValue)) {
                    std::printf("WARNING: Invalid height value at (%d,%d)\n", x, z);
                    heightValue = 0.0f;
                }
                float fx = (static_cast<float>(x) / (width - 1) - 0.5f) * scale.x;
                float fz = (static_cast<float>(z) / (height - 1) - 0.5f) * scale.z;
                float fy = heightmapData[z * width + x] * scale.y;

                vertex.pos = glm::vec3(fx, fy, fz);
                vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f); // default normal
                vertex.uv = glm::vec2(static_cast<float>(x) / (width - 1), static_cast<float>(z) / (height - 1));
                vertex.tangent = glm::vec3(1.0f, 0.0f, 0.0f);

                // Calculate normal using central difference method if not at the edge
                if (x > 0 && x < width - 1 && z > 0 && z < height - 1) {
                    float hL = heightmapData[z * width + (x - 1)];
                    float hR = heightmapData[z * width + (x + 1)];
                    float hD = heightmapData[(z - 1) * width + x];
                    float hU = heightmapData[(z + 1) * width + x];

                    glm::vec3 tangent(2.0f, (hR - hL) * scale.y, 0.0f);
                    glm::vec3 bitangent(0.0f, (hU - hD) * scale.y, 2.0f);
                    vertex.normal = glm::normalize(glm::cross(tangent, bitangent));
                }

                vertices.push_back(vertex);
            }
        }

        // Generate triangle indices
        for (int z = 0; z < height - 1; z++) {
            for (int x = 0; x < width - 1; x++) {
                unsigned int topLeft = z * width + x;
                unsigned int topRight = topLeft + 1;
                unsigned int bottomLeft = (z + 1) * width + x;
                unsigned int bottomRight = bottomLeft + 1;

                // Triangle 1
                indices.push_back(topLeft);
                indices.push_back(bottomLeft);
                indices.push_back(bottomRight);

                // Triangle 2
                indices.push_back(topLeft);
                indices.push_back(bottomRight);
                indices.push_back(topRight);
            }
        }

        ew::MeshData meshData;
        meshData.vertices = std::move(vertices);
        meshData.indices = std::move(indices);

        return ew::Mesh(meshData);
    }
}