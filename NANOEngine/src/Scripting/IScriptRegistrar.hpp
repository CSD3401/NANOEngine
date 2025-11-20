#pragma once

/**
 * @file IScriptRegistrar.hpp
 * @brief Backward compatibility redirect to the new ScriptSDK
 *
 * This file now redirects to the new SDK version of IScriptRegistrar.
 * Existing code that includes this header will automatically use the new SDK.
 */

#include "../../include/ScriptSDK/ScriptAPI.h"

// The IScriptRegistrar interface is now defined in ScriptSDK/ScriptAPI.h
// This header is kept for backward compatibility with existing includes