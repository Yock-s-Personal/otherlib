## the editor's services and their bindings: what an editor window works with that is not
##  the scene itself. apart from the driver so that the test suite can link it, and apart
##  from otherlib so that a packaged game, which links all of otherlib, has none of it
add_library(other_editor_services STATIC)
target_sources(other_editor_services
  PRIVATE
    src/services/edit_stack.cpp
    src/services/editor_camera.cpp
    src/services/editor_services.cpp
    src/services/editor_bindings.cpp
    src/services/editor_project_bindings.cpp
    src/services/compile_service.cpp
)
target_include_directories(other_editor_services
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:${OTHER_INSTALL_INCLUDEDIR}/editor>
)
target_link_libraries(other_editor_services PUBLIC otherlib)

add_static_driver(other_editor
  src/ui/asset-browser/asset_browser_widgets.cpp
  src/ui/asset-browser/asset_browser_tree_node.cpp
  src/ui/asset-browser/asset_browser_grid_node.cpp
  src/ui/asset-browser/asset_browser.cpp
  src/ui/asset-editor/asset_editor_registry.cpp
  src/ui/asset-editor/asset_editor.cpp
  src/ui/asset-editor/asset_editor_node.cpp
  src/ui/node-editor/node_editor_canvas.cpp
  src/ui/node-editor/node_editor_display.cpp
  src/ui/render-pipeline-ui/render_pipeline_widgets.cpp
  src/ui/render-pipeline-ui/render_pipeline_viewer.cpp
  src/ui/render-pipeline-ui/render_pipeline_editor.cpp
  src/ui/viewport/viewport.cpp

  src/editor_context.cpp
  src/editor_driver.cpp
)
target_include_directories(other_editor 
  PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:${OTHER_INSTALL_INCLUDEDIR}/editor>
)

target_link_libraries(other_editor PUBLIC other_editor_services)

add_subdirectory(plugins)
add_subdirectory(api)
add_subdirectory(dotnet)

install(TARGETS other_editor RUNTIME DESTINATION "${OTHER_INSTALL_BINDIR}/${OTHER_LIB_FAMILY}")
install(
  DIRECTORY "${CMAKE_BINARY_DIR}/other-editor/dotnet/${ASSEMBLY_CONFIG}/"
  DESTINATION "${OTHER_INSTALL_ASSEMBLIES}"
  FILES_MATCHING PATTERN "OtherEditorCs.*"
)
install(
  DIRECTORY "${CMAKE_BINARY_DIR}/other-editor/api/${ASSEMBLY_CONFIG}/"
  DESTINATION "${OTHER_INSTALL_ASSEMBLIES}"
  FILES_MATCHING PATTERN "OtherEditor.*"
)