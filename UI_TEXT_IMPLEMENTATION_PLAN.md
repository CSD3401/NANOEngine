# UIText Implementation Plan

## Overview
Implement Unity-style UIText component with full text rendering capabilities.

---

## Step 1: Enhance UIText Component Structure

### Current State:
- ✅ Basic structure exists
- ✅ Text, font, fontSize, color
- ✅ Alignment (horizontal/vertical)
- ✅ Word wrap, rich text flags

### Add Missing Unity Features:

**1.1 Font Style**
```cpp
enum class FontStyle {
    NORMAL,
    BOLD,
    ITALIC,
    BOLD_AND_ITALIC
};
FontStyle fontStyle = FontStyle::NORMAL;
```

**1.2 Overflow Handling**
```cpp
enum class OverflowMode {
    OVERFLOW,      // Text can overflow bounds
    TRUNCATE,      // Text is cut off with "..."
    RESIZE_HEIGHT  // Text area grows to fit content
};
OverflowMode horizontalOverflow = OverflowMode::OVERFLOW;
OverflowMode verticalOverflow = OverflowMode::TRUNCATE;
```

**1.3 Best Fit (Auto-sizing)**
```cpp
bool bestFit = false;
float minSize = 10.0f;
float maxSize = 40.0f;
```

**1.4 Text Spacing**
```cpp
float lineSpacing = 1.0f;        // Line height multiplier
float characterSpacing = 0.0f;   // Character spacing in pixels
float paragraphSpacing = 0.0f;   // Space between paragraphs
```

**1.5 Material Support**
```cpp
std::string materialUUID;  // Material for text rendering (like UIImage)
```

**1.6 Raycast Target**
```cpp
bool raycastTarget = true;  // Can text block UI interactions?
```

**1.7 Text Bounds Calculation**
```cpp
// Runtime: calculated text bounds
float textWidth = 0.0f;
float textHeight = 0.0f;
```

---

## Step 2: Font System Integration

### 2.1 Font Asset System
- Check if font loading system exists
- If not, create font asset loader
- Support TTF/OTF fonts
- Generate font atlas/texture

### 2.2 Font Metrics
- Character width/height
- Baseline, ascent, descent
- Kerning pairs
- Glyph UV coordinates

### 2.3 Font Caching
- Cache loaded fonts by UUID
- Reuse font textures/atlases

---

## Step 3: Text Rendering System

### 3.1 Text Mesh Generation
Create `UITextMeshGenerator` similar to `UIImageMeshGenerator`:
- Generate quads for each character
- Calculate positions based on alignment
- Handle word wrapping
- Handle text overflow

### 3.2 Text Layout Calculation
- Calculate text bounds (width/height)
- Line breaking (word wrap)
- Alignment positioning
- Multi-line support

### 3.3 Integration with UIRenderSystem
- Add UIText rendering path
- Use font texture/material
- Apply color tint
- Handle Z-ordering

---

## Step 4: Text Rendering Features

### 4.1 Basic Text Rendering
- Render single-line text
- Render multi-line text
- Apply font, size, color

### 4.2 Alignment
- Horizontal: LEFT, CENTER, RIGHT, JUSTIFY
- Vertical: TOP, MIDDLE, BOTTOM

### 4.3 Word Wrapping
- Break text at word boundaries
- Respect RectTransform bounds
- Handle overflow modes

### 4.4 Text Overflow
- OVERFLOW: Text extends beyond bounds
- TRUNCATE: Add "..." when text is too long
- RESIZE_HEIGHT: Grow height to fit content

### 4.5 Best Fit
- Automatically adjust fontSize
- Constrain within minSize/maxSize
- Recalculate on text change

---

## Step 5: Advanced Features (Optional)

### 5.1 Rich Text Support
- Basic tags: `<b>`, `<i>`, `<color>`, `<size>`
- Parse rich text markup
- Apply formatting per-character

### 5.2 Font Style
- Bold rendering (or use bold font variant)
- Italic rendering (or use italic font variant)

### 5.3 Text Spacing
- Line spacing multiplier
- Character spacing
- Paragraph spacing

---

## Step 6: Integration Points

### 6.1 UIRenderSystem Integration
- Add UIText rendering in `Update()` or `SubmitDrawCommand()`
- Check for UIText component
- Generate text mesh
- Submit draw commands

### 6.2 UITransformSystem Integration
- Text bounds affect RectTransform size (if RESIZE_HEIGHT)
- Text alignment uses RectTransform bounds

### 6.3 UIInteractionSystem Integration
- Raycast target support
- Text can block button clicks if `raycastTarget = true`

---

## Step 7: Editor Support

### 7.1 Inspector Panel
- Text input field (multi-line)
- Font selection (dropdown/asset picker)
- Font size slider
- Color picker
- Alignment dropdowns
- Word wrap checkbox
- Best fit checkbox
- Overflow mode dropdowns

### 7.2 Gizmo Visualization
- Show text bounds in editor
- Preview text in scene view
- Show alignment guides

---

## Step 8: Serialization

### 8.1 Component Registration
- Add UIText to ComponentKey.hpp
- Add UIText to ComponentTypes tuple in JsonSceneSerializer.cpp
- Include UIText.hpp in JsonSceneSerializer.cpp

### 8.2 Reflection
- Ensure all fields are reflected
- Hidden fields (luid, runtime handles)

---

## Implementation Order (Recommended)

### Phase 1: Core Text Rendering
1. ✅ Enhance UIText component (add missing fields)
2. ⏳ Create font loading system (or integrate existing)
3. ⏳ Create UITextMeshGenerator
4. ⏳ Integrate with UIRenderSystem
5. ⏳ Basic single-line text rendering

### Phase 2: Layout & Alignment
6. ⏳ Multi-line text support
7. ⏳ Word wrapping
8. ⏳ Alignment (horizontal/vertical)
9. ⏳ Text bounds calculation

### Phase 3: Advanced Features
10. ⏳ Overflow handling
11. ⏳ Best fit
12. ⏳ Font style (bold/italic)
13. ⏳ Text spacing

### Phase 4: Polish
14. ⏳ Rich text support (optional)
15. ⏳ Editor gizmo
16. ⏳ Raycast target
17. ⏳ Testing & optimization

---

## Dependencies to Check

1. **Font System**: Does a font loading/rendering system exist?
2. **Text Rendering**: Is there existing text rendering code?
3. **Asset System**: How are fonts loaded (by UUID)?
4. **Material System**: Can text use materials like UIImage?

---

## Unity Text Component Reference

**Core Properties:**
- Text (string)
- Font (Font asset)
- Font Style (Normal, Bold, Italic, BoldAndItalic)
- Font Size (float)
- Line Spacing (float)
- Rich Text (bool)
- Alignment (Horizontal: Left, Center, Right, Justify; Vertical: Top, Middle, Bottom)
- Align By Geometry (bool)
- Horizontal Overflow (Wrap, Overflow)
- Vertical Overflow (Truncate, Overflow)
- Best Fit (bool, with Min/Max Size)
- Color (Color)
- Material (Material)
- Raycast Target (bool)

**Your Current Implementation:**
- ✅ Text
- ✅ Font (UUID)
- ✅ Font Size
- ✅ Color
- ✅ Alignment (horizontal/vertical)
- ✅ Word Wrap
- ✅ Rich Text (flag)
- ❌ Font Style
- ❌ Overflow Modes
- ❌ Best Fit
- ❌ Line/Character/Paragraph Spacing
- ❌ Material
- ❌ Raycast Target

