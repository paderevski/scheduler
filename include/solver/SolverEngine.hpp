#pragma once

#include "SolverTypes.hpp"

SolverResult runSolver(const std::vector<StudentPreferenceRow> &rows,
                       const SolverOptions &options);

bool interruptSolver();
