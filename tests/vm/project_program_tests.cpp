/**
 * \file tests/vm/project_program_tests.cpp
 *
 * a project's boot program: which files go into it, which hooks it calls, and what the
 * generated entry does when it runs.
 **/
#include <filesystem>
#include <fstream>

#include "project/project.hpp"
#include "vm/builtin_command_devices.hpp"
#include "vm/command_bus.hpp"
#include "vm/command_files/ocmd_toolchain.hpp"
#include "vm/device_utils.hpp"
#include "vm/diagnostics/diagnostic_engine.hpp"
#include "vm/other_device.hpp"
#include "vm/project_program.hpp"
#include "vm/vm.hpp"
#include "vm/vm_error.hpp"

#include "other_test.hpp"

namespace other {
  namespace {

    /// what a running boot program asked of its devices, in order
    struct boot_trace {
      ostd::vector<std::string> steps;
      /// project.loaded answers 0 this many times before it answers 1
      uint32_t frames_until_loaded = 3;
      bool refuse_load = false;
    };

    constexpr type_descriptor kNoParams[] = {
      {}
    };
    constexpr type_descriptor kTextParams[] = {
      { VM_TYPE_PTR, "text-ptr" },
      { VM_TYPE_U64, "text-len" },
    };
    constexpr type_descriptor kLogParams[] = {
      { VM_TYPE_PTR, "text-ptr" },
      { VM_TYPE_U64, "text-len" },
      { VM_TYPE_U64, "level" },
    };
    constexpr type_descriptor kMarkParams[] = {
      { VM_TYPE_U64, "number" },
    };
    constexpr type_descriptor kVoidRet = { VM_TYPE_VOID, "void" };
    constexpr type_descriptor kU64Ret = { VM_TYPE_U64, "value" };

    /// the project device's two functions, without a driver behind them
    class traced_project_device : public command_device {
     public:
      enum function : uint8_t {
        LOADED = 0x00,
        LOAD = 0x01,
        NUM_FUNCTIONS
      };
      explicit traced_project_device(boot_trace* trace) : trace(trace) {}

      std::string_view device_name() const override { return "project"; }
      uint8_t device_id() const override { return builtin_command_device_id::OTHER_DEVICE_PROJECT; }

      std::span<const function_descriptor> functions() const override {
        constexpr static function_descriptor kFunctions[] = {
          { LOADED, "loaded", kNoParams, kU64Ret },
          { LOAD, "load", kTextParams, kVoidRet },
        };
        return kFunctions;
      }

      void dispatch(uint8_t function_id, other_command_device* device) override {
        if (function_id == LOADED) {
          const bool loaded = trace->frames_until_loaded == 0;
          if (loaded) {
            trace->steps.push_back("loaded");
          } else {
            --trace->frames_until_loaded;
          }
          device->write_return_register(loaded ? 1 : 0);
          return;
        }
        /// the flag is read before the text, which is the order the real device works in
        const uint64_t flag = device->read_flag_register();
        trace->steps.push_back(std::format("load '{}' with flag {}", detail::read_device_string(device, vm_register_idx::VM_R0, vm_register_idx::VM_R1), flag));
        if (trace->refuse_load) {
          device->write_flag_register(vm_error::VM_BAD_ARGUMENT);
        }
      }

     private:
      boot_trace* trace;
    };

    class traced_core_device : public command_device {
     public:
      enum function : uint8_t {
        LOG = 0x00,
        NUM_FUNCTIONS
      };
      explicit traced_core_device(boot_trace* trace) : trace(trace) {}

      std::string_view device_name() const override { return "core"; }
      uint8_t device_id() const override { return builtin_command_device_id::OTHER_DEVICE_CORE; }

      std::span<const function_descriptor> functions() const override {
        constexpr static function_descriptor kFunctions[] = {
          { LOG, "log", kLogParams, kVoidRet },
        };
        return kFunctions;
      }

      void dispatch(uint8_t, other_command_device* device) override {
        const uint64_t level = device->read_register_as_u64(vm_register_idx::VM_R2);
        trace->steps.push_back(std::format("log '{}' at level {}", detail::read_device_string(device, vm_register_idx::VM_R0, vm_register_idx::VM_R1), level));
      }

     private:
      boot_trace* trace;
    };

    /// something a hook can call to say it ran
    class probe_device : public command_device {
     public:
      enum function : uint8_t {
        MARK = 0x00,
        NUM_FUNCTIONS
      };
      explicit probe_device(boot_trace* trace) : trace(trace) {}

      std::string_view device_name() const override { return "probe"; }
      uint8_t device_id() const override { return builtin_command_device_id::OTHER_DEVICE_USER_RANGE_START; }

      std::span<const function_descriptor> functions() const override {
        constexpr static function_descriptor kFunctions[] = {
          { MARK, "mark", kMarkParams, kVoidRet },
        };
        return kFunctions;
      }

      void dispatch(uint8_t, other_command_device* device) override {
        trace->steps.push_back(std::format("mark {}", device->read_register_as_u64(vm_register_idx::VM_R0)));
      }

     private:
      boot_trace* trace;
    };

    void register_traced_devices(command_bus& bus, boot_trace* trace) {
      bus.register_device(make_scope<traced_core_device>(trace));
      bus.register_device(make_scope<traced_project_device>(trace));
      bus.register_device(make_scope<probe_device>(trace));
    }

    /// a hook that says it ran, and leaves the flag register dirty on its way out
    std::string marking_hook(const std::string_view name, const uint32_t number) {
      return std::format("${}:\n  set r0, {}\n  set rflag, 0\n  syscall probe.mark\n  set rflag, 9\n  ret\nend\n", name, number);
    }

    bool holds(const std::string_view text, const std::string_view part) {
      return text.find(part) != std::string_view::npos;
    }

  }  // namespace

  class project_program_tests : public other_test {
   protected:
    void SetUp() override {
      other_test::SetUp();
      const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
      sandbox = std::filesystem::temp_directory_path() / "other-project-program-tests" / info->name();
      std::filesystem::remove_all(sandbox);
      project_folder = sandbox / "game";
      project_file = project_folder / "game.toml";
      write("game.toml", "[project.metadata]\nname = \"game\"\n");
    }

    void TearDown() override {
      std::error_code ec;
      std::filesystem::remove_all(sandbox, ec);
      other_test::TearDown();
    }

    /// path is from the project's folder
    void write(const filepath& path, const std::string_view text) {
      const filepath full = project_folder / path;
      std::filesystem::create_directories(full.parent_path());
      std::ofstream out(full, std::ios::binary | std::ios::trunc);
      ASSERT_TRUE(out.is_open()) << full.string();
      out.write(text.data(), static_cast<std::streamsize>(text.size()));
    }

    /// linked against the builtin devices, as a driver links its boot file
    project_program_result build(const project_program_options& options = { .project_load_path = "game/game.toml" }) {
      command_bus bus;
      register_builtin_command_devices(bus);
      diagnostic_engine diag;
      return build_project_program(bus, project_file, &diag, options);
    }

    /// linked against the traced devices and run until the device stops
    boot_trace build_and_run(boot_trace trace = {}) {
      command_bus bus;
      register_traced_devices(bus, &trace);
      diagnostic_engine diag;
      const project_program_result built = build_project_program(bus, project_file, &diag, { .project_load_path = "game/game.toml" });
      EXPECT_TRUE(built.success);
      if (!built.success) {
        return trace;
      }

      other_command_device device;
      vm::initialize_device(&device);
      register_traced_devices(*device.bus, &trace);
      vm::load_program_from_bytes(&device, built.bytes);
      for (size_t step = 0; step < 4096 && !vm::has_flag(&device, other_command_device::STOPPED); ++step) {
        vm::step(&device);
      }
      EXPECT_TRUE(vm::has_flag(&device, other_command_device::STOPPED));
      vm::shutdown_device(&device);
      return trace;
    }

    static ostd::vector<std::string> names_of(std::span<const project_vm_source> sources) {
      ostd::vector<std::string> names;
      for (const project_vm_source& source : sources) {
        names.push_back(source.name);
      }
      return names;
    }

    static ostd::vector<std::string> blocks_of(std::span<const project_hook_handler> hooks) {
      ostd::vector<std::string> blocks;
      for (const project_hook_handler& hook : hooks) {
        blocks.push_back(hook.block);
      }
      return blocks;
    }

    /// what project.load leaves in the flag register for a path, on the real device
    uint64_t flag_after_loading(const filepath& path) {
      const std::string text = path.generic_string();
      const oasm_source source = {
        .name = "load.oasm",
        .text = std::format("/entry: main\n#target {{\n  .path : string = \"{}\"\n  .length : = {}\n}}\n"
                            "$main:\n  set r0, target.path\n  set r1, [target.length]\n  set rflag, 0\n  syscall project.load\n  mov rflag, r5\n  stopdev\nend\n",
                            text, text.size()),
      };
      command_bus bus;
      register_builtin_command_devices(bus);
      diagnostic_engine diag;
      const obin_build_result built = ocmd_toolchain{}.build_obin(bus, std::span{ &source, 1 }, &diag);
      EXPECT_TRUE(built.success);
      if (!built.success) {
        return 0;
      }

      other_command_device device;
      vm::initialize_device(&device);
      register_builtin_command_devices(*device.bus);
      vm::load_program_from_bytes(&device, built.bytes);
      for (size_t step = 0; step < 64 && !vm::has_flag(&device, other_command_device::STOPPED); ++step) {
        vm::step(&device);
      }
      const uint64_t flag = device.read_register_as_u64(vm_register_idx::VM_R5);
      vm::shutdown_device(&device);
      return flag;
    }

    filepath sandbox;
    filepath project_folder;
    filepath project_file;
  };

  TEST_F(project_program_tests, a_project_path_is_the_file_or_the_folder_that_holds_it) {
    EXPECT_EQ(project::find_project_file(project_file), project_file);
    EXPECT_EQ(project::find_project_file(project_folder), project_file);
    EXPECT_EQ(project::find_project_file(filepath{ project_folder.string() + "/" }), project_file);

    /// a folder answers only for the file that has its name
    EXPECT_FALSE(project::find_project_file(sandbox).has_value());
    EXPECT_FALSE(project::find_project_file(sandbox / "nothing-here").has_value());

    write("../other/other.oproj", "");
    EXPECT_EQ(project::find_project_file(sandbox / "other"), sandbox / "other" / "other.oproj");
    write("../other/other.toml", "");
    EXPECT_EQ(project::find_project_file(sandbox / "other"), sandbox / "other" / "other.toml");
  }

  TEST_F(project_program_tests, loading_a_path_that_names_no_project_sets_the_flag) {
    /// neither starts a load, so neither reaches the driver this test does not have
    EXPECT_EQ(flag_after_loading(sandbox / "nothing-here"), vm_error::VM_BAD_ARGUMENT);
    EXPECT_EQ(flag_after_loading(sandbox), vm_error::VM_BAD_ARGUMENT);
  }

  TEST_F(project_program_tests, a_project_with_no_vm_sources_still_has_a_boot_program) {
    const project_program_result built = build();
    ASSERT_TRUE(built.success);
    EXPECT_TRUE(built.sources.empty());
    EXPECT_TRUE(built.hooks.empty());
    EXPECT_FALSE(holds(built.boot_unit, "\n  call "));
    EXPECT_FALSE(ocmd_toolchain{}.check_obin(built.bytes).has_value());
  }

  TEST_F(project_program_tests, sources_are_the_boot_file_and_everything_under_assets_vm) {
    write("game.toml", "[vm]\nboot-file = \"scripts/boot.oasm\"\n");
    write("scripts/boot.oasm", "$helper:\n  ret\nend\n");
    write("assets/vm/b.oasm", "$second_helper:\n  ret\nend\n");
    write("assets/vm/deep/a.ocmd", "fn third_helper() {\n}\n");
    write("assets/vm/old.obin", "left over from a build");
    write("assets/vm/notes.txt", "not a source");
    write("assets/scenes/elsewhere.oasm", "$elsewhere:\n  ret\nend\n");

    const opt<project_vm_settings> settings = read_project_vm_settings(project_file);
    ASSERT_TRUE(settings.has_value());
    ASSERT_TRUE(settings->boot_file.has_value());
    EXPECT_EQ(names_of(collect_project_vm_sources(*settings)), (ostd::vector<std::string>{ "assets/vm/b.oasm", "assets/vm/deep/a.ocmd", "scripts/boot.oasm" }));

    const project_program_result built = build();
    ASSERT_TRUE(built.success);
    EXPECT_EQ(built.sources, (ostd::vector<std::string>{ "assets/vm/b.oasm", "assets/vm/deep/a.ocmd", "scripts/boot.oasm" }));
  }

  TEST_F(project_program_tests, a_boot_file_under_assets_vm_is_linked_once) {
    write("game.toml", "[vm]\nboot-file = \"assets/vm/boot.oasm\"\n");
    write("assets/vm/boot.oasm", "$helper:\n  ret\nend\n");

    const project_program_result built = build();
    ASSERT_TRUE(built.success);
    EXPECT_EQ(built.sources, (ostd::vector<std::string>{ "assets/vm/boot.oasm" }));
  }

  TEST_F(project_program_tests, a_boot_file_that_is_not_there_fails_the_build) {
    write("game.toml", "[vm]\nboot-file = \"assets/vm/boot.oasm\"\n");
    EXPECT_FALSE(build().success);
  }

  TEST_F(project_program_tests, a_project_file_that_cannot_be_read_fails_the_build) {
    write("game.toml", "[vm\nboot-file = ");
    EXPECT_FALSE(build().success);

    std::filesystem::remove(project_file);
    EXPECT_FALSE(read_project_vm_settings(project_file).has_value());
    EXPECT_FALSE(build().success);
  }

  TEST_F(project_program_tests, the_same_project_gives_the_same_program) {
    write("assets/vm/one.oasm", "$pre_load:\n  ret\nend\n");
    write("assets/vm/two.ocmd", "on post_load {\n  core log \"ready\" 2\n}\n");

    const project_program_result first = build();
    const project_program_result second = build();
    ASSERT_TRUE(first.success);
    EXPECT_EQ(first.bytes, second.bytes);
  }

  TEST_F(project_program_tests, the_program_loads_the_project_by_the_path_it_is_told) {
    EXPECT_TRUE(holds(build().boot_unit, "\"game/game.toml\""));
    EXPECT_TRUE(holds(build({ .project_load_path = "packaged/game.toml" }).boot_unit, "\"packaged/game.toml\""));
    /// with nothing said it is the project file as the build was given it
    EXPECT_TRUE(holds(build({}).boot_unit, std::format("\"{}\"", project_file.generic_string())));
  }

  TEST_F(project_program_tests, every_hook_is_found_by_the_name_a_script_writes) {
    for (const hook_descriptor& hook : vm_hooks()) {
      EXPECT_EQ(get_hook_id(hook.name), hook.id) << hook.name;
      EXPECT_EQ(hook_name(hook.id), hook.name);
    }
    EXPECT_FALSE(get_hook_id("not_a_hook").has_value());
    /// an engine event is not a hook, whatever it is called
    EXPECT_FALSE(get_hook_id("scene.activated").has_value());
  }

  TEST_F(project_program_tests, a_hook_in_assembly_is_a_block_with_the_hooks_name) {
    write("assets/vm/a.oasm", "$post_load:\n  ret\nend\n");
    write("assets/vm/b.oasm", "$pre_load:\n  call shared\n  ret\nend\n$shared:\n  ret\nend\n");

    const project_program_result built = build();
    ASSERT_TRUE(built.success);
    ASSERT_EQ(built.hooks.size(), 2);
    EXPECT_EQ(built.hooks[0].block, "post_load");
    EXPECT_EQ(built.hooks[0].hook, hook_id::POST_LOAD);
    EXPECT_EQ(built.hooks[0].unit, "assets/vm/a.oasm");
    EXPECT_EQ(built.hooks[1].block, "pre_load");
    EXPECT_EQ(built.hooks[1].hook, hook_id::PRE_LOAD);
    EXPECT_TRUE(built.warnings.empty());

    /// whatever order they were found in, each is called on its own side of the load
    const size_t pre = built.boot_unit.find("call pre_load");
    const size_t load = built.boot_unit.find("syscall project.load\n");
    const size_t post = built.boot_unit.find("call post_load");
    ASSERT_NE(pre, std::string::npos);
    ASSERT_NE(load, std::string::npos);
    ASSERT_NE(post, std::string::npos);
    EXPECT_LT(pre, load);
    EXPECT_LT(load, post);
  }

  TEST_F(project_program_tests, a_hook_in_a_script_is_a_handler_and_every_file_may_have_one) {
    write("assets/vm/audio.ocmd", "on pre_load {\n  core log \"audio\" 2\n}\n");
    write("assets/vm/splash.ocmd", "on pre_load {\n  core log \"splash\" 2\n}\non post_load {\n  core log \"done\" 2\n}\n");

    const project_program_result built = build();
    ASSERT_TRUE(built.success);
    EXPECT_EQ(blocks_of(built.hooks), (ostd::vector<std::string>{ "h_assets_vm_audio_pre_load", "h_assets_vm_splash_pre_load", "h_assets_vm_splash_post_load" }));
    EXPECT_TRUE(built.uncalled_handlers.empty());
  }

  TEST_F(project_program_tests, scripts_and_assembly_can_both_hold_a_hook) {
    write("assets/vm/a.oasm", "$pre_load:\n  ret\nend\n");
    write("assets/vm/b.ocmd", "on pre_load {\n  core log \"from the script\" 2\n}\n");

    const project_program_result built = build();
    ASSERT_TRUE(built.success);
    EXPECT_EQ(blocks_of(built.hooks), (ostd::vector<std::string>{ "pre_load", "h_assets_vm_b_pre_load" }));
  }

  TEST_F(project_program_tests, two_scripts_of_one_name_in_different_folders_build_together) {
    write("assets/vm/menu/setup.ocmd", "fn menu_setup() {\n  core log \"menu\" 2\n}\non pre_load {\n  menu_setup()\n}\n");
    write("assets/vm/world/setup.ocmd", "let seed = 7\non pre_load {\n  core log \"world\" 2\n}\n");

    const project_program_result built = build();
    ASSERT_TRUE(built.success);
    EXPECT_EQ(blocks_of(built.hooks), (ostd::vector<std::string>{ "h_assets_vm_menu_setup_pre_load", "h_assets_vm_world_setup_pre_load" }));
  }

  TEST_F(project_program_tests, a_script_function_can_be_called_from_another_file) {
    write("assets/vm/library.ocmd", "fn announce() {\n  core log \"from the library\" 2\n}\n");
    write("assets/vm/startup.ocmd", "use \"library.ocmd\"\non pre_load {\n  announce()\n}\n");
    EXPECT_TRUE(build().success);
  }

  TEST_F(project_program_tests, a_script_cannot_use_a_file_the_project_does_not_build) {
    write("outside.ocmd", "fn announce() {\n}\n");
    write("assets/vm/startup.ocmd", "use \"../../outside.ocmd\"\non pre_load {\n  announce()\n}\n");
    EXPECT_FALSE(build().success);
  }

  TEST_F(project_program_tests, a_handler_for_an_engine_event_is_linked_and_not_called) {
    write("assets/vm/events.ocmd", "on \"scene.activated\" {\n  core log \"scene\" 2\n}\n");

    const project_program_result built = build();
    ASSERT_TRUE(built.success);
    EXPECT_TRUE(built.hooks.empty());
    EXPECT_EQ(built.uncalled_handlers, (ostd::vector<std::string>{ "scene.activated in assets/vm/events.ocmd" }));
    EXPECT_FALSE(holds(built.boot_unit, "\n  call "));
  }

  TEST_F(project_program_tests, statements_outside_a_function_are_never_run_in_a_project_script) {
    /// in a project every script is a library: the entry is the generated one. the script
    ///  front end warns about them, and a strict build takes a warning as an error
    write("assets/vm/loose.ocmd", "core log \"when would this run\" 2\n");
    EXPECT_TRUE(build().success);
    EXPECT_FALSE(build({ .project_load_path = "game/game.toml", .link = { .strict = true } }).success);
  }

  TEST_F(project_program_tests, an_entry_of_the_projects_own_fails_the_build) {
    write("assets/vm/boot.oasm", "/entry: main\n$main:\n  ret\nend\n");
    EXPECT_FALSE(build().success);
  }

  TEST_F(project_program_tests, names_kept_for_the_generated_unit_fail_the_build) {
    write("assets/vm/code.oasm", "$__boot_helper:\n  ret\nend\n");
    EXPECT_FALSE(build().success);

    write("assets/vm/code.oasm", "#__boot_settings {\n  .value : = 1\n}\n$helper:\n  ret\nend\n");
    EXPECT_FALSE(build().success);

    write("assets/vm/code.oasm", "#settings {\n  .value : = 1\n}\n$helper:\n  ret\nend\n");
    EXPECT_TRUE(build().success);
  }

  TEST_F(project_program_tests, a_hook_that_stops_the_device_is_warned_about) {
    /// no ret of its own is fine: the compiler ends every block with one
    write("assets/vm/a.oasm", "$pre_load:\n  set r1, 1\nend\n");
    write("assets/vm/b.oasm", "$post_load:\n  stopdev\n  ret\nend\n");

    const project_program_result lenient = build();
    EXPECT_TRUE(lenient.success);
    ASSERT_EQ(lenient.warnings.size(), 1);
    EXPECT_TRUE(holds(lenient.warnings[0], "'post_load' in 'assets/vm/b.oasm' stops the device, so nothing after it runs"));

    const project_program_result strict = build({ .project_load_path = "game/game.toml", .link = { .strict = true } });
    EXPECT_FALSE(strict.success);
    EXPECT_TRUE(strict.bytes.empty());
  }

  TEST_F(project_program_tests, the_entry_runs_the_hooks_on_either_side_of_the_load) {
    write("assets/vm/a.oasm", marking_hook("pre_load", 1));
    write("assets/vm/b.oasm", marking_hook("post_load", 2));

    /// the hook left 9 in the flag register; the load must not see it
    const boot_trace trace = build_and_run();
    EXPECT_EQ(trace.steps, (ostd::vector<std::string>{ "mark 1", "load 'game/game.toml' with flag 0", "loaded", "mark 2" }));
    EXPECT_EQ(trace.frames_until_loaded, 0);
  }

  TEST_F(project_program_tests, the_entry_with_no_hooks_loads_the_project_and_stops) {
    const boot_trace trace = build_and_run();
    EXPECT_EQ(trace.steps, (ostd::vector<std::string>{ "load 'game/game.toml' with flag 0", "loaded" }));
  }

  TEST_F(project_program_tests, a_load_that_does_not_start_is_an_error_and_not_a_wait) {
    write("assets/vm/a.oasm", marking_hook("pre_load", 1));
    write("assets/vm/b.oasm", marking_hook("post_load", 2));

    const boot_trace trace = build_and_run({ .refuse_load = true });
    EXPECT_EQ(trace.steps, (ostd::vector<std::string>{
                             "mark 1",
                             "load 'game/game.toml' with flag 0",
                             "log 'the project did not start loading: game/game.toml' at level 4",
                           }));
  }

}  // namespace other
