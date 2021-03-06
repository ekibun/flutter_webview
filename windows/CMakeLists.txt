cmake_minimum_required(VERSION 3.15)
set(PROJECT_NAME "flutter_webview")
project(${PROJECT_NAME} LANGUAGES CXX)
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

# This value is used when generating builds using this plugin, so it must
# not be changed
set(PLUGIN_NAME "flutter_webview_plugin")

add_library(${PLUGIN_NAME} SHARED
  "flutter_webview_plugin.cpp"
)
apply_standard_settings(${PLUGIN_NAME})
set_target_properties(${PLUGIN_NAME} PROPERTIES
  CXX_VISIBILITY_PRESET hidden)
target_compile_definitions(${PLUGIN_NAME} PRIVATE FLUTTER_PLUGIN_IMPL)
target_include_directories(${PLUGIN_NAME} INTERFACE
  "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(${PLUGIN_NAME} PRIVATE flutter flutter_wrapper_plugin
  "${CMAKE_CURRENT_SOURCE_DIR}/webview/WebView2Loader.dll.lib"
)

# List of absolute paths to libraries that should be bundled with the plugin
set(flutter_webview_bundled_libraries
  "${CMAKE_CURRENT_SOURCE_DIR}/webview/WebView2Loader.dll"
  PARENT_SCOPE
)
