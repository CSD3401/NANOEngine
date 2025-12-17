#pragma once

namespace Editor {
    enum class EditorState : unsigned char {
        Edit,
        Play,
        Paused
    };

    extern EditorState g_EditorState;
}
