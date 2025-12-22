image.png# UIText Phase 1 Testing Guide

## Overview
Phase 1 has enhanced the `UIText` component with all missing Unity properties. The component structure is now complete, but **text rendering is not yet implemented** (that's Phase 3-4).

---

## What Was Added

### ✅ New Properties Added:

1. **Font Style** (`FontStyle` enum)
   - NORMAL, BOLD, ITALIC, BOLD_AND_ITALIC

2. **Overflow Modes** (`OverflowMode` enum)
   - Horizontal: WRAP, OVERFLOW
   - Vertical: TRUNCATE, OVERFLOW

3. **Best Fit** (Auto-sizing)
   - `bestFit` (bool)
   - `minSize` (float)
   - `maxSize` (float)

4. **Text Spacing**
   - `lineSpacing` (float) - Line height multiplier
   - `characterSpacing` (float) - Character spacing in pixels
   - `paragraphSpacing` (float) - Space between paragraphs

5. **Material Support**
   - `materialUUID` (string) - Material for text rendering

6. **Raycast Target**
   - `raycastTarget` (bool) - Can text block UI interactions?

7. **Runtime Text Bounds**
   - `textWidth` (float) - Calculated text width
   - `textHeight` (float) - Calculated text height

8. **Alignment Enhancement**
   - Added `JUSTIFY` to horizontal alignment

---

## How to Test Phase 1

### Test 1: Component Compilation
**Goal**: Verify the component compiles without errors.

**Steps**:
1. Build the project
2. Check for compilation errors in `UIText.hpp`
3. ✅ **Expected**: No compilation errors

---

### Test 2: Component Serialization
**Goal**: Verify all new fields are serialized/deserialized correctly.

**Steps**:
1. Create a UIText entity (manually or via editor)
2. Set various properties:
   - `fontStyle = BOLD`
   - `horizontalOverflow = WRAP`
   - `bestFit = true`
   - `lineSpacing = 1.5f`
   - `raycastTarget = false`
3. Save the scene
4. Exit and reload the scene
5. ✅ **Expected**: All properties are preserved

**Note**: You'll need to add editor support first (see below).

---

### Test 3: Inspector Panel (Editor Support Needed)
**Goal**: Verify all new fields appear in the Inspector.

**Status**: ⚠️ **Not Yet Implemented** - Inspector panel needs to be updated.

**What Needs to be Added**:
- Font Style dropdown
- Overflow Mode dropdowns (horizontal/vertical)
- Best Fit checkbox + min/max size fields
- Line/Character/Paragraph spacing fields
- Material UUID field (asset picker)
- Raycast Target checkbox

---

### Test 4: Component Registration
**Goal**: Verify UIText is properly registered in ECS.

**Steps**:
1. Check `ECSCoordinator.cpp` - UIText should be registered
2. Check `ComponentKey.hpp` - UIText should have a key
3. Check `JsonSceneSerializer.cpp` - UIText should be included
4. ✅ **Expected**: All registration points are correct

**Verification**:
```cpp
// In ECSCoordinator.cpp
RegisterComponent<Component::UIText>(); // ✅ Should exist

// In ComponentKey.hpp
NE_COMPONENT_KEY(NE::ECS::Component::UIText, "UIText"); // ✅ Should exist

// In JsonSceneSerializer.cpp
#include "../ECS/Components/UIText.hpp"; // ✅ Should exist
```

---

## Current Limitations

### ⚠️ Not Yet Implemented:
1. **Text Rendering** - Text won't appear on screen yet (Phase 3-4)
2. **Font Loading** - No font system exists yet (Phase 2)
3. **Editor UI** - Inspector panel doesn't show new fields (Phase 6)
4. **Entity Creation** - No `CreateUITextEntity` function yet (optional)

---

## Next Steps

### Phase 2: Font System
- Create Font resource class
- Load TTF/OTF fonts
- Generate font atlas/texture
- Store glyph metrics

### Phase 3: Text Mesh Generation
- Create `UITextMeshGenerator`
- Generate quads for characters
- Calculate positions

### Phase 4: Rendering Integration
- Integrate with `UIRenderSystem`
- Render text on screen

---

## Manual Testing (If Editor Support Exists)

If you manually create a UIText entity in code:

```cpp
// Example: Create UIText entity manually
auto& ecs = GetScene().GetECSCoordinator();
uint32_t textEntity = ecs.CreateEntity();

Component::UIText text;
text.text = "Hello World";
text.fontSize = 24.0f;
text.fontStyle = Component::UIText::FontStyle::BOLD;
text.horizontalAlign = Component::UIText::Alignment::CENTER;
text.bestFit = true;
text.minSize = 10.0f;
text.maxSize = 40.0f;
text.lineSpacing = 1.2f;
text.raycastTarget = false;

ecs.AddComponent(textEntity, text);
```

**Note**: This won't render yet, but you can verify the component structure is correct.

---

## Summary

✅ **Phase 1 Complete**:
- All Unity properties added to UIText component
- Reflection macros updated
- Component registered in ECS
- Serialization support (via reflection)

⏳ **Pending**:
- Font system (Phase 2)
- Text rendering (Phase 3-4)
- Editor UI (Phase 6)

---

## Verification Checklist

- [x] UIText component enhanced with all new fields
- [x] Reflection macros include all new fields
- [x] Component registered in ECSCoordinator
- [x] Component key exists in ComponentKey.hpp
- [x] Component included in JsonSceneSerializer.cpp
- [ ] Inspector panel updated (Phase 6)
- [ ] Font system created (Phase 2)
- [ ] Text rendering implemented (Phase 3-4)

