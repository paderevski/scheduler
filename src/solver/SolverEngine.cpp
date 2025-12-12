#include "solver/SolverEngine.hpp"

#include "models/PreferenceModel.hpp"

#include <QElapsedTimer>
#include <QHash>
#include <QMap>
#include <QSet>
#include <QString>
#include <QStringList>

#include <algorithm>

#if HAVE_OR_TOOLS
#include "ortools/linear_solver/linear_solver.h"
#endif

namespace {

std::string toStdString(const QString &value) {
  return value.trimmed().toStdString();
}

int weightForRank(int rank) {
  constexpr int kBase = 100;
  constexpr int kDecay = 15;
  const int weight = kBase - (rank * kDecay);
  return weight > 5 ? weight : 5;
}

#if HAVE_OR_TOOLS
using operations_research::MPSolver;
using operations_research::MPVariable;

SolverResult runWithOrTools(const std::vector<StudentPreferenceRow> &rows,
                            const SolverOptions &options) {
  SolverResult result;
  result.totalStudents = static_cast<int>(rows.size());

  if (rows.empty()) {
    result.message = "No student preferences loaded.";
    return result;
  }

  // Build activity list and determine capacities
  QMap<QString, int> activityIndex;
  QMap<QString, int> activityCapacityMap;
  std::vector<QString> activities;
  activities.reserve(32);

  for (const auto &row : rows) {
    for (const auto &choice : row.choices) {
      const auto normalized = choice.trimmed();
      if (normalized.isEmpty() || activityIndex.contains(normalized)) {
        continue;
      }
      const int nextIndex = static_cast<int>(activities.size());
      activityIndex.insert(normalized, nextIndex);
      activities.push_back(normalized);

      // Look up capacity for this activity
      const std::string activityStdStr = toStdString(normalized);
      int capacity = options.defaultCapacity;
      auto it = options.activityCapacities.find(activityStdStr);
      if (it != options.activityCapacities.end()) {
        capacity = it->second;
      }

      if (capacity <= 0) {
        result.message =
            "Activity capacity must be greater than zero for " + activityStdStr;
        return result;
      }

      activityCapacityMap.insert(normalized, capacity);
    }
  }

  if (activities.empty()) {
    result.message = "No activities detected in the preference data.";
    return result;
  }

  QElapsedTimer timer;
  timer.start();

  MPSolver solver("engineering_week_scheduler",
                  MPSolver::CBC_MIXED_INTEGER_PROGRAMMING);

  struct VarInfo {
    int studentIndex;
    int activityIndex;
    int choiceRank;
    MPVariable *var;
  };

  std::vector<std::vector<VarInfo>> varMatrix(rows.size());
  varMatrix.reserve(rows.size());

  std::vector<operations_research::MPConstraint *> activityConstraints(
      activities.size(), nullptr);
  for (int i = 0; i < static_cast<int>(activities.size()); ++i) {
    const QString &activityName = activities[i];
    const int capacity =
        activityCapacityMap.value(activityName, options.defaultCapacity);
    activityConstraints[i] = solver.MakeRowConstraint(
        0.0, static_cast<double>(capacity),
        QStringLiteral("activity_%1").arg(i).toStdString());
  }

  std::vector<operations_research::MPConstraint *> studentConstraints(
      rows.size(), nullptr);
  for (int studentIdx = 0; studentIdx < static_cast<int>(rows.size());
       ++studentIdx) {
    const auto &row = rows[studentIdx];
    // Each student MUST be assigned exactly 1 activity
    auto *constraint = solver.MakeRowConstraint(
        1.0, 1.0, QStringLiteral("student_%1").arg(studentIdx).toStdString());
    studentConstraints[studentIdx] = constraint;

    // Build a set of preferred activities for this student
    QSet<int> preferredActivityIndices;
    const auto &choices = row.choices;
    for (int choiceIdx = 0; choiceIdx < choices.size(); ++choiceIdx) {
      const auto activityName = choices[choiceIdx].trimmed();
      if (activityName.isEmpty()) {
        continue;
      }
      const int activityIdx = activityIndex.value(activityName, -1);
      if (activityIdx >= 0) {
        preferredActivityIndices.insert(activityIdx);
      }
    }

    // Create variables for ALL activities
    for (int activityIdx = 0; activityIdx < static_cast<int>(activities.size());
         ++activityIdx) {
      auto *var = solver.MakeIntVar(0.0, 1.0,
                                    QStringLiteral("x_%1_%2")
                                        .arg(studentIdx)
                                        .arg(activityIdx)
                                        .toStdString());
      constraint->SetCoefficient(var, 1.0);
      activityConstraints[activityIdx]->SetCoefficient(var, 1.0);

      // Determine weight/penalty
      int weight;
      int choiceRank = -1;

      // Check if this activity is in the student's preferences
      const QString &activityName = activities[activityIdx];
      for (int choiceIdx = 0; choiceIdx < choices.size(); ++choiceIdx) {
        if (choices[choiceIdx].trimmed() == activityName) {
          // Preferred activity - high weight
          weight = weightForRank(choiceIdx);
          choiceRank = choiceIdx;
          break;
        }
      }

      if (choiceRank < 0) {
        // Non-preferred activity - low weight (penalty)
        // Use weight of 1 so it's only chosen as last resort
        weight = 1;
        choiceRank = -1; // Mark as non-preferred
      }

      // Apply attendance weighting
      double attendanceMultiplier = 1.0;
      QString present = row.present.trimmed().toLower();
      if (present == "yes" || present == "y") {
        // Normalize so "yes" = 1.0
        attendanceMultiplier = 1.0;
      } else if (present == "maybe" || present == "m") {
        // Scale by ratio to "yes" weight
        attendanceMultiplier = options.weightYes > 0
            ? static_cast<double>(options.weightMaybe) / options.weightYes
            : 0.5;
      } else if (present == "no" || present == "n") {
        // Scale by ratio to "yes" weight
        attendanceMultiplier = options.weightYes > 0
            ? static_cast<double>(options.weightNo) / options.weightYes
            : 0.1;
      } else {
        attendanceMultiplier = 0;       // If present field is empty or unrecognized, use 0.0
      }

      varMatrix[studentIdx].push_back(
          VarInfo{studentIdx, activityIdx, choiceRank, var});
      solver.MutableObjective()->SetCoefficient(
          var, static_cast<double>(weight) * attendanceMultiplier);
    }
  }

  solver.MutableObjective()->SetMaximization();
  auto status = solver.Solve();
  result.runtimeMs = timer.elapsed();

  if (status != MPSolver::OPTIMAL && status != MPSolver::FEASIBLE) {
    result.message = "Solver was unable to find a feasible solution.";
    return result;
  }

  result.success = true;
  result.objectiveValue = solver.Objective().Value();
  result.message = "Solver completed successfully.";

  std::vector<int> activityAssignments(activities.size(), 0);

  for (int studentIdx = 0; studentIdx < static_cast<int>(rows.size());
       ++studentIdx) {
    const auto &row = rows[studentIdx];
    bool assigned = false;

    for (const auto &varInfo : varMatrix[studentIdx]) {
      if (varInfo.var->solution_value() >= 0.5) {
        StudentAssignment assignment;
        assignment.studentId = toStdString(row.studentId);
        assignment.firstName = toStdString(row.firstName);
        assignment.lastName = toStdString(row.lastName);
        assignment.grade = toStdString(row.grade);
        assignment.day = toStdString(row.day);
        assignment.teacher = toStdString(row.teacher);
        assignment.pathway = toStdString(row.pathway);
        assignment.present = toStdString(row.present);
        assignment.activity = toStdString(activities[varInfo.activityIndex]);
        assignment.choiceRank = varInfo.choiceRank;

        // Calculate score based on whether it was a preferred choice
        if (varInfo.choiceRank >= 0) {
          // Preferred activity
          assignment.score =
              varInfo.var->solution_value() * weightForRank(varInfo.choiceRank);
          ++result.satisfiedStudents;
        } else {
          // Non-preferred fallback activity
          assignment.score = varInfo.var->solution_value() * 1.0;
          QString displayName = row.fullName();
          if (displayName.isEmpty()) {
            displayName = row.studentId;
          }
          result.warnings.push_back(
              QStringLiteral(
                  "Student %1 assigned to non-preferred activity: %2")
                  .arg(displayName)
                  .arg(activities[varInfo.activityIndex])
                  .toStdString());
        }

        result.assignments.push_back(std::move(assignment));
        ++activityAssignments[varInfo.activityIndex];
        assigned = true;
        break;
      }
    }

    if (!assigned) {
      // This should never happen with the constraint requiring exactly 1
      // assignment
      StudentAssignment assignment;
      assignment.studentId = toStdString(row.studentId);
      assignment.firstName = toStdString(row.firstName);
      assignment.lastName = toStdString(row.lastName);
      assignment.grade = toStdString(row.grade);
      assignment.day = toStdString(row.day);
      assignment.teacher = toStdString(row.teacher);
      assignment.pathway = toStdString(row.pathway);
      assignment.present = toStdString(row.present);
      assignment.activity = "ERROR: Not assigned";
      assignment.choiceRank = -1;
      assignment.score = 0.0;
      result.assignments.push_back(std::move(assignment));
      QString displayName = row.fullName();
      if (displayName.isEmpty()) {
        displayName = row.studentId;
      }
      result.warnings.push_back(
          QStringLiteral(
              "ERROR: Student %1 could not be assigned (solver bug).")
              .arg(displayName)
              .toStdString());
    }
  }

  // Count yes/no/maybe for each activity
  std::vector<int> activityYes(activities.size(), 0);
  std::vector<int> activityNo(activities.size(), 0);
  std::vector<int> activityMaybe(activities.size(), 0);
  std::vector<int> activityUnknown(activities.size(), 0);

  for (const auto &assignment : result.assignments) {
    // Find which activity index this assignment belongs to
    for (int idx = 0; idx < static_cast<int>(activities.size()); ++idx) {
      if (assignment.activity == toStdString(activities[idx])) {
        QString present = QString::fromStdString(assignment.present).trimmed().toLower();
        if (present == "yes" || present == "y") {
          ++activityYes[idx];
        } else if (present == "no" || present == "n") {
          ++activityNo[idx];
        } else if (present == "maybe" || present == "m") {
          ++activityMaybe[idx];
        } else /* unknown status */{
          ++activityUnknown[idx];
        }
        break;
      }
    }
  }

  for (int idx = 0; idx < static_cast<int>(activities.size()); ++idx) {
    ActivitySummaryRow summary;
    summary.activity = toStdString(activities[idx]);
    summary.assigned = activityAssignments[idx];
    summary.capacity =
        activityCapacityMap.value(activities[idx], options.defaultCapacity);
    summary.presentYes = activityYes[idx];
    summary.presentNo = activityNo[idx];
    summary.presentMaybe = activityMaybe[idx];
    result.activitySummary.push_back(std::move(summary));
  }

  return result;
}
#else
SolverResult runGreedySolver(const std::vector<StudentPreferenceRow> &rows,
                             const SolverOptions &options) {
  SolverResult result;
  result.totalStudents = static_cast<int>(rows.size());

  if (rows.empty()) {
    result.message = "No student preferences loaded.";
    return result;
  }
  if (options.activityCapacity <= 0) {
    result.message = "Activity capacity must be greater than zero.";
    return result;
  }

  QElapsedTimer timer;
  timer.start();

  QHash<QString, int> usage;
  QSet<QString> activities;
  for (const auto &row : rows) {
    for (const auto &choice : row.choices) {
      const auto normalized = choice.trimmed();
      if (!normalized.isEmpty()) {
        activities.insert(normalized);
      }
    }
  }

  QStringList activityList = activities.values();
  std::sort(activityList.begin(), activityList.end(),
            [](const QString &a, const QString &b) {
              return a.localeAwareCompare(b) < 0;
            });

  for (const auto &row : rows) {
    bool assigned = false;
    for (int choiceIdx = 0; choiceIdx < row.choices.size(); ++choiceIdx) {
      const auto activityName = row.choices[choiceIdx].trimmed();
      if (activityName.isEmpty()) {
        continue;
      }
      if (usage.value(activityName, 0) < options.activityCapacity) {
        usage[activityName] += 1;

        // Calculate attendance weight multiplier
        double attendanceMultiplier = 1.0;
        QString present = row.present.trimmed().toLower();
        if (present == "yes" || present == "y") {
          attendanceMultiplier = 1.0;
        } else if (present == "maybe" || present == "m") {
          attendanceMultiplier = options.weightYes > 0
              ? static_cast<double>(options.weightMaybe) / options.weightYes
              : 0.5;
        } else if (present == "no" || present == "n") {
          attendanceMultiplier = options.weightYes > 0
              ? static_cast<double>(options.weightNo) / options.weightYes
              : 0.1;
        }

        StudentAssignment assignment;
        assignment.studentId = toStdString(row.studentId);
        assignment.firstName = toStdString(row.firstName);
        assignment.lastName = toStdString(row.lastName);
        assignment.grade = toStdString(row.grade);
        assignment.day = toStdString(row.day);
        assignment.teacher = toStdString(row.teacher);
        assignment.pathway = toStdString(row.pathway);
        assignment.present = toStdString(row.present);
        assignment.activity = toStdString(activityName);
        assignment.choiceRank = choiceIdx;
        assignment.score = weightForRank(choiceIdx) * attendanceMultiplier;
        result.assignments.push_back(std::move(assignment));
        ++result.satisfiedStudents;
        assigned = true;
        break;
      }
    }

    if (!assigned) {
      StudentAssignment assignment;
      assignment.studentId = toStdString(row.studentId);
      assignment.firstName = toStdString(row.firstName);
      assignment.lastName = toStdString(row.lastName);
      assignment.grade = toStdString(row.grade);
      assignment.day = toStdString(row.day);
      assignment.teacher = toStdString(row.teacher);
      assignment.pathway = toStdString(row.pathway);
      assignment.present = toStdString(row.present);
      assignment.activity = "";
      assignment.choiceRank = -1;
      assignment.score = 0.0;
      result.assignments.push_back(std::move(assignment));
      QString displayName = row.fullName();
      if (displayName.isEmpty()) {
        displayName = row.studentId;
      }
      result.warnings.push_back(
          QStringLiteral("Student %1 could not be greedily assigned.")
              .arg(displayName)
              .toStdString());
    }
  }

  // Count yes/no/maybe for each activity
  QMap<QString, int> activityYes;
  QMap<QString, int> activityNo;
  QMap<QString, int> activityMaybe;

  for (const auto &assignment : result.assignments) {
    QString activityName = QString::fromStdString(assignment.activity);
    QString present = QString::fromStdString(assignment.present).trimmed().toLower();

    if (present == "yes" || present == "y") {
      activityYes[activityName]++;
    } else if (present == "no" || present == "n") {
      activityNo[activityName]++;
    } else if (present == "maybe" || present == "m") {
      activityMaybe[activityName]++;
    }
  }

  for (const auto &activityName : activityList) {
    ActivitySummaryRow summary;
    summary.activity = toStdString(activityName);
    summary.assigned = usage.value(activityName, 0);
    summary.capacity = options.activityCapacity;
    summary.presentYes = activityYes.value(activityName, 0);
    summary.presentNo = activityNo.value(activityName, 0);
    summary.presentMaybe = activityMaybe.value(activityName, 0);
    result.activitySummary.push_back(std::move(summary));
  }

  result.success = true;
  result.objectiveValue = static_cast<double>(result.satisfiedStudents);
  result.runtimeMs = timer.elapsed();
  result.message = "Greedy fallback solver completed (OR-Tools not available).";
  return result;
}
#endif

} // namespace

SolverResult runSolver(const std::vector<StudentPreferenceRow> &rows,
                       const SolverOptions &options) {
#if HAVE_OR_TOOLS
  return runWithOrTools(rows, options);
#else
  return runGreedySolver(rows, options);
#endif
}
