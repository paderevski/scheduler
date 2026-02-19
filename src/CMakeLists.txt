set(SCHEDULER_SOURCES
    main.cpp
    ui/MainWindow.cpp
    ui/ColumnMapDialog.cpp
    models/PreferenceModel.cpp
    solver/SolverEngine.cpp
    utils/SpreadsheetBridge.cpp
    resources/images.qrc
)

set(SCHEDULER_HEADERS
    ${CMAKE_SOURCE_DIR}/include/ui/MainWindow.hpp
    ${CMAKE_SOURCE_DIR}/include/ui/ColumnMapDialog.hpp
    ${CMAKE_SOURCE_DIR}/include/models/PreferenceModel.hpp
    ${CMAKE_SOURCE_DIR}/include/solver/SolverEngine.hpp
    ${CMAKE_SOURCE_DIR}/include/solver/SolverTypes.hpp
    ${CMAKE_SOURCE_DIR}/include/utils/SpreadsheetBridge.hpp
)

add_executable(ClickSort
    ${SCHEDULER_SOURCES}
    ${SCHEDULER_HEADERS}
)

if(APPLE)
    set_target_properties(ClickSort PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "ClickSort"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.clicksort.app"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_INFO_STRING "ClickSort"
        MACOSX_BUNDLE_COPYRIGHT "Copyright (c)"
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.clicksort.app"
    )

    set(CLICK_SORT_ICON_PATH "" CACHE FILEPATH "Path to ClickSort .icns icon")
    if(NOT CLICK_SORT_ICON_PATH AND EXISTS "${CMAKE_SOURCE_DIR}/resources/ClickSort.icns")
        set(CLICK_SORT_ICON_PATH "${CMAKE_SOURCE_DIR}/resources/ClickSort.icns"
            CACHE FILEPATH "Path to ClickSort .icns icon" FORCE)
    endif()
    if(CLICK_SORT_ICON_PATH)
        get_filename_component(CLICK_SORT_ICON_NAME "${CLICK_SORT_ICON_PATH}" NAME)
        set_source_files_properties("${CLICK_SORT_ICON_PATH}" PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources"
        )
        target_sources(ClickSort PRIVATE "${CLICK_SORT_ICON_PATH}")
        set_target_properties(ClickSort PROPERTIES
            MACOSX_BUNDLE_ICON_FILE "${CLICK_SORT_ICON_NAME}"
        )
    endif()

    set(QT_LIB_DIR "")
    if(Qt6_DIR)
        set(_qt_dir_guess "${Qt6_DIR}/../..")
        if(EXISTS "${_qt_dir_guess}")
            set(QT_LIB_DIR "${_qt_dir_guess}")
        endif()
    endif()

    if(NOT QT_LIB_DIR)
        get_target_property(_qt_core_location Qt6::Core IMPORTED_LOCATION_RELEASE)
        if(NOT _qt_core_location)
            get_target_property(_qt_core_location Qt6::Core IMPORTED_LOCATION)
        endif()
        if(_qt_core_location)
            get_filename_component(_qt_framework_dir "${_qt_core_location}" DIRECTORY)
            get_filename_component(_qt_lib_guess "${_qt_framework_dir}" DIRECTORY)
            if(EXISTS "${_qt_lib_guess}")
                set(QT_LIB_DIR "${_qt_lib_guess}")
            endif()
        endif()
    endif()

    if(NOT QT_LIB_DIR)
        find_program(QMAKE_EXECUTABLE qmake
            HINTS "${Qt6_DIR}/../../../bin" "${Qt6_DIR}/../../../"
        )
        if(QMAKE_EXECUTABLE)
            execute_process(
                COMMAND "${QMAKE_EXECUTABLE}" -query QT_INSTALL_LIBS
                OUTPUT_VARIABLE _qt_libs
                OUTPUT_STRIP_TRAILING_WHITESPACE
            )
            if(_qt_libs AND EXISTS "${_qt_libs}")
                set(QT_LIB_DIR "${_qt_libs}")
            endif()
        endif()
    endif()
    find_program(MACDEPLOYQT_EXECUTABLE macdeployqt
        HINTS "${Qt6_DIR}/../../../bin" "${Qt6_DIR}/../../../"
    )
    if(MACDEPLOYQT_EXECUTABLE)
        set(MACDEPLOYQT_LIBPATH_ARGS "")
        if(QT_LIB_DIR AND EXISTS "${QT_LIB_DIR}")
            list(APPEND MACDEPLOYQT_LIBPATH_ARGS -libpath "${QT_LIB_DIR}")
        endif()
        add_custom_target(ClickSortDeploy
            COMMAND "${MACDEPLOYQT_EXECUTABLE}" "$<TARGET_BUNDLE_DIR:ClickSort>"
                ${MACDEPLOYQT_LIBPATH_ARGS} -verbose=1
            DEPENDS ClickSort
            COMMENT "Running macdeployqt on ClickSort.app"
        )
    endif()
endif()

target_include_directories(ClickSort
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(ClickSort
    PRIVATE
    Qt6::Widgets
    Qt6::Concurrent
)

if(ortools_FOUND)
    target_link_libraries(ClickSort PRIVATE ortools::ortools)
    target_compile_definitions(ClickSort PRIVATE HAVE_OR_TOOLS=1)
else()
    target_compile_definitions(ClickSort PRIVATE HAVE_OR_TOOLS=0)
endif()

# Optional: enable Qt Charts when found by top-level CMake
if(QT_HAS_CHARTS)
    target_link_libraries(ClickSort PRIVATE Qt6::Charts)
    target_compile_definitions(ClickSort PRIVATE HAVE_QT_CHARTS=1)
else()
    target_compile_definitions(ClickSort PRIVATE HAVE_QT_CHARTS=0)
endif()

target_compile_features(ClickSort PRIVATE cxx_std_20)
