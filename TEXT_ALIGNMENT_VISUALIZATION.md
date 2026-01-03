# UIText Alignment Visualization

## Text Rendering Flow

### Coordinate System
```
Screen/Canvas Coordinate System (Top-Left Origin, Y-Down):
┌─────────────────────────────────────────┐
│ (0, 0) ────────────────► X increases    │
│   │                                      │
│   │                                      │
│   │                                      │
│   ▼                                      │
│ Y increases                              │
│                                          │
│                                          │
│                                          │
└─────────────────────────────────────────┘

Key Points:
- Origin (0, 0) is at TOP-LEFT
- X increases to the RIGHT
- Y increases DOWNWARD (opposite of math coordinates)
- This matches Unity's UI coordinate system
```

### Text Rendering Process

#### Step 1: Container Position
```
Container (RectTransform):
┌─────────────────────────────────────┐
│ Top-Left: (x, y)                    │ ← Passed to GenerateVertices
│                                      │
│                                      │
│                                      │
│                                      │
│ Bottom-Right: (x+width, y+height)   │
└─────────────────────────────────────┘

Input to GenerateVertices:
  x, y, z, width, height
  (x, y) = top-left corner of container
```

#### Step 2: Calculate Text Lines
```
Text: "Hello\nWorld"

After CalculateTextLines:
  Line 1: "Hello" (width: 50.0)
  Line 2: "World" (width: 55.0)
```

#### Step 3: Calculate Baseline Position
```
For BOTTOM alignment:
  firstLineBaseline = y + height - totalTextHeight + ascender
                   = 510 + 60 - 36 + 28.50
                   = 562.50

Baseline position in container:
┌─────────────────────────────────────┐
│ Top: 510                             │
│                                      │
│                                      │
│ ────────────────────────────────    │ ← Baseline: 562.50
│                                      │
│ Bottom: 570                           │
└─────────────────────────────────────┘
```

#### Step 4: Render Characters Line by Line

**For each line (top to bottom):**
```
Line 1: "Hello"
  currentY = firstLineBaseline = 562.50
  currentX = x + horizontalOffset = 760.00 (for LEFT alignment)
  
  Render each character left to right:
```

**Character Rendering (Left to Right):**
```
Baseline: 562.50
    │
    │  currentX = 760.00
    │    │
    │    ▼
    │  ┌───┐  ┌───┐  ┌───┐  ┌───┐  ┌───┐
    │  │ H │  │ e │  │ l │  │ l │  │ o │
    │  └───┘  └───┘  └───┘  └───┘  └───┘
    │    │      │      │      │      │
    │    │      │      │      │      │
    └────┴──────┴──────┴──────┴──────┴──► X increases
    
    For each character:
      1. Get glyph metrics (width, height, bearingX, bearingY)
      2. Calculate position:
         charX = currentX + bearingX
         charY = currentY - bearingY  (bearingY is distance above baseline)
      3. Generate quad vertices:
         Top-Left:     (charX, charY)
         Top-Right:    (charX + width, charY)
         Bottom-Right: (charX + width, charY + height)
         Bottom-Left:  (charX, charY + height)
      4. Advance cursor:
         currentX += advanceX + kerning + characterSpacing
```

#### Step 5: Character Quad Generation

**Single Character Quad:**
```
For character 'H' at baseline 562.50:

Glyph Metrics:
  width = 20.0
  height = 30.0
  bearingX = 2.0
  bearingY = 25.0  (25 pixels above baseline)

Position Calculation:
  charX = currentX + bearingX = 760 + 2 = 762
  charY = currentY - bearingY = 562.50 - 25 = 537.50

Quad Vertices (2 triangles, 6 vertices):
  
  (762, 537.50) ──────────── (782, 537.50)
       │                            │
       │                            │
       │                            │
  (762, 567.50) ──────────── (782, 567.50)
  
  Triangle 1: (762,537.50) → (782,537.50) → (782,567.50)
  Triangle 2: (762,537.50) → (782,567.50) → (762,567.50)
```

#### Step 6: Vertex Order and Rendering

**Vertex Generation Order:**
```
For each character, 6 vertices are added:
  1. Top-Left     (charX, charY)
  2. Top-Right    (charX + width, charY)
  3. Bottom-Right (charX + width, charY + height)
  4. Top-Left     (charX, charY)  [duplicate for second triangle]
  5. Bottom-Right (charX + width, charY + height) [duplicate]
  6. Bottom-Left  (charX, charY + height)

Rendering Order:
  Characters: Left → Right
  Lines: Top → Bottom
```

#### Step 7: Complete Text Rendering Flow

```
Container: (760, 510) size 400x60

Step 1: Calculate alignment
  ┌─────────────────────────────────────┐
  │ Top: 510                             │
  │                                      │
  │ ┌──────────────────────────────┐    │
  │ │ Text Block Top: 534          │    │
  │ │                               │    │
  │ │ Baseline: 562.50              │    │ ← Calculated
  │ │                               │    │
  │ │ Text Block Bottom: 570        │    │
  │ └──────────────────────────────┘    │
  │ Bottom: 570                           │
  └─────────────────────────────────────┘

Step 2: Render Line 1 "Hello"
  currentY = 562.50 (baseline)
  currentX = 760.00 (start from left)
  
  H: charX=762, charY=537.50, width=20, height=30
  e: charX=784, charY=540.00, width=15, height=25
  l: charX=801, charY=540.00, width=10, height=25
  l: charX=813, charY=540.00, width=10, height=25
  o: charX=825, charY=542.00, width=18, height=22

Step 3: Move to Line 2 "World"
  currentY += lineHeight = 562.50 + 36 = 598.50
  currentX = 760.00 (reset to left)
  
  W: charX=760, charY=573.50, ...
  o: ...
  r: ...
  l: ...
  d: ...

Step 4: All vertices sent to GPU
  GPU renders triangles in order
  Text appears on screen!
```

### Rendering Pipeline Summary

```
1. UIText Component
   ↓
2. UIRenderSystem::GenerateScreenSpaceVertices()
   - Gets container position (top-left)
   - Calls UITextMeshGenerator::GenerateVertices()
   ↓
3. UITextMeshGenerator::GenerateVertices()
   - Calculates text lines (with wrapping)
   - Calculates baseline position (based on alignment)
   - For each line:
     - For each character:
       - Gets glyph metrics
       - Calculates character position
       - Generates 6 vertices (2 triangles)
   - Returns vector of vertices
   ↓
4. UIRenderSystem::SubmitDrawCommand()
   - Creates UIDrawCommand
   - Sets vertices
   ↓
5. UIRenderer::Render()
   - Sends vertices to GPU
   ↓
6. GPU Shader (UI_Camera.nanoshader)
   - Converts pixel coordinates to NDC
   - Renders text on screen
```

### Key Rendering Details

**Coordinate System:**
- ✅ Top-left origin (0,0) at top-left of screen
- ✅ Y increases downward
- ✅ X increases rightward
- ✅ Matches Unity's UI system

**Character Positioning:**
- Characters positioned from **baseline** (not top-left)
- `charY = baseline - bearingY` (bearingY is distance above baseline)
- Characters can extend above and below baseline

**Rendering Order:**
- Lines: Top to bottom (first line rendered first)
- Characters: Left to right (first character rendered first)
- Vertices: Generated in triangle strip order

**Vertex Format:**
```
UIVertex {
  float x, y, z;      // Position (pixel coordinates)
  float u, v;         // UV coordinates (font atlas)
  float r, g, b, a;   // Color
}
```

---

# UIText Alignment Visualization

## Coordinate System
```
Y=0 (Top)
  ↓
  ↓
  ↓
Y=height (Bottom)
```

## What is the Baseline?

The **baseline** is an imaginary horizontal line that most characters "sit" on. It's the fundamental reference line for positioning text in typography.

### Visual Example

```
┌─────────────────────────────────────┐
│         Ascender (28.50)            │ ← Top of tallest character (H, b, d)
│         ─────────────────          │ ← CAP LINE (top of capital letters)
│                                     │
│         ─────────────────          │ ← BASELINE (where characters sit) ⭐
│                                     │
│         Descender (7.50)            │ ← Bottom of lowest character (g, p, y)
└─────────────────────────────────────┘
         LineHeight (36.00)
```

### Characters on the Baseline

```
Baseline: ────────────────────────────────
          │
          │  H  e  l  l  o     ← Normal characters sit ON the baseline
          │  ─────────────────
          │
          │  g  p  y  q  j     ← Characters with descenders extend BELOW
          │  ─────────────────
          │     │  │  │  │
          │     │  │  │  │
          └─────┴──┴──┴──┴──► Descenders extend below baseline
```

### Real-World Analogy

Think of the baseline like the **line on notebook paper** where you write:
- Most letters sit on the line: `a`, `e`, `o`, `c`, `m`, `n`, `r`, `s`, `u`, `v`, `w`, `x`, `z`
- Tall letters extend above: `H`, `b`, `d`, `k`, `l`, `t`, `f` (and all capitals)
- Letters with tails extend below: `g`, `p`, `y`, `q`, `j`, `y`

### In Code

```cpp
// The baseline is the Y coordinate where characters are positioned
float currentY = firstLineBaseline;  // e.g., 562.50

// Characters are positioned relative to the baseline:
float charY = currentY - metrics.bearingY;
// bearingY = distance from baseline upward to top of character
// So: charY = baseline - bearingY = top of character quad
```

### Why Baseline Matters

1. **Consistent Alignment**: All characters align along the same baseline, making text look uniform
2. **Proper Spacing**: Line height is measured from baseline to baseline (not top to top)
3. **Character Positioning**: Each character's position is calculated relative to the baseline
4. **Multi-line Text**: Each line has its own baseline, spaced by `lineHeight`

### Example: "Hello" on Baseline

```
Baseline: ──────────────────────────────── (Y = 562.50)
          │
          │  H  e  l  l  o
          │  │  │  │  │  │
          │  │  │  │  │  │
          │  │  │  │  │  │
          │  └──┴──┴──┴──┘
          │     │  │  │  │
          │     │  │  │  │
          └─────┴──┴──┴──┴──► Characters extend above and below
          
H: Top at 537.50, Bottom at 567.50 (extends above baseline)
e: Top at 540.00, Bottom at 565.00 (sits on baseline)
l: Top at 540.00, Bottom at 565.00 (sits on baseline)
o: Top at 542.00, Bottom at 564.00 (sits on baseline)
```

### Key Points

- **Baseline is NOT the bottom of characters** - it's where they "sit"
- **Baseline is NOT the bottom of the text box** - characters extend below it
- **Most characters touch the baseline** - lowercase letters like `a`, `e`, `o`
- **Tall characters extend above** - capitals and letters like `b`, `d`, `h`
- **Some characters extend below** - letters with descenders like `g`, `p`, `y`
- **Each line has its own baseline** - spaced by `lineHeight` from the previous line

## Baseline vs Text Box Boundaries

### Important Distinction

The **baseline** and the **bottom of the text box** are different things:

```
Text Box (Container):
┌─────────────────────────────────────┐
│ Top: 510                             │
│                                      │
│ ┌────────────────────────────────┐  │
│ │ Text Block                      │  │
│ │                                 │  │
│ │ Baseline: 562.50 ──────────────│  │ ← Characters sit HERE
│ │                                 │  │
│ │ Bottom: 570.00                  │  │ ← Text box bottom (contains descenders)
│ └────────────────────────────────┘  │
│ Bottom: 570                           │
└─────────────────────────────────────┘

Key Points:
- Baseline (562.50) is INSIDE the text box
- Text box bottom (570.00) contains the entire text block
- Characters extend BELOW the baseline (descenders)
- Text box must be tall enough to contain everything
```

### Visual Example: "Hello" vs "Hello" with descenders

**Without descenders (like "Hello"):**
```
Text Box:
┌─────────────────────┐
│ Top                  │
│                      │
│ Baseline ────────────│ ← Characters sit here
│ H e l l o            │
│                      │
│ Bottom               │ ← Text box bottom
└─────────────────────┘
```

**With descenders (like "Hello py"):**
```
Text Box:
┌─────────────────────┐
│ Top                  │
│                      │
│ Baseline ────────────│ ← Characters sit here
│ H e l l o   p y      │
│              │  │     │ ← Descenders extend BELOW baseline
│              │  │     │
│ Bottom               │ ← Text box bottom (must contain descenders)
└─────────────────────┘
```

### In Your Code

```cpp
// Baseline position (where characters sit)
float firstLineBaseline = y + height - totalTextHeight + ascender;
// e.g., = 510 + 60 - 36 + 28.50 = 562.50

// Text block bottom (contains everything including descenders)
float textBlockBottom = firstLineTop + totalTextHeight;
// e.g., = 534 + 36 = 570.00

// The baseline is ABOVE the text box bottom
// The text box bottom must contain characters that extend below baseline
```

### Why This Matters for Alignment

When you set **MIDDLE** alignment:
- The **text block** (including descenders) should be centered
- The **baseline** will be positioned to achieve this
- The baseline is NOT centered - it's positioned to center the entire text block

```
MIDDLE Alignment:
┌─────────────────────────────────────┐
│ Top: 510                             │
│                                      │
│ ┌────────────────────────────────┐  │
│ │ Text Block (centered)           │  │
│ │                                 │  │
│ │ Baseline: 550.50 ──────────────│  │ ← Baseline is here
│ │                                 │  │    (NOT at center)
│ │ Bottom: 558.00                  │  │ ← Text block bottom
│ └────────────────────────────────┘  │
│                                      │
│ Bottom: 570                           │
└─────────────────────────────────────┘

Container center: 510 + 60/2 = 540.00
Text block center: 522 + 36/2 = 540.00 ✓ (centered)
Baseline: 550.50 (NOT at 540.00, but positioned to center the text block)
```

## Font Metrics Visualization

```
┌─────────────────────────────────────┐
│         Ascender (28.50)            │ ← Top of tallest character
│                                     │
│         ─────────────────          │ ← Baseline (where characters sit) ⭐
│                                     │
│         Descender (7.50)           │ ← Bottom of lowest character
└─────────────────────────────────────┘
         LineHeight (36.00)
```

## Container and Text Block Positioning

### Example: Container at (760, 510) with size 400x60

```
Container:
┌─────────────────────────────────────────────────────────────┐
│ Top: 510.00                                                  │
│                                                              │
│                                                              │
│                                                              │
│                                                              │
│ Bottom: 570.00                                               │
└─────────────────────────────────────────────────────────────┘
Width: 400.00, Height: 60.00
```

## Alignment Calculations

### TOP Alignment
```
Container:
┌─────────────────────────────────────────────────────────────┐
│ Top: 510.00  ← Container Top                                 │
│                                                              │
│ ┌──────────────────────────┐                                │
│ │ Text Block Top: 510.00    │ ← Aligned with container top   │
│ │                            │                                │
│ │ Baseline: 510 + 28.50     │                                │
│ │         = 538.50          │                                │
│ │                            │                                │
│ │ Text Block Bottom:         │                                │
│ │   510 + 36.00 = 546.00     │                                │
│ └──────────────────────────┘                                │
│                                                              │
│ Bottom: 570.00                                               │
└─────────────────────────────────────────────────────────────┘

Calculation:
  firstLineBaseline = y + ascender
                   = 510 + 28.50
                   = 538.50
  
  textBlockTop = firstLineBaseline - ascender
              = 538.50 - 28.50
              = 510.00  ✓ (matches container top)
```

### MIDDLE Alignment
```
Container:
┌─────────────────────────────────────────────────────────────┐
│ Top: 510.00                                                  │
│                                                              │
│ ┌──────────────────────────┐                                │
│ │ Text Block Top:           │                                │
│ │   510 + (60-36)/2         │                                │
│ │   = 510 + 12 = 522.00     │                                │
│ │                            │                                │
│ │ Baseline: 522 + 28.50     │                                │
│ │         = 550.50          │                                │
│ │                            │                                │
│ │ Text Block Bottom:         │                                │
│ │   522 + 36 = 558.00       │                                │
│ └──────────────────────────┘                                │
│                                                              │
│ Bottom: 570.00                                               │
└─────────────────────────────────────────────────────────────┘

Calculation:
  totalTextHeight = lineHeight = 36.00
  verticalOffset = (height - totalTextHeight) * 0.5
                 = (60 - 36) * 0.5
                 = 12.00
  
  firstLineBaseline = y + verticalOffset + ascender
                   = 510 + 12 + 28.50
                   = 550.50
  
  textBlockTop = firstLineBaseline - ascender
              = 550.50 - 28.50
              = 522.00
  
  textBlockCenter = 522 + 36/2 = 540.00
  containerCenter = 510 + 60/2 = 540.00  ✓ (centered)
```

### BOTTOM Alignment (Current Issue)
```
Container:
┌─────────────────────────────────────────────────────────────┐
│ Top: 510.00                                                  │
│                                                              │
│ ┌──────────────────────────┐                                │
│ │ Text Block Top:           │                                │
│ │   510 + 60 - 36           │                                │
│ │   = 534.00                │                                │
│ │                            │                                │
│ │ Baseline: 534 + 28.50     │                                │
│ │         = 562.50          │                                │
│ │                            │                                │
│ │ Text Block Bottom:         │                                │
│ │   534 + 36 = 570.00       │ ← Should align with container │
│ └──────────────────────────┘                                │
│ Bottom: 570.00  ← Container Bottom                          │
└─────────────────────────────────────────────────────────────┘

Calculation (OLD - INCORRECT):
  totalTextHeight = ascender + descender = 28.50 + 7.50 = 36.00
  verticalOffset = height - totalTextHeight
                 = 60 - 36 = 24.00
  
  firstLineTop = y + verticalOffset
              = 510 + 24 = 534.00
  
  firstLineBaseline = firstLineTop + ascender
                   = 534 + 28.50 = 562.50
  
  textBlockBottom = firstLineTop + totalTextHeight
                  = 534 + 36 = 570.00  ✓ (matches container bottom)

BUT: Actual glyph quads extend beyond this!
  Character quad bottom = baseline - bearingY + height
                        = 562.50 - bearingY + glyphHeight
  
  If glyphHeight > (lineHeight - ascender), text extends beyond!
```

## Character Quad Positioning

### How Character Quads are Generated
```
Baseline (currentY = 562.50)
    │
    │  bearingY (distance from baseline upward)
    │    │
    │    ▼
    │  ┌─────┐
    │  │     │ ← charY = currentY - bearingY
    │  │ Glyph│
    │  │     │
    │  └─────┘
    │    │
    │    │ height (glyph bitmap height)
    │    ▼
    │  charBottom = charY + height
    │
    ▼
```

### The Problem
```
Expected Text Block:
┌──────────────────────────┐
│ Top: 534.00              │
│                          │
│ Baseline: 562.50         │
│                          │
│ Bottom: 570.00           │
└──────────────────────────┘

Actual Character Quads:
┌──────────────────────────┐
│ Top: 534.00              │
│                          │
│ Baseline: 562.50         │
│                          │
│                          │
│ Bottom: 570.00+          │ ← Characters extend here!
└──────────────────────────┘
     ▲
     │
  Actual rendered glyphs can extend beyond
  the calculated textBlockBottom because:
  
  glyphBottom = baseline - bearingY + glyphHeight
  
  For some characters, this can be:
  > baseline + descender
  > baseline + (lineHeight - ascender)
```

## Fixed Calculation (Using lineHeight)

### BOTTOM Alignment (FIXED)
```
Container:
┌─────────────────────────────────────────────────────────────┐
│ Top: 510.00                                                  │
│                                                              │
│ ┌──────────────────────────┐                                │
│ │ Text Block Top:           │                                │
│ │   510 + 60 - 36           │                                │
│ │   = 534.00                │                                │
│ │                            │                                │
│ │ Baseline: 534 + 28.50     │                                │
│ │         = 562.50          │                                │
│ │                            │                                │
│ │ Text Block Bottom:         │                                │
│ │   534 + 36 = 570.00       │                                │
│ └──────────────────────────┘                                │
│ Bottom: 570.00  ← Container Bottom                          │
└─────────────────────────────────────────────────────────────┘

Calculation (NEW - CORRECT):
  totalTextHeight = lineHeight * lines.size()
                  = 36.00 * 1 = 36.00
  
  firstLineBaseline = y + height - totalTextHeight + ascender
                   = 510 + 60 - 36 + 28.50
                   = 562.50
  
  firstLineTop = firstLineBaseline - ascender
              = 562.50 - 28.50
              = 534.00
  
  textBlockBottom = firstLineTop + totalTextHeight
                  = 534 + 36
                  = 570.00  ✓ (matches container bottom)
  
  Since lineHeight already accounts for full glyph extents,
  characters should now fit within bounds.
```

## Key Differences

### OLD Calculation (Problematic)
```
totalTextHeight = ascender + descender
                = 28.50 + 7.50 = 36.00

Problem: This doesn't account for:
- Actual glyph bitmap heights
- Line spacing (lineGap)
- Characters that extend beyond font metrics
```

### NEW Calculation (Fixed)
```
totalTextHeight = lineHeight * lines.size()
                = 36.00 * 1 = 36.00

lineHeight = (ascent - descent + lineGap) * scale
           = (ascent - descent + lineGap) * scale

This accounts for:
✓ Full glyph extents
✓ Line spacing
✓ Actual rendered bounds
```

## Visual Summary

```
┌─────────────────────────────────────────────────────────────┐
│ CONTAINER                                                    │
│ Top: y                                                       │
│                                                              │
│ ┌──────────────────────────┐                                │
│ │ TEXT BLOCK                │                                │
│ │                           │                                │
│ │ Top: firstLineTop         │                                │
│ │      = baseline - ascender│                                │
│ │                           │                                │
│ │ Baseline: firstLineBaseline│                               │
│ │                           │                                │
│ │ Bottom: firstLineTop +    │                                │
│ │         totalTextHeight   │                                │
│ └──────────────────────────┘                                │
│                                                              │
│ Bottom: y + height                                           │
└─────────────────────────────────────────────────────────────┘

For BOTTOM alignment:
  textBlockBottom = containerBottom
  firstLineTop = containerBottom - totalTextHeight
  firstLineBaseline = firstLineTop + ascender
```

