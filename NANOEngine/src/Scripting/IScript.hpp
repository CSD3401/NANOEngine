/**
 * @file IScript.hpp
 * @brief Backward compatibility redirect to the new ScriptSDK
 *
 * This file has been refactored to provide a clean API layer.
 * Existing code will continue to work, but new code should use the
 * ScriptSDK headers directly.
 *
 * MIGRATION PATH:
 * 1. Existing scripts: No changes needed - they will automatically use the new SDK
 * 2. New scripts: Include <ScriptSDK/ScriptAPI.h> directly
 * 3. Standalone script DLLs: Only need ScriptSDK headers + NANOEngine.lib
 */

#pragma once

// Redirect to the clean SDK API
#include "IScript_Compat.hpp"

// This header is now just a thin compatibility layer.
// The actual implementation is in:
//   - include/ScriptSDK/ScriptAPI.h (public interface)
//   - include/ScriptSDK/ScriptTypes.h (public types)
//   - src/Scripting/ScriptAPI.cpp (implementation/adapter)
