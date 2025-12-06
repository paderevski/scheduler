# Engineering Week Scheduler - User Guide

## Overview

The Engineering Week Scheduler is a desktop application that optimally assigns students to activities based on their preferences, while respecting capacity constraints and attendance patterns. The application uses constraint optimization to maximize student satisfaction.

## Table of Contents

1. [Getting Started](#getting-started)
2. [Data Tab](#data-tab)
3. [Options Tab](#options-tab)
4. [Results Tab](#results-tab)
5. [Exporting Results](#exporting-results)
6. [Tips and Best Practices](#tips-and-best-practices)

---

## Getting Started

### Launching the Application

Run the application executable. The main window has three tabs:
- **Data**: Import and view student preference data
- **Options**: Configure solver settings and capacities
- **Results**: View optimization results and activity assignments

### Required Data Format

Your input file (CSV or Excel) should contain the following columns:
- **Student ID**: Unique identifier for each student
- **First Name**: Student's first name
- **Last Name**: Student's last name
- **Grade**: Student's grade level
- **Day**: Day designation (e.g., "A Day", "B Day")
- **Teacher**: Teacher name
- **Pathway**: Student's pathway or program
- **Present**: Attendance likelihood ("Yes", "Maybe", or "No")
- **Choice 1, Choice 2, Choice 3, ...**: Student's activity preferences in order

---

## Data Tab

### Importing Data

1. Click **File → Open Data...** (or press **Cmd+O** on Mac / **Ctrl+O** on Windows)
2. Select your CSV or Excel file
3. Map the columns from your file to the required fields
4. Click **OK** to import

### Column Mapping Dialog

If your spreadsheet uses different column names, the Column Mapping dialog allows you to map your columns to the expected fields:
- Use the dropdown menus to match each required field to a column in your file
- The system will attempt to auto-detect common column names
- Click **OK** when mapping is complete

### Viewing Data

The Data tab displays all imported student records in a table format. You can:
- Scroll through the data
- Verify that all information imported correctly
- Check for any missing or incorrect data

### Diagnostics

The left panel shows:
- **Students**: Total count and any missing student IDs
- **Blank choices**: Number of empty preference fields
- **Unique activities**: Number of distinct activities detected

---

## Options Tab

The Options tab is divided into two sections:

### Left Panel: Filters and Weighting

#### Day Filter

Filter students by day designation:
- **All Days**: Include all students
- **Day A**: Only students assigned to Day A
- **Day B**: Only students assigned to Day B

This filter affects:
- Student count in diagnostics
- Auto-capacity calculation
- Which students the solver processes

#### Attendance Weighting

Control how the solver prioritizes students based on attendance likelihood:

**Enable Attendance Weighting** checkbox:
- **Checked**: Use custom weights to prioritize students by attendance status
- **Unchecked**: Treat all students equally (all weights = 1.0)

When enabled, set weights for each attendance status:
- **Yes**: Students confirmed to attend (default: 100, always normalized to ×1.00)
- **Maybe**: Students who might attend (default: 50, normalized to ×0.50)
- **No**: Students unlikely to attend (default: 10, normalized to ×0.10)

**How It Works:**
- Higher weights mean higher priority for preferred choices
- A "Yes" student's 2nd choice may be prioritized over a "No" student's 1st choice
- Normalized multipliers show the relative weight (Yes is always 1.00)
- All students still get assigned; weighting only affects assignment quality

**Example Scenarios:**
- **Strong prioritization**: Yes=100, Maybe=10, No=1 (heavily favor confirmed attendees)
- **Moderate prioritization**: Yes=100, Maybe=50, No=10 (default, balanced approach)
- **Weak prioritization**: Yes=100, Maybe=90, No=80 (nearly equal treatment)

### Right Panel: Activity Capacities

#### Default Capacity Controls

- **Default capacity**: Set a base capacity value
- **Auto**: Automatically calculate capacity as ⌈filtered students ÷ number of activities⌉
- **Set All**: Apply the default capacity to all activities

#### Activity Capacities Table

Shows all detected activities with:
- **Activity**: Activity name
- **Choice 1, 2, 3**: Count of students who selected this as their 1st, 2nd, or 3rd choice
- **Capacity**: Maximum students for this activity (editable)

**Tips:**
- Review the choice counts to identify popular activities
- Increase capacity for high-demand activities
- Total capacity should equal or exceed the number of students
- Red warning appears if total capacity is insufficient

#### Calculate Button

Click **Calculate** to run the optimization solver. The solver will:
1. Validate that sufficient capacity exists
2. Apply attendance weighting (if enabled)
3. Find optimal assignments to maximize student satisfaction
4. Display results in the Results tab

---

## Results Tab

### Status Bar

Shows:
- Number of satisfied students (got one of their choices)
- Total number of students
- Solver runtime in milliseconds

### Results Table

Displays all assignments with columns:
- **Student ID, First Name, Last Name**
- **Grade, Day, Teacher, Pathway, Present**
- **Activity**: Assigned activity
- **Choice**: Which preference this was (1, 2, 3, or "-" for fallback)
- **Score**: Assignment quality score

### Choice Satisfaction Summary

Table showing satisfaction breakdown:
- **Rank**: 1st Choice, 2nd Choice, 3rd Choice, Not Satisfied
- **Total**: Number of students in this category
- **Total %**: Percentage of all students
- **Yes, Maybe, No**: Breakdown by attendance status

Use this to evaluate:
- Overall satisfaction rate
- Whether attendance weighting is working as intended
- Impact of capacity constraints

### Activity Summary

Tree view showing each activity:
- **Activity**: Activity name
- **Assigned**: Number of students assigned
- **Capacity**: Maximum capacity
- **Yes, Maybe, No**: Attendance breakdown
- **Expected Utilization**: (Yes + Maybe×0.5) ÷ Capacity

Expected utilization helps predict actual attendance:
- 100%: Activity will likely be full
- >100%: Activity may be overcrowded if all "maybe" students attend
- <100%: Activity has room for more students

---

## Exporting Results

### Export Results (CSV)

**File → Export Results (CSV)** or click the button in Results tab:

1. Select a destination folder
2. The following files are created:
   - **results.csv**: Complete assignment table
   - **summary.txt**: Comprehensive text report
   - **[Activity Name].txt**: Individual roster for each activity

#### Summary Report Contents

The `summary.txt` file includes:
- **Solver Options**: Day filter, capacity settings, attendance weights
- **Overall Results**: Student counts, satisfaction rate, runtime
- **Choice Satisfaction Summary**: Detailed breakdown table
- **Activity Summary**: All activities with utilization metrics

#### Activity Roster Files

Each activity gets a text file with:
- Activity name, enrollment, and capacity
- Right-aligned columns: Last Name, First Name, Student ID, Pathway, Grade, Present
- Students sorted alphabetically by last name

### Export Results (XLSX)

Similar to CSV export but creates:
- **results.xlsx**: Excel workbook with assignment table
- **summary.txt**: Same text report
- **[Activity Name].txt**: Same activity rosters

### Export Data

**File menu** also provides:
- **Export Data as CSV**: Export current student preference data
- **Export Data as XLSX**: Export as Excel workbook

Use these to save a modified dataset or share input data.

---

## Tips and Best Practices

### Data Preparation

1. **Clean your data**: Remove duplicate student IDs, fix typos in activity names
2. **Consistent naming**: Use the same activity name spelling throughout
3. **Attendance field**: Use "Yes", "Maybe", or "No" (case-insensitive, "Y"/"M"/"N" also work)
4. **Complete preferences**: Students should provide at least 3 choices

### Setting Capacities

1. **Start with Auto**: Click "Auto" for an initial balanced distribution
2. **Adjust for demand**: Review Choice 1/2/3 counts and increase capacity for popular activities
3. **Account for attendance**: Consider reducing capacity for activities with many "No" students
4. **Buffer room**: Consider setting total capacity slightly above student count

### Using Attendance Weighting

1. **Test different settings**: Run solver multiple times with different weights
2. **Compare satisfaction**: Check the Choice Satisfaction Summary to see impact
3. **Balance fairness**: Very high weights may over-prioritize "Yes" students
4. **Consider your goals**:
   - Maximize room utilization? Use strong weighting
   - Ensure fair access? Use weak weighting or disable

### Interpreting Results

1. **Satisfaction rate**: Percentage of students who got a top-3 choice
2. **Not Satisfied**: Students assigned to fallback (none of their choices available)
3. **Expected Utilization**:
   - <80%: Consider reducing capacity or combining with similar activities
   - >120%: Consider increasing capacity or adding another session
4. **Attendance patterns**: If many "Maybe" students in one activity, have a backup plan

### Iterative Refinement

1. Run the solver with initial settings
2. Review the Choice Satisfaction Summary
3. Adjust capacities for over/under-subscribed activities
4. Modify attendance weights if needed
5. Re-run solver and compare results
6. Export when satisfied with the outcome

### Handling Day A/Day B Schedules

1. **Import all students** in one file
2. **Use Day Filter** to process each day separately:
   - Set filter to "Day A", run solver, export results
   - Set filter to "Day B", run solver, export results
3. **Or import separately**: Split your data file by day before importing

### Troubleshooting

**"Insufficient Capacity" warning:**
- Total capacity is less than number of students
- Increase capacities or click "Auto" to recalculate
- Can proceed anyway, but some students won't be assigned

**"No students match the selected day filter":**
- Check that your data has the correct day values
- Try "All Days" filter
- Verify the Day column was mapped correctly during import

**Blank choices in diagnostics:**
- Normal if students provided fewer than maximum choices
- Ensure students have at least 1-3 choices for best results

**High "Not Satisfied" count:**
- Capacities may be too low for demand distribution
- Some activities may be over-requested
- Consider increasing capacities or adding more activity options

---

## Keyboard Shortcuts

- **Cmd/Ctrl + O**: Open data file
- **Cmd/Ctrl + Q**: Quit application

---

## Technical Details

### Optimization Algorithm

The solver uses OR-Tools' CBC (Coin-or Branch and Cut) mixed-integer programming solver with:
- Binary variables for each student-activity assignment
- Constraint: Each student assigned to exactly one activity
- Constraint: Activity assignments ≤ capacity
- Objective: Maximize sum of (preference weight × attendance multiplier)
- Fallback: Greedy algorithm if OR-Tools solver fails

### Preference Weights

- 1st choice: 100 points
- 2nd choice: 50 points
- 3rd choice: 25 points
- Fallback: 1 point

These are multiplied by attendance weights when weighting is enabled.

### Expected Utilization Calculation

```
Expected = (Yes_count × 1.0) + (Maybe_count × 0.5) + (No_count × 0.0)
Utilization = Expected ÷ Capacity
```

This provides a more realistic estimate than raw assigned count.

---

## Support and Feedback

For issues, questions, or feature requests, please contact your system administrator or the development team.

**Version**: 1.0
**Last Updated**: December 2025
