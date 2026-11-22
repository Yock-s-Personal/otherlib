/**
 * \file ui/console/console_log_pane.cpp
 **/
#include "ui/console/console_log_pane.hpp"

#include "theme/colors.hpp"
#include "ui/unicode.hpp"

namespace other {
  namespace ui {
    namespace console_w {
      namespace {

        /// unrounded width: ImGui::CalcTextSize rounds every call up to a whole pixel, which
        ///  drifts when many small runs are measured and placed one after another
        float measure_text(std::string_view text) {
          return ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, 0.f, text.data(), text.data() + text.size()).x;
        }

        void ensure_layout(log_entry& entry, float wrap_width, float font_size) {
          if (!entry.view.tokenized) {
            entry.view.spans = tokenize_log_message(entry.message);
            entry.view.tokenized = true;
          }
          if (!entry.view.layout.valid_for(wrap_width, font_size)) {
            build_layout(entry.message, entry.view.spans, wrap_width, font_size, measure_text, entry.view.layout);
          }
        }

        ImU32 color_for_span(span_kind kind, const glm::vec4& message_color) {
          using namespace colors::console;
          switch (kind) {
            case span_kind::TYPE: return colors::to_im_col(kResultType);
            case span_kind::VALUE: return colors::to_im_col(kResultValue);
            default: return colors::to_im_col(message_color);
          }
        }

        bool action_available(const log_action& action, const log_entry& entry) {
          return action.invoke != nullptr && (action.is_available == nullptr || action.is_available(entry));
        }

      }  // namespace

      log_pane_result draw_log_pane(log_entry& entry, bool alt_row, bool selected) {
        using namespace colors::console;
        PROFILE_SECTION("console_w::draw_log_pane");

        log_pane_result result{};

        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 cursor = ImGui::GetCursorScreenPos();
        const float avail_w = ImGui::GetContentRegionAvail().x;
        const float line_h = ImGui::GetTextLineHeight();
        const float pad_y = std::max(0.f, (kLogLineHeight - line_h) * 0.5f);

        /// prefix columns: timestamp, prompt symbol (commands), source tag. the message starts
        ///  after them and wrapped lines hang-indent to the same x
        const float timestamp_x = kPaddingX;
        float message_x = timestamp_x;
        if (!entry.timestamp.empty()) {
          message_x += measure_text(entry.timestamp) + kPaddingX;
        }

        const float prompt_x = message_x;
        if (entry.level == log_level::COMMAND) {
          message_x += measure_text(unicode::kPromptSymbol) + kPaddingX;
        }

        const float source_x = message_x;
        std::string source_text = {};
        if (!entry.source.empty()) {
          source_text = std::format("[{}]", entry.source);
          message_x += measure_text(source_text) + kPaddingX;
        }

        const float wrap_w = std::max(avail_w - message_x - kPaddingX, kMinWrapWidth);
        ensure_layout(entry, wrap_w, ImGui::GetFontSize());
        const log_layout& layout = entry.view.layout;

        const uint32_t total_lines = static_cast<uint32_t>(layout.lines.size());
        const bool truncated = total_lines > kMaxCollapsedLines;
        const uint32_t shown_lines = truncated ? kMaxCollapsedLines : total_lines;
        const uint32_t row_lines = shown_lines + (truncated ? 1 : 0);
        const float row_h = std::max(kLogLineHeight, static_cast<float>(row_lines) * line_h + pad_y * 2.f);

        const ImVec2 row_min = cursor;
        const ImVec2 row_max = { cursor.x + avail_w, cursor.y + row_h };

        // rows outside the scroll view only reserve their space
        if (!ImGui::IsRectVisible(row_min, row_max)) {
          ImGui::Dummy({ avail_w, row_h });
          ImGui::SetCursorScreenPos({ cursor.x, row_max.y });
          return result;
        }

        if (selected) {
          dl->AddRectFilled(row_min, row_max, colors::to_im_col(kAutocompleteSelected));
          dl->AddRectFilled(row_min, { row_min.x + 2.f, row_max.y }, colors::to_im_col(kPromptSymbol));
        } else if (alt_row) {
          dl->AddRectFilled(row_min, row_max, colors::to_im_col(kBGAlt));
        }

        /// invisible button for hover / interaction
        ImGui::PushID(reinterpret_cast<const void*>(static_cast<uintptr_t>(entry.id)));
        ImGui::InvisibleButton("##log-pane", { avail_w, row_h }, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
        result.hovered = ImGui::IsItemHovered();
        result.clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
        result.context_requested = ImGui::IsItemClicked(ImGuiMouseButton_Right);
        ImGui::PopID();

        if (result.hovered) {
          dl->AddRectFilled(row_min, row_max, IM_COL32(255, 255, 255, 8));
        }

        const float text_y = cursor.y + pad_y;

        if (!entry.timestamp.empty()) {
          dl->AddText({ cursor.x + timestamp_x, text_y }, colors::to_im_col(kTimestamp), entry.timestamp.c_str());
        }
        if (entry.level == log_level::COMMAND) {
          dl->AddText({ cursor.x + prompt_x, text_y }, colors::to_im_col(kPromptSymbol), unicode::kPromptSymbol);
        }
        if (!source_text.empty()) {
          dl->AddText({ cursor.x + source_x, text_y }, colors::to_im_col(kSource), source_text.c_str());
        }

        /// message
        const glm::vec4 message_color = color_for_level(entry.level);
        const char* text = entry.message.data();
        for (uint32_t line_idx = 0; line_idx < shown_lines; ++line_idx) {
          const layout_line& line = layout.lines[line_idx];
          const float y = text_y + static_cast<float>(line_idx) * line_h;
          for (uint32_t run_idx = line.first_run; run_idx < line.first_run + line.run_count; ++run_idx) {
            const layout_run& run = layout.runs[run_idx];
            dl->AddText({ cursor.x + message_x + run.x, y }, color_for_span(run.span.kind, message_color), text + run.span.offset, text + run.span.offset + run.span.length);
          }
        }

        if (truncated) {
          const std::string hint = std::format("... +{} more lines (click to view)", total_lines - shown_lines);
          const float y = text_y + static_cast<float>(shown_lines) * line_h;
          dl->AddText({ cursor.x + message_x, y }, colors::to_im_col(kHint), hint.c_str());
        }

        /// advance cursor past the row
        ImGui::SetCursorScreenPos({ cursor.x, row_max.y });

        return result;
      }

      detail_pane_result draw_detail_pane(const log_entry& entry, std::span<const log_action> actions) {
        using namespace colors::console;
        PROFILE_SECTION("console_w::draw_log_detail_pane");

        detail_pane_result result{};

        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 cursor = ImGui::GetCursorScreenPos();
        const float avail_w = ImGui::GetContentRegionAvail().x;
        const ImGuiStyle& style = ImGui::GetStyle();

        /// header: level, timestamp, source on the left; actions on the right
        const float header_h = ImGui::GetFrameHeight() + 4.f;
        dl->AddRectFilled(cursor, { cursor.x + avail_w, cursor.y + header_h }, colors::to_im_col(kPromptBG));
        dl->AddLine(cursor, { cursor.x + avail_w, cursor.y }, colors::to_im_col(kBorder), 1.f);

        ImGui::SetCursorScreenPos({ cursor.x + kPaddingX, cursor.y + 2.f });
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(colors::rgba_to_imvec4(color_for_level(entry.level)), "%s", prefix_for_level(entry.level));
        if (!entry.timestamp.empty()) {
          ImGui::SameLine();
          ImGui::TextColored(colors::rgba_to_imvec4(kTimestamp), "%s", entry.timestamp.c_str());
        }
        if (!entry.source.empty()) {
          ImGui::SameLine();
          ImGui::TextColored(colors::rgba_to_imvec4(kSource), "[%s]", entry.source.c_str());
        }

        const char* close_label = "Close";
        auto button_width = [&style](const char* label) {
          return ImGui::CalcTextSize(label).x + style.FramePadding.x * 2.f + style.ItemSpacing.x;
        };

        float buttons_w = button_width(close_label);
        for (const log_action& action : actions) {
          if (action_available(action, entry)) {
            buttons_w += button_width(action.label.c_str());
          }
        }

        ImGui::SameLine();
        const float buttons_x = cursor.x + avail_w - kPaddingX - buttons_w;
        ImGui::SetCursorScreenPos({ std::max(ImGui::GetCursorScreenPos().x, buttons_x), cursor.y + 2.f });

        for (size_t i = 0; i < actions.size(); ++i) {
          const log_action& action = actions[i];
          if (!action_available(action, entry)) {
            continue;
          }

          ImGui::PushID(static_cast<int>(i));
          if (ImGui::Button(action.label.c_str())) {
            action.invoke(entry);
          }
          ImGui::PopID();
          ImGui::SameLine();
        }
        if (ImGui::Button(close_label)) {
          result.close_requested = true;
        }

        /// body: the whole message, wrapped, in its own scroll area
        ImGui::SetCursorScreenPos({ cursor.x, cursor.y + header_h });
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(kPaddingX, kPaddingY));
        if (ImGui::BeginChild("##log-detail-body", ImVec2(0.f, 0.f), ImGuiChildFlags_AlwaysUseWindowPadding)) {
          ImGui::PushStyleColor(ImGuiCol_Text, colors::rgba_to_imvec4(color_for_level(entry.level)));
          ImGui::PushTextWrapPos(0.f);
          ImGui::TextUnformatted(entry.message.data(), entry.message.data() + entry.message.size());
          ImGui::PopTextWrapPos();
          ImGui::PopStyleColor();
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();

        return result;
      }

      bool draw_log_action_menu(const log_entry& entry, std::span<const log_action> actions) {
        bool invoked = false;
        for (size_t i = 0; i < actions.size(); ++i) {
          const log_action& action = actions[i];
          if (!action_available(action, entry)) {
            continue;
          }

          ImGui::PushID(static_cast<int>(i));
          if (ImGui::MenuItem(action.label.c_str())) {
            action.invoke(entry);
            invoked = true;
          }
          ImGui::PopID();
        }
        return invoked;
      }

    }  // namespace console_w
  }  // namespace ui
}  // namespace other