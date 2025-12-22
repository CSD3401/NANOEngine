# UIText Implementation Analysis & Next Steps

## Current State Summary

### ✅ What's Working

1. **UIText Component** - Fully implemented with all Unity properties:
   - Text content, font UUID, fontSize, color
   - Font style (NORMAL, BOLD, ITALIC, BOLD_AND_ITALIC)
   - Alignment (horizontal: LEFT, CENTER, RIGHT, JUSTIFY; vertical: TOP, MIDDLE, BOTTOM)
   - Overflow modes (WRAP, VISIBLE, TRUNCATE)
   - Best fit, text spacing, rich text, material support, raycast target
   - Runtime text bounds (textWidth, textHeight)

2. **Entity Creation** - `CreateUITextEntity()` function exists and works:
   - Creates entity with `EntityMeta` component
   - Creates `UIRectTransform` component with proper parent linkage
   - Creates `UIText` component with default values
   - Located in: `NANOEngine/src/EditorInterface/ECSExports.cpp:417`

3. **Component Registration** - UIText is registered in ECS system

### ❌ What's Missing (Why Text Doesn't Render)

1. **UIRenderSystem Doesn't Collect UIText Entities**
   - **Location**: `NANOEngine/src/ECS/Systems/UIRenderSystem.cpp:91-118`
   - **Problem**: `CollectCanvasChildren()` only collects entities with `UIImage` component (line 98)
   - **Current code**: `if (!m_cm->HasComponent<UIImage>(e)) continue;`
   - **Fix needed**: Also collect entities with `UIText` component

2. **UIRenderSystem Doesn't Render UIText**
   - **Location**: `NANOEngine/src/ECS/Systems/UIRenderSystem.cpp:248-283`
   - **Problem**: `RenderCanvasChildren()` only processes `UIImage` components (line 261)
   - **Current code**: `auto& img = m_cm->GetComponent<UIImage>(e);`
   - **Fix needed**: Add separate rendering path for `UIText` components

3. **No Font System**
   - No font loading/management system exists
   - No font atlas/texture generation
   - No font metrics (glyph widths, kerning, etc.)
   - UIText component has `fontUUID` but no way to load it

4. **No Text Mesh Generator**
   - No `UITextMeshGenerator` class (similar to `UIImageMeshGenerator`)
   - No way to convert text string into vertex quads
   - No text layout calculation (word wrapping, alignment, etc.)

5. **No Text Rendering Path**
   - UIRenderSystem has no code to:
     - Generate vertices for text characters
     - Handle text alignment
     - Handle word wrapping
     - Handle text overflow modes

---

## Next Steps (Implementation Order)

### Phase 2: Font System (Prerequisites)

**Goal**: Create a font loading and management system.

#### 2.1 Create Font Resource Class
- **File**: `NANOEngine/src/Graphics/Core/Font.hpp` and `.cpp`
- **Requirements**:
  - Load TTF/OTF fonts from file or memory
  - Generate font atlas texture (using stb_truetype or similar)
  - Store glyph metrics (width, height, advance, bearing)
  - Store UV coordinates for each glyph
  - Support kerning pairs
  - Cache fonts by UUID

#### 2.2 Font Integration with ResourceManager
- Register Font as a loadable resource type
- Support loading fonts via UUID (like textures and materials)
- Font should implement `IResource` interface

#### 2.3 Font Atlas Generation
- Use library like `stb_truetype.h` (already in codebase: `extern/jolt/TestFramework/External/stb_truetype.h`)
- Generate texture atlas with all printable characters
- Store glyph data (position, size, UVs, advance)

---

### Phase 3: Text Mesh Generation

**Goal**: Convert text strings into renderable vertex quads.

#### 3.1 Create UITextMeshGenerator
- **File**: `NANOEngine/src/Graphics/Core/UITextMeshGenerator.hpp` and `.cpp`
- **Similar to**: `UIImageMeshGenerator` (reference implementation)
- **Functions needed**:
  ```cpp
  std::vector<UIVertex> GenerateTextVertices(
      const UIText& text,
      const Font& font,
      float x, float y, float z,
      float width, float height,
      const Vec4& color
  );
  ```

#### 3.2 Text Layout Calculation
- **Calculate text bounds** (width/height based on font metrics)
- **Word wrapping** (break text at word boundaries, respect RectTransform width)
- **Alignment positioning**:
  - Horizontal: LEFT, CENTER, RIGHT, JUSTIFY
  - Vertical: TOP, MIDDLE, BOTTOM
- **Multi-line support** (handle `\n` and word wrap)
- **Text overflow handling**:
  - VISIBLE: Text extends beyond bounds
  - TRUNCATE: Add "..." when text is too long
  - WRAP: Break into multiple lines

#### 3.3 Character Quad Generation
- For each character in text:
  - Get glyph metrics from font
  - Calculate position based on alignment and previous characters
  - Generate quad vertices (4 vertices per character)
  - Set UV coordinates from font atlas
  - Apply character spacing and line spacing

---

### Phase 4: UIRenderSystem Integration

**Goal**: Make UIRenderSystem render UIText components.

#### 4.1 Update CollectCanvasChildren()
- **File**: `NANOEngine/src/ECS/Systems/UIRenderSystem.cpp:91`
- **Change**: Collect entities with EITHER `UIImage` OR `UIText`
- **Code change**:
  ```cpp
  // OLD:
  if (!m_cm->HasComponent<UIImage>(e)) continue;
  
  // NEW:
  if (!m_cm->HasComponent<UIImage>(e) && !m_cm->HasComponent<UIText>(e)) continue;
  ```

#### 4.2 Add UIText Rendering Path
- **File**: `NANOEngine/src/ECS/Systems/UIRenderSystem.cpp:248`
- **Add**: Separate rendering logic for UIText in `RenderCanvasChildren()`
- **Steps**:
  1. Check if entity has UIText component
  2. Load font if not already loaded (using fontUUID)
  3. Generate text vertices using UITextMeshGenerator
  4. Submit draw command with font texture

#### 4.3 Add Font Loading in Init/OnEntityAdded
- **File**: `NANOEngine/src/ECS/Systems/UIRenderSystem.cpp:34`
- **Similar to**: How UIImage loads textures (lines 42-48)
- **Code**:
  ```cpp
  if (m_cm->HasComponent<UIText>(e)) {
      auto& text = m_cm->GetComponent<UIText>(e);
      if (!text.fontUUID.empty() && text.fontHandle == 0) {
          auto font = ResourceManager::GetInstance()
              .LoadResource<Font>(text.fontUUID);
          if (font) {
              text.fontHandle = font->GetHandle(); // or similar
          }
      }
  }
  ```

#### 4.4 Create GenerateTextVertices() Function
- **File**: `NANOEngine/src/ECS/Systems/UIRenderSystem.cpp`
- **Similar to**: `GenerateScreenSpaceVertices()` and `GenerateWorldSpaceVertices()`
- **Function**:
  ```cpp
  std::vector<UIVertex> GenerateTextVertices(
      Entity entity,
      const UITransformSystem::WorldTransform& worldTransform,
      const Component::UIText& text
  );
  ```

#### 4.5 Create SubmitTextDrawCommand() Function
- **File**: `NANOEngine/src/ECS/Systems/UIRenderSystem.cpp`
- **Similar to**: `SubmitDrawCommand()` for images
- **Differences**:
  - Use font texture instead of image texture
  - Use text color instead of image color
  - Handle text-specific properties (alignment, wrapping, etc.)

---

### Phase 5: Advanced Features (Optional)

#### 5.1 Best Fit (Auto-sizing)
- Calculate text bounds
- Adjust fontSize to fit within RectTransform
- Constrain within minSize/maxSize

#### 5.2 Rich Text Support
- Parse basic tags: `<b>`, `<i>`, `<color>`, `<size>`
- Apply formatting per-character

#### 5.3 Font Style Rendering
- Bold: Use bold font variant or render with offset
- Italic: Apply shear transformation or use italic font variant

---

## Implementation Priority

### 🔴 Critical (Must Have)
1. **Phase 2: Font System** - Can't render text without fonts
2. **Phase 3: Text Mesh Generation** - Need to convert text to vertices
3. **Phase 4: UIRenderSystem Integration** - Need to actually render the text

### 🟡 Important (Should Have)
4. **Basic text rendering** (single-line, simple alignment)
5. **Word wrapping**
6. **Multi-line support**

### 🟢 Nice to Have (Can Add Later)
7. **Best fit**
8. **Rich text**
9. **Font style rendering**

---

## Code Locations Reference

### Files to Modify:
1. `NANOEngine/src/ECS/Systems/UIRenderSystem.cpp` - Add UIText rendering
2. `NANOEngine/src/ECS/Systems/UIRenderSystem.hpp` - Add UIText methods

### Files to Create:
1. `NANOEngine/src/Graphics/Core/Font.hpp` - Font resource class
2. `NANOEngine/src/Graphics/Core/Font.cpp` - Font implementation
3. `NANOEngine/src/Graphics/Core/UITextMeshGenerator.hpp` - Text mesh generator
4. `NANOEngine/src/Graphics/Core/UITextMeshGenerator.cpp` - Text mesh implementation

### Reference Files:
1. `NANOEngine/src/Graphics/Core/UIImageMeshGenerator.hpp` - Reference for mesh generation
2. `NANOEngine/src/ECS/Components/UIText.hpp` - UIText component definition
3. `NANOEngine/src/ECS/Systems/UIRenderSystem.cpp:91-283` - Current rendering logic

---

## Quick Start: Minimal Implementation

To get text rendering working with minimal effort:

1. **Use existing font library** (stb_truetype.h is already in codebase)
2. **Create simple Font class** that loads TTF and generates basic atlas
3. **Create UITextMeshGenerator** that generates quads for simple single-line text
4. **Modify UIRenderSystem** to collect and render UIText entities
5. **Test with simple "Hello World" text**

This will get basic text rendering working, then you can add features incrementally.

---

## Testing Checklist

Once implemented, test:
- [ ] Text appears on screen
- [ ] Text respects RectTransform bounds
- [ ] Text alignment works (LEFT, CENTER, RIGHT)
- [ ] Text color is applied correctly
- [ ] Font size affects text size
- [ ] Word wrapping works
- [ ] Multi-line text works
- [ ] Text renders in both Screen Space and World Space canvases

