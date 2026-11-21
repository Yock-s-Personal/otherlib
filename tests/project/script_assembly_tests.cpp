/**
 * \file tests/project/script_assembly_tests.cpp
 *
 * building a project's C# assembly for a package. these run 'dotnet build' on a project of
 * one file, so they need the .NET SDK, as building the engine does.
 **/
#include <filesystem>
#include <fstream>

#include "project/script_assembly.hpp"

#include "other_test.hpp"

namespace other {

  class script_assembly_tests : public other_test {
   protected:
    void SetUp() override {
      other_test::SetUp();
      const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
      sandbox = std::filesystem::temp_directory_path() / "other-script-assembly-tests" / info->name();
      std::filesystem::remove_all(sandbox);
      csproj = sandbox / "game" / "Game.csproj";
      /// the shape 'oecli create' gives a project: a library with no target framework folder
      write("game/Game.csproj",
            "<Project Sdk=\"Microsoft.NET.Sdk\">\n"
            "  <PropertyGroup>\n"
            "    <OutputType>Library</OutputType>\n"
            "    <TargetFramework>net9.0</TargetFramework>\n"
            "    <AppendTargetFrameworkToOutputPath>false</AppendTargetFrameworkToOutputPath>\n"
            "  </PropertyGroup>\n"
            "</Project>\n");
      write("game/Thing.cs", "namespace Game { public class Thing { public int Value => 42; } }\n");
    }

    void TearDown() override {
      std::error_code ec;
      std::filesystem::remove_all(sandbox, ec);
      other_test::TearDown();
    }

    /// path is from the sandbox
    void write(const filepath& path, const std::string_view text) {
      const filepath full = sandbox / path;
      std::filesystem::create_directories(full.parent_path());
      std::ofstream out(full, std::ios::binary | std::ios::trunc);
      ASSERT_TRUE(out.is_open()) << full.string();
      out.write(text.data(), static_cast<std::streamsize>(text.size()));
    }

    filepath sandbox;
    filepath csproj;
  };

  TEST_F(script_assembly_tests, the_assembly_is_built_and_copied_to_where_it_was_asked_for) {
    const filepath release = sandbox / "package" / "bin";
    const script_assembly_result built = build_script_assembly({ .csproj = csproj, .configuration = "Release", .destination = release });
    ASSERT_TRUE(built.success) << (built.errors.empty() ? "" : built.errors.front());
    EXPECT_TRUE(built.errors.empty());
    EXPECT_EQ(built.assembly, release / "Game.dll");
    EXPECT_TRUE(std::filesystem::is_regular_file(release / "Game.dll"));
    /// a Release build writes symbols too; they are not asked for, so they stay behind
    EXPECT_FALSE(std::filesystem::exists(release / "Game.pdb"));
    EXPECT_TRUE(std::filesystem::is_regular_file(built_assembly_path(csproj, "Release")));

    /// the other configuration is its own build, and takes its symbols when told to
    const filepath debug = sandbox / "package-debug" / "bin";
    const script_assembly_result with_symbols = build_script_assembly({ .csproj = csproj, .configuration = "Debug", .destination = debug, .copy_symbols = true });
    ASSERT_TRUE(with_symbols.success) << (with_symbols.errors.empty() ? "" : with_symbols.errors.front());
    EXPECT_TRUE(std::filesystem::is_regular_file(debug / "Game.dll"));
    EXPECT_TRUE(std::filesystem::is_regular_file(debug / "Game.pdb"));
    EXPECT_TRUE(std::filesystem::is_regular_file(built_assembly_path(csproj, "Debug")));
  }

  TEST_F(script_assembly_tests, a_project_that_does_not_compile_gives_the_compilers_errors_and_copies_nothing) {
    write("game/Thing.cs", "namespace Game { public class Thing { public int Value => missing; } }\n");

    const filepath destination = sandbox / "package" / "bin";
    const script_assembly_result built = build_script_assembly({ .csproj = csproj, .configuration = "Release", .destination = destination });
    EXPECT_FALSE(built.success);
    ASSERT_FALSE(built.errors.empty());
    /// the compiler's line as it wrote it: the file and position first, then the error, with
    ///  msbuild's node number taken off the front. the folders in front of the file's name are
    ///  not compared: a temp folder can be spelt two ways (RUNNER~1 for runneradmin on a build
    ///  machine), and msbuild prints the long one whichever it was given
    const std::string& first = built.errors.front();
    const size_t file_at = first.find("Thing.cs(1,");
    ASSERT_NE(file_at, std::string::npos) << first;
    EXPECT_FALSE(first.starts_with(' ')) << first;
    EXPECT_FALSE(first.substr(0, file_at).contains('>')) << first;
    EXPECT_TRUE(first.substr(0, file_at).ends_with("game\\")) << first;
    EXPECT_TRUE(first.contains("error CS0103")) << first;
    EXPECT_FALSE(std::filesystem::exists(destination / "Game.dll"));
  }

  TEST_F(script_assembly_tests, what_cannot_be_built_is_refused_before_anything_runs) {
    const filepath destination = sandbox / "package" / "bin";

    const script_assembly_result missing = build_script_assembly({ .csproj = sandbox / "game" / "Gone.csproj", .configuration = "Release", .destination = destination });
    EXPECT_FALSE(missing.success);
    ASSERT_EQ(missing.errors.size(), 1);
    EXPECT_TRUE(missing.errors.front().contains("does not exist"));

    /// the engine has four build configurations and .NET projects have two
    const script_assembly_result profile = build_script_assembly({ .csproj = csproj, .configuration = "Profile", .destination = destination });
    EXPECT_FALSE(profile.success);
    ASSERT_EQ(profile.errors.size(), 1);
    EXPECT_TRUE(profile.errors.front().contains("Debug or Release"));

    EXPECT_FALSE(std::filesystem::exists(destination));
    EXPECT_FALSE(std::filesystem::exists(sandbox / "game" / "bin"));
  }

  TEST_F(script_assembly_tests, a_build_leaves_its_assembly_under_bin_and_the_configuration) {
    EXPECT_EQ(built_assembly_path("my-game/MyGame.csproj", "Release"), filepath{ "my-game/bin/Release/MyGame.dll" });
    EXPECT_EQ(built_assembly_path("my-game/MyGame.csproj", "Debug"), filepath{ "my-game/bin/Debug/MyGame.dll" });
  }

}  // namespace other
