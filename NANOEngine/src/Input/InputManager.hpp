#ifndef INPUT_MANAGER_HPP
#define INPUT_MANAGER_HPP

#include <array>
#include <cstdint>
#include <utility>
#include "../NANOEngineAPI.hpp"

namespace NE {

    class InputManager {
    public:
        static NANOENGINE_API void Initialize();
        static NANOENGINE_API void Shutdown();
        static NANOENGINE_API void BeginFrame();

        static NANOENGINE_API void OnKey(int key, int scancode, int action, int mods);
        static NANOENGINE_API void OnMouseButton(int button, int action, int mods);
        static NANOENGINE_API void OnCursorPos(double x, double y);
        static NANOENGINE_API void OnScroll(double xoffset, double yoffset);

        static NANOENGINE_API bool IsKeyDown(int key);
        static NANOENGINE_API bool WasKeyPressed(int key);
        static NANOENGINE_API bool WasKeyReleased(int key);

        static NANOENGINE_API bool IsMouseDown(int button);
        static NANOENGINE_API bool WasMousePressed(int button);
        static NANOENGINE_API bool WasMouseReleased(int button);

        static NANOENGINE_API std::pair<double, double> MousePos();
        static NANOENGINE_API std::pair<double, double> MouseDelta();
        static NANOENGINE_API std::pair<double, double> ScrollDelta();

        static NANOENGINE_API void OnCharInput(uint32_t codepoint);
        static NANOENGINE_API uint32_t PopChar(); // returns 0 if empty (simple FIFO)

        static NANOENGINE_API void SetMouseLocked(bool locked);  // engine can request lock; editor implements actual cursor mode
        static NANOENGINE_API bool IsMouseLocked();

        static NANOENGINE_API void SetCursorVisible(bool visible);
        static NANOENGINE_API bool IsCursorVisible();

    private:
        struct KeyState {
            bool down = false;
            bool pressed = false; 
            bool released = false;
        };

        static constexpr int MaxKeys = 512;
        static constexpr int MaxMouseButtons = 8;

        static std::array<KeyState, MaxKeys>         s_keys;
        static std::array<KeyState, MaxMouseButtons> s_mouse;

        static double s_mouseX;
        static double s_mouseY;
        static double s_prevMouseX;
        static double s_prevMouseY;
        static double s_scrollX;
        static double s_scrollY;

        static bool s_mouseLocked;
        static bool s_cursorVisible;

        // very small ring buffer for chars (UI/widgets that need text)
        static constexpr int CharBufSize = 32;
        static uint32_t s_charBuf[CharBufSize];
        static int s_charHead; // write
        static int s_charTail; // read
    };

}

#endif