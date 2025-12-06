cmake_minimum_required(VERSION 3.24)

project(EngineeringWeekScheduler VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Concurrent)
find_package(Qt6 COMPONENTS Charts QUIET)

# OR-Tools is optional - if not found, a greedy fallback solver will be used
find_package(ortools QUIET)

set(QT_HAS_CHARTS FALSE)

if(ortools_FOUND)
    set(HAVE_OR_TOOLS TRUE)
    message(STATUS "OR-Tools found - using CBC solver")
else()
    message(STATUS "OR-Tools not found - using greedy fallback solver")
endif()

if(Qt6Charts_FOUND)
    set(QT_HAS_CHARTS TRUE)
    message(STATUS "Qt6::Charts target located")
else()
    message(STATUS "Qt6::Charts not found - chart visualizations will be disabled.")
endif()

set(QT_HAS_CHARTS FALSE)

add_subdirectory(src)
