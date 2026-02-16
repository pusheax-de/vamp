#pragma once
// SceneFile.h - Binary serialization/deserialization for .vmp scene files

#include "SceneData.h"
#include <string>

namespace vamp
{

// ---------------------------------------------------------------------------
// SceneFile - load and save .vmp binary scene files
// ---------------------------------------------------------------------------
class SceneFile
{
public:
    // Save a SceneData to a .vmp file. Returns true on success.
    static bool Save(const std::string& filePath, const SceneData& scene);

    // Load a SceneData from a .vmp file. Returns true on success.
    static bool Load(const std::string& filePath, SceneData& scene);
};

} // namespace vamp
