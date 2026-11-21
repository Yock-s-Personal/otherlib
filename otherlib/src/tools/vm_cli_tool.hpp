/**
 * \file tools/vm_cli_tool.hpp
 * oecli vm <build|info>: links .oasm sources into one .obin and reads a .obin header
 **/
#ifndef OTHERLIB_TOOLS_VM_CLI_TOOL_HPP
#define OTHERLIB_TOOLS_VM_CLI_TOOL_HPP

#include "cli/tool.hpp"

namespace other {
  namespace cli {

    class vm_tool final : public tool {
     public:
      vm_tool() = default;
      ~vm_tool() override = default;

      std::string_view name() const override { return "vm"; }
      std::string_view summary() const override { return "Build and inspect VM programs (.oasm, .obin)"; }
      std::string_view usage() const override;

      tool_result execute(tool_context& ctx, std::span<const std::string> args) override;
    };

  }  // namespace cli
}  // namespace other

#endif  // OTHERLIB_TOOLS_VM_CLI_TOOL_HPP
