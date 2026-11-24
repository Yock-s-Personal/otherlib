/**
 * \file ui/script_window.cpp
 **/
#include "ui/script_window.hpp"

#include <chrono>
#include <format>

#include <imgui/imgui.h>

#include "core/logger.hpp"
#include "core/profiler.hpp"
#include "thread/thread_safety.hpp"

#include "theme/colors.hpp"
#include "ui/script_draw_guard.hpp"

namespace other {
  namespace ui {
    namespace {

      script_window::draw_fn draw = nullptr;

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

      /// the script catches what its window throws, so nothing unwinds through here
      bool drawn = false;
      const script_draw_errors errors = guard_script_draw([&]() {
        const auto started = std::chrono::steady_clock::now();
        drawn = draw(window_id);
        last_draw_us = std::chrono::duration<float, std::micro>(std::chrono::steady_clock::now() - started).count();
        ++draws;
      });

      if (drawn && errors.count == 0) {
        return;
      }

      disabled = true;
      if (errors.count == 0) {
        /// the script has logged what it threw
        reason = "its script threw. The log has the error.";
        return;
      }

      reason = errors.describe();
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
