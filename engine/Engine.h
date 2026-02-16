#pragma once
// Engine.h - Master include header for the 2D rendering engine

// Core types and constants
#include "EngineTypes.h"

// D3D12 core
#include "RendererD3D12.h"
#include "DescriptorAllocator.h"
#include "UploadManager.h"

// Resources
#include "Texture2D.h"
#include "RenderTarget.h"
#include "SpriteAtlas.h"

// Pipeline
#include "PipelineStates.h"

// Input
#include "InputSystem.h"

// Scene systems
#include "Camera2D.h"
#include "Grid.h"
#include "BackgroundPager.h"
#include "OccluderSet.h"
#include "RoofSystem.h"
#include "LightSystem.h"
#include "FogRenderer.h"
#include "RenderQueue.h"
#include "SceneRenderer.h"
