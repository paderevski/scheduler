set(SCHEDULER_SOURCES
    main.cpp
    ui/MainWindow.cpp
    models/PreferenceModel.cpp
    solver/SolverEngine.cpp
    utils/SpreadsheetBridge.cpp
)

set(SCHEDULER_HEADERS
    ${CMAKE_SOURCE_DIR}/include/ui/MainWindow.hpp
    ${CMAKE_SOURCE_DIR}/include/models/PreferenceModel.hpp
    ${CMAKE_SOURCE_DIR}/include/solver/SolverEngine.hpp
    ${CMAKE_SOURCE_DIR}/include/solver/SolverTypes.hpp
    ${CMAKE_SOURCE_DIR}/include/utils/SpreadsheetBridge.hpp
)

add_executable(EngineeringWeekScheduler
    ${SCHEDULER_SOURCES}
    ${SCHEDULER_HEADERS}
)

target_include_directories(EngineeringWeekScheduler
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(EngineeringWeekScheduler
    PRIVATE
    Qt6::Widgets
    Qt6::Concurrent
)

if(ortools_FOUND)
    target_link_libraries(EngineeringWeekScheduler PRIVATE ortools::ortools)
    target_compile_definitions(EngineeringWeekScheduler PRIVATE HAVE_OR_TOOLS=1)
else()
    target_compile_definitions(EngineeringWeekScheduler PRIVATE HAVE_OR_TOOLS=0)
endif()

target_compile_features(EngineeringWeekScheduler PRIVATE cxx_std_20)
