/**
 * @file Renderer.h
 * @brief SDK-level Renderer interface
 *
 * This header provides access to renderer operations for scripts.
 * All functions are exported from NANOEngine.dll.
 *
 * IMPORTANT: This header is STANDALONE - no internal engine includes.
 * Declarations are duplicated from the engine and must be kept in sync.
 */

#pragma once

#include <cstdint>
#include <string>

namespace NE {
namespace Renderer {

    //=========================================================================
    // QUERY NAMESPACE - Read-only renderer queries
    //=========================================================================

    namespace Query {
        // Future query functions can go here
    }

    //=========================================================================
    // COMMAND NAMESPACE - Renderer commands
    //=========================================================================

    namespace Command {
        /// Assign a model to an entity by UUID
        /// @param e Entity ID
        /// @param uuid Model asset UUID string
        __declspec(dllimport) void AssignModel(uint32_t e, const std::string& uuid);

        /// Assign a material to an entity by UUID
        /// @param e Entity ID
        /// @param uuid Material asset UUID string
        __declspec(dllimport) void AssignMaterial(uint32_t e, const std::string& uuid);
    }

} // namespace Renderer
} // namespace NE
