#include "InputManager.hpp"
#include <algorithm>
#include <cstring>

namespace NE {
    std::array<InputManager::KeyState, InputManager::MaxKeys>         InputManager::s_keys{};
    std::array<InputManager::KeyState, InputManager::MaxMouseButtons> InputManager::s_mouse{};

    double InputManager::s_mouseX = 0.0;
    double InputManager::s_mouseY = 0.0;
    double InputManager::s_prevMouseX = 0.0;
    double InputManager::s_prevMouseY = 0.0;
    double InputManager::s_scrollX = 0.0;
    double InputManager::s_scrollY = 0.0;

    bool InputManager::s_mouseLocked = false;
    bool InputManager::s_cursorVisible = true;

    uint32_t InputManager::s_charBuf[InputManager::CharBufSize] = {};
    int InputManager::s_charHead = 0;
    int InputManager::s_charTail = 0;

    void InputManager::Initialize() {
        std::fill(s_keys.begin(), s_keys.end(), KeyState{});
        std::fill(s_mouse.begin(), s_mouse.end(), KeyState{});
        s_mouseX = s_mouseY = s_prevMouseX = s_prevMouseY = 0.0;
        s_scrollX = s_scrollY = 0.0;
        s_mouseLocked = false;
        s_cursorVisible = true;
        s_charHead = s_charTail = 0;
    }

    void InputManager::Shutdown() {

    }

    void InputManager::BeginFrame() {
        for (auto& k : s_keys) { k.pressed = false; k.released = false; }
        for (auto& m : s_mouse) { m.pressed = false; m.released = false; }

        s_scrollX = 0.0;
        s_scrollY = 0.0;

        s_prevMouseX = s_mouseX;
        s_prevMouseY = s_mouseY;
    }

    void InputManager::OnKey(int key, int /*sc*/, int action, int /*mods*/) {
        if (key < 0 || key >= MaxKeys) return;

        if (action == 1) {
            if (!s_keys[key].down) s_keys[key].pressed = true;
            s_keys[key].down = true;
        } else if (action == 0) {
            if (s_keys[key].down) s_keys[key].released = true;
            s_keys[key].down = false;
        } else if (action == 2) {
            s_keys[key].down = true;
        }
    }

    void InputManager::OnMouseButton(int button, int action, int /*mods*/) {
        if (button < 0 || button >= MaxMouseButtons) return;

        if (action == 1) {
            if (!s_mouse[button].down) s_mouse[button].pressed = true;
            s_mouse[button].down = true;
        } else if (action == 0) {
            if (s_mouse[button].down) s_mouse[button].released = true;
            s_mouse[button].down = false;
        }
    }

    void InputManager::OnCursorPos(double x, double y) {
        s_mouseX = x;
        s_mouseY = y;
    }

    void InputManager::OnScroll(double xoffset, double yoffset) {
        s_scrollX += xoffset;
        s_scrollY += yoffset;
    }

    void InputManager::OnCharInput(uint32_t cp) {
        int next = (s_charHead + 1) % CharBufSize;
        if (next != s_charTail) {
            s_charBuf[s_charHead] = cp;
            s_charHead = next;
        }
    }

    bool InputManager::IsKeyDown(int key) {
        return (key >= 0 && key < MaxKeys) ? s_keys[key].down : false;
    }
    bool InputManager::WasKeyPressed(int key) {
        return (key >= 0 && key < MaxKeys) ? s_keys[key].pressed : false;
    }
    bool InputManager::WasKeyReleased(int key) {
        return (key >= 0 && key < MaxKeys) ? s_keys[key].released : false;
    }

    bool InputManager::IsMouseDown(int button) {
        return (button >= 0 && button < MaxMouseButtons) ? s_mouse[button].down : false;
    }
    bool InputManager::WasMousePressed(int button) {
        return (button >= 0 && button < MaxMouseButtons) ? s_mouse[button].pressed : false;
    }
    bool InputManager::WasMouseReleased(int button) {
        return (button >= 0 && button < MaxMouseButtons) ? s_mouse[button].released : false;
    }

    std::pair<double, double> InputManager::MousePos() {
        return { s_mouseX, s_mouseY };
    }
    std::pair<double, double> InputManager::MouseDelta() {
        return { s_mouseX - s_prevMouseX, s_mouseY - s_prevMouseY };
    }
    std::pair<double, double> InputManager::ScrollDelta() {
        return { s_scrollX, s_scrollY };
    }

    uint32_t InputManager::PopChar() {
        if (s_charTail == s_charHead) return 0;
        uint32_t v = s_charBuf[s_charTail];
        s_charTail = (s_charTail + 1) % CharBufSize;
        return v;
    }

    void InputManager::SetMouseLocked(bool locked) {
        s_mouseLocked = locked;
    }

    bool InputManager::IsMouseLocked() { return s_mouseLocked; }

    void InputManager::SetCursorVisible(bool visible) {
        s_cursorVisible = visible;
    }

    bool InputManager::IsCursorVisible() { return s_cursorVisible; }

}
