# Copilot instructions

## Project overview

- Desktop app built with Qt6 Widgets; entry point is [src/main.cpp](src/main.cpp).
- Main UI is the monolithic controller in [src/ui/MainWindow.cpp](src/ui/MainWindow.cpp) (tabbed UI, CSV import/export, solver launch, results rendering).
- Student data lives in `PreferenceModel` (Qt `QAbstractTableModel`) with validation coloring; see [include/models/PreferenceModel.hpp](include/models/PreferenceModel.hpp) and [src/models/PreferenceModel.cpp](src/models/PreferenceModel.cpp).
- CSV I/O is centralized in `SpreadsheetBridge`; only CSV is supported by `ReadFile` today. See [include/utils/SpreadsheetBridge.hpp](include/utils/SpreadsheetBridge.hpp) and [src/utils/SpreadsheetBridge.cpp](src/utils/SpreadsheetBridge.cpp).

## Solver architecture

- `runSolver()` is the single entry point; it selects OR-Tools CBC when available, otherwise a greedy fallback. See [include/solver/SolverEngine.hpp](include/solver/SolverEngine.hpp) and [src/solver/SolverEngine.cpp](src/solver/SolverEngine.cpp).
- The OR-Tools path enforces “one activity per student per period” with capacity constraints; weights are based on choice rank, grade 12 bonus, and attendance weights.
- The greedy fallback assigns in preference order per period and logs warnings when assignments are impossible.
- Solver configuration lives in `SolverOptions` (period count, per-activity/per-period capacities, attendance weights). See [include/solver/SolverTypes.hpp](include/solver/SolverTypes.hpp).
- Solver runs asynchronously via `QtConcurrent::run` with a `QFutureWatcher` in the MainWindow; search for the `runSolver` call in [src/ui/MainWindow.cpp](src/ui/MainWindow.cpp).

## Build and run

- CMake + C++20. Top-level config in [CMakeLists.txt](CMakeLists.txt) and target setup in [src/CMakeLists.txt](src/CMakeLists.txt).
- Required: Qt6 Widgets/Concurrent. Optional: Qt6 Charts and OR-Tools (enables CBC solver).
- Build example (from README):
  - `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release`
  - `cmake --build build`
  - Run: `./build/ClickSort`

## Project conventions and patterns

- UI logic is kept inside the `MainWindow::Impl` PIMPL; most features are implemented in one file ([src/ui/MainWindow.cpp](src/ui/MainWindow.cpp)).
- Data flow is CSV -> `SpreadsheetBridge` -> `PreferenceModel` -> `runSolver` -> results table/export; keep new features aligned with this pipeline.
- Validation feedback in the table model uses `Qt::BackgroundRole` and cached duplicate ID checks; invalidate the cache when mutating rows.
- Feature toggles are compile-time via `HAVE_OR_TOOLS` and `HAVE_QT_CHARTS` (set in CMake).
