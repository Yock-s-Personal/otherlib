/**
 * \file tests/cli/vm_tool_tests.cpp
 *
 * the oecli vm tool: build/info over hermetic temp files, in-process through the same
 * registry the executable and in-environment hosts use.
 **/
#include <gtest/gtest.h>

#include <fstream>

#include "other_test.hpp"

#include "cli/tool_registry.hpp"
#include "tools/scene_cli_tool.hpp"
#include "vm/command_files/ocmd_toolchain.hpp"

namespace other {

  class vm_tool_tests : public other_test {
   protected:
    void SetUp() override {
      other_test::SetUp();
      test_root = std::filesystem::temp_directory_path() / "other-vm-tool-tests" / ::testing::UnitTest::GetInstance()->current_test_info()->name();
      std::filesystem::remove_all(test_root);
      std::filesystem::create_directories(test_root);

      ctx = {};
      ctx.working_directory = test_root;
      ctx.out = [this](std::string_view message) { output += std::format("{}\n", message); };
      ctx.err = [this](std::string_view message) { errors += std::format("{}\n", message); };

      cli::register_environment_tools(registry);
    }

    void TearDown() override {
      std::error_code ec;
      std::filesystem::remove_all(test_root, ec);
      other_test::TearDown();
    }

    void write_file(const filepath& path, std::string_view contents) {
      std::ofstream out(path, std::ios::binary | std::ios::trunc);
      ASSERT_TRUE(out.is_open()) << path.string();
      out.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    }

    cli::tool_result run_vm_tool(std::string_view line) {
      return registry.execute_line(ctx, line);
    }

    constexpr static std::string_view kMain = "/entry: main\n$main:\n  call helper\nend\n";
    constexpr static std::string_view kHelper = "$helper:\n  set r1, 7\n  ret\nend\n";
    /// the helper again, behind an instruction that sits outside any block
    constexpr static std::string_view kHelperWithLooseCode = "set r5, 9\n$helper:\n  set r1, 7\n  ret\nend\n";

    cli::tool_registry registry;
    cli::tool_context ctx;
    filepath test_root;
    std::string output;
    std::string errors;
  };

  TEST_F(vm_tool_tests, build_links_sources_into_a_loadable_program) {
    write_file(test_root / "main.oasm", kMain);
    write_file(test_root / "helper.oasm", kHelper);

    const cli::tool_result result = run_vm_tool("vm build main.oasm helper.oasm -o out/boot.obin");
    EXPECT_TRUE(result.success()) << result.message;

    const filepath built = test_root / "out" / "boot.obin";
    ASSERT_TRUE(std::filesystem::exists(built));
    EXPECT_FALSE(ocmd_toolchain{}.read_obin(built).empty());
  }

  TEST_F(vm_tool_tests, build_gives_the_same_file_for_any_input_order) {
    write_file(test_root / "main.oasm", kMain);
    write_file(test_root / "helper.oasm", kHelper);

    ASSERT_TRUE(run_vm_tool("vm build main.oasm helper.oasm -o forward.obin").success());
    ASSERT_TRUE(run_vm_tool("vm build helper.oasm main.oasm -o backward.obin").success());
    EXPECT_EQ(ocmd_toolchain{}.read_obin(test_root / "forward.obin"), ocmd_toolchain{}.read_obin(test_root / "backward.obin"));
  }

  TEST_F(vm_tool_tests, failed_build_writes_nothing) {
    write_file(test_root / "a.oasm", kHelper);
    write_file(test_root / "b.oasm", kHelper);

    /// two files define the same block and neither sets an entry
    EXPECT_FALSE(run_vm_tool("vm build a.oasm b.oasm -o boot.obin").success());
    EXPECT_FALSE(std::filesystem::exists(test_root / "boot.obin"));
  }

  TEST_F(vm_tool_tests, strict_turns_a_link_warning_into_a_failed_build) {
    write_file(test_root / "main.oasm", kMain);
    write_file(test_root / "helper.oasm", kHelperWithLooseCode);

    EXPECT_TRUE(run_vm_tool("vm build main.oasm helper.oasm -o lenient.obin").success());
    EXPECT_TRUE(std::filesystem::exists(test_root / "lenient.obin"));

    EXPECT_FALSE(run_vm_tool("vm build main.oasm helper.oasm -o strict.obin --strict").success());
    EXPECT_FALSE(std::filesystem::exists(test_root / "strict.obin"));
  }

  TEST_F(vm_tool_tests, info_prints_the_header_and_says_whether_it_loads) {
    write_file(test_root / "main.oasm", kMain);
    write_file(test_root / "helper.oasm", kHelper);
    ASSERT_TRUE(run_vm_tool("vm build main.oasm helper.oasm -o boot.obin").success());

    const cli::tool_result good = run_vm_tool("vm info boot.obin");
    EXPECT_TRUE(good.success()) << good.message;
    EXPECT_NE(output.find("format version"), std::string::npos) << output;
    EXPECT_NE(output.find("entry point"), std::string::npos) << output;

    ostd::vector<uint8_t> bytes = ocmd_toolchain{}.read_obin(test_root / "boot.obin");
    ASSERT_FALSE(bytes.empty());
    bytes[0] = 'X';
    write_file(test_root / "bad.obin", std::string_view{ reinterpret_cast<const char*>(bytes.data()), bytes.size() });
    EXPECT_FALSE(run_vm_tool("vm info bad.obin").success());
  }

  TEST_F(vm_tool_tests, bad_arguments_are_reported_not_asserted) {
    write_file(test_root / "main.oasm", kMain);

    EXPECT_FALSE(run_vm_tool("vm").success());
    EXPECT_FALSE(run_vm_tool("vm build main.oasm").success());
    EXPECT_FALSE(run_vm_tool("vm build missing.oasm -o boot.obin").success());
    EXPECT_FALSE(run_vm_tool("vm build main.txt -o boot.obin").success());
    EXPECT_FALSE(run_vm_tool("vm info missing.obin").success());
    EXPECT_FALSE(run_vm_tool("vm frobnicate main.oasm").success());
  }

}  // namespace other
