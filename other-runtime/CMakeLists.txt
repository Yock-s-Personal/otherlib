add_static_driver(other_runtime
  src/runtime_driver.cpp
)
target_include_directories(other_runtime
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
)

install(TARGETS other_runtime RUNTIME DESTINATION "${OTHER_INSTALL_BINDIR}/${OTHER_LIB_FAMILY}")