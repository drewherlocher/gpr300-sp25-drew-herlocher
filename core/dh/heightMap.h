#pragma once
#include <vector>
#include <glm/glm.hpp>
#include "../ew/mesh.h"

namespace dh {
    /**
     * Loads a heightmap from an image file and returns the height data
     * @param filePath - Path to the heightmap image
     * @param normalizeHeight - Whether to normalize heights to 0-1 range
     * @return Vector of height values
     */
    std::vector<float> loadHeightmapData(const char* filePath, bool normalizeHeight = true);

    /**
     * Gets dimensions of a heightmap image
     * @param filePath - Path to the heightmap image
     * @param width - Output parameter for width
     * @param height - Output parameter for height
     * @return True if successful, false otherwise
     */
    bool getHeightmapDimensions(const char* filePath, int& width, int& height);

    /**
     * Creates a mesh from heightmap data
     * @param heightmapData - Vector of height values
     * @param width - Width of the heightmap
     * @param height - Height of the heightmap
     * @param scale - Scale for the mesh (x, y, z)
     * @return Mesh object
     */
    ew::Mesh createHeightmapMesh(const std::vector<float>& heightmapData,
        int width,
        int height,
        glm::vec3 scale = glm::vec3(1.0f));
}