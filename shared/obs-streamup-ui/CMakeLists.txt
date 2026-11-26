# obs-streamup-ui. Either include the helper directly:
#   include(shared/obs-streamup-ui/cmake/streamup-ui.cmake)
# or add this directory, then call streamup_ui_add on the plugin target:
#   add_subdirectory(shared/obs-streamup-ui)
#   streamup_ui_add(${PROJECT_NAME})
# The library compiles into the plugin target; it defines no target of its own.
include("${CMAKE_CURRENT_LIST_DIR}/cmake/streamup-ui.cmake")
