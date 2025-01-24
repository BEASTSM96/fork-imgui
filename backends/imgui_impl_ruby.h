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

// Ruby callbacks (private api)
IMGUI_IMPL_API void     ImGui_ImplRuby_MouseButtonCallback( Saturn::RubyWindow* window, int button, bool state );
IMGUI_IMPL_API void     ImGui_ImplRuby_KeyCallback( Saturn::RubyWindow* window, int scancode, bool state, int mods);
