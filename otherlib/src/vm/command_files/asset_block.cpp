/**
 * \file vm/command_files/asset_block.cpp
 **/
#include "vm/command_files/asset_block.hpp"

namespace other {
  namespace {

    void write_text(std::string& out, const std::string_view text) {
      out += '"';
      for (const char c : text) {
        switch (c) {
          case '"': out += "\\\""; break;
          case '\\': out += "\\\\"; break;
          case '\n': out += "\\n"; break;
          case '\r': out += "\\r"; break;
          case '\t': out += "\\t"; break;
          default:
            /// bytes above ASCII are UTF-8 and go out as they are
            if (static_cast<unsigned char>(c) < 0x20 || c == 0x7f) {
              out += std::format("\\x{:02x}", static_cast<uint8_t>(c));
            } else {
              out += c;
            }
            break;
        }
      }
      out += '"';
    }

    void write_value(std::string& out, const asset_value& value) {
      switch (value.kind) {
        case ASSET_VALUE_INTEGER: out += std::format("{}", value.integer); break;
        case ASSET_VALUE_DECIMAL: {
          /// the shortest text that reads back as the same number
          const std::string number = std::format("{}", value.decimal);
          out += number;
          if (number.find_first_of(".en") == std::string::npos) {
            out += ".0";
          }
        } break;

        case ASSET_VALUE_BOOL: out += value.integer != 0 ? "true" : "false"; break;
        case ASSET_VALUE_TEXT: write_text(out, value.text); break;
        case ASSET_VALUE_WORD: out += value.text; break;
        case ASSET_VALUE_LIST:
          out += '[';
          for (size_t i = 0; i < value.items.size(); ++i) {
            out += i == 0 ? "" : ", ";
            write_value(out, value.items[i]);
          }
          out += ']';
          break;
        default: break;
      }
    }

    void write_body(std::string& out, const asset_node& node, const size_t depth) {
      const std::string indent(depth * 2, ' ');
      for (const asset_field& field : node.fields) {
        constexpr std::string_view kWords[] = { "set", "ref", "use" };
        out += std::format("{}{} {}, ", indent, kWords[field.how], field.name);
        write_value(out, field.value);
        out += '\n';
      }
      for (const asset_node& part : node.parts) {
        out += std::format("{}begin {}{}{}\n", indent, part.kind, part.name.empty() ? "" : ", ", part.name);
        write_body(out, part, depth + 1);
        out += std::format("{}end\n", indent);
      }
      for (const asset_edge& edge : node.edges) {
        out += std::format("{}edge {}, {}\n", indent, edge.from, edge.to);
      }
    }

  }  // namespace

  std::string asset_block_text(const asset_node& asset) {
    std::string text = std::format("%{} : {}\n", asset.name, asset.kind);
    write_body(text, asset, 1);
    text += "end\n";
    return text;
  }

}  // namespace other
