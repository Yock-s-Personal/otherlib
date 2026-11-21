/**
 * \file tests/vm/obin_tests.cpp
 **/
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>

#include "core/version.hpp"

#include "vm/builtin_command_devices.hpp"
#include "vm/command_bus.hpp"
#include "vm/command_files/ocmd_debug_info.hpp"
#include "vm/command_files/ocmd_headers.hpp"
#include "vm/command_files/ocmd_linker.hpp"
#include "vm/command_files/ocmd_toolchain.hpp"
#include "vm/decompiler.hpp"
#include "vm/default_symbol_resolver.hpp"
#include "vm/devices/project_device.hpp"
#include "vm/diagnostics/diagnostic_engine.hpp"
#include "vm/opcode.hpp"
#include "vm/other_device.hpp"
#include "vm/vm.hpp"

#include "other_test.hpp"

namespace other {
  namespace {

    constexpr std::string_view kAnswerSource = R"(
    /entry: main
    #numbers {
      .answer : int32 = 42
    }
    $main:
      set r1, [numbers.answer]
    end
    )";

    /// a block, a label, a data block and two fields: one of everything the debug section names
    constexpr std::string_view kNamedSource = R"(
      /entry: main
      #numbers {
        .answer : = 42
        .text : string = "hello"
      }
      $main:
        set r1, [numbers.answer]
        set r2, numbers.text
        cmplt r1, r2, rflag
        jne done
        rshift r1, r2
      @done:
        syscall project.load
      end
    )";

    constexpr std::string_view kLoadsProject = R"(
    /entry: main
    $main:
      set r0, project_folder.path
      set r1, [project_folder.length]
      syscall project.load
    end
    #project_folder {
      .length: int32 = 12
      .path: string = "test-project"
    }
    )";

  }  // namespace

  class obin_tests : public other_test {
   protected:
    void SetUp() override {
      other_test::SetUp();
      const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
      sandbox = std::filesystem::temp_directory_path() / "other-obin-tests" / info->name();
      std::filesystem::remove_all(sandbox);
      std::filesystem::create_directories(sandbox);
    }

    void TearDown() override {
      std::error_code ec;
      std::filesystem::remove_all(sandbox, ec);
      other_test::TearDown();
    }

    /// linked against the builtin devices, as a driver links its boot file
    static obin_build_result build(std::span<const oasm_source> sources, const ocmd_link_options options = {}) {
      command_bus bus;
      register_builtin_command_devices(bus);
      diagnostic_engine diag;
      return ocmd_toolchain{}.build_obin(bus, sources, &diag, options);
    }

    static obin_build_result build(const std::string_view text, const ocmd_link_options options = {}) {
      const oasm_source source = { .name = "source.oasm", .text = std::string{ text } };
      return build(std::span{ &source, 1 }, options);
    }

    static ocmd_file_header read_header(std::span<const uint8_t> bytes) {
      ocmd_file_header header;
      std::memcpy(&header, bytes.data(), sizeof(header));
      return header;
    }

    static void write_header(std::span<uint8_t> bytes, const ocmd_file_header& header) {
      std::memcpy(bytes.data(), &header, sizeof(header));
    }

    static void write_file(const filepath& path, std::span<const uint8_t> bytes) {
      std::ofstream file(path, std::ios::binary);
      ASSERT_TRUE(file.is_open()) << "failed to create " << path.string();
      file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }

    /// false when the program has not stopped after a generous number of steps
    static bool run_until_stopped(other_command_device& device) {
      for (size_t step = 0; step < 4096 && !vm::has_flag(&device, other_command_device::STOPPED); ++step) {
        vm::step(&device);
      }
      return vm::has_flag(&device, other_command_device::STOPPED);
    }

    filepath sandbox;
  };

  TEST_F(obin_tests, written_program_loads_and_runs) {
    const obin_build_result built = build(kAnswerSource);
    ASSERT_TRUE(built.success);

    const filepath path = sandbox / "answer.obin";
    write_file(path, built.bytes);

    other_command_device device;
    vm::initialize_device(&device);
    vm::load_program_from_file(&device, path);
    EXPECT_FALSE(vm::has_flag(&device, other_command_device::STOPPED));
    EXPECT_TRUE(run_until_stopped(device));
    EXPECT_EQ(device.read_register_as_u64(vm_register_idx::VM_R1), 42);
    vm::shutdown_device(&device);
  }

  TEST_F(obin_tests, write_refuses_bytes_that_would_not_load) {
    obin_build_result built = build(kAnswerSource);
    ASSERT_TRUE(built.success);

    const filepath good = sandbox / "nested" / "answer.obin";
    ASSERT_TRUE(ocmd_toolchain{}.write_obin(good, built.bytes));
    EXPECT_EQ(ocmd_toolchain{}.read_obin(good), built.bytes);

    built.bytes[0] = 'X';
    const filepath bad = sandbox / "bad.obin";
    EXPECT_FALSE(ocmd_toolchain{}.write_obin(bad, built.bytes));
    EXPECT_FALSE(std::filesystem::exists(bad));
  }

  TEST_F(obin_tests, refused_file_is_not_loaded) {
    obin_build_result built = build(kAnswerSource);
    ASSERT_TRUE(built.success);

    built.bytes[0] = 'X';
    const filepath path = sandbox / "bad.obin";
    write_file(path, built.bytes);

    other_command_device device;
    vm::initialize_device(&device);
    vm::load_program_from_file(&device, path);
    EXPECT_TRUE(vm::has_flag(&device, other_command_device::STOPPED));
    EXPECT_EQ(device.program_load_cursor, other_command_device::kProgramStartAddress);
    vm::shutdown_device(&device);
  }

  TEST_F(obin_tests, unknown_extension_is_not_loaded) {
    const obin_build_result built = build(kAnswerSource);
    ASSERT_TRUE(built.success);

    const filepath path = sandbox / "answer.oexe";
    write_file(path, built.bytes);

    other_command_device device;
    vm::initialize_device(&device);
    vm::load_program_from_file(&device, path);
    EXPECT_TRUE(vm::has_flag(&device, other_command_device::STOPPED));
    vm::shutdown_device(&device);
  }

  TEST_F(obin_tests, program_that_no_longer_fits_is_not_loaded) {
    /// the stopdev guard is the only code, so the data section decides the size: as large as a program can be
    ocmd_unit unit{ .name = "big.oasm" };
    unit.program.valid = true;
    unit.program.compiled_data_sections.push_back({ .name = "big", .fields = {}, .data = ostd::vector<uint8_t>(other_command_device::kMaxProgramSize - sizeof(instruction), 0) });

    diagnostic_engine diag;
    const ostd::vector<uint8_t> bytes = ocmd_linker{ std::span<const ocmd_unit>{ &unit, 1 } }.link(make_scope<default_symbol_resolver>(), &diag);
    const filepath path = sandbox / "big.obin";
    write_file(path, bytes);

    other_command_device device;
    vm::initialize_device(&device);
    vm::load_program_from_file(&device, path);
    const uint16_t cursor_after_first = device.program_load_cursor;
    EXPECT_NE(cursor_after_first, other_command_device::kProgramStartAddress);

    /// the first load filled the device; a second is refused where it used to assert
    vm::load_program_from_file(&device, path);
    EXPECT_EQ(device.program_load_cursor, cursor_after_first);
    vm::shutdown_device(&device);
  }

  TEST_F(obin_tests, header_records_the_environment_and_program_size) {
    const obin_build_result built = build(kAnswerSource);
    ASSERT_TRUE(built.success);
    EXPECT_FALSE(ocmd_toolchain{}.check_obin(built.bytes).has_value());

    const ocmd_file_header header = read_header(built.bytes);
    EXPECT_EQ(header.environment_version_major, OTHER_ENVIRONMENT_VERSION_MAJOR);
    EXPECT_EQ(header.environment_version_minor, OTHER_ENVIRONMENT_VERSION_MINOR);
    EXPECT_EQ(header.environment_version_patch, OTHER_ENVIRONMENT_VERSION_PATCH);
    /// the file is the header, the program the header counts, and the debug section after it
    EXPECT_EQ(header.prog_header.has_debug_flag, 1);
    EXPECT_EQ(sizeof(ocmd_file_header) + header.prog_header.program_size + obin_debug_bytes(built.bytes).size(), built.bytes.size());
  }

  TEST_F(obin_tests, read_returns_only_what_check_accepts) {
    obin_build_result built = build(kAnswerSource);
    ASSERT_TRUE(built.success);

    const filepath good = sandbox / "answer.obin";
    write_file(good, built.bytes);
    EXPECT_EQ(ocmd_toolchain{}.read_obin(good), built.bytes);

    built.bytes[0] = 'X';
    const filepath bad = sandbox / "bad.obin";
    write_file(bad, built.bytes);
    EXPECT_TRUE(ocmd_toolchain{}.read_obin(bad).empty());
    EXPECT_TRUE(ocmd_toolchain{}.read_obin(sandbox / "missing.obin").empty());
  }

  TEST_F(obin_tests, check_refuses_input_shorter_than_the_header) {
    const std::array<uint8_t, sizeof(ocmd_file_header) - 1> bytes = {};
    EXPECT_TRUE(ocmd_toolchain{}.check_obin(bytes).has_value());
    EXPECT_TRUE(ocmd_toolchain{}.check_obin({}).has_value());
  }

  TEST_F(obin_tests, check_refuses_wrong_signature) {
    obin_build_result built = build(kAnswerSource);
    ASSERT_TRUE(built.success);

    built.bytes[0] = 'X';
    const opt<std::string> refusal = ocmd_toolchain{}.check_obin(built.bytes);
    ASSERT_TRUE(refusal.has_value());
    EXPECT_NE(refusal->find("signature"), std::string::npos) << *refusal;
  }

  TEST_F(obin_tests, check_refuses_another_format_version) {
    obin_build_result built = build(kAnswerSource);
    ASSERT_TRUE(built.success);

    ocmd_file_header header = read_header(built.bytes);
    header.file_version_minor = static_cast<uint8_t>(header.file_version_minor + 1);
    write_header(built.bytes, header);

    const opt<std::string> refusal = ocmd_toolchain{}.check_obin(built.bytes);
    ASSERT_TRUE(refusal.has_value());
    EXPECT_NE(refusal->find("format version"), std::string::npos) << *refusal;
  }

  TEST_F(obin_tests, check_refuses_another_environment_version) {
    obin_build_result built = build(kAnswerSource);
    ASSERT_TRUE(built.success);

    ocmd_file_header header = read_header(built.bytes);
    header.environment_version_patch = static_cast<uint8_t>(header.environment_version_patch + 1);
    write_header(built.bytes, header);

    const opt<std::string> refusal = ocmd_toolchain{}.check_obin(built.bytes);
    ASSERT_TRUE(refusal.has_value());
    EXPECT_NE(refusal->find("linked by engine"), std::string::npos) << *refusal;
  }

  TEST_F(obin_tests, check_refuses_truncated_and_padded_programs) {
    const obin_build_result built = build(kAnswerSource);
    ASSERT_TRUE(built.success);

    /// the cut lands in the data section, which only the recorded size can catch
    const std::span<const uint8_t> truncated = std::span{ built.bytes }.first(built.bytes.size() - 1);
    EXPECT_TRUE(ocmd_toolchain{}.check_obin(truncated).has_value());

    ostd::vector<uint8_t> padded = built.bytes;
    padded.push_back(0);
    EXPECT_TRUE(ocmd_toolchain{}.check_obin(padded).has_value());
  }

  TEST_F(obin_tests, check_refuses_entry_point_outside_code) {
    obin_build_result built = build(kAnswerSource);
    ASSERT_TRUE(built.success);

    ocmd_file_header header = read_header(built.bytes);
    header.prog_header.entry_point_address = header.prog_header.data_section_offset;
    write_header(built.bytes, header);

    const opt<std::string> refusal = ocmd_toolchain{}.check_obin(built.bytes);
    ASSERT_TRUE(refusal.has_value());
    EXPECT_NE(refusal->find("entry point"), std::string::npos) << *refusal;
  }

  TEST_F(obin_tests, builtin_devices_resolve_syscalls) {
    const obin_build_result built = build(kLoadsProject);
    ASSERT_TRUE(built.success);

    /// main is two sets and then the syscall
    const ocmd_file_header header = read_header(built.bytes);
    const size_t syscall_offset = sizeof(ocmd_file_header) + header.prog_header.entry_point_address + 2 * sizeof(instruction);
    ASSERT_LE(syscall_offset + sizeof(instruction), built.bytes.size());

    instruction syscall_instruction = { 0 };
    std::memcpy(&syscall_instruction, built.bytes.data() + syscall_offset, sizeof(syscall_instruction));
    EXPECT_EQ(syscall_instruction.lower, command_bus::create_syscall_id(builtin_command_device_id::OTHER_DEVICE_PROJECT, project_device::LOAD));
  }

  TEST_F(obin_tests, syscall_without_its_device_fails_the_build) {
    const oasm_source source = { .name = "source.oasm", .text = std::string{ kLoadsProject } };
    command_bus empty_bus;
    diagnostic_engine diag;
    const obin_build_result built = ocmd_toolchain{}.build_obin(empty_bus, std::span{ &source, 1 }, &diag);
    EXPECT_FALSE(built.success);
    EXPECT_TRUE(built.bytes.empty());
  }

  TEST_F(obin_tests, sources_link_the_same_in_any_order) {
    const oasm_source main_source = {
      .name = "a.oasm",
      .text = "/entry: main\n$main:\n  call helper\nend\n",
    };
    const oasm_source helper_source = {
      .name = "b.oasm",
      .text = "$helper:\n  set r1, 7\n  ret\nend\n",
    };

    const std::array forward = { main_source, helper_source };
    const std::array backward = { helper_source, main_source };
    const obin_build_result forward_built = build(forward);
    const obin_build_result backward_built = build(backward);
    ASSERT_TRUE(forward_built.success);
    ASSERT_TRUE(backward_built.success);
    EXPECT_EQ(forward_built.bytes, backward_built.bytes);
  }

  TEST_F(obin_tests, source_that_does_not_compile_fails_the_build) {
    const std::array sources = {
      oasm_source{ .name = "a.oasm", .text = "$main:\n  ret\nend\n" },
      oasm_source{ .name = "b.oasm", .text = "#broken {\n  .value : int32 =\n" },
    };
    const obin_build_result built = build(sources);
    EXPECT_FALSE(built.success);
    EXPECT_TRUE(built.bytes.empty());
  }

  TEST_F(obin_tests, labels_in_a_row_name_the_same_instruction) {
    /// the first of two labels used to keep instruction 0, which made this program loop forever
    const obin_build_result built = build(R"(
      set r1, 1
      set r8, 0
      mov r8, rflag
      je first
      set r1, 99
    @first:
    @second:
      set r2, 2
    )");
    ASSERT_TRUE(built.success);

    other_command_device device;
    vm::initialize_device(&device);
    vm::load_program_from_bytes(&device, built.bytes);
    EXPECT_TRUE(run_until_stopped(device));
    EXPECT_EQ(device.read_register_as_u64(vm_register_idx::VM_R1), 1);
    EXPECT_EQ(device.read_register_as_u64(vm_register_idx::VM_R2), 2);
    vm::shutdown_device(&device);
  }

  TEST_F(obin_tests, a_label_after_the_last_instruction_ends_the_program) {
    const obin_build_result built = build(R"(
      set r1, 1
      goto done
      set r1, 99
    @done:
    )");
    ASSERT_TRUE(built.success);

    other_command_device device;
    vm::initialize_device(&device);
    vm::load_program_from_bytes(&device, built.bytes);
    EXPECT_TRUE(run_until_stopped(device));
    EXPECT_EQ(device.read_register_as_u64(vm_register_idx::VM_R1), 1);
    vm::shutdown_device(&device);
  }

  TEST_F(obin_tests, a_64_bit_field_has_its_own_eight_bytes) {
    /// 'int64' and 'uint64' used to make a field of no size, so it shared the next field's bytes
    const obin_build_result built = build(R"(
      /entry: main
      #numbers {
        .big : int64 = 100000
        .next : uint64 = 7
      }
      $main:
        set r1, [numbers.big]
        set r2, [numbers.next]
      end
    )");
    ASSERT_TRUE(built.success);

    other_command_device device;
    vm::initialize_device(&device);
    vm::load_program_from_bytes(&device, built.bytes);
    EXPECT_TRUE(run_until_stopped(device));
    EXPECT_EQ(device.read_register_as_u64(vm_register_idx::VM_R1), 100000);
    EXPECT_EQ(device.read_register_as_u64(vm_register_idx::VM_R2), 7);
    vm::shutdown_device(&device);
  }

  TEST_F(obin_tests, set_refuses_a_value_it_cannot_hold) {
    EXPECT_TRUE(build("set r1, 255\n").success);
    /// 300 used to be stored as 44
    EXPECT_FALSE(build("set r1, 300\n").success);
  }

  TEST_F(obin_tests, disassembly_reads_back_what_was_assembled) {
    const obin_build_result built = build(kNamedSource);
    ASSERT_TRUE(built.success);

    command_bus bus;
    register_builtin_command_devices(bus);
    const disassembly listing = decompiler::disassemble(built.bytes, &bus);
    ASSERT_TRUE(listing.error.empty()) << listing.error;
    /// with the debug section a build writes by default, the listing has the source's own names
    for (const std::string_view expected : { "entry:", "$main:", "source.oasm", "set r1, [numbers.answer]", "; starts as 42", "set r2, numbers.text", "; \"hello\"",
                                             "cmplt r1, r2, rflag", "jne done", "@done:", "rshift r1, r2", "syscall project.load", "#numbers", ".answer", ".text" }) {
      EXPECT_TRUE(listing.text.contains(expected)) << expected << "\n"
                                                   << listing.text;
    }

    /// without one it falls back to addresses and labels made up from them
    const obin_build_result bare = build(kNamedSource, { .debug_info = false });
    ASSERT_TRUE(bare.success);
    const disassembly bare_listing = decompiler::disassemble(bare.bytes, &bus);
    ASSERT_TRUE(bare_listing.error.empty()) << bare_listing.error;
    for (const std::string_view expected : { "entry:", "set r1, [0x", "; starts as 42", "; \"hello\"", "jne l_", "no debug section" }) {
      EXPECT_TRUE(bare_listing.text.contains(expected)) << expected << "\n"
                                                        << bare_listing.text;
    }
    EXPECT_FALSE(bare_listing.text.contains("numbers"));

    /// without a bus a syscall is its number: device 4, function 1
    EXPECT_TRUE(decompiler::disassemble(built.bytes).text.contains("syscall 0x0401"));
    /// naming a right shift used to hit an assert: the table of names was one row short
    EXPECT_TRUE(opcode_to_detailed_string(opcode_shift_right_x_by_y_set_z(1, 2, 1)).contains(">>"));
  }

  TEST_F(obin_tests, disassembly_refuses_bytes_that_are_not_a_program) {
    obin_build_result built = build(kAnswerSource);
    ASSERT_TRUE(built.success);

    EXPECT_FALSE(decompiler::disassemble(std::span<const uint8_t>{ built.bytes }.first(8)).error.empty());

    ostd::vector<uint8_t> wrong_signature = built.bytes;
    wrong_signature[0] = 'X';
    EXPECT_FALSE(decompiler::disassemble(wrong_signature).error.empty());

    /// a code section that claims to run past the end of the file
    ocmd_file_header header = read_header(built.bytes);
    header.prog_header.data_section_offset = 0xFFF0;
    write_header(built.bytes, header);
    EXPECT_FALSE(decompiler::disassemble(built.bytes).error.empty());
  }

  TEST_F(obin_tests, debug_section_names_what_was_linked) {
    const obin_build_result built = build(kNamedSource);
    ASSERT_TRUE(built.success);
    const ocmd_file_header header = read_header(built.bytes);
    ASSERT_EQ(header.prog_header.has_debug_flag, 1);

    const opt<obin_debug_info> debug = obin_debug_info::decode(obin_debug_bytes(built.bytes));
    ASSERT_TRUE(debug.has_value());
    const auto find = [&debug](const debug_symbol_kind kind, const std::string_view name) -> const debug_symbol* {
      const auto found = std::ranges::find_if(debug->symbols, [&](const debug_symbol& symbol) { return symbol.kind == kind && symbol.name == name; });
      return found != debug->symbols.end() ? &*found : nullptr;
    };

    const debug_symbol* unit = find(DEBUG_SYMBOL_UNIT, "source.oasm");
    const debug_symbol* block = find(DEBUG_SYMBOL_BLOCK, "main");
    const debug_symbol* label = find(DEBUG_SYMBOL_LABEL, "done");
    const debug_symbol* data = find(DEBUG_SYMBOL_DATA, "numbers");
    const debug_symbol* answer = find(DEBUG_SYMBOL_FIELD, "answer");
    const debug_symbol* text = find(DEBUG_SYMBOL_FIELD, "text");
    ASSERT_NE(unit, nullptr);
    ASSERT_NE(block, nullptr);
    ASSERT_NE(label, nullptr);
    ASSERT_NE(data, nullptr);
    ASSERT_NE(answer, nullptr);
    ASSERT_NE(text, nullptr);

    EXPECT_EQ(block->address, header.prog_header.entry_point_address);
    EXPECT_EQ(&debug->symbols[block->parent], unit);
    EXPECT_EQ(&debug->symbols[label->parent], block);
    /// five instructions sit between the start of the block and the label
    EXPECT_EQ(label->address, block->address + 5 * sizeof(instruction));

    EXPECT_EQ(data->address, header.prog_header.data_section_offset);
    EXPECT_EQ(&debug->symbols[data->parent], unit);
    EXPECT_EQ(&debug->symbols[answer->parent], data);
    EXPECT_EQ(answer->address, data->address);
    EXPECT_EQ(answer->size, 8);
    /// a text keeps the length it was written with, not the length it is padded to
    EXPECT_EQ(text->address, data->address + 8);
    EXPECT_EQ(text->size, 5);
  }

  TEST_F(obin_tests, debug_section_stays_out_of_the_device) {
    const obin_build_result with_debug = build(kAnswerSource);
    const obin_build_result without_debug = build(kAnswerSource, { .debug_info = false });
    ASSERT_TRUE(with_debug.success);
    ASSERT_TRUE(without_debug.success);

    EXPECT_EQ(read_header(without_debug.bytes).prog_header.has_debug_flag, 0);
    EXPECT_TRUE(obin_debug_bytes(without_debug.bytes).empty());
    EXPECT_GT(with_debug.bytes.size(), without_debug.bytes.size());
    /// what the device runs is the same either way
    EXPECT_TRUE(std::ranges::equal(obin_program_bytes(with_debug.bytes), obin_program_bytes(without_debug.bytes)));

    for (const obin_build_result* built : { &with_debug, &without_debug }) {
      other_command_device device;
      vm::initialize_device(&device);
      vm::load_program_from_bytes(&device, built->bytes);
      /// the load cursor moves by the program alone
      EXPECT_EQ(device.program_load_cursor, other_command_device::kProgramStartAddress + obin_program_bytes(built->bytes).size());
      EXPECT_TRUE(run_until_stopped(device));
      EXPECT_EQ(device.read_register_as_u64(vm_register_idx::VM_R1), 42);
      vm::shutdown_device(&device);
    }
  }

  TEST_F(obin_tests, damaged_debug_section_is_refused) {
    const obin_build_result built = build(kAnswerSource);
    ASSERT_TRUE(built.success);
    const ocmd_toolchain toolchain;
    const size_t debug_start = built.bytes.size() - obin_debug_bytes(built.bytes).size();
    ASSERT_LT(debug_start, built.bytes.size());

    EXPECT_TRUE(toolchain.check_obin(std::span<const uint8_t>{ built.bytes }.first(built.bytes.size() - 1)).has_value());

    ostd::vector<uint8_t> wrong_signature = built.bytes;
    wrong_signature[debug_start] = 'X';
    EXPECT_TRUE(toolchain.check_obin(wrong_signature).has_value());

    /// the flag promises a section the file does not have
    ostd::vector<uint8_t> cut_off = built.bytes;
    cut_off.resize(debug_start);
    EXPECT_TRUE(toolchain.check_obin(cut_off).has_value());

    /// with the flag cleared as well, what is left is a whole program again
    ocmd_file_header header = read_header(cut_off);
    header.prog_header.has_debug_flag = 0;
    write_header(cut_off, header);
    EXPECT_FALSE(toolchain.check_obin(cut_off).has_value());
  }

  TEST_F(obin_tests, debug_info_reads_back_what_it_wrote) {
    obin_debug_info info;
    info.symbols.push_back({ .kind = DEBUG_SYMBOL_UNIT, .name = "boot.ocmd" });
    info.symbols.push_back({ .kind = DEBUG_SYMBOL_BLOCK, .name = "f_add-three", .address = 0x40, .size = 24, .parent = 0 });
    info.symbols.push_back({ .kind = DEBUG_SYMBOL_LABEL, .name = "l_0", .address = 0x48, .parent = 1 });

    const ostd::vector<uint8_t> section = info.encode();
    const opt<obin_debug_info> read = obin_debug_info::decode(section);
    ASSERT_TRUE(read.has_value());
    ASSERT_EQ(read->symbols.size(), 3);
    EXPECT_EQ(read->symbols[0].parent, debug_symbol::kNoParent);
    EXPECT_EQ(read->symbols[1].kind, DEBUG_SYMBOL_BLOCK);
    EXPECT_EQ(read->symbols[1].name, "f_add-three");
    EXPECT_EQ(read->symbols[1].address, 0x40);
    EXPECT_EQ(read->symbols[1].size, 24);
    EXPECT_EQ(read->symbols[2].parent, 1);

    /// a chunk of a kind this build does not know is stepped over, so older tools read newer files
    ostd::vector<uint8_t> extended = section;
    const debug_chunk_header unknown = { .kind = 0x7FFF, .size = 4 };
    const uint8_t* unknown_bytes = reinterpret_cast<const uint8_t*>(&unknown);
    extended.insert(extended.end(), unknown_bytes, unknown_bytes + sizeof(unknown));
    extended.insert(extended.end(), { 1, 2, 3, 4 });
    debug_section_header header;
    std::memcpy(&header, extended.data(), sizeof(header));
    header.size = static_cast<uint32_t>(extended.size());
    header.chunk_count = 2;
    std::memcpy(extended.data(), &header, sizeof(header));
    const opt<obin_debug_info> stepped_over = obin_debug_info::decode(extended);
    ASSERT_TRUE(stepped_over.has_value());
    EXPECT_EQ(stepped_over->symbols.size(), 3);

    /// a parent that comes later, or not at all, is refused; so is a section that is cut short
    obin_debug_info orphan;
    orphan.symbols.push_back({ .kind = DEBUG_SYMBOL_BLOCK, .name = "orphan", .parent = 5 });
    EXPECT_FALSE(obin_debug_info::decode(orphan.encode()).has_value());
    EXPECT_FALSE(obin_debug_info::decode(std::span<const uint8_t>{ section }.first(section.size() - 1)).has_value());
    EXPECT_FALSE(obin_debug_info::decode({}).has_value());
  }

  TEST_F(obin_tests, debug_lines_read_back_and_answer_for_an_address) {
    obin_debug_info info;
    info.symbols.push_back({ .kind = DEBUG_SYMBOL_UNIT, .name = "boot.ocmd" });
    info.symbols.push_back({ .kind = DEBUG_SYMBOL_BLOCK, .name = "m_boot", .address = 0x40, .size = 24, .parent = 0 });
    info.lines.push_back({ .address = 0x40, .unit = 0, .line = 6, .column = 3 });
    info.lines.push_back({ .address = 0x48, .unit = 0, .line = 7, .column = 12 });
    info.lines.push_back({ .address = 0x50 });

    const opt<obin_debug_info> read = obin_debug_info::decode(info.encode());
    ASSERT_TRUE(read.has_value());
    ASSERT_EQ(read->lines.size(), 3);
    EXPECT_EQ(read->symbols.size(), 2);

    /// an entry covers every address from its own up to the next entry's
    const debug_line_entry* first = read->line_at(0x44);
    ASSERT_NE(first, nullptr);
    EXPECT_EQ(first->line, 6);
    EXPECT_EQ(first->column, 3);
    const debug_line_entry* second = read->line_at(0x48);
    ASSERT_NE(second, nullptr);
    EXPECT_EQ(second->line, 7);
    EXPECT_EQ(second->column, 12);
    /// before the first entry, and from an entry with no line onward, there is no source line
    EXPECT_EQ(read->line_at(0x3c), nullptr);
    EXPECT_EQ(read->line_at(0x50), nullptr);
    EXPECT_EQ(read->line_at(0x400), nullptr);

    /// a line has to name a unit, and the entries have to be in address order
    obin_debug_info wrong_unit = info;
    wrong_unit.lines[0].unit = 1;
    EXPECT_FALSE(obin_debug_info::decode(wrong_unit.encode()).has_value());
    obin_debug_info out_of_order = info;
    out_of_order.lines[1].address = 0x10;
    EXPECT_FALSE(obin_debug_info::decode(out_of_order.encode()).has_value());
  }

  TEST_F(obin_tests, debug_lines_point_at_the_assembly_that_was_written) {
    const obin_build_result built = build(kNamedSource);
    ASSERT_TRUE(built.success);
    const ocmd_file_header header = read_header(built.bytes);
    const opt<obin_debug_info> debug = obin_debug_info::decode(obin_debug_bytes(built.bytes));
    ASSERT_TRUE(debug.has_value());

    /// kNamedSource starts with a line break, so '$main:' is its line 7 and the code starts on 8
    const uint32_t entry = header.prog_header.entry_point_address;
    const debug_line_entry* first = debug->line_at(entry);
    ASSERT_NE(first, nullptr);
    EXPECT_EQ(debug->symbols[first->unit].name, "source.oasm");
    EXPECT_EQ(first->line, 8);
    EXPECT_EQ(first->column, 9);

    const debug_line_entry* jump = debug->line_at(entry + 3 * sizeof(instruction));
    ASSERT_NE(jump, nullptr);
    EXPECT_EQ(jump->line, 11);
    /// the label line itself makes no instruction: the syscall after it is line 14
    const debug_line_entry* syscall_line = debug->line_at(entry + 5 * sizeof(instruction));
    ASSERT_NE(syscall_line, nullptr);
    EXPECT_EQ(syscall_line->line, 14);

    /// the stops that close the block and the code were written by the assembler, not by the source
    EXPECT_EQ(debug->line_at(entry + 6 * sizeof(instruction)), nullptr);
    EXPECT_EQ(debug->line_at(header.prog_header.data_section_offset - sizeof(instruction)), nullptr);
  }

  TEST_F(obin_tests, lines_are_counted_through_a_block_comment) {
    /// the lexer used to lose count of the lines inside the comment, so every line after it was too low
    const obin_build_result built = build("/;\n  two lines\n  of comment\n;/\n; and one more\nset r1, 7\n");
    ASSERT_TRUE(built.success);
    const opt<obin_debug_info> debug = obin_debug_info::decode(obin_debug_bytes(built.bytes));
    ASSERT_TRUE(debug.has_value());

    const debug_line_entry* first = debug->line_at(read_header(built.bytes).prog_header.entry_point_address);
    ASSERT_NE(first, nullptr);
    EXPECT_EQ(first->line, 6);
    EXPECT_EQ(first->column, 1);
  }

  TEST_F(obin_tests, data_listing_shows_each_field_as_its_type_and_where_it_is_used) {
    const obin_build_result built = build(R"(
      /entry: main
      #numbers {
        .limit : = 100000
        .small : int32 = 12
        .below : int32 = -3
        .ratio : float = 1.5
        .where : address = 0x0040
        .bytes : blob = 0x610a62
        .greeting : string = "hello"
        .unused : string = 'say "hi"'
      }
      $main:
        set r1, [numbers.limit]
        write r1, numbers.small
        set r2, numbers.greeting
      end
    )");
    ASSERT_TRUE(built.success);

    const disassembly listing = decompiler::list_data(built.bytes);
    ASSERT_TRUE(listing.error.empty()) << listing.error;
    /// the type each field was declared with, and its bytes read as that type
    for (const std::string_view expected : { "#numbers", "source.oasm", ".limit", "int64", "100000", ".small", "int32", " 12\n", " -3\n", "float", "1.5", "address", "0x0040",
                                             "blob", "61 0a 62", "string", "\"hello\"", "\"say \\\"hi\\\"\"" }) {
      EXPECT_TRUE(listing.text.contains(expected)) << expected << "\n"
                                                   << listing.text;
    }
    /// how the code uses each one, in which block, and on which line of the source
    for (const std::string_view expected : { "read ", "written ", "address ", "$main", "source.oasm:14:9", "source.oasm:15:9", "source.oasm:16:9", "not used by the code" }) {
      EXPECT_TRUE(listing.text.contains(expected)) << expected << "\n"
                                                   << listing.text;
    }

    /// without the debug section the fields are found from the addresses the code names
    const obin_build_result bare = build(kNamedSource, { .debug_info = false });
    ASSERT_TRUE(bare.success);
    const disassembly bare_listing = decompiler::list_data(bare.bytes);
    ASSERT_TRUE(bare_listing.error.empty()) << bare_listing.error;
    for (const std::string_view expected : { "no debug section", "data_", " 42\n", "\"hello\"", "read ", "address " }) {
      EXPECT_TRUE(bare_listing.text.contains(expected)) << expected << "\n"
                                                        << bare_listing.text;
    }
    EXPECT_FALSE(bare_listing.text.contains("numbers"));

    EXPECT_FALSE(decompiler::list_data(std::span<const uint8_t>{ built.bytes }.first(8)).error.empty());
  }

  TEST_F(obin_tests, debug_section_records_how_each_field_was_declared) {
    const obin_build_result built = build("#numbers {\n  .plain : = 5\n  .small : int32 = 12\n  .text : string = \"hello\"\n  .raw := DE AD BE EF\n}\nset r1, 1\n");
    ASSERT_TRUE(built.success);
    const opt<obin_debug_info> debug = obin_debug_info::decode(obin_debug_bytes(built.bytes));
    ASSERT_TRUE(debug.has_value());

    const auto type_of = [&debug](const std::string_view name) -> debug_field_type {
      const auto found = std::ranges::find_if(debug->symbols, [&](const debug_symbol& symbol) { return symbol.kind == DEBUG_SYMBOL_FIELD && symbol.name == name; });
      return found != debug->symbols.end() ? found->type : NUM_DEBUG_FIELD_TYPES;
    };
    /// a field with no type written is what the assembler deduced it to be
    EXPECT_EQ(type_of("plain"), DEBUG_FIELD_I64);
    EXPECT_EQ(type_of("small"), DEBUG_FIELD_I32);
    EXPECT_EQ(type_of("text"), DEBUG_FIELD_STRING);
    EXPECT_EQ(type_of("raw"), DEBUG_FIELD_BLOB);
    EXPECT_EQ(debug_field_type_name(DEBUG_FIELD_I32), "int32");
    EXPECT_EQ(debug_field_type_name(DEBUG_FIELD_UNKNOWN), "");
  }

}  // namespace other
