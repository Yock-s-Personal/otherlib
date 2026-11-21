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
#include "vm/command_files/ocmd_debug_info.hpp"
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

  TEST_F(vm_tool_tests, build_takes_a_script_and_the_files_it_uses) {
    write_file(test_root / "boot.ocmd", "use \"lib.ocmd\"\nproject load folder()\n");
    write_file(test_root / "lib.ocmd", "fn folder() { return \"test-project\" }\n");

    const cli::tool_result result = run_vm_tool("vm build boot.ocmd -o boot.obin");
    ASSERT_TRUE(result.success()) << result.message << errors;
    EXPECT_NE(result.message.find("linked 2 source(s)"), std::string::npos) << result.message;
    EXPECT_FALSE(ocmd_toolchain{}.read_obin(test_root / "boot.obin").empty());
    /// the OASM is an in-between form and is only written when asked for
    EXPECT_FALSE(std::filesystem::exists(test_root / "boot.ocmd.oasm"));
  }

  TEST_F(vm_tool_tests, emit_oasm_writes_text_that_builds_the_same_program) {
    write_file(test_root / "boot.ocmd", "use \"lib.ocmd\"\nproject load folder()\n");
    write_file(test_root / "lib.ocmd", "fn folder() { return \"test-project\" }\n");
    ASSERT_TRUE(run_vm_tool("vm build boot.ocmd -o boot.obin --emit-oasm").success());
    ASSERT_TRUE(std::filesystem::exists(test_root / "boot.ocmd.oasm"));
    ASSERT_TRUE(std::filesystem::exists(test_root / "lib.ocmd.oasm"));

    /// the listing sorts next to the script it came from, so the link order is the same
    ASSERT_TRUE(run_vm_tool("vm build boot.ocmd.oasm lib.ocmd.oasm -o again.obin").success());
    /// the programs are the same; the debug sections differ, because each names its own source files
    const ostd::vector<uint8_t> first = ocmd_toolchain{}.read_obin(test_root / "boot.obin");
    const ostd::vector<uint8_t> again = ocmd_toolchain{}.read_obin(test_root / "again.obin");
    ASSERT_FALSE(first.empty());
    EXPECT_TRUE(std::ranges::equal(obin_program_bytes(first), obin_program_bytes(again)));
    EXPECT_NE(first, again);
  }

  TEST_F(vm_tool_tests, a_script_links_with_hand_written_oasm) {
    write_file(test_root / "boot.ocmd", "let seven = helper()\n");
    write_file(test_root / "helper.oasm", "$f_helper:\n  set rf, 7\n  ret\nend\n");
    /// the script does not know helper exists, so it cannot call it yet
    EXPECT_FALSE(run_vm_tool("vm build boot.ocmd helper.oasm -o boot.obin").success());

    write_file(test_root / "boot.ocmd", "core log_u64 1 2\n");
    EXPECT_TRUE(run_vm_tool("vm build boot.ocmd helper.oasm -o boot.obin").success());
  }

  TEST_F(vm_tool_tests, a_script_with_errors_writes_nothing) {
    write_file(test_root / "boot.ocmd", "project load\n");
    EXPECT_FALSE(run_vm_tool("vm build boot.ocmd -o boot.obin --emit-oasm").success());
    EXPECT_FALSE(std::filesystem::exists(test_root / "boot.obin"));
    EXPECT_FALSE(std::filesystem::exists(test_root / "boot.ocmd.oasm"));

    write_file(test_root / "boot.ocmd", "use \"lib.ocmd\"\n");
    write_file(test_root / "lib.ocmd", "core log \"library statement\" 2\n");
    EXPECT_TRUE(run_vm_tool("vm build boot.ocmd -o lenient.obin").success());
    EXPECT_FALSE(run_vm_tool("vm build boot.ocmd -o strict.obin --strict").success());
    EXPECT_FALSE(std::filesystem::exists(test_root / "strict.obin"));
  }

  TEST_F(vm_tool_tests, view_prints_a_program_in_the_assemblers_words) {
    write_file(test_root / "main.oasm", kMain);
    write_file(test_root / "helper.oasm", kHelper);
    ASSERT_TRUE(run_vm_tool("vm build main.oasm helper.oasm -o boot.obin").success());
    output.clear();

    const cli::tool_result result = run_vm_tool("vm view boot.obin");
    ASSERT_TRUE(result.success()) << result.message << errors;
    EXPECT_NE(output.find("entry:"), std::string::npos) << output;
    EXPECT_NE(output.find("set r1, 7"), std::string::npos) << output;
    /// the names come from the debug section a build writes unless told not to
    EXPECT_NE(output.find("$helper:"), std::string::npos) << output;
    EXPECT_NE(output.find("call helper"), std::string::npos) << output;
    EXPECT_NE(output.find("helper.oasm"), std::string::npos) << output;

    /// asked to ignore it, view names a block by its address, as it does for a file without one
    output.clear();
    ASSERT_TRUE(run_vm_tool("vm view boot.obin --no-debug").success());
    EXPECT_NE(output.find("call f_"), std::string::npos) << output;
    EXPECT_EQ(output.find("$helper:"), std::string::npos) << output;

    /// a program is known by what is in it, not by its name
    ASSERT_TRUE(run_vm_tool("vm build main.oasm helper.oasm -o boot").success());
    output.clear();
    ASSERT_TRUE(run_vm_tool("vm view boot").success());
    EXPECT_NE(output.find("set r1, 7"), std::string::npos) << output;
  }

  TEST_F(vm_tool_tests, no_debug_leaves_the_section_out_of_the_file) {
    write_file(test_root / "main.oasm", kMain);
    write_file(test_root / "helper.oasm", kHelper);
    ASSERT_TRUE(run_vm_tool("vm build main.oasm helper.oasm -o with.obin").success());
    ASSERT_TRUE(run_vm_tool("vm build main.oasm helper.oasm -o without.obin --no-debug").success());

    const ostd::vector<uint8_t> with = ocmd_toolchain{}.read_obin(test_root / "with.obin");
    const ostd::vector<uint8_t> without = ocmd_toolchain{}.read_obin(test_root / "without.obin");
    ASSERT_FALSE(with.empty());
    ASSERT_FALSE(without.empty());
    EXPECT_GT(with.size(), without.size());
    EXPECT_TRUE(obin_debug_bytes(without).empty());
    EXPECT_FALSE(obin_debug_bytes(with).empty());
    /// what the device runs is the same either way
    EXPECT_TRUE(std::ranges::equal(obin_program_bytes(with), obin_program_bytes(without)));

    ASSERT_TRUE(run_vm_tool("vm info with.obin").success());
    EXPECT_NE(output.find("not loaded into the device"), std::string::npos) << output;
    output.clear();
    ASSERT_TRUE(run_vm_tool("vm info without.obin").success());
    EXPECT_NE(output.find("debug section: none"), std::string::npos) << output;

    output.clear();
    ASSERT_TRUE(run_vm_tool("vm view without.obin").success());
    EXPECT_NE(output.find("no debug section"), std::string::npos) << output;
  }

  TEST_F(vm_tool_tests, view_builds_sources_without_writing_anything) {
    write_file(test_root / "boot.ocmd", "let limit = 100000\nproject load \"test-project\"\ncore log_u64 limit 2\n");

    const cli::tool_result result = run_vm_tool("vm view boot.ocmd");
    ASSERT_TRUE(result.success()) << result.message << errors;
    EXPECT_NE(output.find("syscall project.load"), std::string::npos) << output;
    EXPECT_NE(output.find("; \"test-project\""), std::string::npos) << output;
    EXPECT_NE(output.find("; starts as 100000"), std::string::npos) << output;

    /// the script is still the only file in the folder
    EXPECT_EQ(std::distance(std::filesystem::directory_iterator{ test_root }, std::filesystem::directory_iterator{}), 1);
  }

  TEST_F(vm_tool_tests, view_puts_each_script_line_above_its_instructions) {
    write_file(test_root / "boot.ocmd", "let limit = 100000\nproject load \"test-project\"\n  core log_u64 limit 2\n");

    ASSERT_TRUE(run_vm_tool("vm view boot.ocmd").success()) << errors;
    EXPECT_NE(output.find("boot.ocmd:2:1"), std::string::npos) << output;
    EXPECT_NE(output.find("project load \"test-project\""), std::string::npos) << output;
    /// the column is where the statement starts, and the text is shown without its indent
    EXPECT_NE(output.find("boot.ocmd:3:3"), std::string::npos) << output;
    EXPECT_NE(output.find(" core log_u64 limit 2"), std::string::npos) << output;

    /// the program carries the line numbers; the text is read from the script when it is there
    ASSERT_TRUE(run_vm_tool("vm build boot.ocmd -o boot.obin").success());
    std::filesystem::remove(test_root / "boot.ocmd");
    output.clear();
    ASSERT_TRUE(run_vm_tool("vm view boot.obin").success());
    EXPECT_NE(output.find("boot.ocmd:2:1"), std::string::npos) << output;
    EXPECT_EQ(output.find("project load \"test-project\""), std::string::npos) << output;

    /// hand-written assembly is what the listing already shows, so its lines are not repeated
    write_file(test_root / "main.oasm", kMain);
    write_file(test_root / "helper.oasm", kHelper);
    output.clear();
    ASSERT_TRUE(run_vm_tool("vm view main.oasm helper.oasm").success());
    EXPECT_EQ(output.find("; main.oasm:3:3"), std::string::npos) << output;
    /// a block says which line of its file it starts on instead
    EXPECT_NE(output.find("$main:    ; main.oasm:3"), std::string::npos) << output;
    EXPECT_NE(output.find("$helper:    ; helper.oasm:2"), std::string::npos) << output;
  }

  TEST_F(vm_tool_tests, data_lists_each_field_with_its_type_and_its_uses) {
    write_file(test_root / "boot.ocmd", "let limit = 100000\nproject load \"test-project\"\ncore log_u64 limit 2\n");

    ASSERT_TRUE(run_vm_tool("vm data boot.ocmd").success()) << errors;
    for (const std::string_view expected : { ".v_limit", "int64", "100000", "string", "\"test-project\"", "read ", "address ", "$m_boot", "boot.ocmd:3:1", "core log_u64 limit 2" }) {
      EXPECT_NE(output.find(expected), std::string::npos) << expected << "\n" << output;
    }
    /// only the data: no instruction is listed
    EXPECT_EQ(output.find("syscall"), std::string::npos) << output;

    /// a program file works as well as its sources, with or without a debug section
    ASSERT_TRUE(run_vm_tool("vm build boot.ocmd -o boot.obin --no-debug").success());
    output.clear();
    ASSERT_TRUE(run_vm_tool("vm data boot.obin").success());
    EXPECT_NE(output.find("no debug section"), std::string::npos) << output;
    EXPECT_NE(output.find("\"test-project\""), std::string::npos) << output;

    EXPECT_FALSE(run_vm_tool("vm data boot.ocmd -o out.obin").success());
    EXPECT_FALSE(run_vm_tool("vm data missing.obin").success());
  }

  TEST_F(vm_tool_tests, view_refuses_what_it_cannot_show) {
    write_file(test_root / "main.oasm", kMain);
    write_file(test_root / "not-a-program.obin", "this is only text, though it is longer than a program's header");

    EXPECT_FALSE(run_vm_tool("vm view not-a-program.obin").success());
    EXPECT_FALSE(run_vm_tool("vm view missing.obin").success());
    EXPECT_FALSE(run_vm_tool("vm view main.oasm -o out.obin").success());
    EXPECT_FALSE(run_vm_tool("vm view main.oasm not-a-program.obin").success());
    /// main calls a helper that is not among the sources
    EXPECT_FALSE(run_vm_tool("vm view main.oasm").success());
  }

}  // namespace other
