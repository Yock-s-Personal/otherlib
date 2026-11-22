/**
 * \file tests/scene/project_tool_tests.cpp
 *
 * the build an open project's scripts get: 'dotnet build' started by the engine itself, in
 * the engine's configuration, into the project's own bin folder. these need the .NET SDK,
 * as building the engine does.
 **/
#include <filesystem>
#include <fstream>

#include "tools/project_tool.hpp"

#include "other_test.hpp"

namespace other {

  class project_tool_tests : public other_test {
   protected:
    void SetUp() override {
      other_test::SetUp();
      const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
      sandbox = std::filesystem::temp_directory_path() / "other-project-tool-tests" / info->name();
      std::filesystem::remove_all(sandbox);
      csproj = sandbox / "Game.csproj";
      /// the shape 'oecli create' gives a project: a library with no target framework folder
      write("Game.csproj",
            "<Project Sdk=\"Microsoft.NET.Sdk\">\n"
            "  <PropertyGroup>\n"
            "    <OutputType>Library</OutputType>\n"
            "    <TargetFramework>net9.0</TargetFramework>\n"
            "    <AppendTargetFrameworkToOutputPath>false</AppendTargetFrameworkToOutputPath>\n"
            "  </PropertyGroup>\n"
            "</Project>\n");
      write("Thing.cs", "namespace Game { public class Thing { public int Value => 42; } }\n");
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

  TEST_F(project_tool_tests, a_project_is_built_where_the_engine_looks_for_its_assembly) {
    EXPECT_EQ(project_tool::build_project(csproj), 0);
    EXPECT_TRUE(std::filesystem::is_regular_file(sandbox / "bin" / get_project_build_config_string() / "Game.dll"));
    EXPECT_TRUE(project_tool::read_build_log(csproj).empty());
  }

  TEST_F(project_tool_tests, a_build_that_fails_says_so_and_leaves_the_compiler_errors_to_read) {
    write("Thing.cs", "namespace Game { public class Thing { public int Value => ; } }\n");
    EXPECT_NE(project_tool::build_project(csproj), 0);
    const std::string errors = project_tool::read_build_log(csproj);
    EXPECT_NE(errors.find("error CS"), std::string::npos) << errors;
    EXPECT_NE(errors.find("Thing.cs"), std::string::npos) << errors;
  }

}  // namespace other
