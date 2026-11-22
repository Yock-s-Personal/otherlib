/**
 * \file ui/script_window.cpp
 **/
#include "ui/script_window.hpp"

#include <chrono>
#include <cstring>
#include <format>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include "core/logger.hpp"
#include "core/profiler.hpp"
#include "thread/thread_safety.hpp"

#include "theme/colors.hpp"

namespace other {
  namespace ui {
    namespace {

      script_window::draw_fn draw = nullptr;

      /// what ImGui objected to while a script drew: the first objection, and how many
      struct draw_errors {
        std::string first;
        uint32_t count = 0;
      };

      void note_imgui_error(ImGuiContext*, void* user_data, const char* message) {
        auto* errors = static_cast<draw_errors*>(user_data);
        if (errors->count++ == 0) {
          errors->first = message;
        }
      }

    }  // namespace

    script_window::script_window(event_system* events, const std::string_view name, const std::string_view title, natural_t window_id, glm::vec2 default_size, int32_t flags)
        : ui_window(events, title, false, flags), name(name), window_id(window_id), default_size(default_size) {
    }

    void script_window::set_draw_function(draw_fn fn) {
      draw = fn;
    }

    void script_window::enable() {
      disabled = false;
      reason.clear();
    }

    void script_window::on_prepare_render() {
      /// first use only: a size saved in the layout wins after that
      if (default_size.x > 0.f && default_size.y > 0.f) {
        ImGui::SetNextWindowSize(ImVec2(default_size.x, default_size.y), ImGuiCond_FirstUseEver);
      }
    }

    void script_window::on_render_body() {
      ASSERT_MAIN_THREAD();
      PROFILE_SECTION("script_window::on_render_body");
      if (disabled) {
        render_disabled_notice();
        return;
      }
      if (draw == nullptr) {
        return;
      }

      /// a script's mistake with ImGui must not stop the editor, or be said every frame.
      ///  while the script draws, ImGui hands what it objects to here: no assert, and
      ///  nothing in its own log or tooltip
      ImGuiContext& context = *ImGui::GetCurrentContext();
      ImGuiIO& io = context.IO;
      const bool asserts = io.ConfigErrorRecoveryEnableAssert;
      const bool debug_log = io.ConfigErrorRecoveryEnableDebugLog;
      const bool tooltip = io.ConfigErrorRecoveryEnableTooltip;
      const ImGuiErrorCallback callback = context.ErrorCallback;
      void* const callback_data = context.ErrorCallbackUserData;

      draw_errors errors;
      io.ConfigErrorRecoveryEnableAssert = false;
      io.ConfigErrorRecoveryEnableDebugLog = false;
      io.ConfigErrorRecoveryEnableTooltip = false;
      context.ErrorCallback = note_imgui_error;
      context.ErrorCallbackUserData = &errors;

      ImGuiErrorRecoveryState before{};
      ImGui::ErrorRecoveryStoreState(&before);

      /// the script catches what its window throws, so nothing unwinds through here
      const auto started = std::chrono::steady_clock::now();
      const bool drawn = draw(window_id);
      last_draw_us = std::chrono::duration<float, std::micro>(std::chrono::steady_clock::now() - started).count();
      ++draws;

      /// what it left open (a tree node, a pushed id) is closed here, each as an objection
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

      if (drawn && errors.count == 0) {
        return;
      }

      disabled = true;
      if (errors.count == 0) {
        /// the script has logged what it threw
        reason = "its script threw. The log has the error.";
        return;
      }

      reason = errors.count == 1 ? std::format("ImGui: {}", errors.first) : std::format("ImGui: {} (and {} more)", errors.first, errors.count - 1);
      CORE_LOG_ERROR("The window '{}' is stopped until it is asked to draw again. {}", name, reason);
    }

    void script_window::render_disabled_notice() {
      ImGui::PushStyleColor(ImGuiCol_Text, colors::rgba_to_imvec4(colors::kFriendlyErrorRed));
      ImGui::TextWrapped("This window was stopped after an error while it was drawn: %s", reason.c_str());
      ImGui::PopStyleColor();
      if (ImGui::Button("Draw it again")) {
        enable();
      }
    }

  }  // namespace ui
}  // namespace other
