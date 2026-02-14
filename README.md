# ClickSort (C++/Qt Application)

This repository is the starting point for a Qt-based desktop application that ingests student preference data (CSV), runs an OR-Tools optimization model, and produces session assignments for Engineering Week.

## Current status

- ✅ Project scaffolding with CMake, Qt 6 (Widgets, optional Charts) and OR-Tools linkage stubs
- ✅ Baseline GUI with a tabbed interface plus CSV import preview
- ✅ Editable table model with validation-aware highlighting and diagnostics summary
- ✅ OR-Tools solver integration running on a background thread with progress feedback, results tables, and optional Qt Charts utilization views
- ✅ CSV import/export flows and basic UX polish

## Prerequisites

1. **Qt 6.5 or newer** (Widgets required, Charts optional but recommended for the Results visualization). Install via the Qt Maintenance Tool or your package manager and set `CMAKE_PREFIX_PATH` accordingly. If Qt Charts is missing, the Results tab gracefully falls back to table summaries only.
2. **OR-Tools** C++ distribution with CMake package exports. Download from the [official releases](https://developers.google.com/optimization) and install/extract somewhere stable.
3. A C++20 capable compiler (Clang 14+, GCC 11+, MSVC 2022).

Ensure `CMAKE_PREFIX_PATH` contains the Qt and OR-Tools installation directories before configuring the project.

```bash
export CMAKE_PREFIX_PATH="/path/to/Qt/6.5.0/gcc_64/lib/cmake:/path/to/ortools/lib/cmake"
```

## Configure & build

```bash
cd ~/github/projects/engineering-week-scheduler
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

To run the prototype after a successful build:

```bash
./build/ClickSort
```

## Feature highlights

- **Data ingestion/export**: load CSV preference sheets, edit them directly, and export back to CSV.
- **Validation feedback**: missing IDs or blank choices are highlighted inline and surfaced in the diagnostics list.
- **Background solver**: the Calculate action spawns an OR-Tools CBC model on a background thread, keeping the UI responsive while reporting progress.
- **Guaranteed assignments**: Every student is assigned to an activity. The solver tries to match students to their top 3-5 choices (weights decay by 15 per rank starting at 100, minimum 5), but if capacity constraints prevent this, students are assigned to non-preferred activities (weight 1) as a fallback. This ensures no student is left unassigned.
- **Results presentation**: assignments are rendered in a sortable table with per-activity summaries, utilization metrics, and optional charts when Qt Charts is available. Results can also be exported to CSV.

## Solver Model

The OR-Tools CBC mixed-integer programming solver:

- **Decision variables**: Binary variables for each student-activity-period pair (1 = assigned, 0 = not assigned)
- **Constraints**:
  - Each student must be assigned to exactly 1 activity
  - Each activity-period has a maximum capacity (configurable)
- **Objective**: Maximize total satisfaction by weighting preferred choices higher
  - Choice 1: weight 100
  - Choice 2: weight 85
  - Choice 3: weight 70
  - Choice 4: weight 55
  - Choice 5: weight 40
  - Non-preferred: weight 1 (fallback only when necessary)
- **Result**: All students get assigned, with preferences maximized subject to capacity constraints

## Next steps

- Support per-activity capacity overrides and additional scheduling constraints (time slots, activity compatibility, etc.).
- Persist solver configurations and diagnostics between sessions; add richer analytics/visuals once Qt Charts is present.
- Extend export flows (PDF summaries, per-activity rosters).
- Add cancellation controls, fine-grained progress updates, and more robust error messaging for solver edge cases.

## Repository structure

```
include/          # Public headers (models, UI stubs)
resources/        # Placeholder for future Qt resources (icons, translations)
src/              # Application sources (main, models, UI)
```

Feel free to iterate on this scaffold and extend the solver/visualization capabilities as requirements evolve.
