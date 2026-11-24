include(CSharpUtilities)
add_library(OtherEditorCs SHARED)
set_target_properties(OtherEditorCs PROPERTIES
  DOTNET_SDK "Microsoft.NET.Sdk"
  DOTNET_TARGET_FRAMEWORK "net9.0"
  VS_GLOBAL_AllowUnsafeBlocks "true"
  VS_GLOBAL_Nullable "enable"
)

target_sources(OtherEditorCs PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src/project-overview-window.cs
  ${CMAKE_CURRENT_SOURCE_DIR}/src/scene-hierarchy-window.cs
  ${CMAKE_CURRENT_SOURCE_DIR}/src/scene-list-window.cs
  ${CMAKE_CURRENT_SOURCE_DIR}/src/object-editor-window.cs
  ${CMAKE_CURRENT_SOURCE_DIR}/src/project-compiler-window.cs
  ${CMAKE_CURRENT_SOURCE_DIR}/src/project-creator-window.cs
  ${CMAKE_CURRENT_SOURCE_DIR}/src/profiler-window.cs
  ${CMAKE_CURRENT_SOURCE_DIR}/src/select-tool.cs
  ${CMAKE_CURRENT_SOURCE_DIR}/src/transform-tools.cs
  ${CMAKE_CURRENT_SOURCE_DIR}/src/scene-toolbar.cs
)
target_link_libraries(OtherEditorCs PUBLIC OtherEditor OtherCs OtherCsBindings)