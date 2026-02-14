#include "solver/SolverEngine.hpp"

#include "models/PreferenceModel.hpp"

#include <QElapsedTimer>
#include <QHash>
#include <QMap>
#include <QSet>
#include <QString>
#include <QStringList>

#include <algorithm>
#include <random>

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
    }
  }

  if (activities.empty()) {
    result.message = "No activities detected in the preference data.";
    return result;
  }

  const int periodCount = std::max(1, options.periodCount);
  result.totalStudents = static_cast<int>(rows.size()) * periodCount;
  QMap<QString, std::vector<int>> activityPeriodCapacityMap;
  for (const auto &activity : activities) {
    const std::string activityStdStr = toStdString(activity);
    std::vector<int> capacities(periodCount, options.defaultCapacity);
    auto it = options.activityPeriodCapacities.find(activityStdStr);
    if (it != options.activityPeriodCapacities.end() &&
        static_cast<int>(it->second.size()) == periodCount) {
      capacities = it->second;
    } else {
      auto legacyIt = options.activityCapacities.find(activityStdStr);
      if (legacyIt != options.activityCapacities.end()) {
        capacities.assign(periodCount, legacyIt->second);
      }
    }

    for (int periodIdx = 0; periodIdx < periodCount; ++periodIdx) {
      if (capacities[periodIdx] <= 0) {
        result.message = "Activity capacity must be greater than zero for " +
                         activityStdStr + " period " +
                         std::to_string(periodIdx + 1);
        return result;
      }
    }

    activityPeriodCapacityMap.insert(activity, capacities);
  }

  QElapsedTimer timer;
  timer.start();

  MPSolver solver("engineering_week_scheduler",
                  MPSolver::CBC_MIXED_INTEGER_PROGRAMMING);

  struct VarInfo {
    int studentIndex;
    int activityIndex;
    int periodIndex;
    int choiceRank;
    MPVariable *var;
    double objectiveCoefficient; // ADD THIS LINE
  };

  std::vector<std::vector<VarInfo>> varMatrix(rows.size());
  varMatrix.reserve(rows.size());

  std::vector<std::vector<operations_research::MPConstraint *>>
      activityPeriodConstraints(
          activities.size(), std::vector<operations_research::MPConstraint *>(
                                 periodCount, nullptr));
  for (int activityIdx = 0; activityIdx < static_cast<int>(activities.size());
       ++activityIdx) {
    const QString &activityName = activities[activityIdx];
    const auto capacities = activityPeriodCapacityMap.value(activityName);
    for (int periodIdx = 0; periodIdx < periodCount; ++periodIdx) {
      const int capacity = capacities[periodIdx];
      activityPeriodConstraints[activityIdx][periodIdx] =
          solver.MakeRowConstraint(0.0, static_cast<double>(capacity),
                                   QStringLiteral("activity_%1_period_%2")
                                       .arg(activityIdx)
                                       .arg(periodIdx)
                                       .toStdString());
    }
  }

  std::vector<std::vector<operations_research::MPConstraint *>>
      studentPeriodConstraints(rows.size(),
                               std::vector<operations_research::MPConstraint *>(
                                   periodCount, nullptr));
  std::vector<std::vector<operations_research::MPConstraint *>>
      studentActivityConstraints(
          rows.size(), std::vector<operations_research::MPConstraint *>(
                           activities.size(), nullptr));
  for (int studentIdx = 0; studentIdx < static_cast<int>(rows.size());
       ++studentIdx) {
    const auto &row = rows[studentIdx];
    // Each student MUST be assigned exactly 1 activity per period
    for (int periodIdx = 0; periodIdx < periodCount; ++periodIdx) {
      studentPeriodConstraints[studentIdx][periodIdx] =
          solver.MakeRowConstraint(1.0, 1.0,
                                   QStringLiteral("student_%1_period_%2")
                                       .arg(studentIdx)
                                       .arg(periodIdx)
                                       .toStdString());
    }

    // Each student can take an activity at most once across all periods.
    for (int activityIdx = 0; activityIdx < static_cast<int>(activities.size());
         ++activityIdx) {
      studentActivityConstraints[studentIdx][activityIdx] =
          solver.MakeRowConstraint(0.0, 1.0,
                                   QStringLiteral("student_%1_activity_%2")
                                       .arg(studentIdx)
                                       .arg(activityIdx)
                                       .toStdString());
    }

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

    // Create variables for ALL activities and periods
    for (int activityIdx = 0; activityIdx < static_cast<int>(activities.size());
         ++activityIdx) {
      for (int periodIdx = 0; periodIdx < periodCount; ++periodIdx) {
        auto *var = solver.MakeIntVar(0.0, 1.0,
                                      QStringLiteral("x_%1_%2_%3")
                                          .arg(studentIdx)
                                          .arg(activityIdx)
                                          .arg(periodIdx)
                                          .toStdString());
        studentPeriodConstraints[studentIdx][periodIdx]->SetCoefficient(var,
                                                                        1.0);
        studentActivityConstraints[studentIdx][activityIdx]->SetCoefficient(
            var, 1.0);
        activityPeriodConstraints[activityIdx][periodIdx]->SetCoefficient(var,
                                                                          1.0);

        // Determine weight/penalty
        int weight = 1;
        int choiceRank = -1;
        // Check if grade is 12 (double weight)
        const bool isGrade12 = (row.grade.trimmed() == "12");

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

        if (isGrade12) {
          weight *= 2;
        }

        // Apply attendance weighting
        double attendanceMultiplier = 1.0;
        QString present = row.present.trimmed().toLower();
        if (present == "yes" || present == "y") {
          // Normalize so "yes" = 1.0
          attendanceMultiplier = 1.0;
        } else if (present == "maybe" || present == "m") {
          // Scale by ratio to "yes" weight
          attendanceMultiplier =
              options.weightYes > 0
                  ? static_cast<double>(options.weightMaybe) / options.weightYes
                  : 0.5;
        } else if (present == "no" || present == "n") {
          // Scale by ratio to "yes" weight
          attendanceMultiplier =
              options.weightYes > 0
                  ? static_cast<double>(options.weightNo) / options.weightYes
                  : 0.1;
        } else {
          attendanceMultiplier =
              0; // If present field is empty or unrecognized, use 0.0
        }

        const double finalWeight =
            static_cast<double>(weight) * attendanceMultiplier;

        varMatrix[studentIdx].push_back(VarInfo{
            studentIdx, activityIdx, periodIdx, choiceRank, var, finalWeight});
        solver.MutableObjective()->SetCoefficient(var, finalWeight);
      }
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

  std::vector<std::vector<int>> activityAssignments(
      activities.size(), std::vector<int>(periodCount, 0));

  for (int studentIdx = 0; studentIdx < static_cast<int>(rows.size());
       ++studentIdx) {
    const auto &row = rows[studentIdx];
    std::vector<bool> assignedPeriod(periodCount, false);

    for (const auto &varInfo : varMatrix[studentIdx]) {
      if (varInfo.var->solution_value() < 0.5) {
        continue;
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
      assignment.activity = toStdString(activities[varInfo.activityIndex]);
      assignment.period = varInfo.periodIndex;
      assignment.choiceRank = varInfo.choiceRank;

      // Use the actual objective function coefficient
      assignment.score = varInfo.objectiveCoefficient;

      if (varInfo.choiceRank >= 0) {
        ++result.satisfiedStudents;
      } else {
        // Non-preferred fallback activity
        QString displayName = row.fullName();
        if (displayName.isEmpty()) {
          displayName = row.studentId;
        }
        result.warnings.push_back(
            QStringLiteral("Student %1 assigned to non-preferred activity: %2")
                .arg(displayName)
                .arg(activities[varInfo.activityIndex])
                .toStdString());
      }

      result.assignments.push_back(std::move(assignment));
      ++activityAssignments[varInfo.activityIndex][varInfo.periodIndex];
      if (varInfo.periodIndex >= 0 && varInfo.periodIndex < periodCount) {
        assignedPeriod[varInfo.periodIndex] = true;
      }
    }

    for (int periodIdx = 0; periodIdx < periodCount; ++periodIdx) {
      if (assignedPeriod[periodIdx]) {
        continue;
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
      assignment.activity = "ERROR: Not assigned";
      assignment.period = periodIdx;
      assignment.choiceRank = -1;
      assignment.score = 0.0;
      result.assignments.push_back(std::move(assignment));
      QString displayName = row.fullName();
      if (displayName.isEmpty()) {
        displayName = row.studentId;
      }
      result.warnings.push_back(
          QStringLiteral("ERROR: Student %1 could not be assigned in period %2 "
                         "(solver bug).")
              .arg(displayName)
              .arg(periodIdx + 1)
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
        QString present =
            QString::fromStdString(assignment.present).trimmed().toLower();
        if (present == "yes" || present == "y") {
          ++activityYes[idx];
        } else if (present == "no" || present == "n") {
          ++activityNo[idx];
        } else if (present == "maybe" || present == "m") {
          ++activityMaybe[idx];
        } else /* unknown status */ {
          ++activityUnknown[idx];
        }
        break;
      }
    }
  }

  for (int idx = 0; idx < static_cast<int>(activities.size()); ++idx) {
    ActivitySummaryRow summary;
    summary.activity = toStdString(activities[idx]);
    int totalAssigned = 0;
    for (int periodIdx = 0; periodIdx < periodCount; ++periodIdx) {
      totalAssigned += activityAssignments[idx][periodIdx];
    }
    summary.assigned = totalAssigned;
    const auto capacities = activityPeriodCapacityMap.value(activities[idx]);
    int totalCapacity = 0;
    for (int periodIdx = 0; periodIdx < periodCount; ++periodIdx) {
      totalCapacity += capacities[periodIdx];
    }
    summary.capacity = totalCapacity;
    summary.assignedPerPeriod = activityAssignments[idx];
    summary.capacityPerPeriod = capacities;
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
  const int periodCount = std::max(1, options.periodCount);
  result.totalStudents = static_cast<int>(rows.size()) * periodCount;

  QElapsedTimer timer;
  timer.start();

  QHash<QString, std::vector<int>> usage;
  QHash<QString, std::vector<int>> activityCapacityMap;
  QSet<QString> activities;
  for (const auto &row : rows) {
    for (const auto &choice : row.choices) {
      const auto normalized = choice.trimmed();
      if (!normalized.isEmpty()) {
        activities.insert(normalized);
      }
    }
  }

  for (const auto &activity : activities) {
    const std::string activityStdStr = toStdString(activity);
    std::vector<int> capacities(periodCount, options.defaultCapacity);
    auto it = options.activityPeriodCapacities.find(activityStdStr);
    if (it != options.activityPeriodCapacities.end() &&
        static_cast<int>(it->second.size()) == periodCount) {
      capacities = it->second;
    } else {
      auto legacyIt = options.activityCapacities.find(activityStdStr);
      if (legacyIt != options.activityCapacities.end()) {
        capacities.assign(periodCount, legacyIt->second);
      }
    }
    for (int periodIdx = 0; periodIdx < periodCount; ++periodIdx) {
      if (capacities[periodIdx] <= 0) {
        result.message = "Activity capacity must be greater than zero.";
        return result;
      }
    }
    activityCapacityMap.insert(activity, capacities);
    usage.insert(activity, std::vector<int>(periodCount, 0));
  }

  QStringList activityList = activities.values();
  std::sort(activityList.begin(), activityList.end(),
            [](const QString &a, const QString &b) {
              return a.localeAwareCompare(b) < 0;
            });

  for (const auto &row : rows) {
    QSet<QString> assignedActivities;
    for (int periodIdx = 0; periodIdx < periodCount; ++periodIdx) {
      bool assigned = false;
      for (int choiceIdx = 0; choiceIdx < row.choices.size(); ++choiceIdx) {
        const auto activityName = row.choices[choiceIdx].trimmed();
        if (activityName.isEmpty()) {
          continue;
        }
        if (assignedActivities.contains(activityName)) {
          continue;
        }
        auto usageIt = usage.find(activityName);
        auto capacityIt = activityCapacityMap.find(activityName);
        if (usageIt == usage.end() || capacityIt == activityCapacityMap.end()) {
          continue;
        }

        if (usageIt.value()[periodIdx] >= capacityIt.value()[periodIdx]) {
          continue;
        }
        usageIt.value()[periodIdx] += 1;

        // Calculate attendance weight multiplier
        double attendanceMultiplier = 1.0;
        QString present = row.present.trimmed().toLower();
        if (present == "yes" || present == "y") {
          attendanceMultiplier = 1.0;
        } else if (present == "maybe" || present == "m") {
          attendanceMultiplier =
              options.weightYes > 0
                  ? static_cast<double>(options.weightMaybe) / options.weightYes
                  : 0.5;
        } else if (present == "no" || present == "n") {
          attendanceMultiplier =
              options.weightYes > 0
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
        assignment.period = periodIdx;
        assignment.choiceRank = choiceIdx;
        int baseWeight = weightForRank(choiceIdx);
        if (row.grade.trimmed() == "12") {
          baseWeight *= 2;
        }
        assignment.score = baseWeight * attendanceMultiplier;
        result.assignments.push_back(std::move(assignment));
        ++result.satisfiedStudents;
        assignedActivities.insert(activityName);
        assigned = true;
        break;
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
        assignment.period = periodIdx;
        assignment.choiceRank = -1;
        assignment.score = 0.0;
        result.assignments.push_back(std::move(assignment));
        QString displayName = row.fullName();
        if (displayName.isEmpty()) {
          displayName = row.studentId;
        }
        result.warnings.push_back(
            QStringLiteral(
                "Student %1 could not be greedily assigned in period %2.")
                .arg(displayName)
                .arg(periodIdx + 1)
                .toStdString());
      }
    }
  }

  // Count yes/no/maybe for each activity
  QMap<QString, int> activityYes;
  QMap<QString, int> activityNo;
  QMap<QString, int> activityMaybe;

  for (const auto &assignment : result.assignments) {
    QString activityName = QString::fromStdString(assignment.activity);
    QString present =
        QString::fromStdString(assignment.present).trimmed().toLower();

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
    const auto assignedPerPeriod = usage.value(activityName);
    const auto capacityPerPeriod = activityCapacityMap.value(activityName);
    int totalAssigned = 0;
    int totalCapacity = 0;
    for (int periodIdx = 0; periodIdx < periodCount; ++periodIdx) {
      totalAssigned += assignedPerPeriod[periodIdx];
      totalCapacity += capacityPerPeriod[periodIdx];
    }
    summary.assigned = totalAssigned;
    summary.capacity = totalCapacity;
    summary.assignedPerPeriod = assignedPerPeriod;
    summary.capacityPerPeriod = capacityPerPeriod;
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
  std::vector<StudentPreferenceRow> shuffledRows = rows;
  if (shuffledRows.size() > 1) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::shuffle(shuffledRows.begin(), shuffledRows.end(), gen);
  }
  return runWithOrTools(shuffledRows, options);
#else
  std::vector<StudentPreferenceRow> shuffledRows = rows;
  if (shuffledRows.size() > 1) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::shuffle(shuffledRows.begin(), shuffledRows.end(), gen);
  }
  return runGreedySolver(shuffledRows, options);
#endif
}
