# StreamUP UI library: one call adds it to a plugin target.
#
#   include("${CMAKE_CURRENT_SOURCE_DIR}/shared/obs-streamup-ui/cmake/streamup-ui.cmake")
#   streamup_ui_add(${PROJECT_NAME})
#
# The library is compiled straight into the plugin (no shared sub-target, no
# cross-plugin link), so each plugin stays self-contained and builds in any
# config or order. The target must link libobs and obs-frontend-api; the call
# turns AUTOMOC and AUTORCC on for it.
#
# Plain (not cached) variables: several plugins in one OBS tree each include
# their own copy, and each call must use the copy its plugin included.

set(STREAMUP_UI_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
set(STREAMUP_UI_VERSION "1.0.0")

function(streamup_ui_add target)
  set_target_properties(${target} PROPERTIES AUTOMOC ON AUTORCC ON)
  target_include_directories(${target} PRIVATE
    "${STREAMUP_UI_ROOT}/include"
    "${STREAMUP_UI_ROOT}/include/streamup/ui" # flat includes inside switch-button.cpp
  )
  target_sources(${target} PRIVATE
    "${STREAMUP_UI_ROOT}/src/switch-button.cpp"
    "${STREAMUP_UI_ROOT}/src/feedback.cpp"
    "${STREAMUP_UI_ROOT}/src/kit.cpp"
    "${STREAMUP_UI_ROOT}/src/theme.cpp"
    "${STREAMUP_UI_ROOT}/include/streamup/ui/switch-button.hpp" # Q_OBJECT -> AUTOMOC
    "${STREAMUP_UI_ROOT}/include/streamup/ui/ios-checkbox.hpp"  # Q_OBJECT -> AUTOMOC
    "${STREAMUP_UI_ROOT}/include/streamup/ui/kit.hpp"
    "${STREAMUP_UI_ROOT}/include/streamup/ui/theme.hpp"
    "${STREAMUP_UI_ROOT}/streamup-ui-icons.qrc"
  )
  target_compile_definitions(${target} PRIVATE STREAMUP_UI_VERSION="${STREAMUP_UI_VERSION}")
endfunction()
