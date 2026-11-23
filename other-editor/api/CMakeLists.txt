## the editor's C# API (Other.Editor). it is the editor's own: the editor loads it when it
##  starts and a packaged game has no copy. the editor's windows (OtherEditorCs) and, later,
##  a project's own editor windows are written against it
include(CSharpUtilities)
add_library(OtherEditor SHARED)
set_target_properties(OtherEditor PROPERTIES
  DOTNET_SDK "Microsoft.NET.Sdk"
  DOTNET_TARGET_FRAMEWORK "net9.0"
  VS_GLOBAL_AllowUnsafeBlocks "true"
  VS_GLOBAL_Nullable "enable"
)

target_sources(OtherEditor PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src/selection.cs
  ${CMAKE_CURRENT_SOURCE_DIR}/src/edits.cs
  ${CMAKE_CURRENT_SOURCE_DIR}/src/scene-view.cs
  ${CMAKE_CURRENT_SOURCE_DIR}/src/playback.cs
  ${CMAKE_CURRENT_SOURCE_DIR}/src/commands.cs
  ${CMAKE_CURRENT_SOURCE_DIR}/src/editor-commands.cs
)
target_link_libraries(OtherEditor PUBLIC OtherCs OtherCsBindings)
