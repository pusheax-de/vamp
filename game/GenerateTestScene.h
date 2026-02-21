#pragma once
// GenerateTestScene.h - Creates a test .vmp scene for development

#include "SceneData.h"
#include "SceneFile.h"
#include <string>

namespace vamp
{

// Generate and save a test scene to the given file path.
// Returns true on success.
bool GenerateTestScene(const std::string& filePath);

} // namespace vamp
