#include "ui/MainWindow.hpp"

#include "models/PreferenceModel.hpp"
#include "solver/SolverEngine.hpp"
#include "utils/SpreadsheetBridge.hpp"

#include <QAbstractItemView>
#include <QAction>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QProgressDialog>
#include <QPushButton>
#include <QRegularExpression>
#include <QSet>
#include <QSpinBox>
#include <QString>
#include <QStringList>
#include <QTabWidget>
#include <QTableView>
#include <QTableWidget>
#include <QTextStream>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QtConcurrent>

#include <algorithm>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace {
QStringList defaultHeaders() {
  return {QStringLiteral("Student ID"), QStringLiteral("Student Name"),
          QStringLiteral("Choice 1"), QStringLiteral("Choice 2"),
          QStringLiteral("Choice 3")};
}

QString toQString(const std::string &value) {
  return QString::fromStdString(value);
}

SpreadsheetTable modelToTable(const PreferenceModel &model) {
  SpreadsheetTable table;
  const auto &headers = model.headers();
  const auto headerSource = headers.isEmpty() ? defaultHeaders() : headers;
  const int columnCount = headerSource.size();

  for (const auto &header : headerSource) {
    table.headers.push_back(header.toStdString());
  }

  table.rows.reserve(model.rows().size());
  for (const auto &row : model.rows()) {
    std::vector<std::string> line(columnCount);
    if (columnCount > 0) {
      line[0] = row.studentId.trimmed().toStdString();
    }
    if (columnCount > 1) {
      line[1] = row.studentName.trimmed().toStdString();
    }
    for (int col = 2; col < columnCount; ++col) {
      const int choiceIndex = col - 2;
      if (choiceIndex < row.choices.size()) {
        line[col] = row.choices[choiceIndex].trimmed().toStdString();
      } else {
        line[col] = std::string();
      }
    }
    table.rows.push_back(std::move(line));
  }
  return table;
}

std::vector<StudentPreferenceRow> rowsFromTable(const SpreadsheetTable &table,
                                                QStringList &headersOut) {
  headersOut.clear();
  if (!table.headers.empty()) {
    for (const auto &header : table.headers) {
      headersOut.append(QString::fromStdString(header));
    }
  }
  if (headersOut.size() < 2) {
    headersOut = defaultHeaders();
  }

  int maxColumns = headersOut.size();
  for (const auto &row : table.rows) {
    maxColumns = std::max(maxColumns, static_cast<int>(row.size()));
  }
  while (headersOut.size() < maxColumns) {
    const int columnIndex = headersOut.size();
    if (columnIndex == 0) {
      headersOut.append(QStringLiteral("Student ID"));
    } else if (columnIndex == 1) {
      headersOut.append(QStringLiteral("Student Name"));
    } else {
      headersOut.append(QStringLiteral("Choice %1").arg(columnIndex - 1));
    }
  }

  std::vector<StudentPreferenceRow> rows;
  rows.reserve(table.rows.size());
  for (const auto &line : table.rows) {
    if (line.empty()) {
      continue;
    }
    StudentPreferenceRow row;
    if (!line.empty()) {
      row.studentId = QString::fromStdString(line[0]).trimmed();
    }
    if (line.size() > 1) {
      row.studentName = QString::fromStdString(line[1]).trimmed();
    }
    const int headerCount = headersOut.size();
    for (int col = 2; col < headerCount; ++col) {
      if (col < static_cast<int>(line.size())) {
        row.choices.append(QString::fromStdString(line[col]).trimmed());
      } else {
        row.choices.append(QString());
      }
    }
    rows.push_back(std::move(row));
  }

  return rows;
}

SpreadsheetTable resultsToTable(const SolverResult &result) {
  SpreadsheetTable table;
  table.headers = {"Student ID", "Student Name", "Assigned Activity",
                   "Choice Rank", "Score"};
  for (const auto &assignment : result.assignments) {
    std::vector<std::string> row;
    row.reserve(5);
    row.push_back(assignment.studentId);
    row.push_back(assignment.studentName);
    row.push_back(assignment.activity);
    if (assignment.choiceRank >= 0) {
      row.push_back(std::to_string(assignment.choiceRank + 1));
    } else {
      row.push_back(std::string());
    }
    std::ostringstream scoreStream;
    scoreStream.setf(std::ios::fixed);
    scoreStream.precision(1);
    scoreStream << assignment.score;
    row.push_back(scoreStream.str());
    table.rows.push_back(std::move(row));
  }
  return table;
}

} // namespace

class MainWindow::Impl {
public:
  explicit Impl(MainWindow *q)
      : q_ptr(q), tabWidget(new QTabWidget(q)), tableView(new QTableView(q)),
        model(new PreferenceModel(q)), diagnostics(new QListWidget(q)),
        studentCountLabel(new QLabel(q)), choiceCountLabel(new QLabel(q)),
        activityCountLabel(new QLabel(q)), capacityTable(new QTableWidget(q)),
        totalCapacityLabel(new QLabel(q)), defaultCapacitySpin(new QSpinBox(q)),
        setAllCapacitiesButton(new QPushButton(QStringLiteral("Set All"), q)),
        runButton(new QPushButton(QStringLiteral("Calculate"), q)),
        resultsStatus(new QLabel(QStringLiteral("No solver run yet."), q)),
        resultsTable(new QTableWidget(q)), activitySummary(new QTreeWidget(q)),
        exportResultsCsvButton(
            new QPushButton(QStringLiteral("Export Results (CSV)"), q)),
        exportResultsXlsxButton(
            new QPushButton(QStringLiteral("Export Results (XLSX)"), q)),
        solverWatcher(new QFutureWatcher<SolverResult>(q)),
        progressDialog(new QProgressDialog(QStringLiteral("Running solver..."),
                                           QString(), 0, 0, q))
#if HAVE_QT_CHARTS
        ,
        activityChart(new QtCharts::QChartView(new QtCharts::QChart(), q))
#endif
  {
    setupUi();
    setupMenu();
    connectSignals();
    updateSummary();
  }

  void setupUi() {
    tableView->setModel(model);
    tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    diagnostics->setSelectionMode(QAbstractItemView::NoSelection);

    // Setup capacity table
    capacityTable->setColumnCount(2);
    capacityTable->setHorizontalHeaderLabels(
        {QStringLiteral("Activity"), QStringLiteral("Capacity")});
    capacityTable->horizontalHeader()->setSectionResizeMode(
        0, QHeaderView::Stretch);
    capacityTable->horizontalHeader()->setSectionResizeMode(
        1, QHeaderView::ResizeToContents);
    capacityTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    defaultCapacitySpin->setRange(1, 500);
    defaultCapacitySpin->setValue(20);
    defaultCapacitySpin->setToolTip(
        QStringLiteral("Default capacity for all activities"));

    auto *dataLayout = new QVBoxLayout();
    dataLayout->addWidget(tableView);
    auto *dataWidget = new QWidget(q_ptr);
    dataWidget->setLayout(dataLayout);

    // Left side: Diagnostics
    auto *leftLayout = new QVBoxLayout();
    leftLayout->addWidget(
        new QLabel(QStringLiteral("<b>Diagnostics</b>"), q_ptr));
    leftLayout->addWidget(diagnostics);
    leftLayout->addWidget(studentCountLabel);
    leftLayout->addWidget(choiceCountLabel);
    leftLayout->addWidget(activityCountLabel);
    leftLayout->addStretch();

    // Right side: Capacities
    auto *rightLayout = new QVBoxLayout();
    rightLayout->addWidget(
        new QLabel(QStringLiteral("<b>Activity Capacities</b>"), q_ptr));

    // "Set All" controls
    auto *setAllLayout = new QHBoxLayout();
    setAllLayout->addWidget(
        new QLabel(QStringLiteral("Default capacity:"), q_ptr));
    setAllLayout->addWidget(defaultCapacitySpin);
    setAllLayout->addWidget(setAllCapacitiesButton);
    setAllLayout->addStretch();
    rightLayout->addLayout(setAllLayout);

    // Capacity table
    rightLayout->addWidget(capacityTable);

    // Total capacity label
    totalCapacityLabel->setStyleSheet(
        QStringLiteral("QLabel { padding: 5px; }"));
    rightLayout->addWidget(totalCapacityLabel);

    // Two-column layout for Options tab
    auto *optionsColumnsLayout = new QHBoxLayout();
    optionsColumnsLayout->addLayout(leftLayout, 1);
    optionsColumnsLayout->addLayout(rightLayout, 1);

    auto *optionsMainLayout = new QVBoxLayout();
    optionsMainLayout->addLayout(optionsColumnsLayout);
    optionsMainLayout->addWidget(runButton);

    auto *optionsWidget = new QWidget(q_ptr);
    optionsWidget->setLayout(optionsMainLayout);

    resultsTable->setColumnCount(5);
    resultsTable->setHorizontalHeaderLabels(
        {QStringLiteral("Student ID"), QStringLiteral("Student Name"),
         QStringLiteral("Activity"), QStringLiteral("Choice"),
         QStringLiteral("Score")});
    resultsTable->horizontalHeader()->setSectionResizeMode(
        QHeaderView::Stretch);
    resultsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    activitySummary->setColumnCount(4);
    activitySummary->setHeaderLabels(
        {QStringLiteral("Activity"), QStringLiteral("Assigned"),
         QStringLiteral("Capacity"), QStringLiteral("Utilization")});

    auto *resultsLayout = new QVBoxLayout();
    resultsLayout->addWidget(resultsStatus);
    resultsLayout->addWidget(resultsTable);
    resultsLayout->addWidget(activitySummary);
    auto *resultsButtonLayout = new QHBoxLayout();
    resultsButtonLayout->addStretch();
    resultsButtonLayout->addWidget(exportResultsCsvButton);
    resultsButtonLayout->addWidget(exportResultsXlsxButton);
    resultsLayout->addLayout(resultsButtonLayout);

    auto *resultsWidget = new QWidget(q_ptr);
    resultsWidget->setLayout(resultsLayout);

    tabWidget->addTab(dataWidget, QStringLiteral("Data"));
    tabWidget->addTab(optionsWidget, QStringLiteral("Options"));
    tabWidget->addTab(resultsWidget, QStringLiteral("Results"));

    auto *centralLayout = new QVBoxLayout();
    centralLayout->addWidget(tabWidget);
    auto *centralWidget = new QWidget(q_ptr);
    centralWidget->setLayout(centralLayout);
    q_ptr->setCentralWidget(centralWidget);

    exportResultsCsvButton->setEnabled(false);
    exportResultsXlsxButton->setEnabled(false);

    progressDialog->setCancelButton(nullptr);
    progressDialog->setWindowModality(Qt::WindowModal);
    progressDialog->setMinimumDuration(INT_MAX); // Prevent auto-show
    progressDialog->setAutoReset(false);
    progressDialog->setAutoClose(false);
    progressDialog->reset(); // Ensure it starts hidden
    progressDialog->hide();

    QObject::connect(runButton, &QPushButton::clicked, q_ptr,
                     [this]() { onRunClicked(); });
    QObject::connect(setAllCapacitiesButton, &QPushButton::clicked, q_ptr,
                     [this]() { setAllCapacities(); });
    QObject::connect(exportResultsCsvButton, &QPushButton::clicked, q_ptr,
                     [this]() { exportResultsCsv(); });
    QObject::connect(exportResultsXlsxButton, &QPushButton::clicked, q_ptr,
                     [this]() { exportResultsXlsx(); });
  }

  void setupMenu() {
    auto *fileMenu = q_ptr->menuBar()->addMenu(QStringLiteral("File"));
    auto *openAction = fileMenu->addAction(QStringLiteral("Open Data..."));
    QObject::connect(openAction, &QAction::triggered, q_ptr,
                     [this]() { openDataFile(); });

    auto *exportCsvAction =
        fileMenu->addAction(QStringLiteral("Export Data as CSV..."));
    QObject::connect(exportCsvAction, &QAction::triggered, q_ptr,
                     [this]() { exportDataCsv(); });

    auto *exportXlsxAction =
        fileMenu->addAction(QStringLiteral("Export Data as XLSX..."));
    QObject::connect(exportXlsxAction, &QAction::triggered, q_ptr,
                     [this]() { exportDataXlsx(); });

    fileMenu->addSeparator();
    auto *quitAction = fileMenu->addAction(QStringLiteral("Quit"));
    QObject::connect(quitAction, &QAction::triggered, q_ptr, &QWidget::close);
  }

  void connectSignals() {
    QObject::connect(model, &QAbstractItemModel::dataChanged, q_ptr,
                     [this](const QModelIndex &, const QModelIndex &,
                            const QList<int> &) { updateSummary(); });
    QObject::connect(model, &QAbstractItemModel::modelReset, q_ptr,
                     [this]() { updateSummary(); });

    QObject::connect(solverWatcher, &QFutureWatcher<SolverResult>::finished,
                     q_ptr, [this]() { handleSolverFinished(); });

    QObject::connect(setAllCapacitiesButton, &QPushButton::clicked, q_ptr,
                     [this]() { setAllCapacities(); });
  }

  void appendDiagnostic(const QString &message) {
    diagnostics->addItem(message);
    diagnostics->scrollToBottom();
  }

  void updateSummary() {
    const auto summary = model->summarize();
    studentCountLabel->setText(QStringLiteral("Students: %1 (missing IDs: %2)")
                                   .arg(summary.studentCount)
                                   .arg(summary.missingStudentIds));
    choiceCountLabel->setText(
        QStringLiteral("Blank choices: %1").arg(summary.missingChoices));

    QSet<QString> activities;
    for (const auto &row : model->rows()) {
      for (const auto &choice : row.choices) {
        const auto normalized = choice.trimmed();
        if (!normalized.isEmpty()) {
          activities.insert(normalized);
        }
      }
    }
    activityCountLabel->setText(
        QStringLiteral("Unique activities: %1").arg(activities.size()));

    const bool hasData = model->rowCount() > 0;
    runButton->setEnabled(hasData && !solverWatcher->isRunning());

    // Update capacity table with detected activities
    updateCapacityTable(activities);
  }

  void updateCapacityTable(const QSet<QString> &activities) {
    // Store current capacities before clearing
    QMap<QString, int> currentCapacities;
    for (int row = 0; row < capacityTable->rowCount(); ++row) {
      auto *activityItem = capacityTable->item(row, 0);
      auto *spinBox =
          qobject_cast<QSpinBox *>(capacityTable->cellWidget(row, 1));
      if (activityItem && spinBox) {
        currentCapacities[activityItem->text()] = spinBox->value();
      }
    }

    // Rebuild table
    capacityTable->setRowCount(0);
    QStringList sortedActivities = activities.values();
    sortedActivities.sort(Qt::CaseInsensitive);

    for (const QString &activity : sortedActivities) {
      int row = capacityTable->rowCount();
      capacityTable->insertRow(row);

      auto *activityItem = new QTableWidgetItem(activity);
      activityItem->setFlags(Qt::ItemIsEnabled);
      capacityTable->setItem(row, 0, activityItem);

      auto *spinBox = new QSpinBox(q_ptr);
      spinBox->setRange(1, 500);
      // Use stored capacity if available, otherwise use default
      int capacity =
          currentCapacities.value(activity, defaultCapacitySpin->value());
      spinBox->setValue(capacity);

      // Connect spinbox to update total capacity when changed
      QObject::connect(spinBox, QOverload<int>::of(&QSpinBox::valueChanged),
                       q_ptr, [this]() { updateTotalCapacity(); });

      capacityTable->setCellWidget(row, 1, spinBox);
    }

    updateTotalCapacity();
  }

  void updateTotalCapacity() {
    int totalCapacity = 0;
    for (int row = 0; row < capacityTable->rowCount(); ++row) {
      auto *spinBox =
          qobject_cast<QSpinBox *>(capacityTable->cellWidget(row, 1));
      if (spinBox) {
        totalCapacity += spinBox->value();
      }
    }

    int studentCount = model->rowCount();
    bool sufficient = totalCapacity >= studentCount;

    QString labelText =
        QStringLiteral("<b>Total Capacity:</b> %1 / %2 students")
            .arg(totalCapacity)
            .arg(studentCount);

    if (studentCount > 0) {
      if (sufficient) {
        totalCapacityLabel->setStyleSheet(
            QStringLiteral("QLabel { padding: 5px; background-color: #d4edda; "
                           "color: #155724; border: 1px solid #c3e6cb; "
                           "border-radius: 3px; }"));
        labelText += QStringLiteral(" ✓");
      } else {
        totalCapacityLabel->setStyleSheet(
            QStringLiteral("QLabel { padding: 5px; background-color: #f8d7da; "
                           "color: #721c24; border: 1px solid #f5c6cb; "
                           "border-radius: 3px; }"));
        labelText += QStringLiteral(" ⚠ WARNING: Insufficient capacity!");
      }
    } else {
      totalCapacityLabel->setStyleSheet(
          QStringLiteral("QLabel { padding: 5px; }"));
    }

    totalCapacityLabel->setText(labelText);
  }

  void setAllCapacities() {
    int defaultValue = defaultCapacitySpin->value();
    for (int row = 0; row < capacityTable->rowCount(); ++row) {
      auto *spinBox =
          qobject_cast<QSpinBox *>(capacityTable->cellWidget(row, 1));
      if (spinBox) {
        spinBox->setValue(defaultValue);
      }
    }
    // updateTotalCapacity() will be called automatically via valueChanged
    // signals
  }

  void openDataFile() {
    const QString path = QFileDialog::getOpenFileName(
        q_ptr, QStringLiteral("Open student preferences"), {},
        QStringLiteral("Data Files (*.csv *.xlsx);;CSV Files (*.csv);;Excel "
                       "Files (*.xlsx)"));
    if (path.isEmpty()) {
      return;
    }
    loadDataFromPath(path);
  }

  bool loadDataFromPath(const QString &path) {
    SpreadsheetTable table;
    std::string error;
    if (!SpreadsheetBridge::ReadFile(path.toStdString(), table, &error)) {
      QMessageBox::warning(
          q_ptr, QStringLiteral("Unable to load data"),
          QStringLiteral("%1\n%2").arg(path, QString::fromStdString(error)));
      return false;
    }

    QStringList headers;
    auto rows = rowsFromTable(table, headers);
    if (rows.empty()) {
      QMessageBox::warning(
          q_ptr, QStringLiteral("Empty data"),
          QStringLiteral("The selected file contained no rows."));
      return false;
    }

    diagnostics->clear();
    model->setHeaders(headers);
    model->setRows(std::move(rows));
    lastDataPath = path;
    appendDiagnostic(QStringLiteral("Loaded %1 students from %2.")
                         .arg(model->rowCount())
                         .arg(QFileInfo(path).fileName()));
    return true;
  }

  void exportDataCsv() {
    const QString path = QFileDialog::getSaveFileName(
        q_ptr, QStringLiteral("Export data as CSV"),
        lastDataPath.isEmpty() ? QString() : lastDataPath,
        QStringLiteral("CSV Files (*.csv)"));
    if (path.isEmpty()) {
      return;
    }
    auto table = modelToTable(*model);
    std::string error;
    if (!SpreadsheetBridge::WriteCsv(path.toStdString(), table, &error)) {
      QMessageBox::warning(
          q_ptr, QStringLiteral("Unable to export"),
          QStringLiteral("%1\n%2").arg(path, QString::fromStdString(error)));
      return;
    }
    appendDiagnostic(QStringLiteral("Exported data to %1").arg(path));
  }

  void exportDataXlsx() {
    const QString path = QFileDialog::getSaveFileName(
        q_ptr, QStringLiteral("Export data as XLSX"),
        lastDataPath.isEmpty() ? QString() : lastDataPath,
        QStringLiteral("Excel Files (*.xlsx)"));
    if (path.isEmpty()) {
      return;
    }
    auto table = modelToTable(*model);
    std::string error;
    if (!SpreadsheetBridge::WriteXlsx(path.toStdString(), table, &error)) {
      QMessageBox::warning(
          q_ptr, QStringLiteral("Unable to export"),
          QStringLiteral("%1\n%2").arg(path, QString::fromStdString(error)));
      return;
    }
    appendDiagnostic(QStringLiteral("Exported data to %1").arg(path));
  }

  void onRunClicked() {
    if (solverWatcher->isRunning()) {
      return;
    }
    if (model->rowCount() == 0) {
      QMessageBox::information(
          q_ptr, QStringLiteral("No data"),
          QStringLiteral(
              "Import student preferences before running the solver."));
      return;
    }

    // Check total capacity
    int totalCapacity = 0;
    for (int row = 0; row < capacityTable->rowCount(); ++row) {
      auto *spinBox =
          qobject_cast<QSpinBox *>(capacityTable->cellWidget(row, 1));
      if (spinBox) {
        totalCapacity += spinBox->value();
      }
    }

    int studentCount = model->rowCount();
    if (totalCapacity < studentCount) {
      auto reply = QMessageBox::warning(
          q_ptr, QStringLiteral("Insufficient Capacity"),
          QStringLiteral(
              "Total capacity (%1) is less than number of students (%2).\n\n"
              "The solver will fail to assign all students.\n\n"
              "Do you want to proceed anyway?")
              .arg(totalCapacity)
              .arg(studentCount),
          QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

      if (reply != QMessageBox::Yes) {
        return;
      }
    }

    const auto &rowsRef = model->rows();
    std::vector<StudentPreferenceRow> rowsCopy(rowsRef.begin(), rowsRef.end());
    SolverOptions options;
    options.defaultCapacity = defaultCapacitySpin->value();

    // Collect per-activity capacities from table
    for (int row = 0; row < capacityTable->rowCount(); ++row) {
      auto *activityItem = capacityTable->item(row, 0);
      auto *spinBox =
          qobject_cast<QSpinBox *>(capacityTable->cellWidget(row, 1));
      if (activityItem && spinBox) {
        std::string activityName = activityItem->text().toStdString();
        options.activityCapacities[activityName] = spinBox->value();
      }
    }

    appendDiagnostic(
        QStringLiteral("Launching solver with per-activity capacities..."));
    progressDialog->setLabelText(QStringLiteral("Calculating assignments..."));
    progressDialog->show();
    runButton->setEnabled(false);

    auto future = QtConcurrent::run([rows = std::move(rowsCopy), options]() {
      return runSolver(rows, options);
    });
    solverWatcher->setFuture(future);
  }

  void handleSolverFinished() {
    progressDialog->hide();
    runButton->setEnabled(model->rowCount() > 0);

    const auto result = solverWatcher->result();
    if (!result.success) {
      resultsStatus->setText(
          QStringLiteral("Solver failed: %1").arg(toQString(result.message)));
      appendDiagnostic(
          QStringLiteral("Solver failure: %1").arg(toQString(result.message)));
      refreshResultExports();
      return;
    }

    lastResult = result;
    hasResult = true;

    populateResults(result);
    appendDiagnostic(
        QStringLiteral("Solver satisfied %1/%2 students (objective %3, %4 ms)")
            .arg(result.satisfiedStudents)
            .arg(result.totalStudents)
            .arg(result.objectiveValue, 0, 'f', 1)
            .arg(result.runtimeMs));

    for (const auto &warning : result.warnings) {
      appendDiagnostic(QStringLiteral("Warning: %1").arg(toQString(warning)));
    }

    refreshResultExports();
  }

  void populateResults(const SolverResult &result) {
    resultsTable->setRowCount(static_cast<int>(result.assignments.size()));
    int rowIndex = 0;
    for (const auto &assignment : result.assignments) {
      const auto studentId = toQString(assignment.studentId);
      const auto studentName = toQString(assignment.studentName);
      const auto activity = toQString(assignment.activity);
      const QString rank = assignment.choiceRank >= 0
                               ? QString::number(assignment.choiceRank + 1)
                               : QStringLiteral("-");
      const QString score = QString::number(assignment.score, 'f', 1);

      auto makeItem = [](const QString &text) {
        auto *item = new QTableWidgetItem(text);
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        return item;
      };

      resultsTable->setItem(rowIndex, 0, makeItem(studentId));
      resultsTable->setItem(rowIndex, 1, makeItem(studentName));
      resultsTable->setItem(rowIndex, 2, makeItem(activity));
      resultsTable->setItem(rowIndex, 3, makeItem(rank));
      resultsTable->setItem(rowIndex, 4, makeItem(score));
      ++rowIndex;
    }

    activitySummary->clear();
    for (const auto &summary : result.activitySummary) {
      auto *item = new QTreeWidgetItem(activitySummary);
      item->setText(0, toQString(summary.activity));
      item->setText(1, QString::number(summary.assigned));
      item->setText(2, QString::number(summary.capacity));
      const double utilization =
          summary.capacity == 0 ? 0.0
                                : static_cast<double>(summary.assigned) /
                                      static_cast<double>(summary.capacity);
      item->setText(3,
                    QStringLiteral("%1%").arg(utilization * 100.0, 0, 'f', 1));
    }
    activitySummary->resizeColumnToContents(0);

    resultsStatus->setText(QStringLiteral("Satisfied %1/%2 students in %3 ms")
                               .arg(result.satisfiedStudents)
                               .arg(result.totalStudents)
                               .arg(result.runtimeMs));

#if HAVE_QT_CHARTS
    if (activityChart) {
      auto *chart = new QtCharts::QChart();
      chart->setTitle(QStringLiteral("Activity utilization"));
      auto *assignedSet = new QtCharts::QBarSet(QStringLiteral("Assigned"));
      auto *capacitySet = new QtCharts::QBarSet(QStringLiteral("Capacity"));
      QStringList categories;

      for (const auto &summary : result.activitySummary) {
        categories.append(toQString(summary.activity));
        assignedSet->append(summary.assigned);
        capacitySet->append(summary.capacity);
      }

      auto *series = new QtCharts::QBarSeries();
      series->append(assignedSet);
      series->append(capacitySet);
      chart->addSeries(series);

      auto *axisX = new QtCharts::QBarCategoryAxis();
      axisX->append(categories);
      chart->addAxis(axisX, Qt::AlignBottom);
      series->attachAxis(axisX);

      auto *axisY = new QtCharts::QValueAxis();
      axisY->setTitleText(QStringLiteral("Students"));
      chart->addAxis(axisY, Qt::AlignLeft);
      series->attachAxis(axisY);

      activityChart->setChart(chart);
    }
#endif
  }

  void refreshResultExports() {
    const bool enabled = hasResult;
    exportResultsCsvButton->setEnabled(enabled);
    exportResultsXlsxButton->setEnabled(enabled);
  }

  void exportResultsCsv() {
    if (!hasResult) {
      return;
    }
    const QString folderPath = QFileDialog::getExistingDirectory(
        q_ptr, QStringLiteral("Select folder for results export"), {},
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (folderPath.isEmpty()) {
      return;
    }

    QDir dir(folderPath);
    if (!dir.exists()) {
      QMessageBox::warning(
          q_ptr, QStringLiteral("Invalid folder"),
          QStringLiteral("The selected folder does not exist."));
      return;
    }

    // Export main CSV
    const QString csvPath = dir.filePath(QStringLiteral("results.csv"));
    auto table = resultsToTable(*lastResult);
    std::string error;
    if (!SpreadsheetBridge::WriteCsv(csvPath.toStdString(), table, &error)) {
      QMessageBox::warning(
          q_ptr, QStringLiteral("Unable to export CSV"),
          QStringLiteral("%1\n%2").arg(csvPath, QString::fromStdString(error)));
      return;
    }

    // Export individual activity rosters
    int filesCreated = 0;
    for (const auto &activitySum : lastResult->activitySummary) {
      const QString activity = toQString(activitySum.activity);

      // Collect students for this activity
      QStringList studentNames;
      for (const auto &assignment : lastResult->assignments) {
        if (assignment.activity == activitySum.activity) {
          QString name = toQString(assignment.studentName);
          if (name.isEmpty()) {
            name = toQString(assignment.studentId);
          }
          studentNames.append(name);
        }
      }

      // Sort student names alphabetically
      studentNames.sort(Qt::CaseInsensitive);

      // Create sanitized filename
      QString filename = activity;
      filename.replace(QRegularExpression(QStringLiteral("[/\\\\:*?\"<>|]")), QStringLiteral("_"));
      filename = dir.filePath(filename + QStringLiteral(".txt"));

      QFile file(filename);
      if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        appendDiagnostic(QStringLiteral("Warning: Could not create %1").arg(filename));
        continue;
      }

      QTextStream out(&file);
      out << activity << "\n";
      out << "Enrollment: " << activitySum.assigned << "\n";
      out << "Capacity: " << activitySum.capacity << "\n";
      out << "\n";

      for (const QString &name : studentNames) {
        out << name << "\n";
      }

      file.close();
      filesCreated++;
    }

    appendDiagnostic(QStringLiteral("Exported results.csv and %1 activity rosters to %2")
                         .arg(filesCreated)
                         .arg(folderPath));

    QMessageBox::information(
        q_ptr, QStringLiteral("Export Successful"),
        QStringLiteral("Successfully exported results.csv and %1 activity roster files to:\n%2")
            .arg(filesCreated)
            .arg(folderPath));
  }

  void exportResultsXlsx() {
    if (!hasResult) {
      return;
    }
    const QString folderPath = QFileDialog::getExistingDirectory(
        q_ptr, QStringLiteral("Select folder for results export"), {},
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (folderPath.isEmpty()) {
      return;
    }

    QDir dir(folderPath);
    if (!dir.exists()) {
      QMessageBox::warning(
          q_ptr, QStringLiteral("Invalid folder"),
          QStringLiteral("The selected folder does not exist."));
      return;
    }

    // Export main XLSX
    const QString xlsxPath = dir.filePath(QStringLiteral("results.xlsx"));
    auto table = resultsToTable(*lastResult);
    std::string error;
    if (!SpreadsheetBridge::WriteXlsx(xlsxPath.toStdString(), table, &error)) {
      QMessageBox::warning(
          q_ptr, QStringLiteral("Unable to export XLSX"),
          QStringLiteral("%1\n%2").arg(xlsxPath, QString::fromStdString(error)));
      return;
    }

    // Export individual activity rosters
    int filesCreated = 0;
    for (const auto &activitySum : lastResult->activitySummary) {
      const QString activity = toQString(activitySum.activity);

      // Collect students for this activity
      QStringList studentNames;
      for (const auto &assignment : lastResult->assignments) {
        if (assignment.activity == activitySum.activity) {
          QString name = toQString(assignment.studentName);
          if (name.isEmpty()) {
            name = toQString(assignment.studentId);
          }
          studentNames.append(name);
        }
      }

      // Sort student names alphabetically
      studentNames.sort(Qt::CaseInsensitive);

      // Create sanitized filename
      QString filename = activity;
      filename.replace(QRegularExpression(QStringLiteral("[/\\\\:*?\"<>|]")), QStringLiteral("_"));
      filename = dir.filePath(filename + QStringLiteral(".txt"));

      QFile file(filename);
      if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        appendDiagnostic(QStringLiteral("Warning: Could not create %1").arg(filename));
        continue;
      }

      QTextStream out(&file);
      out << activity << "\n";
      out << "Enrollment: " << activitySum.assigned << "\n";
      out << "Capacity: " << activitySum.capacity << "\n";
      out << "\n";

      for (const QString &name : studentNames) {
        out << name << "\n";
      }

      file.close();
      filesCreated++;
    }

    appendDiagnostic(QStringLiteral("Exported results.xlsx and %1 activity rosters to %2")
                         .arg(filesCreated)
                         .arg(folderPath));

    QMessageBox::information(
        q_ptr, QStringLiteral("Export Successful"),
        QStringLiteral("Successfully exported results.xlsx and %1 activity roster files to:\n%2")
            .arg(filesCreated)
            .arg(folderPath));
  }

  MainWindow *q_ptr;
  QTabWidget *tabWidget;
  QTableView *tableView;
  PreferenceModel *model;
  QListWidget *diagnostics;
  QLabel *studentCountLabel;
  QLabel *choiceCountLabel;
  QLabel *activityCountLabel;
  QTableWidget *capacityTable;
  QLabel *totalCapacityLabel;
  QSpinBox *defaultCapacitySpin;
  QPushButton *setAllCapacitiesButton;
  QPushButton *runButton;
  QLabel *resultsStatus;
  QTableWidget *resultsTable;
  QTreeWidget *activitySummary;
  QPushButton *exportResultsCsvButton;
  QPushButton *exportResultsXlsxButton;
  QFutureWatcher<SolverResult> *solverWatcher;
  QProgressDialog *progressDialog;
  QString lastDataPath;
  std::optional<SolverResult> lastResult;
  bool hasResult = false;
};

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_impl(std::make_unique<Impl>(this)) {
  setWindowTitle(QStringLiteral("Engineering Week Scheduler"));
  resize(1100, 760);
}

MainWindow::~MainWindow() = default;
