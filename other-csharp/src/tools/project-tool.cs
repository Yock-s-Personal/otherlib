using System;
using System.Runtime.CompilerServices;
using OtherCsBindings;

namespace Other.Toolset
{
  class ProjectTool : Tool
  {
    public struct ProjectArgs
    {
      public NativeString Name;
      public NativeString WorkingDirectory;
    }

    private string name;
    private string working_directory;
    private string csproj_file_path;

    public ProjectTool() : base()
    {
      name = string.Empty;
      working_directory = string.Empty;
      csproj_file_path = string.Empty;
    }

    public void SetData(ProjectArgs args)
    {
      name = args.Name.ToString()!;
      working_directory = args.WorkingDirectory.ToString()!;
    }

    public void CreateDefaultCsproj(NativeString path)
    {
      // errors and returns if the file already exists, else creates a default csproj
      string csproj_path = path.ToString()!;

      if (System.IO.File.Exists(csproj_path))
      {
        Console.WriteLine($"Error: File '{csproj_path}' already exists.");
        return;
      }
      
      csproj_file_path = csproj_path;
      string csproj_content = 
      $@"
<Project Sdk=""Microsoft.NET.Sdk"">
  <PropertyGroup>
    <OutputType>Library</OutputType>
    <TargetFramework>net9.0</TargetFramework>
    <RootNamespace>{name}</RootNamespace>
    <AppendTargetFrameworkToOutputPath>false</AppendTargetFrameworkToOutputPath>
  </PropertyGroup>
</Project>
";

      System.IO.File.WriteAllText(csproj_file_path, csproj_content);
    }
  }
}