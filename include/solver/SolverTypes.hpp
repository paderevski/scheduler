#pragma once

#include <map>
#include <string>
#include <vector>

struct StudentPreferenceRow;

struct SolverOptions {
  // Map from activity name to capacity
  // If empty, uses defaultCapacity for all activities
  std::map<std::string, int> activityCapacities;
  int defaultCapacity = 20; // Used when activityCapacities is empty

  // Period-aware capacities (preferred when periodCount > 0)
  int periodCount = 3;
  std::vector<std::string> periodLabels;
  std::map<std::string, std::vector<int>> activityPeriodCapacities;

  // Attendance weights (raw values, will be normalized so "yes" = 1.0)
  int weightYes = 100;
  int weightMaybe = 50;
  int weightNo = 10;
};

struct StudentAssignment {
  std::string studentId;
  std::string firstName;
  std::string lastName;
  std::string grade;
  std::string day;
  std::string teacher;
  std::string pathway;
  std::string present;
  std::string activity;
  int period = -1;
  int choiceRank = -1; // 0-based index into the student's preference list
  double score = 0.0;

  std::string fullName() const {
    if (firstName.empty() && lastName.empty()) {
      return "";
    }
    if (firstName.empty()) {
      return lastName;
    }
    if (lastName.empty()) {
      return firstName;
    }
    return firstName + " " + lastName;
  }
};

struct ActivitySummaryRow {
  std::string activity;
  int assigned = 0;
  int capacity = 0;
  int presentYes = 0;
  int presentNo = 0;
  int presentMaybe = 0;
  std::vector<int> assignedPerPeriod;
  std::vector<int> capacityPerPeriod;
};

struct SolverResult {
  bool success = false;
  std::string message;
  std::vector<std::string> warnings;
  std::vector<StudentAssignment> assignments;
  std::vector<ActivitySummaryRow> activitySummary;
  int satisfiedStudents = 0;
  int totalStudents = 0;
  double objectiveValue = 0.0;
  long long runtimeMs = 0;
};
