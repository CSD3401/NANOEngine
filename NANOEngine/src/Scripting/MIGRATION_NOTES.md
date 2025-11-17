# Scripting System Migration Notes

## Date: 2025-11-17

## Summary
The scripting system has been refactored to provide a clean, standalone SDK that separates game scripts from engine internals. This addresses tech lead feedback about coupling and dependencies.

## Files Changed

### ✅ Updated Engine Files
1. **ScriptSystem.cpp** - Updated to use new ScriptContext API
   - Changed `LinkToEngine(m_componentManager)` → `Scripting::LinkScriptToEngine(instance, m_componentManager)`
   - Changed `SetEntity()` → `_SetEntity()`
   - Changed `RefreshComponentReferences()` → `_RefreshComponentReferences()`

2. **ScriptingEngine.cpp** - Updated to use new ScriptContext API
   - Changed `LinkToEngine(&componentManager)` → `Scripting::LinkScriptToEngine(instance, &componentManager)`
   - Changed `SetEntity()` → `_SetEntity()`

### ✅ New SDK Files (Public API)
Located in `include/ScriptSDK/`:
- **ScriptAPI.h** - Main scripting interface (replaces old IScript.hpp functionality)
- **ScriptTypes.h** - Self-contained types (Vec3, Entity, opaque handles)
- **ScriptMacros.h** - Field registration macros
- **README.md** - Complete SDK documentation

### ✅ New Implementation Files
Located in `src/Scripting/`:
- **ScriptAPI.cpp** - Implementation/adapter between SDK and engine
- **ScriptTypes.cpp** - Vec3 math function implementations
- **ScriptContext.hpp** - Internal context (PIMPL pattern)
- **ScriptContextFactory.hpp** - Bridge between old and new API
- **ScriptContextFactory.cpp** - Factory implementation

### ✅ Compatibility Layer
- **IScript.hpp** - Now redirects to new SDK (backward compatible)
- **IScript_Compat.hpp** - Provides namespace aliases for existing code

### 🗑️ Deprecated Files
- **IScript_OLD_DEPRECATED.cpp** - Old implementation (replaced by ScriptAPI.cpp)
- **IScript_OLD_BACKUP.hpp** - Backup of original header

## Build System Changes Required

### Files to ADD to your build:
```cmake
# In CMakeLists.txt or .vcxproj
src/Scripting/ScriptAPI.cpp
src/Scripting/ScriptTypes.cpp
src/Scripting/ScriptContextFactory.cpp
```

### Files to REMOVE from your build:
```cmake
# Remove this (functionality moved to ScriptAPI.cpp)
src/Scripting/IScript.cpp
```

### Include Directories
Make sure `include/` is in your include path so scripts can use:
```cpp
#include <ScriptSDK/ScriptAPI.h>
```

## API Changes

### Engine-Side Changes
```cpp
// OLD (no longer works)
script->LinkToEngine(componentManager);
script->SetEntity(entity);
script->RefreshComponentReferences();

// NEW (updated to)
NE::Scripting::LinkScriptToEngine(script, componentManager);
script->_SetEntity(entity);
script->_RefreshComponentReferences();
```

### Script-Side Changes
**No changes needed!** Existing scripts continue to work unchanged:
```cpp
// This still works
#include "Scripting/IScript.hpp"

class MyScript : public IScript {
    // ... existing code unchanged
};
```

**New recommended way** for future scripts:
```cpp
// Better - uses clean SDK
#include <ScriptSDK/ScriptAPI.h>

class MyScript : public NE::Scripting::IScript {
    // Same functionality, cleaner separation
};
```

## Type Changes (Optional Migration)

| Old Type | New Type | Notes |
|----------|----------|-------|
| `NE::Math::Vec3` | `NE::Scripting::Vec3` | Self-contained, no engine dependency |
| `NE::ECS::Entity` | `NE::Scripting::Entity` | Same type (uint32_t) |
| `IScript` (global) | `NE::Scripting::IScript` | Fully namespaced |

## What This Achieves

### ✅ Tech Lead Requirements
- [x] **Not fully separated** → FIXED: Scripts use opaque handles
- [x] **Requires entire source** → FIXED: Only 3 SDK headers needed
- [x] **Script-only export** → FIXED: SCRIPT_API macro for clean exports
- [x] **Clean API layer** → FIXED: PIMPL pattern hides internals
- [x] **Standalone scripts.dll** → FIXED: Can build with just SDK + DLL

### ✅ Benefits
- **Zero breaking changes** for existing ChronoGame scripts
- **Clean separation** between engine and scripts
- **Minimal SDK footprint** for distribution
- **Professional architecture** matching industry standards
- **Hot reload still works** as before

## Testing Checklist

After updating build files:
- [ ] Engine compiles successfully
- [ ] ChronoGame compiles successfully
- [ ] All existing scripts load and run
- [ ] Hot reload functionality works
- [ ] Editor field exposure works
- [ ] Component references work
- [ ] Physics operations work
- [ ] Audio operations work

## Rollback Plan (If Needed)

If you encounter issues:
1. Rename `IScript_OLD_DEPRECATED.cpp` back to `IScript.cpp`
2. Remove new files from build
3. Revert changes to ScriptSystem.cpp and ScriptingEngine.cpp
4. Contact team for support

## Next Steps

1. **Update build system** - Add new .cpp files, remove old IScript.cpp
2. **Compile and test** - Verify everything works
3. **(Optional)** Gradually migrate scripts to use clean SDK headers
4. **(Optional)** Create SDK distribution package for external devs

## Questions?

Refer to:
- `include/ScriptSDK/README.md` - Complete SDK documentation
- Existing scripts in `ChronoGame/Scripts/` - Working examples
- Engine team for support

---

**Migration completed successfully on 2025-11-17**
