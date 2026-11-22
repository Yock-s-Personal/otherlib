/**
 * \file tests/vm/command_lowering_tests.cpp
 **/
#include <filesystem>
#include <fstream>

#include "vm/builtin_command_devices.hpp"
#include "vm/command_bus.hpp"
#include "vm/command_files/ocmd_debug_info.hpp"
#include "vm/command_files/ocmd_headers.hpp"
#include "vm/command_files/ocmd_toolchain.hpp"
#include "vm/command_language/command_lowering.hpp"
#include "vm/diagnostics/diagnostic_engine.hpp"
#include "vm/other_device.hpp"
#include "vm/vm.hpp"

#include "other_test.hpp"

namespace other {
  namespace {

    /// every run test ends in out(...): rf still holds its argument when the program stops
    constexpr std::string_view kOut = "fn out(value) { return value }\n";

  }  // namespace

  class command_lowering_tests : public other_test {
   protected:
    void SetUp() override {
      other_test::SetUp();
      const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
      sandbox = std::filesystem::temp_directory_path() / "other-command-tests" / info->name();
      std::filesystem::remove_all(sandbox);
      std::filesystem::create_directories(sandbox);
    }

    void TearDown() override {
      std::error_code ec;
      std::filesystem::remove_all(sandbox, ec);
      other_test::TearDown();
    }

    /// the OASM for text as the only file of a build; empty when it has errors
    static std::string lower(const std::string_view text, const command_lowering_options options = {}) {
      command_bus bus;
      register_builtin_command_devices(bus);
      diagnostic_engine diag;
      const opt<command_file> file = ocmd_toolchain{}.parse_command_source("test.ocmd", text, &diag);
      if (!file.has_value()) {
        return {};
      }
      return command_lowering{ bus, "test.ocmd", {}, options }.lower(*file, &diag).oasm;
    }

    static bool run_until_stopped(other_command_device& device) {
      for (size_t step = 0; step < 4096 && !vm::has_flag(&device, other_command_device::STOPPED); ++step) {
        vm::step(&device);
      }
      return vm::has_flag(&device, other_command_device::STOPPED);
    }

    /// builds text with kOut in front, runs it, and gives back the last value passed to out
    static opt<uint64_t> run(const std::string_view text) {
      command_bus bus;
      register_builtin_command_devices(bus);
      diagnostic_engine diag;
      ocmd_toolchain toolchain;

      opt<command_file> file = toolchain.parse_command_source("test.ocmd", std::format("{}{}", kOut, text), &diag);
      if (!file.has_value()) {
        return std::nullopt;
      }
      const command_unit unit = { .name = "test.ocmd", .file = std::move(*file) };
      const ostd::vector<oasm_source> sources = toolchain.lower_command_units(bus, std::span{ &unit, 1 }, &diag);
      if (sources.empty()) {
        return std::nullopt;
      }
      const obin_build_result built = toolchain.build_obin(bus, sources, &diag);
      if (!built.success) {
        return std::nullopt;
      }

      other_command_device device;
      vm::initialize_device(&device);
      vm::load_program_from_bytes(&device, built.bytes);
      const bool stopped = run_until_stopped(device);
      const uint64_t result = device.read_register_as_u64(vm_register_idx::VM_RF);
      vm::shutdown_device(&device);
      return stopped ? opt<uint64_t>{ result } : std::nullopt;
    }

    void write_file(const filepath& path, const std::string_view text) {
      std::filesystem::create_directories(path.parent_path());
      std::ofstream out(path, std::ios::binary | std::ios::trunc);
      ASSERT_TRUE(out.is_open()) << path.string();
      out << text;
    }

    filepath sandbox;
  };

  TEST_F(command_lowering_tests, a_command_with_a_text_fills_pointer_and_length) {
    const std::string oasm = lower("project load \"test-project\"\n");
    EXPECT_TRUE(oasm.contains("/entry: m_test"));
    EXPECT_TRUE(oasm.contains(".s_0 : string = \"test-project\""));
    EXPECT_TRUE(oasm.contains("set r0, d_test.s_0\n  set r1, 12\n  set rflag, 0\n  syscall project.load"));
  }

  TEST_F(command_lowering_tests, every_spelling_of_a_command_lowers_the_same) {
    const std::string words = lower("core log_u64 7 2\n");
    ASSERT_FALSE(words.empty());
    EXPECT_EQ(lower("core.log_u64 7 2\n"), words);
    EXPECT_EQ(lower("core:log_u64 7 2\n"), words);
    EXPECT_EQ(lower("core.log_u64(7, 2)\n"), words);
    EXPECT_EQ(lower("core log-u64 7 2\n"), words);
  }

  TEST_F(command_lowering_tests, generated_names_cannot_be_read_as_hex_or_keywords) {
    /// 'face', 'add' and 'data' are a number and two keywords to the assembler
    const opt<uint64_t> result = run("let face = 40\nfn add(data) { return data + 2 }\nout(add(face))\n");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 42);
  }

  TEST_F(command_lowering_tests, arithmetic_and_precedence) {
    EXPECT_EQ(run("out(1 + 2 * 3)\n"), 7);
    EXPECT_EQ(run("out((1 + 2) * 3)\n"), 9);
    EXPECT_EQ(run("out(17 / 5 + 17 % 5)\n"), 5);
    EXPECT_EQ(run("out(0 - 1 == -1)\n"), 1);
  }

  TEST_F(command_lowering_tests, numbers_of_every_size_load_whole) {
    /// 'set' holds 8 bits and 'add' 16, so the three sizes take three routes
    EXPECT_EQ(run("out(255)\n"), 255);
    EXPECT_EQ(run("out(300)\n"), 300);
    EXPECT_EQ(run("out(65535)\n"), 65535);
    EXPECT_EQ(run("out(100000)\n"), 100000);
    EXPECT_EQ(run("out(0x7FFFFFFFFFFFFFFF)\n"), 0x7FFFFFFFFFFFFFFF);
    EXPECT_TRUE(lower("core log_u64 0xFFFFFFFFFFFFFFFF 2\n").empty());
  }

  TEST_F(command_lowering_tests, comparisons_give_one_or_zero) {
    EXPECT_EQ(run("out(3 < 5)\n"), 1);
    EXPECT_EQ(run("out(3 > 5)\n"), 0);
    EXPECT_EQ(run("out(5 <= 5)\n"), 1);
    EXPECT_EQ(run("out(4 >= 5)\n"), 0);
    EXPECT_EQ(run("out(4 != 5)\n"), 1);
    EXPECT_EQ(run("out(!7)\n"), 0);
  }

  TEST_F(command_lowering_tests, logic_skips_the_side_it_does_not_need) {
    const std::string_view counted = "let calls = 0\nfn touch() {\n  calls = calls + 1\n  return 1\n}\n";
    EXPECT_EQ(run(std::format("{}let r = false && touch()\nout(calls)\n", counted)), 0);
    EXPECT_EQ(run(std::format("{}let r = true || touch()\nout(calls)\n", counted)), 0);
    EXPECT_EQ(run(std::format("{}let r = true && touch()\nout(calls + r)\n", counted)), 2);
    EXPECT_EQ(run("out(5 && 9)\n"), 1);
  }

  TEST_F(command_lowering_tests, if_else_and_nested_blocks) {
    EXPECT_EQ(run("let r = 0\nif (2 > 1) { r = 10 } else { r = 20 }\nout(r)\n"), 10);
    EXPECT_EQ(run("let r = 0\nif (2 < 1) { r = 10 }\nelse if (3 < 1) { r = 20 }\nelse { r = 30 }\nout(r)\n"), 30);
    /// the closing labels of both ifs land on the same instruction
    EXPECT_EQ(run("let r = 1\nif (true) {\n  if (false) { r = 2 }\n}\nout(r)\n"), 1);
  }

  TEST_F(command_lowering_tests, while_with_break_and_continue) {
    EXPECT_EQ(run("let i = 0\nlet sum = 0\nwhile (true) {\n  i = i + 1\n  if (i == 2) { continue }\n  if (i > 4) { break }\n  sum = sum + i\n}\nout(sum)\n"), 8);
  }

  TEST_F(command_lowering_tests, a_block_has_its_own_variables) {
    EXPECT_EQ(run("let x = 1\nif (true) {\n  let x = x + 10\n  x = x + 100\n}\nout(x)\n"), 1);
    EXPECT_TRUE(lower("if (true) { let y = 1 }\ncore log_u64 y 2\n").empty());
  }

  TEST_F(command_lowering_tests, calls_inside_arguments_keep_earlier_values) {
    const std::string_view functions = "fn double(n) { return n * 2 }\nfn add-three(a, b, c) { return a + b + c }\n";
    EXPECT_EQ(run(std::format("{}out(add-three(double(1), double(2), 3))\n", functions)), 9);
    EXPECT_EQ(run(std::format("{}out(double(5) + double(double(1)) * 2)\n", functions)), 18);
    EXPECT_EQ(run(std::format("{}let r = add-three 1 double(2) 3\nout(r)\n", functions)), 8);
    /// parentheses hold an expression, and a command with words is not one
    EXPECT_FALSE(run(std::format("{}let r = add-three 1 (double 2) 3\nout(r)\n", functions)).has_value());
  }

  TEST_F(command_lowering_tests, a_function_without_return_gives_null) {
    EXPECT_EQ(run("fn nothing() { }\nlet r = nothing()\nout(r == null)\n"), 1);
  }

  TEST_F(command_lowering_tests, command_status_drives_and_and_or) {
    const std::string oasm = lower("project load \"a\" && core log \"ok\" 2 || core log \"failed\" 4\n");
    ASSERT_FALSE(oasm.contains("\n@l_0:\n@l_1:"));
    EXPECT_TRUE(oasm.contains("syscall project.load\n  jne l_0\n"));
    EXPECT_TRUE(oasm.contains("@l_0:\n  je l_1\n"));
  }

  TEST_F(command_lowering_tests, a_text_in_a_variable_carries_its_length) {
    const std::string oasm = lower("fn say(text) { core log text 2 }\nsay \"hi\"\n");
    EXPECT_TRUE(oasm.contains(".s_0_hi : = 8589934592"));
    EXPECT_TRUE(oasm.contains("rshift r1, re\n  lshift r0, re\n  rshift r0, re"));
  }

  TEST_F(command_lowering_tests, a_text_the_assembler_cannot_quote_goes_out_as_bytes) {
    EXPECT_TRUE(lower("core log \"say 'hi'\" 2\n").contains(".s_0 : string = \"say 'hi'\""));
    EXPECT_TRUE(lower("core log \"say \\\"hi\\\"\" 2\n").contains(".s_0 : string = 'say \"hi\"'"));
    EXPECT_TRUE(lower("core log \"a\\nb\" 2\n").contains(".s_0 : blob = 0x610a62"));
    EXPECT_TRUE(lower("core log \"\" 2\n").contains(".s_0 : blob = 0x00"));
  }

  TEST_F(command_lowering_tests, file_level_literals_need_no_code) {
    const std::string oasm = lower("let limit = 100000\nlet name = \"ship\"\nlet ready = true\n", { .entry_unit = false });
    EXPECT_TRUE(oasm.contains(".v_limit : = 100000"));
    EXPECT_TRUE(oasm.contains(".v_ready : = 1"));
    EXPECT_TRUE(oasm.contains(".s_0 : string = \"ship\""));
    EXPECT_FALSE(oasm.contains("/entry"));
    EXPECT_FALSE(oasm.contains("$"));
  }

  TEST_F(command_lowering_tests, statements_outside_the_entry_file_are_dropped) {
    const std::string_view library = "let factor = 2\ncore log \"not run\" 2\nfn double(n) { return n * factor }\n";
    const std::string lenient = lower(library, { .entry_unit = false });
    EXPECT_TRUE(lenient.contains("$f_double:"));
    EXPECT_FALSE(lenient.contains("syscall core.log"));
    EXPECT_TRUE(lower(library, { .entry_unit = false, .strict = true }).empty());
  }

  TEST_F(command_lowering_tests, hooks_become_named_blocks) {
    command_bus bus;
    register_builtin_command_devices(bus);
    diagnostic_engine diag;
    const opt<command_file> file = ocmd_toolchain{}.parse_command_source("boot.ocmd", "on pre_load { core log \"pre\" 2 }\non \"asset.loaded\" { }\n", &diag);
    ASSERT_TRUE(file.has_value());

    const command_lowering_result result = command_lowering{ bus, "boot.ocmd", {} }.lower(*file, &diag);
    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.hooks.size(), 2);
    EXPECT_EQ(result.hooks[0].block, "h_boot_pre_load");
    EXPECT_FALSE(result.hooks[0].engine_event);
    EXPECT_EQ(result.hooks[1].block, "h_boot_asset_loaded");
    EXPECT_TRUE(result.hooks[1].engine_event);
    EXPECT_TRUE(result.oasm.contains("$h_boot_pre_load:"));

    EXPECT_TRUE(lower("on startup { }\n").empty());
    EXPECT_TRUE(lower("on pre_load { }\non pre_load { }\n").empty());
  }

  TEST_F(command_lowering_tests, wrong_names_and_wrong_calls_are_errors) {
    EXPECT_TRUE(lower("frobnicate 1 2\n").empty());
    EXPECT_TRUE(lower("core shout \"x\"\n").empty());
    EXPECT_TRUE(lower("core 5\n").empty());
    EXPECT_TRUE(lower("core log \"only one\"\n").empty());
    EXPECT_TRUE(lower("let x = core.log(\"a\", 2)\n").empty());
    EXPECT_TRUE(lower("let a = b + 1\n").empty());
    EXPECT_TRUE(lower("fn f(a) { return a }\nf(1, 2)\n").empty());
    EXPECT_TRUE(lower("let a = 1\nlet a = 2\n").empty());
    EXPECT_TRUE(lower("fn f() { }\nfn f() { }\n").empty());
    EXPECT_TRUE(lower("let name = \"a\"\nname = \"b\"\n").empty());
  }

  TEST_F(command_lowering_tests, what_the_first_version_cannot_do_is_an_error) {
    EXPECT_TRUE(lower("fn a(n) { return b(n) }\nfn b(n) { return a(n) }\n").empty());
    EXPECT_TRUE(lower("return 5\n").empty());
    EXPECT_TRUE(lower("break\n").empty());
    EXPECT_TRUE(lower("let x = 1 + (2 + (3 + (4 + (5 + (6 + (7 + 8))))))\n").empty());
    EXPECT_TRUE(lower("fn many(a, b, c, d, e, f, g, h, i) { }\n").empty());
  }

  TEST_F(command_lowering_tests, a_used_file_is_built_with_the_file_that_names_it) {
    write_file(sandbox / "main.ocmd", "use \"lib/math.ocmd\"\nfn out(value) { return value }\nout(double(21))\n");
    write_file(sandbox / "lib" / "math.ocmd", "use \"../main.ocmd\"\nlet factor = 2\nfn double(n) { return n * factor }\n");

    diagnostic_engine diag;
    ocmd_toolchain toolchain;
    const ostd::vector<command_unit> units = toolchain.read_command_units(sandbox / "main.ocmd", "main.ocmd", &diag);
    /// the two files use each other; each is read once
    ASSERT_EQ(units.size(), 2);
    EXPECT_EQ(units[0].name, "main.ocmd");
    EXPECT_EQ(units[1].name, "lib/math.ocmd");

    other_command_device device;
    vm::initialize_device(&device);
    vm::load_program_from_file(&device, sandbox / "main.ocmd");
    EXPECT_TRUE(run_until_stopped(device));
    EXPECT_EQ(device.read_register_as_u64(vm_register_idx::VM_RF), 42);
    vm::shutdown_device(&device);
  }

  TEST_F(command_lowering_tests, a_missing_or_broken_file_loads_nothing) {
    write_file(sandbox / "missing-use.ocmd", "use \"nowhere.ocmd\"\n");
    write_file(sandbox / "broken.ocmd", "let = 5\n");
    write_file(sandbox / "a" / "same.ocmd", "use \"../b/same.ocmd\"\n");
    write_file(sandbox / "b" / "same.ocmd", "fn f() { }\n");

    diagnostic_engine diag;
    ocmd_toolchain toolchain;
    EXPECT_TRUE(toolchain.read_command_units(sandbox / "missing-use.ocmd", "missing-use.ocmd", &diag).empty());
    EXPECT_TRUE(toolchain.read_command_units(sandbox / "broken.ocmd", "broken.ocmd", &diag).empty());

    /// two files with one name would define the same symbols
    command_bus bus;
    register_builtin_command_devices(bus);
    const ostd::vector<command_unit> same = toolchain.read_command_units(sandbox / "a" / "same.ocmd", "a/same.ocmd", &diag);
    ASSERT_EQ(same.size(), 2);
    EXPECT_TRUE(toolchain.lower_command_units(bus, same, &diag).empty());

    other_command_device device;
    vm::initialize_device(&device);
    vm::load_program_from_file(&device, sandbox / "broken.ocmd");
    EXPECT_TRUE(vm::has_flag(&device, other_command_device::STOPPED));
    EXPECT_EQ(device.program_load_cursor, other_command_device::kProgramStartAddress);
    vm::shutdown_device(&device);
  }

  TEST_F(command_lowering_tests, the_lines_of_a_script_reach_the_program) {
    /// line 5 has a call inside a command, so it has two places of its own: columns 1 and 14
    constexpr std::string_view kScript = "let limit = 100000\nfn twice(n) {\n  return n * 2\n}\ncore log_u64 twice(limit) 2\n";

    command_bus bus;
    register_builtin_command_devices(bus);
    diagnostic_engine diag;
    ocmd_toolchain toolchain;
    opt<command_file> file = toolchain.parse_command_source("test.ocmd", kScript, &diag);
    ASSERT_TRUE(file.has_value());

    /// the lowering says where each line of the assembly it writes came from
    const command_lowering_result lowered = command_lowering{ bus, "test.ocmd", {} }.lower(*file, &diag);
    ASSERT_TRUE(lowered.success);
    EXPECT_EQ(lowered.origins.size(), static_cast<size_t>(std::ranges::count(lowered.oasm, '\n')));
    EXPECT_EQ(lowered.origins.front().line, 0);

    const command_unit unit = { .name = "test.ocmd", .file = std::move(*file) };
    const ostd::vector<oasm_source> sources = toolchain.lower_command_units(bus, std::span{ &unit, 1 }, &diag);
    ASSERT_EQ(sources.size(), 1);
    const obin_build_result built = toolchain.build_obin(bus, sources, &diag);
    ASSERT_TRUE(built.success);

    const opt<obin_debug_info> debug = obin_debug_info::decode(obin_debug_bytes(built.bytes));
    ASSERT_TRUE(debug.has_value());
    ASSERT_FALSE(debug->lines.empty());

    /// what the program records is the script's lines, not those of the assembly in between
    const auto has = [&debug](const uint32_t line, const uint32_t column) -> bool {
      return std::ranges::any_of(debug->lines, [&](const debug_line_entry& entry) { return entry.line == line && entry.column == column; });
    };
    EXPECT_TRUE(has(2, 1));
    EXPECT_TRUE(has(3, 3));
    EXPECT_TRUE(has(5, 14));
    EXPECT_TRUE(has(5, 1));
    for (const debug_line_entry& entry : debug->lines) {
      EXPECT_LE(entry.line, 5);
      if (entry.line != 0) {
        EXPECT_EQ(debug->symbols[entry.unit].name, "test.ocmd");
      }
    }

    ocmd_file_header header;
    std::memcpy(&header, built.bytes.data(), sizeof(header));
    const debug_line_entry* entry_line = debug->line_at(header.prog_header.entry_point_address);
    ASSERT_NE(entry_line, nullptr);
    EXPECT_EQ(entry_line->line, 5);
  }

}  // namespace other
