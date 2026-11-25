/**
 * \file tests/vm/command_declaration_tests.cpp
 **/
#include <filesystem>
#include <fstream>

#include "vm/builtin_command_devices.hpp"
#include "vm/command_bus.hpp"
#include "vm/command_files/asset_footprint.hpp"
#include "vm/command_files/ocmd_debug_info.hpp"
#include "vm/command_files/ocmd_headers.hpp"
#include "vm/command_files/ocmd_toolchain.hpp"
#include "vm/command_language/command_declarations.hpp"
#include "vm/decompiler.hpp"
#include "vm/diagnostics/diagnostic_engine.hpp"
#include "vm/diagnostics/diagnostic_sink.hpp"
#include "vm/other_device.hpp"
#include "vm/vm.hpp"

#include "other_test.hpp"

namespace other {
  namespace {

    /// a small set of kinds with one of everything a struct can say
    constexpr std::string_view kKinds = R"(
enum shade { light, dark }

struct paint(shade, coats) {
  shade shade
  int   coats = 1
  float gloss = 0.5
  text  label = ""
  bool  sealed = false
  int[] mix = []
  name  maker = nobody
  any   note = 0
}

struct wall {
  paint finish
  float height = 2.5
}

struct door names wall {
  int width = 90
}

struct room {
  wall[]  walls
  door[]  doors
  paint[] paints
  graph walls
}
)";

    class collecting_sink : public diagnostic_sink {
     public:
      void handle(diagnostic& diag) override {
        found.push_back(diag);
      }

      ostd::vector<diagnostic> found;
    };

  }  // namespace

  class command_declaration_tests : public other_test {
   protected:
    struct checked {
      /// the text could be read at all
      bool read = false;
      bool passed = false;
      ostd::vector<diagnostic> errors;
      command_check_counts counts;

      bool says(const std::string_view words) const {
        return std::ranges::any_of(errors, [&](const diagnostic& error) { return error.final_message.contains(words); });
      }
    };

    void SetUp() override {
      other_test::SetUp();
      const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
      sandbox = std::filesystem::temp_directory_path() / "other-declaration-tests" / info->name();
      std::filesystem::remove_all(sandbox);
      std::filesystem::create_directories(sandbox);
    }

    void TearDown() override {
      std::error_code ec;
      std::filesystem::remove_all(sandbox, ec);
      other_test::TearDown();
    }

    void write_file(const filepath& path, const std::string_view text) const {
      std::filesystem::create_directories(path.parent_path());
      std::ofstream file(path, std::ios::binary | std::ios::trunc);
      file << text;
    }

    /// text as a file of declarations in a build that also holds kinds. a line and a column
    ///  of an error are text's own
    static checked check(const std::string_view text, const std::string_view kinds = kKinds) {
      diagnostic_engine diag;
      collecting_sink sink;
      const scoped_diagnostic_sink sink_scope(&diag, "collecting-sink", &sink);
      ocmd_toolchain toolchain;

      checked result;
      opt<command_file> kinds_file = toolchain.parse_command_source("kinds.oass", kinds, &diag, true);
      opt<command_file> file = toolchain.parse_command_source("test.oass", text, &diag, true);
      if (kinds_file.has_value() && file.has_value()) {
        result.read = true;
        const command_unit units[] = { { .name = "kinds.oass", .file = std::move(*kinds_file) }, { .name = "test.oass", .file = std::move(*file) } };
        result.passed = toolchain.check_command_units(units, &diag, &result.counts);
      }
      result.errors = sink.found;
      return result;
    }

    struct built {
      /// the OASM made from each unit, in the units' order; empty when the build has errors
      ostd::vector<oasm_source> sources;
      /// the asset blocks of all of it, read back by the assembler
      ostd::vector<asset_node> assets;
      ostd::vector<diagnostic> errors;
    };

    /// lowers units and assembles what comes of it
    static built build(std::span<const command_unit> units) {
      command_bus bus;
      register_builtin_command_devices(bus);
      diagnostic_engine diag;
      collecting_sink sink;
      const scoped_diagnostic_sink sink_scope(&diag, "collecting-sink", &sink);
      ocmd_toolchain toolchain;

      built result;
      result.sources = toolchain.lower_command_units(bus, units, &diag);
      for (const ocmd_unit& unit : toolchain.compile_units(result.sources, &diag)) {
        result.assets.append_range(unit.program.assets);
      }
      for (const diagnostic& found : sink.found) {
        if (found.severity >= VM_DIAGNOSTIC_ERROR) {
          result.errors.push_back(found);
        }
      }
      return result;
    }

    /// text as a file of declarations in a build that also holds kKinds
    static built build(const std::string_view text) {
      diagnostic_engine diag;
      ocmd_toolchain toolchain;
      opt<command_file> kinds_file = toolchain.parse_command_source("kinds.oass", kKinds, &diag, true);
      opt<command_file> file = toolchain.parse_command_source("test.oass", text, &diag, true);
      if (!kinds_file.has_value() || !file.has_value()) {
        return {};
      }
      const command_unit units[] = { { .name = "kinds.oass", .file = std::move(*kinds_file) }, { .name = "test.oass", .file = std::move(*file) } };
      return build(units);
    }

    /// the linked program of units, whole; empty when the build has errors
    static ostd::vector<uint8_t> link(std::span<const command_unit> units, const ocmd_link_options options = {}) {
      command_bus bus;
      register_builtin_command_devices(bus);
      diagnostic_engine diag;
      ocmd_toolchain toolchain;
      const ostd::vector<oasm_source> sources = units.empty() ? ostd::vector<oasm_source>{} : toolchain.lower_command_units(bus, units, &diag);
      return sources.empty() ? ostd::vector<uint8_t>{} : toolchain.build_obin(bus, sources, &diag, options).bytes;
    }

    /// the same for a file and what it uses
    static ostd::vector<uint8_t> link(const filepath& file, const ocmd_link_options options = {}) {
      diagnostic_engine diag;
      return link(ocmd_toolchain{}.read_command_units(file, file.filename().string(), &diag), options);
    }

    static ocmd_file_header header_of(const std::span<const uint8_t> bytes) {
      ocmd_file_header header;
      std::memcpy(&header, bytes.data(), sizeof(header));
      return header;
    }

    /// the footprint of the asset of this kind and name in a linked program; none when it has none
    static ostd::vector<uint8_t> footprint_in(const std::span<const uint8_t> program, const std::string_view kind, const std::string_view name) {
      const opt<obin_assets> assets = obin_assets::decode(obin_asset_bytes(program));
      const obin_asset* found = assets.has_value() ? assets->find(asset_hash(kind), asset_hash(name)) : nullptr;
      return found != nullptr ? ostd::vector<uint8_t>{ found->footprint.begin(), found->footprint.end() } : ostd::vector<uint8_t>{};
    }

    static const asset_field* field_of(const asset_node& node, const std::string_view name) {
      const auto found = std::ranges::find(node.fields, name, &asset_field::name);
      return found != node.fields.end() ? &*found : nullptr;
    }

    static size_t parts_of(const asset_node& node, const std::string_view kind) {
      size_t count = 0;
      for (const asset_node& part : node.parts) {
        count += part.kind == kind ? 1 : 0;
        count += parts_of(part, kind);
      }
      return count;
    }

    /// builds the file as the entry of a program, runs it, and gives back what is in rf at the end
    opt<uint64_t> run(const filepath& script) const {
      command_bus bus;
      register_builtin_command_devices(bus);
      diagnostic_engine diag;
      ocmd_toolchain toolchain;
      const ostd::vector<command_unit> units = toolchain.read_command_units(script, script.filename().string(), &diag);
      const ostd::vector<oasm_source> sources = units.empty() ? ostd::vector<oasm_source>{} : toolchain.lower_command_units(bus, units, &diag);
      if (sources.empty()) {
        return std::nullopt;
      }
      const obin_build_result linked = toolchain.build_obin(bus, sources, &diag);
      if (!linked.success) {
        return std::nullopt;
      }

      other_command_device device;
      vm::initialize_device(&device);
      vm::load_program_from_bytes(&device, linked.bytes);
      for (size_t step = 0; step < 4096 && !vm::has_flag(&device, other_command_device::STOPPED); ++step) {
        vm::step(&device);
      }
      const bool stopped = vm::has_flag(&device, other_command_device::STOPPED);
      const uint64_t result = device.read_register_as_u64(vm_register_idx::VM_RF);
      vm::shutdown_device(&device);
      return stopped ? opt<uint64_t>{ result } : std::nullopt;
    }

    static size_t parts_of(const command_declaration& declaration, const std::string_view kind) {
      size_t count = 0;
      for (const command_declaration& part : declaration.parts) {
        count += part.kind == kind ? 1 : 0;
        count += parts_of(part, kind);
      }
      return count;
    }

    filepath sandbox;
  };

  TEST_F(command_declaration_tests, a_declaration_that_fits_its_struct_passes) {
    const checked result = check(R"(
room hall {
  paint white { shade = light  coats = 2  gloss = 1  label = "eggshell"  mix = [1, 2, 3]  maker = someone  note = [1.5, "x"] }
  paint slate dark
  wall north { finish = white }
  wall south { finish = slate  height = 3 }
  door north { width = 80 }
  north |> south
}
)");
    ASSERT_TRUE(result.read);
    EXPECT_TRUE(result.passed);
    EXPECT_TRUE(result.errors.empty());
    EXPECT_EQ(result.counts.enums, 1);
    EXPECT_EQ(result.counts.structs, 4);
    /// the room and the six things in it
    EXPECT_EQ(result.counts.declarations, 6);
  }

  TEST_F(command_declaration_tests, an_error_is_at_the_line_and_column_of_what_is_wrong) {
    const checked result = check("room hall {\n  paint white light\n  wall north { finish = whit }\n}\n");
    ASSERT_TRUE(result.read);
    EXPECT_FALSE(result.passed);
    ASSERT_EQ(result.errors.size(), 1);
    EXPECT_EQ(result.errors[0].error_code, SEMA_UNKNOWN_NAME);
    EXPECT_EQ(result.errors[0].phase, VM_PHASE_SEMA);
    EXPECT_EQ(result.errors[0].span.start.line, 3);
    EXPECT_EQ(result.errors[0].span.start.column, 25);
    EXPECT_TRUE(result.says("no paint 'whit' is declared here"));
  }

  TEST_F(command_declaration_tests, a_kind_has_to_be_a_struct) {
    const checked result = check("carpet red { pile = 3 }\n");
    EXPECT_FALSE(result.passed);
    EXPECT_TRUE(result.says("'carpet' is not a struct"));
    /// an enum is a type and not a kind of thing
    EXPECT_TRUE(check("shade grey { }\n").says("'shade' is not a struct"));
  }

  TEST_F(command_declaration_tests, a_field_has_to_be_one_the_struct_has) {
    const checked result = check("paint white { shade = light  smell = 3 }\n");
    EXPECT_FALSE(result.passed);
    EXPECT_TRUE(result.says("a paint has no field 'smell'"));
    EXPECT_TRUE(check("paint white { shade = light  shade = dark }\n").says("is given its 'shade' twice"));
    /// a list of a struct's type is the declarations inside, and is given no value
    EXPECT_TRUE(check("room hall { walls = [] }\n").says("is not given a value"));
  }

  TEST_F(command_declaration_tests, a_field_with_no_default_has_to_be_given) {
    const checked result = check("paint white { coats = 2 }\n");
    EXPECT_FALSE(result.passed);
    EXPECT_TRUE(result.says("does not say its 'shade'"));
    EXPECT_TRUE(check("room hall { paint white light\n wall north { } }\n").says("does not say its 'finish'"));
  }

  TEST_F(command_declaration_tests, a_value_has_to_be_of_the_fields_type) {
    EXPECT_TRUE(check("paint p { shade = light  coats = \"two\" }\n").says("is a whole number; this is a text"));
    EXPECT_TRUE(check("paint p { shade = light  coats = 1.5 }\n").says("is a whole number; this is a number with a point"));
    EXPECT_TRUE(check("paint p { shade = light  gloss = shiny }\n").says("is a number; this is a name"));
    EXPECT_TRUE(check("paint p { shade = light  label = eggshell }\n").says("goes in quotes"));
    EXPECT_TRUE(check("paint p { shade = light  maker = \"someone\" }\n").says("written without quotes"));
    EXPECT_TRUE(check("paint p { shade = light  sealed = 1 }\n").says("is true or false"));
    EXPECT_TRUE(check("paint p { shade = light  mix = 3 }\n").says("is a list of int"));
    EXPECT_TRUE(check("paint p { shade = light  mix = [1, two] }\n").says("is a whole number; this is a name"));
    /// a whole number is a number, and a number below zero is one
    EXPECT_TRUE(check("paint p { shade = light  gloss = -2  coats = -1 }\n").passed);
  }

  TEST_F(command_declaration_tests, a_name_has_to_be_a_value_of_the_enum) {
    const checked result = check("paint p { shade = grey }\n");
    EXPECT_FALSE(result.passed);
    ASSERT_EQ(result.errors.size(), 1);
    EXPECT_EQ(result.errors[0].error_code, SEMA_BAD_VALUE);
    EXPECT_TRUE(result.says("is one of light, dark; this is a name, 'grey'"));
    EXPECT_TRUE(check("paint p { shade = \"light\" }\n").says("is one of light, dark"));
  }

  TEST_F(command_declaration_tests, a_reference_names_a_declaration_of_that_kind) {
    /// a wall of that name is there, and a wall is not a paint
    EXPECT_TRUE(check("room hall { paint white light\n wall north { finish = white }\n wall south { finish = north } }\n").says("no paint 'north' is declared here"));
    EXPECT_TRUE(check("room hall { paint white light\n wall north { finish = 3 } }\n").says("the name of a paint"));
    /// what is declared outside everything can be named from inside
    EXPECT_TRUE(check("paint white light\nroom hall { wall north { finish = white } }\n").passed);
    /// and what is inside another declaration cannot be named from outside it
    EXPECT_FALSE(check("room hall { paint white light }\nroom shed { wall north { finish = white } }\n").passed);
  }

  TEST_F(command_declaration_tests, a_declaration_named_after_another_needs_it_to_be_there) {
    EXPECT_TRUE(check("room hall { paint white light\n wall north { finish = white }\n door north { } }\n").passed);
    EXPECT_TRUE(check("room hall { paint white light\n wall north { finish = white }\n door east { } }\n").says("'east' is not a wall declared around this door"));
    EXPECT_TRUE(check("room hall { door { } }\n").says("is named after the wall it is about"));
  }

  TEST_F(command_declaration_tests, a_short_form_gives_its_fields_in_order) {
    EXPECT_TRUE(check("paint white light 3\n").passed);
    EXPECT_TRUE(check("paint white light\n").passed);
    const checked many = check("paint white light 3 0.5\n");
    EXPECT_TRUE(many.says("takes 2 value(s), shade coats, and this gives 3"));
    ASSERT_EQ(many.errors.size(), 1);
    /// at the value there is no place for
    EXPECT_EQ(many.errors[0].span.start.column, 21);
    EXPECT_TRUE(check("paint white 3 light\n").says("is one of light, dark"));
    EXPECT_TRUE(check("paint white\n").says("does not say its 'shade'"));
    EXPECT_TRUE(check("room hall { paint white light\n wall north white }\n").says("a wall has no short form"));
  }

  TEST_F(command_declaration_tests, edges_join_what_is_in_the_graph_and_make_no_ring) {
    constexpr std::string_view kWalls = "room hall { paint white light\n wall a { finish = white }\n wall b { finish = white }\n wall c { finish = white }\n";
    EXPECT_TRUE(check(std::format("{} a |> b |> c\n}}\n", kWalls)).passed);
    EXPECT_TRUE(check(std::format("{} a |> d\n}}\n", kWalls)).says("'d' is not a wall of room 'hall'"));
    /// a paint is in the room and is not one of the things its graph joins
    EXPECT_TRUE(check(std::format("{} a |> white\n}}\n", kWalls)).says("'white' is not a wall"));

    const checked ring = check(std::format("{} a |> b\n b |> c\n c |> a\n}}\n", kWalls));
    EXPECT_TRUE(ring.says("in a ring"));
    ASSERT_EQ(ring.errors.size(), 1);
    /// the edge that closes it
    EXPECT_EQ(ring.errors[0].span.start.line, 7);
    EXPECT_TRUE(check(std::format("{} a |> a\n}}\n", kWalls)).says("in a ring"));

    EXPECT_TRUE(check("paint white { shade = light\n a |> b }\n").says("a paint is not a graph"));
  }

  TEST_F(command_declaration_tests, two_declarations_of_one_kind_cannot_share_a_name) {
    EXPECT_TRUE(check("paint white light\npaint white dark\n").says("paint 'white' is declared more than once"));
    EXPECT_TRUE(check("room hall { paint white light\n paint white dark }\n").says("has two of paint 'white'"));
    /// a wall and a paint are different kinds
    EXPECT_TRUE(check("room hall { paint north light\n wall north { finish = north } }\n").passed);
    EXPECT_TRUE(check("room { }\n").says("needs a name"));
  }

  TEST_F(command_declaration_tests, a_struct_and_an_enum_have_to_make_sense) {
    const auto wrong = [](const std::string_view kinds, const std::string_view words) -> bool {
      const checked result = check("", kinds);
      return result.read && !result.passed && result.says(words);
    };
    EXPECT_TRUE(wrong("struct a { colour tint }\n", "'colour' is not a type"));
    EXPECT_TRUE(wrong("struct a { int x\n int x }\n", "two fields named 'x'"));
    EXPECT_TRUE(wrong("struct a { int x = \"one\" }\n", "is a whole number; this is a text"));
    EXPECT_TRUE(wrong("struct a { int x }\nstruct a { int y }\n", "'a' is declared more than once"));
    EXPECT_TRUE(wrong("struct int { int x }\n", "is a type the language has already"));
    EXPECT_TRUE(wrong("struct a(y) { int x }\n", "which is not one of its fields"));
    EXPECT_TRUE(wrong("struct a names b { int x }\n", "which is not a struct"));
    EXPECT_TRUE(wrong("struct a { int[] xs\n graph xs }\n", "is not one of its lists of a struct"));
    EXPECT_TRUE(wrong("struct b { int x }\nstruct a { b other = thing }\n", "names a declaration"));
    EXPECT_TRUE(wrong("enum e { }\n", "has no values"));
    EXPECT_TRUE(wrong("enum e { one, two, one }\n", "has 'one' twice"));
    EXPECT_TRUE(wrong("enum e { one }\nstruct e { int x }\n", "'e' is declared more than once"));
  }

  TEST_F(command_declaration_tests, a_file_of_declarations_holds_nothing_that_runs) {
    diagnostic_engine diag;
    ocmd_toolchain toolchain;
    /// a line of words reads as a short form there, and is held to a struct like any other
    EXPECT_TRUE(check("core log \"hello\" 2\n").says("'core' is not a struct"));
    EXPECT_FALSE(toolchain.parse_command_source("test.oass", "core.log(\"hello\", 2)\n", &diag, true).has_value());
    EXPECT_FALSE(toolchain.parse_command_source("test.oass", "fn f() { }\n", &diag, true).has_value());
    EXPECT_FALSE(toolchain.parse_command_source("test.oass", "let x = 5\n", &diag, true).has_value());
    EXPECT_TRUE(toolchain.parse_command_source("test.oass", "use \"kinds.oass\"\npaint white light\n", &diag, true).has_value());
  }

  TEST_F(command_declaration_tests, declarations_stand_between_the_statements_of_a_script) {
    diagnostic_engine diag;
    ocmd_toolchain toolchain;
    opt<command_file> file =
      toolchain.parse_command_source("test.ocmd", std::format("{}core log \"a\" 2\npaint white {{ shade = light }}\nlet slate = paint {{ shade = dark }}\ncore log \"b\" 2\n", kKinds), &diag);
    ASSERT_TRUE(file.has_value());
    EXPECT_EQ(file->structs.size(), 4);
    EXPECT_EQ(file->declarations.size(), 1);
    ASSERT_EQ(file->statements.size(), 3);
    ASSERT_EQ(file->statements[1].declared.size(), 1);
    EXPECT_EQ(file->statements[1].name, "slate");
    EXPECT_TRUE(file->statements[1].declared[0].name.empty());

    /// one bound to a variable is known by the variable and needs no name of its own
    const command_unit unit = { .name = "test.ocmd", .file = std::move(*file) };
    command_check_counts counts;
    EXPECT_TRUE(toolchain.check_command_units(std::span{ &unit, 1 }, &diag, &counts));
    EXPECT_EQ(counts.declarations, 2);

    /// in a script a line of words is a command, so a short form is not read as a declaration
    opt<command_file> words = toolchain.parse_command_source("test.ocmd", "paint white light\n", &diag);
    ASSERT_TRUE(words.has_value());
    EXPECT_TRUE(words->declarations.empty());
    EXPECT_EQ(words->statements.size(), 1);

    EXPECT_FALSE(toolchain.parse_command_source("test.ocmd", "fn f() { let p = paint { shade = dark } }\n", &diag).has_value());
    EXPECT_FALSE(toolchain.parse_command_source("test.ocmd", "let half = 0.5\n", &diag).has_value());
  }

  TEST_F(command_declaration_tests, a_variable_given_the_path_of_a_declarations_file_is_given_what_it_declares) {
    write_file(sandbox / "kinds.any", kKinds);
    /// the ending plays no part: the file is brought in for what it begins with
    write_file(sandbox / "paints" / "white.paint", "# the hall's\nuse \"../kinds.any\"\npaint { shade = light }\n");
    write_file(sandbox / "settings.toml", "[window]\nwidth = 3\n");
    write_file(sandbox / "main.ocmd", "let white = \"paints/white.paint\"\nlet settings = \"settings.toml\"\nlet nowhere = \"paints/none.paint\"\ncore log \"hello\" 2\n");

    diagnostic_engine diag;
    ocmd_toolchain toolchain;
    const ostd::vector<command_unit> units = toolchain.read_command_units(sandbox / "main.ocmd", "main.ocmd", &diag);
    ASSERT_EQ(units.size(), 3);
    EXPECT_EQ(units[0].name, "main.ocmd");
    EXPECT_EQ(units[1].name, "paints/white.paint");
    EXPECT_TRUE(units[1].included);
    EXPECT_EQ(units[2].name, "paints/../kinds.any");
    EXPECT_FALSE(units[2].included);

    ASSERT_EQ(units[0].file.statements.size(), 4);
    EXPECT_EQ(units[0].file.statements[0].included, "paints/white.paint");
    /// a path to anything else, or to nothing, is a text as it was
    EXPECT_TRUE(units[0].file.statements[1].included.empty());
    EXPECT_TRUE(units[0].file.statements[2].included.empty());

    command_check_counts counts;
    EXPECT_TRUE(toolchain.check_command_units(units, &diag, &counts));
    EXPECT_EQ(counts.declarations, 1);

    /// and the build that holds them is made: the asset is a block of the file it is written in
    const built made = build(units);
    ASSERT_EQ(made.sources.size(), 3);
    EXPECT_FALSE(made.sources[0].text.contains("%"));
    EXPECT_TRUE(made.sources[1].text.contains("%white : paint\n"));
    ASSERT_EQ(made.assets.size(), 1);
    EXPECT_EQ(made.assets[0].name, "white");
  }

  TEST_F(command_declaration_tests, a_text_that_could_not_be_a_path_is_not_looked_for) {
    /// a text is anything a script says: these are read as texts and nothing is looked for
    write_file(sandbox / "main.ocmd", "let odd = \"a<b>|c?*\"\nlet name = \"\xC3\x8D\xC3\xB1igo\"\nlet long = \"" + std::string(2000, 'x') + "\"\nlet none = \"\"\n");

    diagnostic_engine diag;
    ocmd_toolchain toolchain;
    const ostd::vector<command_unit> units = toolchain.read_command_units(sandbox / "main.ocmd", "main.ocmd", &diag);
    ASSERT_EQ(units.size(), 1);
    ASSERT_EQ(units[0].file.statements.size(), 4);
    for (const command_stmt& stmt : units[0].file.statements) {
      EXPECT_TRUE(stmt.included.empty());
    }
  }

  TEST_F(command_declaration_tests, a_file_given_to_a_variable_declares_one_thing) {
    write_file(sandbox / "kinds.any", kKinds);
    write_file(sandbox / "two.paints", "use \"kinds.any\"\npaint white { shade = light }\npaint slate dark\n");
    write_file(sandbox / "main.ocmd", "let both = \"two.paints\"\n");

    diagnostic_engine diag;
    collecting_sink sink;
    const scoped_diagnostic_sink sink_scope(&diag, "collecting-sink", &sink);
    ocmd_toolchain toolchain;
    const ostd::vector<command_unit> units = toolchain.read_command_units(sandbox / "main.ocmd", "main.ocmd", &diag);
    ASSERT_EQ(units.size(), 3);
    EXPECT_FALSE(toolchain.check_command_units(units, &diag));
    ASSERT_EQ(sink.found.size(), 1);
    EXPECT_TRUE(sink.found[0].final_message.contains("which declares 2 things"));
    EXPECT_EQ(sink.found[0].span.start.line, 1);

    /// brought in with 'use', a file of several is what it is
    write_file(sandbox / "used.ocmd", "use \"two.paints\"\n");
    const ostd::vector<command_unit> used = toolchain.read_command_units(sandbox / "used.ocmd", "used.ocmd", &diag);
    ASSERT_EQ(used.size(), 3);
    EXPECT_TRUE(toolchain.check_command_units(used, &diag));
  }

  TEST_F(command_declaration_tests, an_asset_block_is_the_whole_declaration_in_its_structs_order) {
    const built made = build(R"(
paint slate dark
room hall {
  wall north { height = 3  finish = white }
  paint white { gloss = 1  shade = light  mix = [1, 2] }
  wall south { finish = slate }
  door north { }
  north |> south
}
)");
    EXPECT_TRUE(made.errors.empty());
    ASSERT_EQ(made.assets.size(), 2);

    /// every field of the struct, in the struct's order; what was not said is the default
    const asset_node& slate = made.assets[0];
    EXPECT_EQ(slate.kind, "paint");
    EXPECT_EQ(slate.name, "slate");
    const asset_node expected_slate = {
      .kind = "paint",
      .name = "slate",
      .fields = {
        { .name = "shade", .value = { .kind = ASSET_VALUE_WORD, .text = "dark" } },
        { .name = "coats", .value = { .kind = ASSET_VALUE_INTEGER, .integer = 1 } },
        { .name = "gloss", .value = { .kind = ASSET_VALUE_DECIMAL, .decimal = 0.5 } },
        { .name = "label", .value = { .kind = ASSET_VALUE_TEXT } },
        { .name = "sealed", .value = { .kind = ASSET_VALUE_BOOL } },
        { .name = "mix", .value = { .kind = ASSET_VALUE_LIST } },
        { .name = "maker", .value = { .kind = ASSET_VALUE_WORD, .text = "nobody" } },
        { .name = "note", .value = { .kind = ASSET_VALUE_INTEGER } },
      },
    };
    EXPECT_EQ(slate, expected_slate);

    /// the parts by the struct's lists, whatever order they were written in
    const asset_node& hall = made.assets[1];
    EXPECT_TRUE(hall.fields.empty());
    ASSERT_EQ(hall.parts.size(), 4);
    EXPECT_EQ(hall.parts[0].kind, "wall");
    EXPECT_EQ(hall.parts[0].name, "north");
    EXPECT_EQ(hall.parts[1].name, "south");
    EXPECT_EQ(hall.parts[2].kind, "door");
    EXPECT_EQ(hall.parts[2].name, "north");
    EXPECT_EQ(hall.parts[3].kind, "paint");

    /// a paint of this room is a part of the asset; one declared outside is another asset
    const asset_field* inside = field_of(hall.parts[0], "finish");
    const asset_field* outside = field_of(hall.parts[1], "finish");
    ASSERT_TRUE(inside != nullptr && outside != nullptr);
    EXPECT_EQ(inside->how, ASSET_FIELD_REF);
    EXPECT_EQ(inside->value.text, "white");
    EXPECT_EQ(outside->how, ASSET_FIELD_USE);
    EXPECT_EQ(outside->value.text, "slate");
    EXPECT_EQ(hall.parts[0].fields[0].name, "finish");

    /// a whole number given to a float is written as one, so the point being typed changes nothing
    const asset_field* height = field_of(hall.parts[0], "height");
    const asset_field* gloss = field_of(hall.parts[3], "gloss");
    ASSERT_TRUE(height != nullptr && gloss != nullptr);
    EXPECT_EQ(height->value.kind, ASSET_VALUE_DECIMAL);
    EXPECT_DOUBLE_EQ(height->value.decimal, 3.0);
    EXPECT_EQ(gloss->value.kind, ASSET_VALUE_DECIMAL);
    EXPECT_EQ(field_of(hall.parts[3], "mix")->value.items.size(), 2);
    EXPECT_EQ(field_of(hall.parts[2], "width")->value.integer, 90);

    ASSERT_EQ(hall.edges.size(), 1);
    EXPECT_EQ(hall.edges[0].from, "north");
    EXPECT_EQ(hall.edges[0].to, "south");

    /// what was written with a point and what was written without are one asset
    const built again = build("paint slate { coats = 1  shade = dark  gloss = 0.5 }\nroom hall {\n paint white light 1\n wall south { finish = slate  height = 2.5 }\n wall north { finish = white  height = 3.0 }\n }\n");
    ASSERT_EQ(again.assets.size(), 2);
    EXPECT_EQ(again.assets[0], slate);
    EXPECT_NE(again.assets[1], hall);
  }

  TEST_F(command_declaration_tests, what_the_assembler_reads_back_is_what_was_lowered) {
    const built made = build("paint white { shade = light  label = \"egg\\\"shell\\n\"  note = [1.5, [dark, true], -3] }\n");
    EXPECT_TRUE(made.errors.empty());
    ASSERT_EQ(made.sources.size(), 2);
    ASSERT_EQ(made.assets.size(), 1);
    EXPECT_EQ(field_of(made.assets[0], "label")->value.text, "egg\"shell\n");
    /// the block written from the asset the assembler read is the block it was given
    EXPECT_TRUE(made.sources[1].text.ends_with(asset_block_text(made.assets[0])));
    EXPECT_TRUE(made.sources[1].text.contains("  set note, [1.5, [dark, true], -3]\n"));
  }

  TEST_F(command_declaration_tests, an_asset_is_called_by_its_name_its_variable_or_its_file) {
    write_file(sandbox / "kinds.any", kKinds);
    write_file(sandbox / "the hall's.paint", "use \"kinds.any\"\npaint { shade = light }\n");
    write_file(sandbox / "named.paint", "use \"kinds.any\"\npaint eggshell { shade = light }\n");
    write_file(sandbox / "main.ocmd", "use \"kinds.any\"\nlet slate = paint { shade = dark }\nlet other = paint ivory { shade = light }\npaint chalk { shade = light }\nlet hall = \"the hall's.paint\"\nlet shell = \"named.paint\"\n");

    diagnostic_engine diag;
    ocmd_toolchain toolchain;
    const ostd::vector<command_unit> units = toolchain.read_command_units(sandbox / "main.ocmd", "main.ocmd", &diag);
    ASSERT_EQ(units.size(), 4);
    const built made = build(units);
    EXPECT_TRUE(made.errors.empty());
    ostd::vector<std::string> names;
    for (const asset_node& asset : made.assets) {
      names.push_back(asset.name);
    }
    /// a name that the assembler could not read as a word is made into one
    EXPECT_EQ(names, (ostd::vector<std::string>{ "chalk", "slate", "ivory", "the_hall_s", "eggshell" }));

    /// two things of one kind cannot be called the same, however they came by the name
    write_file(sandbox / "twice.ocmd", "use \"kinds.any\"\npaint slate { shade = light }\nlet slate = paint { shade = dark }\n");
    const built twice = build(toolchain.read_command_units(sandbox / "twice.ocmd", "twice.ocmd", &diag));
    EXPECT_TRUE(twice.sources.empty());
    ASSERT_EQ(twice.errors.size(), 1);
    EXPECT_TRUE(twice.errors[0].final_message.contains("already has a paint called 'slate'"));
    EXPECT_EQ(twice.errors[0].span.start.line, 3);
  }

  TEST_F(command_declaration_tests, a_variable_given_an_asset_stands_for_the_assets_name) {
    /// a text is its length above bit 32 and its address below it
    write_file(sandbox / "kinds.any", kKinds);
    write_file(sandbox / "inline.ocmd", "use \"kinds.any\"\nfn out(value) { return value }\nlet slate = paint { shade = dark }\nout(slate)\n");
    const opt<uint64_t> by_variable = run(sandbox / "inline.ocmd");
    ASSERT_TRUE(by_variable.has_value());
    EXPECT_EQ(*by_variable >> 32, 5);

    write_file(sandbox / "white.paint", "use \"kinds.any\"\npaint eggshell { shade = light }\n");
    write_file(sandbox / "file.ocmd", "fn out(value) { return value }\nlet white = \"white.paint\"\nout(white)\n");
    const opt<uint64_t> by_file = run(sandbox / "file.ocmd");
    ASSERT_TRUE(by_file.has_value());
    EXPECT_EQ(*by_file >> 32, 8);

    /// a path to anything else is the text it was
    write_file(sandbox / "text.ocmd", "fn out(value) { return value }\nlet white = \"nothing.paint\"\nout(white)\n");
    const opt<uint64_t> as_text = run(sandbox / "text.ocmd");
    ASSERT_TRUE(as_text.has_value());
    EXPECT_EQ(*as_text >> 32, 13);
  }

  TEST_F(command_declaration_tests, a_file_of_declarations_can_share_a_scripts_name) {
    write_file(sandbox / "main.things", std::format("{}paint white light\n", kKinds));
    write_file(sandbox / "main.ocmd", "use \"main.things\"\ncore log \"hello\" 2\n");
    diagnostic_engine diag;
    ocmd_toolchain toolchain;
    const built made = build(toolchain.read_command_units(sandbox / "main.ocmd", "main.ocmd", &diag));
    EXPECT_TRUE(made.errors.empty());
    EXPECT_EQ(made.sources.size(), 2);
    EXPECT_EQ(made.assets.size(), 1);
  }

  TEST_F(command_declaration_tests, a_field_whose_default_is_null_need_not_be_said_and_is_then_not_written) {
    constexpr std::string_view kLabels = "struct shelf { int width = 40 }\nstruct label {\n  text words = null\n  float size = null\n  int[] marks = null\n  shelf sits = null\n  asset picture = null\n  asset icon\n  int copies = 1\n}\n";
    write_file(sandbox / "labels.things",
               std::format("{}shelf low {{ }}\nlabel plain {{ icon = \"icons/plain.png\" }}\nlabel full {{ words = \"full\"  size = 2  marks = [1]  sits = low  picture = \"p.png\"  icon = \"i.png\"  copies = 3 }}\nlabel unsaid {{ icon = \"i.png\"  words = null  size = null  copies = null }}\n", kLabels));
    diagnostic_engine diag;
    const built made = build(ocmd_toolchain{}.read_command_units(sandbox / "labels.things", "labels.things", &diag));
    EXPECT_TRUE(made.errors.empty());
    ASSERT_EQ(made.assets.size(), 4);

    /// what is null is not in the block at all; a field of the plain type 'asset' is another
    ///  asset by where its file is, which is a 'use' with a path
    const asset_node& plain = made.assets[1];
    ASSERT_EQ(plain.fields.size(), 2);
    EXPECT_EQ(plain.fields[0].name, "icon");
    EXPECT_EQ(plain.fields[0].how, ASSET_FIELD_USE);
    EXPECT_EQ(plain.fields[0].value.kind, ASSET_VALUE_TEXT);
    EXPECT_EQ(plain.fields[0].value.text, "icons/plain.png");
    EXPECT_EQ(plain.fields[1].name, "copies");
    ASSERT_EQ(made.sources.size(), 1);
    EXPECT_TRUE(made.sources[0].text.contains("%plain : label\n  use icon, \"icons/plain.png\"\n  set copies, 1\nend\n"));

    const asset_node& full = made.assets[2];
    ASSERT_EQ(full.fields.size(), 7);
    EXPECT_EQ(field_of(full, "words")->value.text, "full");
    EXPECT_EQ(field_of(full, "size")->value.kind, ASSET_VALUE_DECIMAL);
    EXPECT_EQ(field_of(full, "marks")->value.items.size(), 1);
    /// a field of a struct's type can be left unsaid too; said, it names the other asset
    EXPECT_EQ(field_of(full, "sits")->how, ASSET_FIELD_USE);
    EXPECT_EQ(field_of(full, "sits")->value.kind, ASSET_VALUE_WORD);
    EXPECT_EQ(field_of(full, "picture")->how, ASSET_FIELD_USE);
    EXPECT_EQ(field_of(full, "copies")->value.integer, 3);

    /// given null is not given: what the struct says the field is, is what it is
    const asset_node& unsaid = made.assets[3];
    ASSERT_EQ(unsaid.fields.size(), 2);
    EXPECT_EQ(field_of(unsaid, "copies")->value.integer, 1);

    /// so null does not make a field that has to be said one that need not be
    EXPECT_TRUE(check("label none { }\n", kLabels).says("does not say its 'icon'"));
    EXPECT_TRUE(check("label none { icon = null }\n", kLabels).says("does not say its 'icon'"));
    EXPECT_TRUE(check("label bad { icon = plain }\n", kLabels).says("by the path of its file in quotes"));
    EXPECT_FALSE(check("label bad { icon = \"\" }\n", kLabels).passed);
    EXPECT_TRUE(check("label fine { icon = \"i.png\"  sits = null }\n", kLabels).passed);
    /// a list has no fields to leave unsaid
    EXPECT_FALSE(check("label bad { icon = \"i.png\"  marks = [1, null] }\n", kLabels).read);
    EXPECT_FALSE(check("", "struct s { int[] xs = [null] }\n").read);
  }

  TEST_F(command_declaration_tests, a_struct_cannot_have_two_lists_of_one_struct) {
    const checked result = check("", "struct nail { int length = 3 }\nstruct board {\n  nail[] left\n  nail[] right\n}\n");
    EXPECT_TRUE(result.read);
    EXPECT_FALSE(result.passed);
    EXPECT_TRUE(result.says("has two lists of nail"));
  }

  TEST_F(command_declaration_tests, the_default_pipelines_asset_block_is_the_pipeline) {
    diagnostic_engine diag;
    ocmd_toolchain toolchain;
    const ostd::vector<command_unit> units = toolchain.read_command_units("resources/editor-assets/default-pipeline.oass", "resources/editor-assets/default-pipeline.oass", &diag);
    ASSERT_EQ(units.size(), 2);
    const built made = build(units);
    for (const diagnostic& error : made.errors) {
      ADD_FAILURE() << error.span.start.line << ":" << error.span.start.column << ": " << error.final_message;
    }
    ASSERT_EQ(made.sources.size(), 2);
    ASSERT_EQ(made.assets.size(), 1);

    const asset_node& pipeline = made.assets[0];
    EXPECT_EQ(pipeline.kind, "pipeline");
    EXPECT_EQ(pipeline.name, "default-instancing");
    /// read back, it is the block that was written
    EXPECT_TRUE(made.sources[0].text.ends_with(asset_block_text(pipeline)));

    /// as many of everything as the declaration has
    const command_declaration& declared = units[0].file.declarations[0];
    for (const std::string_view kind : { "buffer", "texture", "shader", "binding", "material-layout", "lighting", "param", "slot", "pass", "read", "write", "bind", "set", "with" }) {
      EXPECT_EQ(parts_of(pipeline, kind), parts_of(declared, kind)) << kind;
    }
    EXPECT_EQ(parts_of(pipeline, "pass"), 14);
    EXPECT_EQ(pipeline.edges.size(), 5);

    /// what the declaration left to its struct is written out
    const auto part = [&](const std::string_view kind, const std::string_view name) -> const asset_node& {
      const auto found = std::ranges::find_if(pipeline.parts, [&](const asset_node& node) { return node.kind == kind && node.name == name; });
      EXPECT_TRUE(found != pipeline.parts.end()) << kind << " " << name;
      return found != pipeline.parts.end() ? *found : pipeline;
    };
    const asset_node& colour = part("texture", "color_texture");
    ASSERT_EQ(colour.fields.size(), 4);
    EXPECT_EQ(field_of(colour, "format")->value.text, "rgba16f");
    EXPECT_TRUE(field_of(colour, "size")->value.items.empty());
    EXPECT_EQ(field_of(colour, "seed")->value.kind, ASSET_VALUE_TEXT);
    EXPECT_EQ(field_of(part("texture", "voxel_texture"), "size")->value.items.size(), 3);
    EXPECT_EQ(field_of(part("pass", "geometry-pass"), "framebuffer")->value.integer, 1);
    EXPECT_EQ(field_of(part("pass", "voxelize-pass"), "framebuffer")->value.integer, 0);
    EXPECT_EQ(field_of(part("pass", "geometry-pass"), "clear")->value.items[0].text, "default");
    EXPECT_TRUE(field_of(part("pass", "grid-pass"), "clear")->value.items.empty());
    /// a pass's shader and the pipeline's display are parts of this asset
    EXPECT_EQ(field_of(part("pass", "shading-pass"), "shader")->how, ASSET_FIELD_REF);
    EXPECT_EQ(field_of(pipeline, "display")->how, ASSET_FIELD_REF);
    EXPECT_EQ(field_of(pipeline, "display")->value.text, "smaa_texture");
    /// a number of the tonemap pass, as it was written
    const asset_node& tonemap = part("pass", "tonemap-pass");
    const auto exposure = std::ranges::find_if(tonemap.parts, [](const asset_node& node) { return node.kind == "set" && node.name == "OE_exposure"; });
    ASSERT_TRUE(exposure != tonemap.parts.end());
    EXPECT_DOUBLE_EQ(field_of(*exposure, "value")->value.decimal, 0.6);
  }

  TEST_F(command_declaration_tests, a_linked_program_carries_its_assets_after_everything_else) {
    write_file(sandbox / "kinds.any", kKinds);
    constexpr std::string_view kScript = "use \"kinds.any\"\nfn out(value) {{ return value }}\npaint slate {{ shade = dark }}\nlet white = paint {{ shade = light  coats = {} }}\nout(white)\n";
    write_file(sandbox / "main.ocmd", std::format(kScript, 2));
    const ostd::vector<uint8_t> bytes = link(sandbox / "main.ocmd");
    ASSERT_FALSE(bytes.empty());

    const ocmd_file_header header = header_of(bytes);
    EXPECT_EQ(header.prog_header.has_debug_flag, 1);
    EXPECT_EQ(header.prog_header.has_assets_flag, 1);
    /// the program, its debug section, its assets, and nothing between or after them
    const std::span<const uint8_t> asset_bytes = obin_asset_bytes(bytes);
    EXPECT_EQ(sizeof(header) + obin_program_bytes(bytes).size() + obin_debug_bytes(bytes).size() + asset_bytes.size(), bytes.size());
    EXPECT_EQ(obin_program_bytes(bytes).size(), header.prog_header.program_size);
    EXPECT_EQ(asset_bytes.data() + asset_bytes.size(), bytes.data() + bytes.size());
    EXPECT_FALSE(ocmd_toolchain{}.check_obin(bytes).has_value());

    const opt<obin_assets> assets = obin_assets::decode(asset_bytes);
    ASSERT_TRUE(assets.has_value());
    ASSERT_EQ(assets->assets.size(), 2);
    EXPECT_TRUE(assets->find(asset_hash("paint"), asset_hash("slate")) != nullptr);
    EXPECT_TRUE(assets->find(asset_hash("paint"), asset_hash("white")) != nullptr);

    /// the device is given the program and none of the assets, and runs it as it did
    other_command_device device;
    vm::initialize_device(&device);
    vm::load_program_from_bytes(&device, bytes);
    EXPECT_EQ(device.program_load_cursor, other_command_device::kProgramStartAddress + obin_program_bytes(bytes).size());
    for (size_t step = 0; step < 4096 && !vm::has_flag(&device, other_command_device::STOPPED); ++step) {
      vm::step(&device);
    }
    EXPECT_TRUE(vm::has_flag(&device, other_command_device::STOPPED));
    EXPECT_EQ(device.read_register_as_u64(vm_register_idx::VM_RF) >> 32, 5);
    vm::shutdown_device(&device);

    /// what an asset holds is no part of the program
    write_file(sandbox / "main.ocmd", std::format(kScript, 3));
    const ostd::vector<uint8_t> repainted = link(sandbox / "main.ocmd");
    EXPECT_TRUE(std::ranges::equal(obin_program_bytes(repainted), obin_program_bytes(bytes)));
    EXPECT_NE(footprint_in(repainted, "paint", "white"), footprint_in(bytes, "paint", "white"));
    EXPECT_EQ(footprint_in(repainted, "paint", "slate"), footprint_in(bytes, "paint", "slate"));

    /// the assets are what the program is about, so they are there with or without its names
    const ostd::vector<uint8_t> bare = link(sandbox / "main.ocmd", { .debug_info = false });
    EXPECT_EQ(header_of(bare).prog_header.has_debug_flag, 0);
    EXPECT_EQ(header_of(bare).prog_header.has_assets_flag, 1);
    EXPECT_TRUE(obin_debug_bytes(bare).empty());
    EXPECT_TRUE(std::ranges::equal(obin_asset_bytes(bare), obin_asset_bytes(repainted)));
    EXPECT_FALSE(ocmd_toolchain{}.check_obin(bare).has_value());

    /// and a program with none is the file it always was
    write_file(sandbox / "plain.ocmd", "core log \"hello\" 2\n");
    const ostd::vector<uint8_t> plain = link(sandbox / "plain.ocmd");
    EXPECT_EQ(header_of(plain).prog_header.has_assets_flag, 0);
    EXPECT_TRUE(obin_asset_bytes(plain).empty());
    EXPECT_EQ(sizeof(header) + obin_program_bytes(plain).size() + obin_debug_bytes(plain).size(), plain.size());
  }

  TEST_F(command_declaration_tests, the_debug_section_says_what_an_asset_is_called_and_where_it_was_declared) {
    write_file(sandbox / "kinds.any", kKinds);
    write_file(sandbox / "main.ocmd", "use \"kinds.any\"\ncore log \"hello\" 2\npaint slate { shade = dark }\nlet white = paint { shade = light }\n");
    const ostd::vector<uint8_t> bytes = link(sandbox / "main.ocmd");
    const opt<obin_debug_info> debug = obin_debug_info::decode(obin_debug_bytes(bytes));
    ASSERT_TRUE(debug.has_value());

    /// the words the asset section has only the hashes of
    EXPECT_EQ(debug->asset_words, (ostd::vector<std::string>{ "coats", "gloss", "label", "maker", "mix", "note", "paint", "sealed", "shade" }));
    ASSERT_EQ(debug->assets.size(), 2);
    const auto where = [&](const std::string_view name) -> const debug_asset& {
      const auto found = std::ranges::find(debug->assets, asset_hash(name), &debug_asset::name);
      EXPECT_TRUE(found != debug->assets.end()) << name;
      return found != debug->assets.end() ? *found : debug->assets.front();
    };
    EXPECT_EQ(where("slate").kind, asset_hash("paint"));
    ASSERT_LT(where("slate").unit, debug->symbols.size());
    EXPECT_EQ(debug->symbols[where("slate").unit].name, "main.ocmd");
    EXPECT_EQ(where("slate").line, 3);
    EXPECT_EQ(where("slate").column, 1);
    /// one given to a variable is where its declaration starts, after the '='
    EXPECT_EQ(where("white").line, 4);
    EXPECT_EQ(where("white").column, 13);

    /// a listing of the program shows each as the block it was linked from, under where it came from
    const disassembly listing = decompiler::disassemble(bytes);
    ASSERT_TRUE(listing.error.empty()) << listing.error;
    EXPECT_TRUE(listing.text.contains("asset section: "));
    EXPECT_TRUE(listing.text.contains("\nassets\n"));
    EXPECT_TRUE(listing.text.contains("; main.ocmd:3:1\n; footprint "));
    EXPECT_TRUE(listing.text.contains("%slate : paint\n  set coats, 1\n  set gloss, 0.5\n  set label, \"\"\n  set maker, nobody\n  set mix, []\n  set note, 0\n  set sealed, false\n  set shade, dark\nend\n"));
    EXPECT_TRUE(listing.text.contains("; main.ocmd:4:13\n"));

    /// without the debug section the words are gone and their hashes are what there is
    const disassembly bare = decompiler::disassemble(link(sandbox / "main.ocmd", { .debug_info = false }));
    ASSERT_TRUE(bare.error.empty()) << bare.error;
    EXPECT_TRUE(bare.text.contains("no debug section to name them"));
    EXPECT_TRUE(bare.text.contains(std::format("%slate : #{:016x}\n", asset_hash("paint"))));
    EXPECT_TRUE(bare.text.contains(std::format("  set #{:016x}, dark\n", asset_hash("shade"))));
    EXPECT_FALSE(bare.text.contains("main.ocmd:3:1"));
  }

  TEST_F(command_declaration_tests, a_footprint_does_not_depend_on_how_or_beside_what_it_was_written) {
    write_file(sandbox / "one.things", std::format("{}paint white {{ shade = light  coats = 2  gloss = 1 }}\n", kKinds));
    write_file(sandbox / "deep" / "er" / "two.things", std::format("{}paint slate dark\npaint white {{\n  gloss = 1.0\n  coats = 2\n  shade = light\n}}\nroom hall {{ }}\n", kKinds));
    write_file(sandbox / "three.things", std::format("{}paint white {{ shade = light  coats = 3  gloss = 1 }}\n", kKinds));

    const ostd::vector<uint8_t> alone = footprint_in(link(sandbox / "one.things"), "paint", "white");
    const ostd::vector<uint8_t> beside = footprint_in(link(sandbox / "deep" / "er" / "two.things"), "paint", "white");
    const ostd::vector<uint8_t> other = footprint_in(link(sandbox / "three.things"), "paint", "white");
    ASSERT_FALSE(alone.empty());
    EXPECT_EQ(alone, beside);
    ASSERT_FALSE(other.empty());
    EXPECT_NE(alone, other);
  }

  TEST_F(command_declaration_tests, a_program_whose_assets_were_cut_or_changed_is_refused) {
    write_file(sandbox / "one.things", std::format("{}paint white light\n", kKinds));
    const ostd::vector<uint8_t> bytes = link(sandbox / "one.things");
    ASSERT_FALSE(bytes.empty());
    ocmd_toolchain toolchain;
    ASSERT_FALSE(toolchain.check_obin(bytes).has_value());

    ostd::vector<uint8_t> changed = bytes;
    changed.back() ^= 0x01;
    EXPECT_TRUE(toolchain.check_obin(changed).has_value());
    ostd::vector<uint8_t> cut = bytes;
    cut.pop_back();
    EXPECT_TRUE(toolchain.check_obin(cut).has_value());
    ostd::vector<uint8_t> longer = bytes;
    longer.push_back(0);
    EXPECT_TRUE(toolchain.check_obin(longer).has_value());
    /// a file that says it has none and has bytes where they would be
    ostd::vector<uint8_t> unsaid = bytes;
    ocmd_file_header header = header_of(unsaid);
    header.prog_header.has_assets_flag = 0;
    std::memcpy(unsaid.data(), &header, sizeof(header));
    EXPECT_TRUE(toolchain.check_obin(unsaid).has_value());
  }

  TEST_F(command_declaration_tests, two_sources_cannot_have_one_asset) {
    command_bus bus;
    register_builtin_command_devices(bus);
    diagnostic_engine diag;
    ocmd_toolchain toolchain;
    const oasm_source main = { .name = "main.oasm", .text = "/entry: main\n$main:\n  ret\nend\n%gold : material\n  set roughness, 0.3\nend\n" };
    const oasm_source same[] = { main, { .name = "other.oasm", .text = "%gold : material\n  set roughness, 0.5\nend\n" } };
    EXPECT_FALSE(toolchain.build_obin(bus, same, &diag).success);

    /// a texture and a material can both be called gold
    const oasm_source apart[] = { main, { .name = "other.oasm", .text = "%gold : texture\n  set size, [4, 4]\nend\n" } };
    const obin_build_result built = toolchain.build_obin(bus, apart, &diag);
    ASSERT_TRUE(built.success);
    const opt<obin_assets> assets = obin_assets::decode(obin_asset_bytes(built.bytes));
    ASSERT_TRUE(assets.has_value());
    EXPECT_EQ(assets->assets.size(), 2);
  }

  TEST_F(command_declaration_tests, the_default_pipelines_footprint_is_its_asset_block) {
    diagnostic_engine diag;
    const ostd::vector<command_unit> units = ocmd_toolchain{}.read_command_units("resources/editor-assets/default-pipeline.oass", "resources/editor-assets/default-pipeline.oass", &diag);
    ASSERT_EQ(units.size(), 2);
    const built made = build(units);
    ASSERT_EQ(made.assets.size(), 1);
    const ostd::vector<uint8_t> bytes = link(units);
    ASSERT_FALSE(bytes.empty());
    EXPECT_FALSE(ocmd_toolchain{}.check_obin(bytes).has_value());

    const ostd::vector<uint8_t> footprint = footprint_in(bytes, "pipeline", "default-instancing");
    EXPECT_EQ(footprint, asset_footprint(made.assets[0]));
    const opt<footprint_node> pipeline = read_asset_footprint(footprint);
    ASSERT_TRUE(pipeline.has_value());
    EXPECT_EQ(pipeline->kind, asset_hash("pipeline"));
    EXPECT_EQ(pipeline->name, "default-instancing");
    /// a field is found by the hash of its name
    const footprint_field* display = pipeline->field(asset_hash("display"));
    ASSERT_TRUE(display != nullptr);
    EXPECT_EQ(display->how, ASSET_FIELD_REF);
    EXPECT_EQ(display->value.text, "smaa_texture");
    EXPECT_EQ(std::ranges::count(pipeline->parts, asset_hash("pass"), &footprint_node::kind), 14);
    EXPECT_EQ(std::ranges::count(pipeline->parts, asset_hash("texture"), &footprint_node::kind), 20);
    EXPECT_EQ(pipeline->edges.size(), 5);
    /// none of it is in the 64 KB the device holds
    EXPECT_LT(obin_program_bytes(bytes).size(), 64);
  }

  TEST_F(command_declaration_tests, the_default_pipeline_is_written_out_whole_and_checks) {
    diagnostic_engine diag;
    collecting_sink sink;
    const scoped_diagnostic_sink sink_scope(&diag, "collecting-sink", &sink);
    ocmd_toolchain toolchain;
    const ostd::vector<command_unit> units = toolchain.read_command_units("resources/editor-assets/default-pipeline.oass", "resources/editor-assets/default-pipeline.oass", &diag);
    ASSERT_EQ(units.size(), 2);
    EXPECT_TRUE(toolchain.check_command_units(units, &diag));
    for (const diagnostic& error : sink.found) {
      ADD_FAILURE() << error.span.start.line << ":" << error.span.start.column << ": " << error.final_message;
    }

    /// what default-pipeline.toml holds, counted there by hand
    ASSERT_EQ(units[0].file.declarations.size(), 1);
    const command_declaration& pipeline = units[0].file.declarations[0];
    EXPECT_EQ(pipeline.kind, "pipeline");
    EXPECT_EQ(pipeline.name, "default-instancing");
    EXPECT_EQ(parts_of(pipeline, "buffer"), 3);
    EXPECT_EQ(parts_of(pipeline, "texture"), 20);
    EXPECT_EQ(parts_of(pipeline, "shader"), 15);
    EXPECT_EQ(parts_of(pipeline, "binding"), 6);
    EXPECT_EQ(parts_of(pipeline, "param"), 4);
    EXPECT_EQ(parts_of(pipeline, "slot"), 2);
    EXPECT_EQ(parts_of(pipeline, "pass"), 14);
    EXPECT_EQ(parts_of(pipeline, "read"), 40);
    EXPECT_EQ(parts_of(pipeline, "write"), 22);
    EXPECT_EQ(parts_of(pipeline, "bind"), 19);
    EXPECT_EQ(parts_of(pipeline, "set"), 4);
    EXPECT_EQ(parts_of(pipeline, "with"), 5);
    EXPECT_EQ(pipeline.edges.size(), 5);
  }

}  // namespace other
