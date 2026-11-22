/**
 * \file tests/ui/headless_imgui.hpp
 **/
#ifndef OTHER_TESTS_UI_HEADLESS_IMGUI_HPP
#define OTHER_TESTS_UI_HEADLESS_IMGUI_HPP

#include <utility>

#include <imgui/imgui.h>

namespace other {

  /// an ImGui context with no window and no renderer behind it: frames are laid out and
  ///  thrown away, which is all a test of what gets drawn needs
  class headless_imgui {
   public:
    headless_imgui() {
      context = ImGui::CreateContext();
      ImGuiIO& io = ImGui::GetIO();
      io.IniFilename = nullptr;
      io.DisplaySize = ImVec2(1280.f, 720.f);
      io.DeltaTime = 1.f / 60.f;

      uint8_t* pixels = nullptr;
      int32_t width = 0;
      int32_t height = 0;
      io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    }

    ~headless_imgui() {
      ImGui::DestroyContext(context);
    }

    headless_imgui(const headless_imgui&) = delete;
    headless_imgui& operator=(const headless_imgui&) = delete;

    template <typename Draw>
    void frame(Draw&& draw) {
      ImGui::NewFrame();
      std::forward<Draw>(draw)();
      ImGui::EndFrame();
    }

   private:
    ImGuiContext* context = nullptr;
  };

}  // namespace other

#endif  // OTHER_TESTS_UI_HEADLESS_IMGUI_HPP
