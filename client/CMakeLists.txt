cmake_minimum_required(VERSION 3.22)

# Fail fast on the known-bad configuration instead of hanging for minutes:
# Dropbox sync locks stall cmake configure/build inside the synced tree.
# Placed before project() so the abort is instant (no compiler detection).
string(FIND "${CMAKE_BINARY_DIR}" "Dropbox" _nczx_dropbox_pos)
if(NOT _nczx_dropbox_pos EQUAL -1)
    message(FATAL_ERROR
        "Build dir is inside Dropbox: sync locks hang builds. "
        "Use client/build-pc.cmd (configure+build+ctest, incremental) "
        "or, from client/: cmake --preset msvc")
endif()

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/../VERSION"
    MIRRORSHIFT_SOURCE_VERSION LIMIT_COUNT 1)
string(STRIP "${MIRRORSHIFT_SOURCE_VERSION}" MIRRORSHIFT_SOURCE_VERSION)
if(MIRRORSHIFT_SOURCE_VERSION MATCHES "^([0-9]+\\.[0-9]+)(-dev([0-9][0-9][0-9]|ESP))?$")
    set(MIRRORSHIFT_PROJECT_VERSION "${CMAKE_MATCH_1}.0")
    set(MIRRORSHIFT_BUNDLE_SHORT_VERSION "${CMAKE_MATCH_1}")
elseif(MIRRORSHIFT_SOURCE_VERSION MATCHES "^([0-9]+\\.[0-9]+\\.[0-9]+)(-dev([0-9][0-9][0-9]|ESP))?$")
    set(MIRRORSHIFT_PROJECT_VERSION "${CMAKE_MATCH_1}")
    set(MIRRORSHIFT_BUNDLE_SHORT_VERSION "${CMAKE_MATCH_1}")
else()
    message(FATAL_ERROR
        "VERSION must contain major.minor[-devNNN|-devESP] or "
        "major.minor.patch[-devNNN|-devESP]")
endif()
set(MIRRORSHIFT_DISPLAY_VERSION "${MIRRORSHIFT_SOURCE_VERSION}")

project(MirrorShift VERSION "${MIRRORSHIFT_PROJECT_VERSION}" LANGUAGES C CXX)

set(MIRRORSHIFT_WINDOWS_RESOURCE "${CMAKE_CURRENT_BINARY_DIR}/mirrorshift.rc")
set(MIRRORSHIFT_WINDOWS_ICON "${CMAKE_CURRENT_SOURCE_DIR}/mirrorshift.ico")
configure_file(mirrorshift.rc.in "${MIRRORSHIFT_WINDOWS_RESOURCE}" @ONLY)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network)
set(QT_VERSION_MAJOR 6)

# All shared C sources are globbed: every refactor phase adds files under
# src/common and a hand-maintained list drifts (recurring unresolved-symbol
# link failures). CONFIGURE_DEPENDS re-runs the glob at build time.
file(GLOB MIRRORSHIFT_COMMON_SOURCES CONFIGURE_DEPENDS
    ../src/common/*/*.c
)
add_library(mirrorshift-common STATIC ${MIRRORSHIFT_COMMON_SOURCES})
target_include_directories(mirrorshift-common PUBLIC ../src)
set_target_properties(mirrorshift-common PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
    AUTOMOC OFF
    AUTOUIC OFF
    AUTORCC OFF
)

set(MIRRORSHIFT_DESKTOP_CORE_SOURCES
    ../src/pc/client/input_helpers.cpp
    ../src/pc/client/desktop_session_controller.cpp
    ../src/pc/client/desktop_transport_codec.cpp
    ../src/pc/client/direct_session_adapter.cpp
    ../src/pc/client/mqtt_session_adapter.cpp
    ../src/pc/client/save_game_store.cpp
    ../src/pc/client/input_helpers.h
    ../src/pc/client/desktop_session_controller.h
    ../src/pc/client/desktop_transport_codec.h
    ../src/pc/client/direct_session_adapter.h
    ../src/pc/client/mqtt_session_adapter.h
    ../src/pc/client/save_game_store.h
)

add_library(mirrorshift-desktop-core STATIC ${MIRRORSHIFT_DESKTOP_CORE_SOURCES})
target_include_directories(mirrorshift-desktop-core PUBLIC ../src)
target_link_libraries(mirrorshift-desktop-core
    PUBLIC
        mirrorshift-common
        Qt${QT_VERSION_MAJOR}::Core
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Network
)
set_target_properties(mirrorshift-desktop-core PROPERTIES
    AUTOMOC OFF
    AUTOUIC OFF
    AUTORCC OFF
)

set(MIRRORSHIFT_CLIENT_UI_SOURCES
    ../src/pc/client/main_window.cpp
    ../src/pc/client/app_banner.cpp
    ../src/pc/client/piece_renderer.cpp
    ../src/pc/client/main_window.h
    ../src/pc/client/app_banner.h
    ../src/pc/client/piece_renderer.h
    ../src/pc/client/ui_theme.h
    ../src/pc/client/window_chrome.cpp
    ../src/pc/client/window_chrome.h
)
set(MIRRORSHIFT_CLIENT_APP_SOURCES
    ../src/pc/client/main.cpp
)

if(WIN32)
    list(APPEND MIRRORSHIFT_CLIENT_UI_SOURCES
        ../src/pc/client/win_window_chrome.h
        ../src/pc/client/win_window_chrome.cpp
    )
    list(APPEND MIRRORSHIFT_CLIENT_APP_SOURCES "${MIRRORSHIFT_WINDOWS_RESOURCE}")
elseif(APPLE)
    enable_language(OBJCXX)
    list(APPEND MIRRORSHIFT_CLIENT_UI_SOURCES
        ../src/pc/client/mac_window_chrome.h
        ../src/pc/client/mac_window_chrome.mm
    )
    list(APPEND MIRRORSHIFT_CLIENT_APP_SOURCES mirrorshift.icns)
    set_source_files_properties(mirrorshift.icns PROPERTIES
        MACOSX_PACKAGE_LOCATION Resources
    )
endif()

add_library(mirrorshift-client-ui STATIC ${MIRRORSHIFT_CLIENT_UI_SOURCES})
target_include_directories(mirrorshift-client-ui PUBLIC ../src)
target_compile_definitions(mirrorshift-client-ui
    PRIVATE
        NETCHESSZX_PC_MQTT_TX_FAILURE_TEST=1
        NETCHESSZX_APP_VERSION="${MIRRORSHIFT_DISPLAY_VERSION}"
)
target_link_libraries(mirrorshift-client-ui
    PUBLIC
        mirrorshift-desktop-core
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Network
)
if(WIN32)
    target_link_libraries(mirrorshift-client-ui PUBLIC dwmapi)
endif()
set_target_properties(mirrorshift-client-ui PROPERTIES AUTOUIC OFF)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_standard_project_setup()
    qt_add_executable(mirrorshift-client ${MIRRORSHIFT_CLIENT_APP_SOURCES})
else()
    add_executable(mirrorshift-client ${MIRRORSHIFT_CLIENT_APP_SOURCES})
endif()

# No .ui files in this project; AUTOUIC would misread src/common/ui_messages.h
# (ui_ prefix) as a uic-generated header and demand a missing messages.ui.
set_target_properties(mirrorshift-client PROPERTIES
    AUTOUIC OFF
    OUTPUT_NAME "MirrorShift"
)

target_include_directories(mirrorshift-client PRIVATE
    ../src
)
target_link_libraries(mirrorshift-client PRIVATE
    mirrorshift-client-ui
)

if(APPLE)
    target_link_options(mirrorshift-client PRIVATE
        "$<$<CONFIG:Release>:-Wl,-dead_strip>"
    )
elseif(MSVC)
    target_compile_options(mirrorshift-client-ui PRIVATE
        "$<$<CONFIG:Release>:/Gy>"
        "$<$<CONFIG:Release>:/Gw>"
    )
    target_link_options(mirrorshift-client PRIVATE
        "$<$<CONFIG:Release>:/OPT:REF>"
        "$<$<CONFIG:Release>:/OPT:ICF>"
    )
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(mirrorshift-client-ui PRIVATE
        "$<$<CONFIG:Release>:-ffunction-sections>"
        "$<$<CONFIG:Release>:-fdata-sections>"
    )
    target_link_options(mirrorshift-client PRIVATE
        "$<$<CONFIG:Release>:-Wl,--gc-sections>"
    )
endif()

set(MIRRORSHIFT_DESKTOP_ASSET_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/../assets/pc-client")
set(MIRRORSHIFT_DESKTOP_WORDMARK mirrorshift-wordmark.png)
set(MIRRORSHIFT_DESKTOP_APP_ICON mirrorshift-icon.png)
set(MIRRORSHIFT_DESKTOP_ABOUT_ART about/mirrorshift-about.png)
if(APPLE)
    set(MIRRORSHIFT_DESKTOP_ASSET_ROOT
        "$<TARGET_BUNDLE_CONTENT_DIR:mirrorshift-client>/Resources/assets")
else()
    set(MIRRORSHIFT_DESKTOP_ASSET_ROOT
        "$<TARGET_FILE_DIR:mirrorshift-client>/assets")
endif()
set(MIRRORSHIFT_DESKTOP_ASSET_DEST "${MIRRORSHIFT_DESKTOP_ASSET_ROOT}/pc-client")

add_custom_command(TARGET mirrorshift-client POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${MIRRORSHIFT_DESKTOP_ASSET_DEST}"
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "${MIRRORSHIFT_DESKTOP_ASSET_DEST}/about"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MIRRORSHIFT_DESKTOP_ASSET_SOURCE}/${MIRRORSHIFT_DESKTOP_WORDMARK}"
            "${MIRRORSHIFT_DESKTOP_ASSET_DEST}/${MIRRORSHIFT_DESKTOP_WORDMARK}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MIRRORSHIFT_DESKTOP_ASSET_SOURCE}/${MIRRORSHIFT_DESKTOP_APP_ICON}"
            "${MIRRORSHIFT_DESKTOP_ASSET_DEST}/${MIRRORSHIFT_DESKTOP_APP_ICON}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MIRRORSHIFT_DESKTOP_ASSET_SOURCE}/${MIRRORSHIFT_DESKTOP_ABOUT_ART}"
            "${MIRRORSHIFT_DESKTOP_ASSET_DEST}/${MIRRORSHIFT_DESKTOP_ABOUT_ART}"
    COMMENT "Staging shared desktop assets"
    VERBATIM
)
if(APPLE)
    set(MIRRORSHIFT_NOTICE_DEST
        "$<TARGET_FILE_DIR:mirrorshift-client>/../Resources")
    add_custom_command(TARGET mirrorshift-client POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${MIRRORSHIFT_NOTICE_DEST}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/../LICENSE"
                "${MIRRORSHIFT_NOTICE_DEST}/LICENSE"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/../THIRD_PARTY_NOTICES.md"
                "${MIRRORSHIFT_NOTICE_DEST}/THIRD_PARTY_NOTICES.md"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/../licenses"
                "${MIRRORSHIFT_NOTICE_DEST}/licenses"
        COMMENT "Staging licences and third-party notices"
        VERBATIM
    )
endif()
include(CTest)
if(BUILD_TESTING)
    function(mirrorshift_add_desktop_test target_name test_name source)
        add_executable(${target_name} ${source})
        target_include_directories(${target_name} PRIVATE ../src)
        target_link_libraries(${target_name} PRIVATE
            mirrorshift-desktop-core
            ${ARGN}
        )
        set_target_properties(${target_name} PROPERTIES
            AUTOMOC OFF
            AUTOUIC OFF
            AUTORCC OFF
        )
        add_test(NAME ${test_name} COMMAND ${target_name})
    endfunction()

    mirrorshift_add_desktop_test(
        mirrorshift-desktop-transport-codec-test
        pc-desktop-transport-codec
        ../tests/pc/test_desktop_transport_codec.cpp
    )
    mirrorshift_add_desktop_test(
        mirrorshift-desktop-session-controller-test
        pc-desktop-session-controller
        ../tests/pc/test_desktop_session_controller.cpp
    )
    mirrorshift_add_desktop_test(
        mirrorshift-save-game-store-test
        pc-save-game-store
        ../tests/pc/test_save_game_store.cpp
    )
    mirrorshift_add_desktop_test(
        mirrorshift-input-helpers-test
        pc-input-helpers
        ../tests/pc/test_input_helpers.cpp
    )
    mirrorshift_add_desktop_test(
        mirrorshift-direct-adapter-test
        pc-direct-adapter
        ../tests/pc/test_direct_session_adapter.cpp
        Qt${QT_VERSION_MAJOR}::Network
    )
    mirrorshift_add_desktop_test(
        mirrorshift-mqtt-adapter-test
        pc-mqtt-adapter
        ../tests/pc/test_mqtt_session_adapter.cpp
    )

    add_executable(mirrorshift-mqtt-tx-failure-test
        ../tests/pc/test_mqtt_tx_failure.cpp
    )
    target_include_directories(mirrorshift-mqtt-tx-failure-test PRIVATE ../src)
    target_link_libraries(mirrorshift-mqtt-tx-failure-test PRIVATE
        mirrorshift-client-ui
    )
    set_target_properties(mirrorshift-mqtt-tx-failure-test PROPERTIES AUTOUIC OFF)
    add_test(NAME pc-mqtt-tx-failure COMMAND mirrorshift-mqtt-tx-failure-test)

    add_test(NAME pc-client-wordmark
        COMMAND ${CMAKE_COMMAND} -E compare_files
                "${MIRRORSHIFT_DESKTOP_ASSET_SOURCE}/${MIRRORSHIFT_DESKTOP_WORDMARK}"
                "${MIRRORSHIFT_DESKTOP_ASSET_DEST}/${MIRRORSHIFT_DESKTOP_WORDMARK}"
    )
    add_test(NAME pc-client-app-icon
        COMMAND ${CMAKE_COMMAND} -E compare_files
                "${MIRRORSHIFT_DESKTOP_ASSET_SOURCE}/${MIRRORSHIFT_DESKTOP_APP_ICON}"
                "${MIRRORSHIFT_DESKTOP_ASSET_DEST}/${MIRRORSHIFT_DESKTOP_APP_ICON}"
    )
    add_test(NAME pc-client-about-art
        COMMAND ${CMAKE_COMMAND} -E compare_files
                "${MIRRORSHIFT_DESKTOP_ASSET_SOURCE}/${MIRRORSHIFT_DESKTOP_ABOUT_ART}"
                "${MIRRORSHIFT_DESKTOP_ASSET_DEST}/${MIRRORSHIFT_DESKTOP_ABOUT_ART}"
    )

    if(WIN32)
        # Qt DLLs live outside the build tree; without this, ctest fails with
        # STATUS_DLL_NOT_FOUND (0xc0000135) unless the caller hand-edits PATH.
        set_tests_properties(pc-desktop-session-controller
                             pc-desktop-transport-codec
                             pc-save-game-store
                             pc-input-helpers
                             pc-direct-adapter
                             pc-mqtt-adapter
                             pc-mqtt-tx-failure PROPERTIES
            ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
        )
    endif()
endif()

set_target_properties(mirrorshift-client PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
    MACOSX_BUNDLE_BUNDLE_NAME "MirrorShift"
    MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.ignaciomonge.mirrorshift"
    MACOSX_BUNDLE_ICON_FILE "mirrorshift.icns"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${MIRRORSHIFT_BUNDLE_SHORT_VERSION}"
)

if(UNIX AND NOT APPLE)
    include(GNUInstallDirs)
    install(TARGETS mirrorshift-client
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
    install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/assets"
        DESTINATION "${CMAKE_INSTALL_BINDIR}")
    install(FILES io.github.ignaciomonge.mirrorshift.desktop
        DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
    install(FILES mirrorshift.png
        DESTINATION "${CMAKE_INSTALL_DATADIR}/pixmaps"
        RENAME mirrorshift.png)
    install(FILES ../LICENSE ../THIRD_PARTY_NOTICES.md
        DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/MirrorShift")
    install(DIRECTORY ../licenses/
        DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/MirrorShift/licenses")
endif()
