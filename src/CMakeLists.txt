set(SCHEDULER_SOURCES
    main.cpp
    ui/MainWindow.cpp
    ui/ColumnMapDialog.cpp
    models/PreferenceModel.cpp
    solver/SolverEngine.cpp
    utils/SpreadsheetBridge.cpp
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
