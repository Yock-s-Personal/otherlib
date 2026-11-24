/**
 * \file ui/script_draw_guard.cpp
 **/
#include "ui/script_draw_guard.hpp"

#include <cstring>
#include <format>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include "core/profiler.hpp"
#include "thread/thread_safety.hpp"

#include "scripting/dotnet_bindings/ui_draw_list_bindings.hpp"

namespace other {
  namespace ui {
    namespace {

      void note_imgui_error(ImGuiContext*, void* user_data, const char* message) {
        auto* errors = static_cast<script_draw_errors*>(user_data);
        if (errors->count++ == 0) {
          errors->first = message;
        }
      }

    }  // namespace

    std::string script_draw_errors::describe() const {
      if (count == 0) {
        return "";
      }
      return count == 1 ? std::format("ImGui: {}", first) : std::format("ImGui: {} (and {} more)", first, count - 1);
    }

    script_draw_errors guard_script_draw(const std::function<void()>& draw) {
      ASSERT_MAIN_THREAD();
      PROFILE_SECTION("ui::guard_script_draw");
      ImGuiContext& context = *ImGui::GetCurrentContext();
      ImGuiIO& io = context.IO;
      const bool asserts = io.ConfigErrorRecoveryEnableAssert;
      const bool debug_log = io.ConfigErrorRecoveryEnableDebugLog;
      const bool tooltip = io.ConfigErrorRecoveryEnableTooltip;
      const ImGuiErrorCallback callback = context.ErrorCallback;
      void* const callback_data = context.ErrorCallbackUserData;

      script_draw_errors errors;
      io.ConfigErrorRecoveryEnableAssert = false;
      io.ConfigErrorRecoveryEnableDebugLog = false;
      io.ConfigErrorRecoveryEnableTooltip = false;
      context.ErrorCallback = note_imgui_error;
      context.ErrorCallbackUserData = &errors;

      ImGuiErrorRecoveryState before{};
      ImGui::ErrorRecoveryStoreState(&before);

      draw();

      if (bindings::close_script_clip_rects() > 0) {
        note_imgui_error(&context, &errors, "Missing PopClipRect()");
      }
      ImGuiErrorRecoveryState after{};
      ImGui::ErrorRecoveryStoreState(&after);
      if (std::memcmp(&before, &after, sizeof(before)) != 0) {
        ImGui::ErrorRecoveryTryToRecoverState(&before);
      }

      io.ConfigErrorRecoveryEnableAssert = asserts;
      io.ConfigErrorRecoveryEnableDebugLog = debug_log;
      io.ConfigErrorRecoveryEnableTooltip = tooltip;
      context.ErrorCallback = callback;
      context.ErrorCallbackUserData = callback_data;
      return errors;
    }

  }  // namespace ui
}  // namespace other
