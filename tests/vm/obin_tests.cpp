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
#include "vm/command_files/ocmd_headers.hpp"
#include "vm/command_files/ocmd_linker.hpp"
#include "vm/command_files/ocmd_toolchain.hpp"
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
    static obin_build_result build(std::span<const oasm_source> sources) {
      command_bus bus;
      register_builtin_command_devices(bus);
      diagnostic_engine diag;
      return ocmd_toolchain{}.build_obin(bus, sources, &diag);
    }

    static obin_build_result build(const std::string_view text) {
      const oasm_source source = { .name = "source.oasm", .text = std::string{ text } };
      return build(std::span{ &source, 1 });
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
    EXPECT_EQ(header.prog_header.program_size, built.bytes.size() - sizeof(ocmd_file_header));
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

}  // namespace other
