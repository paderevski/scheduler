# Filter Implementation Plan - Option 3: Column-Based Dynamic Filters

## Overview
Add filtering capability to the data table with dropdowns for key columns (Grade, Day, Teacher, Pathway). Filters are dynamically populated from actual data and affect what gets passed to the solver.

## Changes Required

### 1. Add QSortFilterProxyModel
**File:** `src/ui/MainWindow.cpp`

**Add include:**
```cpp
#include <QSortFilterProxyModel>
```

**Add member variables to `Impl` class (around line 2110):**
```cpp
QSortFilterProxyModel *proxyModel;
QComboBox *gradeFilterCombo;
QComboBox *dayFilterCombo;
QComboBox *teacherFilterCombo;
QComboBox *pathwayFilterCombo;
```

**Initialize in constructor (around line 302):**
```cpp
proxyModel(new QSortFilterProxyModel(q)),
gradeFilterCombo(new QComboBox(q)),
dayFilterCombo(new QComboBox(q)),
teacherFilterCombo(new QComboBox(q)),
pathwayFilterCombo(new QComboBox(q)),
```

### 2. Setup Proxy Model in setupUi()
**File:** `src/ui/MainWindow.cpp` (around line 355)

**Current code:**
```cpp
tableView->setModel(model);
```

**Replace with:**
```cpp
proxyModel->setSourceModel(model);
proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
tableView->setModel(proxyModel);
tableView->setSortingEnabled(true);  // Enable column sorting
```

### 3. Add Filter UI Controls
**File:** `src/ui/MainWindow.cpp` in `setupUi()` (before dataStack creation, around line 376)

**Add this section:**
```cpp
// === FILTER CONTROLS ===
// Add a horizontal layout above the table with filter dropdowns
auto *filterLayout = new QHBoxLayout();

// Grade filter
filterLayout->addWidget(new QLabel(QStringLiteral("Grade:"), q_ptr));
filterLayout->addWidget(gradeFilterCombo);

// Day filter
filterLayout->addWidget(new QLabel(QStringLiteral("Day:"), q_ptr));
filterLayout->addWidget(dayFilterCombo);

// Teacher filter
filterLayout->addWidget(new QLabel(QStringLiteral("Teacher:"), q_ptr));
filterLayout->addWidget(teacherFilterCombo);

// Pathway filter
filterLayout->addWidget(new QLabel(QStringLiteral("Pathway:"), q_ptr));
filterLayout->addWidget(pathwayFilterCombo);

filterLayout->addStretch();  // Push filters to left
```

**Update dataLayout creation to include filters:**
```cpp
// Create data view with filters and stacked widget
auto *dataStackedWidget = new QStackedWidget(q_ptr);
dataStackedWidget->addWidget(welcomeWidget);  // Index 0
dataStackedWidget->addWidget(tableView);      // Index 1

auto *dataLayout = new QVBoxLayout();
dataLayout->addLayout(filterLayout);          // Add filters above table
dataLayout->addWidget(dataStackedWidget);
auto *dataWidget = new QWidget(q_ptr);
dataWidget->setLayout(dataLayout);

// Store stacked widget for later switching
dataStack = dataStackedWidget;
```

### 4. Create Method to Populate Filter Combos
**File:** `src/ui/MainWindow.cpp` (add new method after `updateRecentProjects()`, around line 750)

```cpp
void updateFilterCombos() {
  // Extract unique values from loaded data for each filter column
  const auto &rows = model->rows();

  // Helper to populate a combo with unique values from a column
  auto populateCombo = [](QComboBox *combo, const QSet<QString> &values) {
    QString currentValue = combo->currentData().toString();
    combo->clear();
    combo->addItem(QStringLiteral("All"), QString());  // Empty = no filter

    QStringList sortedValues = values.values();
    sortedValues.sort();
    for (const QString &value : sortedValues) {
      if (!value.isEmpty()) {
        combo->addItem(value, value);
      }
    }

    // Restore previous selection if it still exists
    int index = combo->findData(currentValue);
    if (index >= 0) {
      combo->setCurrentIndex(index);
    }
  };

  // Collect unique values for each column
  QSet<QString> grades, days, teachers, pathways;
  for (const auto &row : rows) {
    grades.insert(row.grade);
    days.insert(row.day);
    teachers.insert(row.teacher);
    pathways.insert(row.pathway);
  }

  // Populate combos
  populateCombo(gradeFilterCombo, grades);
  populateCombo(dayFilterCombo, days);
  populateCombo(teacherFilterCombo, teachers);
  populateCombo(pathwayFilterCombo, pathways);
}
```

### 5. Create Custom Filter Logic
**File:** `src/ui/MainWindow.cpp` (add new method after `updateFilterCombos()`)

```cpp
void applyFilters() {
  // Custom filter function that checks all four filter combos
  proxyModel->setFilterRole(Qt::DisplayRole);
  proxyModel->setFilterKeyColumn(-1);  // Filter across all columns initially

  // Qt's built-in filtering only handles single column, so we need custom logic
  // We'll use a lambda-based filter
  proxyModel->setFilterFixedString("");  // Clear simple filter

  // Instead, we need to subclass QSortFilterProxyModel or use setFilterRegularExpression
  // For now, we'll create a simple approach using invalidateFilter
  proxyModel->invalidate();  // Force re-filter

  // Update diagnostics to show filtered count
  updateSummary();
}
```

**Note:** Qt's built-in `QSortFilterProxyModel` only supports single-column filtering easily. For multi-column filtering, we need to either:
- **Option A:** Subclass `QSortFilterProxyModel` and override `filterAcceptsRow()` (recommended, ~30 lines)
- **Option B:** Chain multiple proxy models (one per column filter)

### 6. Subclass QSortFilterProxyModel (Recommended Approach)
**File:** `src/ui/MainWindow.cpp` (add before MainWindow::Impl class, around line 290)

```cpp
// Custom proxy model that supports multi-column filtering
class MultiColumnFilterProxyModel : public QSortFilterProxyModel {
public:
  explicit MultiColumnFilterProxyModel(QObject *parent = nullptr)
      : QSortFilterProxyModel(parent) {}

  void setColumnFilter(int column, const QString &filter) {
    columnFilters[column] = filter;
    invalidateFilter();
  }

  void clearColumnFilter(int column) {
    columnFilters.remove(column);
    invalidateFilter();
  }

protected:
  bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override {
    // Check each active column filter
    for (auto it = columnFilters.constBegin(); it != columnFilters.constEnd(); ++it) {
      int column = it.key();
      QString filterValue = it.value();

      if (filterValue.isEmpty()) continue;  // Skip empty filters

      QModelIndex index = sourceModel()->index(sourceRow, column, sourceParent);
      QString cellValue = sourceModel()->data(index, Qt::DisplayRole).toString();

      if (cellValue != filterValue) {
        return false;  // Row doesn't match this filter
      }
    }
    return true;  // Row matches all filters
  }

private:
  QMap<int, QString> columnFilters;  // Column index -> filter value
};
```

**Update member variable declaration:**
```cpp
MultiColumnFilterProxyModel *proxyModel;  // Instead of QSortFilterProxyModel
```

**Update initialization:**
```cpp
proxyModel(new MultiColumnFilterProxyModel(q)),
```

### 7. Connect Filter Combos to Proxy Model
**File:** `src/ui/MainWindow.cpp` in `setupUi()` (after creating filter layout)

```cpp
// Connect filter combos to update the proxy model
// Assume column indices: Grade=3, Day=4, Teacher=5, Pathway=6 (adjust based on your headers)
QObject::connect(gradeFilterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), q_ptr,
                 [this](int) {
                   proxyModel->setColumnFilter(3, gradeFilterCombo->currentData().toString());
                   updateSummary();
                 });

QObject::connect(dayFilterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), q_ptr,
                 [this](int) {
                   proxyModel->setColumnFilter(4, dayFilterCombo->currentData().toString());
                   updateSummary();
                 });

QObject::connect(teacherFilterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), q_ptr,
                 [this](int) {
                   proxyModel->setColumnFilter(5, teacherFilterCombo->currentData().toString());
                   updateSummary();
                 });

QObject::connect(pathwayFilterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), q_ptr,
                 [this](int) {
                   proxyModel->setColumnFilter(6, pathwayFilterCombo->currentData().toString());
                   updateSummary();
                 });
```

**Note:** Column indices need to match actual data structure. Check `defaultHeaders()` function to confirm.

### 8. Update Data Loading to Populate Filters
**File:** `src/ui/MainWindow.cpp` in `loadDataFromPath()` (around line 1030)

**Add after successful data load:**
```cpp
diagnostics->clear();
model->setHeaders(headers);
model->setRows(std::move(*rowsOpt));
lastDataPath = path;

// NEW: Populate filter combos with unique values from data
updateFilterCombos();

appendDiagnostic(QStringLiteral("Loaded %1 students from %2.")
                     .arg(model->rowCount())
                     .arg(QFileInfo(path).fileName()));
```

### 9. Update Project Load to Restore Filters
**File:** `src/ui/MainWindow.cpp` in `doLoadProject()` (around line 1430)

**After loading data, add:**
```cpp
// Load student data
// ... existing code ...

// NEW: Update filter combos after loading project data
updateFilterCombos();

// TODO: Optionally save/restore filter selections in project file JSON
```

### 10. Remove Old Day Filter
**File:** `src/ui/MainWindow.cpp`

**Remove these sections:**
- Day filter combo in left layout (around line 410-420)
- Day filter member variable (around line 2130: `QComboBox *dayFilterCombo;`)
- Day filter connections in `connectSignals()`
- `lastDayFilter` member variable (around line 2160)
- Day filter usage in `onRunClicked()` (around line 870)

### 11. Update Solver to Use Filtered Rows
**File:** `src/ui/MainWindow.cpp` in `onRunClicked()` (around line 850)

**Current code gets all rows:**
```cpp
const auto &allRows = model->rows();
```

**Replace with filtered rows:**
```cpp
// Get only the VISIBLE (filtered) rows from the proxy model
std::vector<StudentPreferenceRow> filteredRows;
for (int i = 0; i < proxyModel->rowCount(); ++i) {
  QModelIndex proxyIndex = proxyModel->index(i, 0);
  QModelIndex sourceIndex = proxyModel->mapToSource(proxyIndex);
  int sourceRow = sourceIndex.row();
  filteredRows.push_back(model->rows()[sourceRow]);
}

// Use filteredRows instead of allRows for solver
```

### 12. Update Diagnostics to Show Filtered Count
**File:** `src/ui/MainWindow.cpp` in `updateSummary()` (around line 800)

**Current code:**
```cpp
studentCountLabel->setText(
    QStringLiteral("Students: %1").arg(model->rowCount()));
```

**Replace with:**
```cpp
int totalCount = model->rowCount();
int visibleCount = proxyModel ? proxyModel->rowCount() : totalCount;

if (visibleCount < totalCount) {
  studentCountLabel->setText(
      QStringLiteral("Students: %1 of %2 (filtered)").arg(visibleCount).arg(totalCount));
} else {
  studentCountLabel->setText(
      QStringLiteral("Students: %1").arg(totalCount));
}
```

### 13. Clear Results When Filter Changes
**File:** `src/ui/MainWindow.cpp`

**In filter combo connections (step 7), add:**
```cpp
QObject::connect(gradeFilterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), q_ptr,
                 [this](int) {
                   proxyModel->setColumnFilter(3, gradeFilterCombo->currentData().toString());

                   // Clear/invalidate results if filter changes after solver run
                   if (hasResult) {
                     resultsStatus->setText(QStringLiteral("Filter changed - results outdated"));
                     resultsStatus->setStyleSheet(QStringLiteral("color: orange;"));
                   }

                   updateSummary();
                 });
```

## Testing Checklist

- [ ] Load CSV file → filter combos populate with unique values
- [ ] Select "Grade = 9" → table shows only grade 9 students
- [ ] Select "Day = A Day" + "Grade = 9" → table shows only matching students
- [ ] Student count diagnostic shows "X of Y (filtered)"
- [ ] Click "Calculate" → solver runs on filtered data only
- [ ] Results reflect filtered subset
- [ ] Change filter after solving → warning appears
- [ ] Click column headers → table sorts correctly
- [ ] Save project → reload project → filter combos repopulate correctly
- [ ] Clear all filters (select "All") → table shows all data

## Column Index Reference

Based on `defaultHeaders()` function, typical column order is:
1. Student ID (0)
2. First Name (1)
3. Last Name (2)
4. Grade (3)
5. Day (4)
6. Teacher (5)
7. Pathway (6)
8. Present (7)
9. Choice 1, 2, 3... (8+)

**Verify these indices match your actual data structure before implementing!**

## Estimated Lines of Code
- MultiColumnFilterProxyModel class: ~40 lines
- Filter UI setup: ~30 lines
- updateFilterCombos method: ~40 lines
- Filter connections: ~30 lines
- Solver integration: ~15 lines
- Diagnostic updates: ~10 lines
- Removal of old day filter: ~20 lines removed

**Total: ~165 new lines, ~20 lines removed**

## Future Enhancements (Not in This Plan)
- Save/restore filter selections in project files
- "Clear All Filters" button
- Filter by text search (contains/starts with) instead of exact match
- Filter on "Present" column (Yes/Maybe/No)
- Show filter icon/indicator when filters are active
