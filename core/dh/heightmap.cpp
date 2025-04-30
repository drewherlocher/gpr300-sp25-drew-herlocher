/*
*   Author: dh
*/
#include "heightmap.h"
#include "../ew/external/glad.h"
#include "../ew/external/stb_image.h"
#include "../ew/texture.h" // Include texture loading
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
        unsigned char* data = stbi_load(filePath, &width, &height, &numComponents, 1); // Force 1 component (grayscale)

        if (data == nullptr) {
            std::printf("Failed to load heightmap image %s\n", filePath);
            return std::vector<float>();
        }

        std::vector<float> heights(width * height);
        float minHeight = 255.0f;
        float maxHeight = 0.0f;

        if (normalizeHeight) {
            // Find min and max heights for normalization
            for (int i = 0; i < width * height; i++) {
                float h = static_cast<float>(data[i]);
                minHeight = std::min(minHeight, h);
                maxHeight = std::max(maxHeight, h);
            }
        }

        float range = maxHeight - minHeight;
        for (int i = 0; i < width * height; i++) {
            if (normalizeHeight && range > 0) {
                heights[i] = (static_cast<float>(data[i]) - minHeight) / range;
            }
            else {
                heights[i] = static_cast<float>(data[i]) / 255.0f;
            }
        }

        stbi_image_free(data);
        return heights;
    }

    ew::Mesh createHeightmapMesh(const std::vector<float>& heightmapData, int width, int height, glm::vec3 scale) {
        if (heightmapData.empty() || width <= 0 || height <= 0) {
            std::printf("Invalid heightmap data or dimensions\n");
            return ew::Mesh(ew::MeshData{}); // Return empty mesh
        }

        std::vector<ew::Vertex> vertices;
        std::vector<unsigned int> indices;

        vertices.reserve(width * height);
        indices.reserve((width - 1) * (height - 1) * 6);

        for (int z = 0; z < height; z++) {
            for (int x = 0; x < width; x++) {
                ew::Vertex vertex;

                float fx = (static_cast<float>(x) / (width - 1) - 0.5f) * scale.x;
                float fz = (static_cast<float>(z) / (height - 1) - 0.5f) * scale.z;
                float fy = heightmapData[z * width + x] * scale.y;

                vertex.pos = glm::vec3(fx, fy, fz);
                vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f); // default normal
                vertex.uv = glm::vec2(static_cast<float>(x) / (width - 1), static_cast<float>(z) / (height - 1));
                vertex.tangent = glm::vec3(1.0f, 0.0f, 0.0f); // optional, unused for now

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