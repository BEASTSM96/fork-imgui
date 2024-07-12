// dear imgui: Platform Backend for Ruby
// This needs to be used along with a Renderer (e.g. OpenGL3, Vulkan, WebGPU..)

#pragma once

#include "imgui.h"      // IMGUI_IMPL_API

namespace Saturn {
    class RubyWindow;
}

IMGUI_IMPL_API bool     ImGui_ImplRuby_InitForOpenGL( Saturn::RubyWindow* pWindow );
IMGUI_IMPL_API bool     ImGui_ImplRuby_InitForVulkan( Saturn::RubyWindow* pWindow );
IMGUI_IMPL_API bool     ImGui_ImplRuby_InitForOther( Saturn::RubyWindow* pWindow );
IMGUI_IMPL_API void     ImGui_ImplRuby_Shutdown();
IMGUI_IMPL_API void     ImGui_ImplRuby_NewFrame();

// Ruby callbacks
IMGUI_IMPL_API void     ImGui_ImplRuby_WindowFocusCallback( Saturn::RubyWindow* window, bool focused);
IMGUI_IMPL_API void     ImGui_ImplRuby_CursorEnterCallback( Saturn::RubyWindow* window, bool entered);
IMGUI_IMPL_API void     ImGui_ImplRuby_CursorPosCallback(   Saturn::RubyWindow* window, float x, float y );
IMGUI_IMPL_API void     ImGui_ImplRuby_MouseButtonCallback( Saturn::RubyWindow* window, int button, bool state );
IMGUI_IMPL_API void     ImGui_ImplRuby_ScrollCallback( double xoffset, double yoffset);
IMGUI_IMPL_API void     ImGui_ImplRuby_KeyCallback( Saturn::RubyWindow* window, int scancode, bool state, int mods);
IMGUI_IMPL_API void     ImGui_ImplRuby_CharCallback( unsigned int c );
IMGUI_IMPL_API void     ImGui_ImplRuby_MouseHoverWindowCallback( bool state );
IMGUI_IMPL_API void     ImGui_ImplRuby_UpdateEvents();
