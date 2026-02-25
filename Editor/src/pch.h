/*!
\file       pch.h
\brief      Precompiled header for Editor - improves build performance by precompiling
            frequently used standard library and third-party headers.
*/

#ifndef EDITOR_PCH_H
#define EDITOR_PCH_H

// Standard library headers (most expensive to compile)
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Windows headers (when on Windows)
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

#endif // EDITOR_PCH_H
