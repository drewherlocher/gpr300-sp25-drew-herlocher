#pragma once
#include <vector>
#include <glm/glm.hpp>
#include "../ew/mesh.h"

namespace dh {

    std::vector<float> loadHeightmapData(const char* filePath, bool normalizeHeight = true);
   
    bool getHeightmapDimensions(const char* filePath, int& width, int& height);

    ew::Mesh createHeightmapMesh(const std::vector<float>& heightmapData,
        int width,
        int height,
        glm::vec3 scale = glm::vec3(1.0f));
}