#pragma once

// This file defines the import/export macros for the Engine DLL.

// We define a macro, ENGINE_BUILD_DLL, only when building the Engine project itself.
#ifdef ENGINE_BUILD_DLL
    // If we are building the Engine DLL, classes are marked for EXPORT.
#define ENGINE_API __declspec(dllexport)
#else
    // If another project (like GameCode) includes this header, classes are marked for IMPORT.
#define ENGINE_API __declspec(dllimport)
#endif