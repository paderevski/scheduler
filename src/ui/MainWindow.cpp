#include "ui/MainWindow.hpp"

#include "models/PreferenceModel.hpp"
#include "solver/SolverEngine.hpp"
#include "ui/ColumnMapDialog.hpp"
#include "utils/SpreadsheetBridge.hpp"

#include <QAbstractItemView>
#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QPdfWriter>
#include <QPixmap>
#include <QProgressDialog>
#include <QPushButton>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>
#include <QSpinBox>
#include <QStackedWidget>
#include <QString>
#include <QStringList>
#include <QTabWidget>
#include <QTableView>
#include <QTableWidget>
#include <QTextStream>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QtConcurrent>

#if HAVE_QT_CHARTS
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLegend>
#include <QtCharts/QPieSeries>
#include <QtCharts/QPieSlice>
#endif

#include <algorithm>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace {
constexpr int kRequiredChoiceCount = 3;
constexpr int kMaxChoiceCount = 5;

QStringList defaultHeaders() {
  return {QStringLiteral("Student ID"), QStringLiteral("First Name"),
          QStringLiteral("Last Name"),  QStringLiteral("Grade"),
          QStringLiteral("Day"),        QStringLiteral("Teacher"),
          QStringLiteral("Pathway"),    QStringLiteral("Present"),
          QStringLiteral("Choice 1"),   QStringLiteral("Choice 2"),
          QStringLiteral("Choice 3"),   QStringLiteral("Choice 4"),
          QStringLiteral("Choice 5")};
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
      line[1] = row.firstName.trimmed().toStdString();
    }
    if (columnCount > 2) {
      line[2] = row.lastName.trimmed().toStdString();
    }
    if (columnCount > 3) {
      line[3] = row.grade.trimmed().toStdString();
    }
    if (columnCount > 4) {
      line[4] = row.day.trimmed().toStdString();
    }
    if (columnCount > 5) {
      line[5] = row.teacher.trimmed().toStdString();
    }
    if (columnCount > 6) {
      line[6] = row.pathway.trimmed().toStdString();
    }
    if (columnCount > 7) {
      line[7] = row.present.trimmed().toStdString();
    }
    for (int col = 8; col < columnCount; ++col) {
      const int choiceIndex = col - 8;
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

std::optional<std::vector<StudentPreferenceRow>>
rowsFromTableWithMapping(const SpreadsheetTable &table,
                         const ColumnMapping &mapping,
                         QStringList &headersOut) {
  headersOut = defaultHeaders();

  std::vector<StudentPreferenceRow> rows;
  rows.reserve(table.rows.size());

  for (const auto &line : table.rows) {
    if (line.empty()) {
      continue;
    }

    StudentPreferenceRow row;

    // Extract required fields
    if (mapping.studentIdColumn >= 0 &&
        mapping.studentIdColumn < static_cast<int>(line.size())) {
      row.studentId =
          QString::fromStdString(line[mapping.studentIdColumn]).trimmed();
    }

    if (mapping.firstNameColumn >= 0 &&
        mapping.firstNameColumn < static_cast<int>(line.size())) {
      row.firstName =
          QString::fromStdString(line[mapping.firstNameColumn]).trimmed();
    }

    if (mapping.lastNameColumn >= 0 &&
        mapping.lastNameColumn < static_cast<int>(line.size())) {
      row.lastName =
          QString::fromStdString(line[mapping.lastNameColumn]).trimmed();
    }

    // Extract optional fields
    if (mapping.gradeColumn >= 0 &&
        mapping.gradeColumn < static_cast<int>(line.size())) {
      row.grade = QString::fromStdString(line[mapping.gradeColumn]).trimmed();
    }

    if (mapping.dayColumn >= 0 &&
        mapping.dayColumn < static_cast<int>(line.size())) {
      row.day = QString::fromStdString(line[mapping.dayColumn]).trimmed();
    }

    if (mapping.teacherColumn >= 0 &&
        mapping.teacherColumn < static_cast<int>(line.size())) {
      row.teacher =
          QString::fromStdString(line[mapping.teacherColumn]).trimmed();
    }

    if (mapping.pathwayColumn >= 0 &&
        mapping.pathwayColumn < static_cast<int>(line.size())) {
      row.pathway =
          QString::fromStdString(line[mapping.pathwayColumn]).trimmed();
    }

    if (mapping.presentColumn >= 0 &&
        mapping.presentColumn < static_cast<int>(line.size())) {
      row.present =
          QString::fromStdString(line[mapping.presentColumn]).trimmed();
    }
    if (row.present.trimmed().isEmpty()) {
      row.present = QStringLiteral("yes");
    }

    // Extract choices
    if (mapping.choice1Column >= 0 &&
        mapping.choice1Column < static_cast<int>(line.size())) {
      row.choices.append(
          QString::fromStdString(line[mapping.choice1Column]).trimmed());
    } else {
      row.choices.append(QString());
    }

    if (mapping.choice2Column >= 0 &&
        mapping.choice2Column < static_cast<int>(line.size())) {
      row.choices.append(
          QString::fromStdString(line[mapping.choice2Column]).trimmed());
    } else {
      row.choices.append(QString());
    }

    if (mapping.choice3Column >= 0 &&
        mapping.choice3Column < static_cast<int>(line.size())) {
      row.choices.append(
          QString::fromStdString(line[mapping.choice3Column]).trimmed());
    } else {
      row.choices.append(QString());
    }

    if (mapping.choice4Column >= 0 &&
        mapping.choice4Column < static_cast<int>(line.size())) {
      row.choices.append(
          QString::fromStdString(line[mapping.choice4Column]).trimmed());
    } else if (mapping.choice4Column >= 0) {
      row.choices.append(QString());
    }

    if (mapping.choice5Column >= 0 &&
        mapping.choice5Column < static_cast<int>(line.size())) {
      row.choices.append(
          QString::fromStdString(line[mapping.choice5Column]).trimmed());
    } else if (mapping.choice5Column >= 0) {
      row.choices.append(QString());
    }

    rows.push_back(std::move(row));
  }

  return rows;
}

SpreadsheetTable resultsToTable(const SolverResult &result) {
  SpreadsheetTable table;
  table.headers = {"Student ID", "First Name",  "Last Name",
                   "Grade",      "Day",         "Teacher",
                   "Pathway",    "Present",     "Assigned Activity",
                   "Period",     "Choice Rank", "Score"};
  for (const auto &assignment : result.assignments) {
    std::vector<std::string> row;
    row.reserve(12);
    row.push_back(assignment.studentId);
    row.push_back(assignment.firstName);
    row.push_back(assignment.lastName);
    row.push_back(assignment.grade);
    row.push_back(assignment.day);
    row.push_back(assignment.teacher);
    row.push_back(assignment.pathway);
    row.push_back(assignment.present);
    row.push_back(assignment.activity);
    if (assignment.period >= 0) {
      row.push_back("P" + std::to_string(assignment.period + 1));
    } else {
      row.push_back(std::string());
    }
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

// Shared struct for student roster information
struct StudentInfo {
  QString lastName;
  QString firstName;
  QString studentId;
  QString pathway;
  QString grade;
  QString present;
  QString period;
};

// Struct to hold choice satisfaction counts
struct ChoiceSatisfactionCounts {
  int choice1Total = 0, choice1Yes = 0, choice1Maybe = 0, choice1No = 0;
  int choice2Total = 0, choice2Yes = 0, choice2Maybe = 0, choice2No = 0;
  int choice3Total = 0, choice3Yes = 0, choice3Maybe = 0, choice3No = 0;
  int choice4Total = 0, choice4Yes = 0, choice4Maybe = 0, choice4No = 0;
  int choice5Total = 0, choice5Yes = 0, choice5Maybe = 0, choice5No = 0;
  int notSatisfiedTotal = 0, notSatisfiedYes = 0, notSatisfiedMaybe = 0,
      notSatisfiedNo = 0;
};

struct ChoiceSatisfactionSummary {
  ChoiceSatisfactionCounts counts;
  int studentCount = 0;
};

ChoiceSatisfactionCounts countChoiceSatisfaction(const SolverResult &result) {
  ChoiceSatisfactionCounts counts;

  for (const auto &assignment : result.assignments) {
    QString present = toQString(assignment.present).trimmed().toLower();
    bool isYes = (present == "yes" || present == "y");
    bool isMaybe = (present == "maybe" || present == "m");
    bool isNo = (present == "no" || present == "n");

    if (assignment.choiceRank == 0) {
      counts.choice1Total++;
      if (isYes)
        counts.choice1Yes++;
      else if (isMaybe)
        counts.choice1Maybe++;
      else if (isNo)
        counts.choice1No++;
    } else if (assignment.choiceRank == 1) {
      counts.choice2Total++;
      if (isYes)
        counts.choice2Yes++;
      else if (isMaybe)
        counts.choice2Maybe++;
      else if (isNo)
        counts.choice2No++;
    } else if (assignment.choiceRank == 2) {
      counts.choice3Total++;
      if (isYes)
        counts.choice3Yes++;
      else if (isMaybe)
        counts.choice3Maybe++;
      else if (isNo)
        counts.choice3No++;
    } else if (assignment.choiceRank == 3) {
      counts.choice4Total++;
      if (isYes)
        counts.choice4Yes++;
      else if (isMaybe)
        counts.choice4Maybe++;
      else if (isNo)
        counts.choice4No++;
    } else if (assignment.choiceRank == 4) {
      counts.choice5Total++;
      if (isYes)
        counts.choice5Yes++;
      else if (isMaybe)
        counts.choice5Maybe++;
      else if (isNo)
        counts.choice5No++;
    } else {
      counts.notSatisfiedTotal++;
      if (isYes)
        counts.notSatisfiedYes++;
      else if (isMaybe)
        counts.notSatisfiedMaybe++;
      else if (isNo)
        counts.notSatisfiedNo++;
    }
  }

  return counts;
}

ChoiceSatisfactionSummary
countChoiceSatisfactionByStudent(const SolverResult &result) {
  ChoiceSatisfactionSummary summary;
  if (result.assignments.empty()) {
    return summary;
  }

  struct StudentFlags {
    bool choice1 = false;
    bool choice2 = false;
    bool choice3 = false;
    bool choice4 = false;
    bool choice5 = false;
    bool other = false;
    QString attendance;
  };

  QHash<QString, StudentFlags> perStudent;
  perStudent.reserve(static_cast<int>(result.assignments.size()));

  for (const auto &assignment : result.assignments) {
    const QString studentId = toQString(assignment.studentId).trimmed();
    const QString firstName = toQString(assignment.firstName).trimmed();
    const QString lastName = toQString(assignment.lastName).trimmed();
    const QString grade = toQString(assignment.grade).trimmed();
    const QString day = toQString(assignment.day).trimmed();
    const QString teacher = toQString(assignment.teacher).trimmed();
    const QString pathway = toQString(assignment.pathway).trimmed();
    const QString present = toQString(assignment.present).trimmed().toLower();

    QString key;
    if (!studentId.isEmpty()) {
      key = studentId;
    } else {
      key =
          QStringLiteral("%1|%2|%3|%4|%5|%6|%7")
              .arg(firstName, lastName, grade, day, teacher, pathway, present);
    }

    auto &flags = perStudent[key];
    if (flags.attendance.isEmpty()) {
      flags.attendance = present;
    }

    if (assignment.choiceRank == 0) {
      flags.choice1 = true;
    } else if (assignment.choiceRank == 1) {
      flags.choice2 = true;
    } else if (assignment.choiceRank == 2) {
      flags.choice3 = true;
    } else if (assignment.choiceRank == 3) {
      flags.choice4 = true;
    } else if (assignment.choiceRank == 4) {
      flags.choice5 = true;
    } else {
      flags.other = true;
    }
  }

  summary.studentCount = perStudent.size();
  for (auto it = perStudent.cbegin(); it != perStudent.cend(); ++it) {
    const auto &flags = it.value();
    const bool isYes = (flags.attendance == "yes" || flags.attendance == "y");
    const bool isMaybe =
        (flags.attendance == "maybe" || flags.attendance == "m");
    const bool isNo = (flags.attendance == "no" || flags.attendance == "n");

    if (flags.choice1) {
      summary.counts.choice1Total++;
      if (isYes)
        summary.counts.choice1Yes++;
      else if (isMaybe)
        summary.counts.choice1Maybe++;
      else if (isNo)
        summary.counts.choice1No++;
    }
    if (flags.choice2) {
      summary.counts.choice2Total++;
      if (isYes)
        summary.counts.choice2Yes++;
      else if (isMaybe)
        summary.counts.choice2Maybe++;
      else if (isNo)
        summary.counts.choice2No++;
    }
    if (flags.choice3) {
      summary.counts.choice3Total++;
      if (isYes)
        summary.counts.choice3Yes++;
      else if (isMaybe)
        summary.counts.choice3Maybe++;
      else if (isNo)
        summary.counts.choice3No++;
    }
    if (flags.choice4) {
      summary.counts.choice4Total++;
      if (isYes)
        summary.counts.choice4Yes++;
      else if (isMaybe)
        summary.counts.choice4Maybe++;
      else if (isNo)
        summary.counts.choice4No++;
    }
    if (flags.choice5) {
      summary.counts.choice5Total++;
      if (isYes)
        summary.counts.choice5Yes++;
      else if (isMaybe)
        summary.counts.choice5Maybe++;
      else if (isNo)
        summary.counts.choice5No++;
    }
    if (flags.other) {
      summary.counts.notSatisfiedTotal++;
      if (isYes)
        summary.counts.notSatisfiedYes++;
      else if (isMaybe)
        summary.counts.notSatisfiedMaybe++;
      else if (isNo)
        summary.counts.notSatisfiedNo++;
    }
  }

  return summary;
}

} // namespace

class MainWindow::Impl {
public:
  explicit Impl(MainWindow *q)
      : q_ptr(q), tabWidget(new QTabWidget(q)), welcomeWidget(new QWidget(q)),
        tableView(new QTableView(q)), model(new PreferenceModel(q)),
        diagnostics(new QListWidget(q)), recentProjectsList(new QListWidget(q)),
        studentCountLabel(new QLabel(q)), choiceCountLabel(new QLabel(q)),
        activityCountLabel(new QLabel(q)), capacityTable(new QTableWidget(q)),
        totalCapacityLabel(new QLabel(q)), defaultCapacitySpin(new QSpinBox(q)),
        periodCountSpin(new QSpinBox(q)),
        setAllCapacitiesButton(new QPushButton(QStringLiteral("Set All"), q)),
        autoCapacityButton(new QPushButton(QStringLiteral("Auto"), q)),
        weightingEnabledCheck(
            new QCheckBox(QStringLiteral("Enable Attendance Weighting"), q)),
        weightYesSpin(new QSpinBox(q)), weightMaybeSpin(new QSpinBox(q)),
        weightNoSpin(new QSpinBox(q)), weightYesNormLabel(new QLabel(q)),
        weightMaybeNormLabel(new QLabel(q)), weightNoNormLabel(new QLabel(q)),
        dayFilterCombo(new QComboBox(q)),
        runButton(new QPushButton(QStringLiteral("Calculate"), q)),
        resultsStatus(new QLabel(QStringLiteral("No solver run yet."), q)),
        resultsTable(new QTableWidget(q)),
        satisfactionSummary(new QTableWidget(q)),
        activitySummary(new QTreeWidget(q)),
        exportResultsCsvButton(
            new QPushButton(QStringLiteral("Export Results (CSV)"), q)),
        solverWatcher(new QFutureWatcher<SolverResult>(q)),
        progressDialog(new QProgressDialog(QStringLiteral("Running solver..."),
                                           QString(), 0, 0, q))
#if HAVE_QT_CHARTS
        ,
        overallChart(new QChartView(new QChart(), q)),
        yesChart(new QChartView(new QChart(), q)),
        maybeChart(new QChartView(new QChart(), q)),
        noChart(new QChartView(new QChart(), q))
#endif
  {
    setupUi();
    setupMenu();
    connectSignals();
    updateWeightControls();
    updateSummary();
  }

  void setupUi() {
    tableView->setModel(model);
    tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    diagnostics->setSelectionMode(QAbstractItemView::NoSelection);

    // Setup capacity table (headers assigned in updateCapacityTable)
    capacityTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    defaultCapacitySpin->setRange(0, 500);
    defaultCapacitySpin->setValue(0);
    defaultCapacitySpin->setToolTip(
        QStringLiteral("Default capacity for all activities"));

    periodCountSpin->setRange(1, 10);
    periodCountSpin->setValue(3);
    periodCountSpin->setToolTip(
        QStringLiteral("Number of periods per activity"));

    autoCapacityButton->setToolTip(QStringLiteral(
        "Calculate: (filtered students) / (# activities), rounded up"));

    // Create welcome screen
    setupWelcomeScreen();

    // Create data view with stacked widget to toggle between welcome and table
    auto *dataStackedWidget = new QStackedWidget(q_ptr);
    dataStackedWidget->addWidget(welcomeWidget); // Index 0
    dataStackedWidget->addWidget(tableView);     // Index 1

    auto *dataLayout = new QVBoxLayout();
    dataLayout->addWidget(dataStackedWidget);
    auto *dataWidget = new QWidget(q_ptr);
    dataWidget->setLayout(dataLayout);

    // Store stacked widget for later switching
    dataStack = dataStackedWidget;

    // Left side: Diagnostics
    auto *leftLayout = new QVBoxLayout();
    leftLayout->addWidget(
        new QLabel(QStringLiteral("<b>Diagnostics</b>"), q_ptr));
    leftLayout->addWidget(diagnostics);
    leftLayout->addWidget(studentCountLabel);
    leftLayout->addWidget(choiceCountLabel);
    leftLayout->addWidget(activityCountLabel);

    // Day filter
    auto *dayFilterLayout = new QHBoxLayout();
    dayFilterLayout->addWidget(
        new QLabel(QStringLiteral("Filter by Day:"), q_ptr));
    dayFilterCombo->addItem(QStringLiteral("All Days"), QString());
    dayFilterCombo->addItem(QStringLiteral("Day A"), QStringLiteral("A Day"));
    dayFilterCombo->addItem(QStringLiteral("Day B"), QStringLiteral("B Day"));
    dayFilterLayout->addWidget(dayFilterCombo);
    dayFilterLayout->addStretch();
    leftLayout->addLayout(dayFilterLayout);

    // Attendance weighting
    leftLayout->addWidget(
        new QLabel(QStringLiteral("<b>Attendance Weighting</b>"), q_ptr));
    leftLayout->addWidget(weightingEnabledCheck);
    weightingEnabledCheck->setChecked(true);

    weightYesSpin->setRange(1, 1000);
    weightYesSpin->setValue(100);
    weightYesSpin->setMinimumWidth(80);
    weightMaybeSpin->setRange(0, 1000);
    weightMaybeSpin->setValue(50);
    weightMaybeSpin->setMinimumWidth(80);
    weightNoSpin->setRange(0, 1000);
    weightNoSpin->setValue(10);
    weightNoSpin->setMinimumWidth(80);

    auto *weightGridLayout = new QGridLayout();
    weightGridLayout->addWidget(new QLabel(QStringLiteral("Yes:"), q_ptr), 0,
                                0);
    weightGridLayout->addWidget(weightYesSpin, 0, 1);
    weightGridLayout->addWidget(weightYesNormLabel, 0, 2);

    weightGridLayout->addWidget(new QLabel(QStringLiteral("Maybe:"), q_ptr), 1,
                                0);
    weightGridLayout->addWidget(weightMaybeSpin, 1, 1);
    weightGridLayout->addWidget(weightMaybeNormLabel, 1, 2);

    weightGridLayout->addWidget(new QLabel(QStringLiteral("No:"), q_ptr), 2, 0);
    weightGridLayout->addWidget(weightNoSpin, 2, 1);
    weightGridLayout->addWidget(weightNoNormLabel, 2, 2);

    weightGridLayout->setColumnStretch(3, 1);
    leftLayout->addLayout(weightGridLayout);

    updateWeightLabels();

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
    setAllLayout->addSpacing(10);
    setAllLayout->addWidget(new QLabel(QStringLiteral("Periods:"), q_ptr));
    setAllLayout->addWidget(periodCountSpin);
    setAllLayout->addWidget(autoCapacityButton);
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

    resultsTable->setColumnCount(12);
    resultsTable->setHorizontalHeaderLabels(
        {QStringLiteral("Student ID"), QStringLiteral("First Name"),
         QStringLiteral("Last Name"), QStringLiteral("Grade"),
         QStringLiteral("Day"), QStringLiteral("Teacher"),
         QStringLiteral("Pathway"), QStringLiteral("Present"),
         QStringLiteral("Activity"), QStringLiteral("Period"),
         QStringLiteral("Choice"), QStringLiteral("Score")});
    resultsTable->horizontalHeader()->setSectionResizeMode(
        QHeaderView::Stretch);
    resultsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    {
      const int periodCount = periodCountSpin->value();
      QStringList activityHeaders = {QStringLiteral("Activity"),
                                     QStringLiteral("Assigned"),
                                     QStringLiteral("Capacity")};
      for (int periodIdx = 0; periodIdx < periodCount; ++periodIdx) {
        activityHeaders.append(QStringLiteral("P%1").arg(periodIdx + 1));
      }
      activityHeaders.append(QStringLiteral("Yes"));
      activityHeaders.append(QStringLiteral("Maybe"));
      activityHeaders.append(QStringLiteral("No"));
      activityHeaders.append(QStringLiteral("Expected Util."));
      activitySummary->setColumnCount(activityHeaders.size());
      activitySummary->setHeaderLabels(activityHeaders);
    }

    // Setup satisfaction summary table
    satisfactionSummary->setColumnCount(6);
    satisfactionSummary->setHorizontalHeaderLabels(
        {QStringLiteral("Choice Rank"), QStringLiteral("Total"),
         QStringLiteral("Total %"), QStringLiteral("Yes"),
         QStringLiteral("Maybe"), QStringLiteral("No")});
    satisfactionSummary->horizontalHeader()->setSectionResizeMode(
        QHeaderView::Stretch);
    satisfactionSummary->setEditTriggers(QAbstractItemView::NoEditTriggers);
    satisfactionSummary->verticalHeader()->setVisible(false);

    auto *resultsLayout = new QVBoxLayout();
    resultsLayout->addWidget(resultsStatus);
    resultsLayout->addWidget(
        new QLabel(QStringLiteral("<b>Choice Satisfaction</b>"), q_ptr));
    resultsLayout->addWidget(satisfactionSummary);
    resultsLayout->addWidget(
        new QLabel(QStringLiteral("<b>Activity Assignments</b>"), q_ptr));
    resultsLayout->addWidget(resultsTable);
    resultsLayout->addWidget(
        new QLabel(QStringLiteral("<b>Activity Utilization</b>"), q_ptr));
    resultsLayout->addWidget(activitySummary);
#if HAVE_QT_CHARTS
    overallChart->setMinimumHeight(200);
    yesChart->setMinimumHeight(200);
    maybeChart->setMinimumHeight(200);
    noChart->setMinimumHeight(200);

    auto *chartsLabel = new QLabel(
        QStringLiteral("<b>Choice Satisfaction by Attendance</b>"), q_ptr);
    resultsLayout->addWidget(chartsLabel);

    auto *chartsGrid = new QGridLayout();
    chartsGrid->addWidget(overallChart, 0, 0);
    chartsGrid->addWidget(yesChart, 0, 1);
    chartsGrid->addWidget(maybeChart, 0, 2);
    chartsGrid->addWidget(noChart, 0, 3);
    resultsLayout->addLayout(chartsGrid);
#endif
    auto *resultsButtonLayout = new QHBoxLayout();
    resultsButtonLayout->addStretch();
    resultsButtonLayout->addWidget(exportResultsCsvButton);
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
    QObject::connect(autoCapacityButton, &QPushButton::clicked, q_ptr,
                     [this]() { autoSetCapacity(); });
    QObject::connect(exportResultsCsvButton, &QPushButton::clicked, q_ptr,
                     [this]() { exportResultsCsv(); });
  }

  void setupWelcomeScreen() {
    // === LAYOUT BASICS ===
    // QVBoxLayout arranges widgets vertically (top to bottom)
    // QHBoxLayout arranges widgets horizontally (left to right)
    // The welcomeWidget is the parent container for this entire screen

    auto *layout = new QVBoxLayout(welcomeWidget);
    layout->setContentsMargins(
        0, 0, 0, 0);        // Padding: left, top, right, bottom (in pixels)
    layout->setSpacing(10); // Space between widgets (in pixels)

    // === LOGO IMAGE ===
    // Add logo: prefer embedded resource, fall back to repo-relative file for
    // dev
    QPixmap logoPixmap;
    // Try resource first
    logoPixmap.load(QStringLiteral(":/images/logo.jpeg"));
    if (logoPixmap.isNull()) {
      // Fallback: relative to source tree (useful during development)
      const QString devPath =
          QDir(QCoreApplication::applicationDirPath())
              .absoluteFilePath(QStringLiteral("../../include/logo.jpeg"));
      if (QFile::exists(devPath)) {
        logoPixmap.load(devPath);
      }
    }

    if (!logoPixmap.isNull()) {
      logoPixmap = logoPixmap.scaledToWidth(
          300, Qt::SmoothTransformation); // Resize to 300px wide
      auto *logoLabel =
          new QLabel(welcomeWidget);    // QLabel can display text OR images
      logoLabel->setPixmap(logoPixmap); // Put the image into the label
      logoLabel->setAlignment(Qt::AlignCenter); // Center horizontally
      layout->addWidget(logoLabel);             // Add to the vertical layout
    }

    layout->addSpacing(0); // Add 20px of empty vertical space

    // === WELCOME TEXT ===
    // QLabel supports basic HTML tags like <h2>, <b>, <i>, <font color="red">,
    // etc.
    auto *welcomeLabel = new QLabel(
        QStringLiteral("<h2>Welcome to ClickSort</h2>"), welcomeWidget);
    welcomeLabel->setAlignment(Qt::AlignCenter); // Center the text
    layout->addWidget(welcomeLabel);

    layout->addSpacing(0); // More vertical space before columns

    // === TWO-COLUMN LAYOUT ===
    // HBoxLayout will place Recent Projects (left) and Action Buttons (right)
    // side-by-side
    auto *columnsLayout = new QHBoxLayout();
    columnsLayout->setSpacing(20); // 40px gap between the two columns

    // --- LEFT COLUMN: Recent Projects ---
    auto *recentLayout = new QVBoxLayout(); // Vertical layout for this column
    auto *recentLabel =
        new QLabel(QStringLiteral("<b>Recent Projects</b>"), welcomeWidget);
    recentLabel->setStyleSheet(
        QStringLiteral("font-size: 16pt; font-weight: bold;"));

    recentLayout->addWidget(recentLabel);

    // QListWidget displays a list of items (like a file browser)
    recentProjectsList->setSelectionMode(
        QAbstractItemView::SingleSelection);   // Only one item selectable
    recentProjectsList->setMaximumHeight(200); // Limit height to 300px

    // CSS-like styling: border, rounded corners, padding, hover effects
    recentProjectsList->setStyleSheet(QStringLiteral(
        "QListWidget { border: 1px solid #ccc; border-radius: 4px; padding: "
        "5px; font-size: 14pt; }"
        "QListWidget::item { padding: 2px; }"
        "QListWidget::item:hover { background-color: #e8e8e8; }"));

    // QObject::connect hooks up events: when user double-clicks item, run this
    // code
    QObject::connect(
        recentProjectsList, &QListWidget::itemDoubleClicked, q_ptr,
        [this](QListWidgetItem *item) {
          QString path =
              item->data(Qt::UserRole).toString(); // Get stored file path
          if (!path.isEmpty() && QFile::exists(path)) {
            doLoadProject(path); // Open the project
          }
        });
    recentLayout->addWidget(recentProjectsList);

    // Small button below the list
    auto *clearRecentButton =
        new QPushButton(QStringLiteral("Clear Recent"), welcomeWidget);
    clearRecentButton->setMaximumWidth(
        120); // Limit width so it doesn't stretch
    QObject::connect(clearRecentButton, &QPushButton::clicked, q_ptr, [this]() {
      QSettings
          settings; // QSettings stores app preferences/data between sessions
      settings.remove(
          QStringLiteral("recentProjects")); // Delete the saved list
      updateRecentProjects();                // Refresh the display
    });
    recentLayout->addWidget(clearRecentButton);
    recentLayout
        ->addStretch(); // Push everything up (fills remaining space at bottom)

    // --- RIGHT COLUMN: Action Buttons ---
    auto *buttonLayout = new QVBoxLayout(); // Vertical layout for this column
    auto *actionsLabel = new QLabel(QStringLiteral("Actions"), welcomeWidget);
    actionsLabel->setStyleSheet(
        QStringLiteral("font-size: 16pt; font-weight: bold;"));
    buttonLayout->addWidget(actionsLabel);
    buttonLayout->addSpacing(5); // Small space below label

    // Three large buttons with consistent styling
    auto *openProjectButton =
        new QPushButton(QStringLiteral("Open Project"), welcomeWidget);
    openProjectButton->setMinimumHeight(50); // Make button 50px tall
    openProjectButton->setStyleSheet(QStringLiteral("font-size: 14pt;"));
    QObject::connect(openProjectButton, &QPushButton::clicked, q_ptr,
                     [this]() { loadProject(); }); // What happens when clicked
    buttonLayout->addWidget(openProjectButton);

    auto *importDataButton =
        new QPushButton(QStringLiteral("Import Data"), welcomeWidget);
    importDataButton->setMinimumHeight(50);
    importDataButton->setStyleSheet(QStringLiteral("font-size: 14pt;"));
    QObject::connect(importDataButton, &QPushButton::clicked, q_ptr,
                     [this]() { openDataFile(); });
    buttonLayout->addWidget(importDataButton);

    auto *newProjectButton =
        new QPushButton(QStringLiteral("New Project"), welcomeWidget);
    newProjectButton->setMinimumHeight(50);
    newProjectButton->setStyleSheet(QStringLiteral("font-size: 14pt;"));
    QObject::connect(newProjectButton, &QPushButton::clicked, q_ptr, [this]() {
      model->setRows({});            // Clear all data
      currentProjectPath.clear();    // Forget current project path
      updateWindowTitle();           // Update window title bar
      dataStack->setCurrentIndex(0); // Switch to welcome screen (index 0)
    });
    buttonLayout->addWidget(newProjectButton);
    buttonLayout->addStretch(); // Push buttons to top of column

    // === ASSEMBLE THE COLUMNS ===
    // The number "1" means both columns get equal width (1:1 ratio)
    // Use "2" for one column to make it twice as wide as the other
    columnsLayout->addLayout(recentLayout, 1); // Add left column (ratio: 1)
    columnsLayout->addLayout(buttonLayout, 1); // Add right column (ratio: 1)
    layout->addLayout(
        columnsLayout); // Add the two-column layout to main vertical layout
    // layout->addStretch();                        // Push everything to top of
    // screen

    // === POPULATE DATA ===
    updateRecentProjects(); // Load and display recent projects from QSettings
  }

  void updateRecentProjects() {
    // Refresh the list widget with current recent projects from storage
    recentProjectsList->clear(); // Remove all items from the list

    // QSettings persists data between app sessions (like Windows Registry or
    // macOS preferences)
    QSettings settings;
    QStringList recent =
        settings.value(QStringLiteral("recentProjects")).toStringList();

    // Loop through each saved project path
    for (const QString &path : recent) {
      if (QFile::exists(path)) { // Only show if file still exists on disk
        // QListWidgetItem represents one row in the list
        auto *item = new QListWidgetItem(
            QFileInfo(path).fileName()); // Display just filename
        item->setData(
            Qt::UserRole,
            path); // Store full path invisibly (UserRole = custom data)
        item->setToolTip(path);            // Show full path on hover
        recentProjectsList->addItem(item); // Add to list widget
      }
    }

    // If no projects, show a placeholder message
    if (recentProjectsList->count() == 0) {
      auto *item = new QListWidgetItem(QStringLiteral("No recent projects"));
      item->setFlags(
          Qt::NoItemFlags); // Make it non-selectable and non-clickable
      item->setForeground(QColor(Qt::gray)); // Gray text color
      recentProjectsList->addItem(item);
    }
  }

  void addToRecentProjects(const QString &path) {
    // Add a project to the recent list (called when saving or loading)
    QSettings settings;
    QStringList recent =
        settings.value(QStringLiteral("recentProjects")).toStringList();

    // Remove if already exists (prevents duplicates)
    recent.removeAll(path);

    // Add to front of list (most recent first)
    recent.prepend(path);

    // Keep only 5 most recent projects
    while (recent.size() > 5) {
      recent.removeLast(); // Remove oldest
    }

    // Save back to persistent storage
    settings.setValue(QStringLiteral("recentProjects"), recent);

    // Update the UI to show new list
    updateRecentProjects();
  }

  void setupMenu() {
    auto *fileMenu = q_ptr->menuBar()->addMenu(QStringLiteral("File"));

    auto *openProjectAction =
        fileMenu->addAction(QStringLiteral("Open Project..."));
    openProjectAction->setShortcut(QKeySequence::Open);
    QObject::connect(openProjectAction, &QAction::triggered, q_ptr,
                     [this]() { loadProject(); });

    auto *saveProjectAction =
        fileMenu->addAction(QStringLiteral("Save Project"));
    saveProjectAction->setShortcut(QKeySequence::Save);
    QObject::connect(saveProjectAction, &QAction::triggered, q_ptr,
                     [this]() { saveProject(); });

    auto *saveProjectAsAction =
        fileMenu->addAction(QStringLiteral("Save Project As..."));
    saveProjectAsAction->setShortcut(QKeySequence::SaveAs);
    QObject::connect(saveProjectAsAction, &QAction::triggered, q_ptr,
                     [this]() { saveProjectAs(); });

    fileMenu->addSeparator();

    auto *openDataAction =
        fileMenu->addAction(QStringLiteral("Import Data from CSV..."));
    QObject::connect(openDataAction, &QAction::triggered, q_ptr,
                     [this]() { openDataFile(); });

    auto *exportCsvAction =
        fileMenu->addAction(QStringLiteral("Export Data as CSV..."));
    QObject::connect(exportCsvAction, &QAction::triggered, q_ptr,
                     [this]() { exportDataCsv(); });

    fileMenu->addSeparator();
    auto *quitAction = fileMenu->addAction(QStringLiteral("Quit"));
    quitAction->setShortcut(QKeySequence::Quit);
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

    QObject::connect(dayFilterCombo,
                     QOverload<int>::of(&QComboBox::currentIndexChanged), q_ptr,
                     [this]() { updateSummary(); });

    QObject::connect(weightYesSpin, QOverload<int>::of(&QSpinBox::valueChanged),
                     q_ptr, [this]() { updateWeightLabels(); });
    QObject::connect(weightMaybeSpin,
                     QOverload<int>::of(&QSpinBox::valueChanged), q_ptr,
                     [this]() { updateWeightLabels(); });
    QObject::connect(weightNoSpin, QOverload<int>::of(&QSpinBox::valueChanged),
                     q_ptr, [this]() { updateWeightLabels(); });
    QObject::connect(weightingEnabledCheck, &QCheckBox::toggled, q_ptr,
                     [this]() { updateWeightControls(); });
    QObject::connect(periodCountSpin,
                     QOverload<int>::of(&QSpinBox::valueChanged), q_ptr,
                     [this]() { updateSummary(); });
  }

  void appendDiagnostic(const QString &message) {
    diagnostics->addItem(message);
    diagnostics->scrollToBottom();
  }

  void updateWeightControls() {
    bool enabled = weightingEnabledCheck->isChecked();
    weightYesSpin->setEnabled(enabled);
    weightMaybeSpin->setEnabled(enabled);
    weightNoSpin->setEnabled(enabled);
    updateWeightLabels();
  }

  void updateWeightLabels() {
    if (!weightingEnabledCheck->isChecked()) {
      // When disabled, all weights are 1.0
      weightYesNormLabel->setText(QStringLiteral("(×1.00)"));
      weightMaybeNormLabel->setText(QStringLiteral("(×1.00)"));
      weightNoNormLabel->setText(QStringLiteral("(×1.00)"));
      return;
    }

    int yesVal = weightYesSpin->value();
    int maybeVal = weightMaybeSpin->value();
    int noVal = weightNoSpin->value();

    // Normalize so "yes" = 1.0
    double normYes = 1.0;
    double normMaybe =
        yesVal > 0 ? static_cast<double>(maybeVal) / yesVal : 0.0;
    double normNo = yesVal > 0 ? static_cast<double>(noVal) / yesVal : 0.0;

    weightYesNormLabel->setText(
        QStringLiteral("(×%1)").arg(normYes, 0, 'f', 2));
    weightMaybeNormLabel->setText(
        QStringLiteral("(×%1)").arg(normMaybe, 0, 'f', 2));
    weightNoNormLabel->setText(QStringLiteral("(×%1)").arg(normNo, 0, 'f', 2));
  }

  int countFilteredStudents() const {
    const QString dayFilter = dayFilterCombo->currentData().toString();
    int count = 0;
    for (const auto &row : model->rows()) {
      if (dayFilter.isEmpty() || row.day == dayFilter) {
        count++;
      }
    }
    return count;
  }

  void updateSummary() {
    const QString dayFilter = dayFilterCombo->currentData().toString();

    // Count filtered students
    int filteredStudentCount = 0;
    int missingIds = 0;
    int blankChoices = 0;

    QSet<QString> activities;
    for (const auto &row : model->rows()) {
      // Apply day filter
      if (!dayFilter.isEmpty() && row.day != dayFilter) {
        continue;
      }

      // Skip students with invalid attendance values
      QString attendance = row.present.trimmed().toLower();
      const bool validAttendance = attendance.isEmpty() ||
                                   attendance == "yes" || attendance == "no" ||
                                   attendance == "maybe" || attendance == "y" ||
                                   attendance == "n" || attendance == "m";
      if (!validAttendance) {
        continue;
      }

      filteredStudentCount++;

      if (row.studentId.trimmed().isEmpty()) {
        missingIds++;
      }

      for (int i = 0; i < row.choices.size(); ++i) {
        const auto normalized = row.choices[i].trimmed();
        if (normalized.isEmpty()) {
          if (i < kRequiredChoiceCount) {
            blankChoices++;
          }
        } else {
          activities.insert(normalized);
        }
      }
    }

    QString dayFilterText = dayFilter.isEmpty()
                                ? QStringLiteral("")
                                : QStringLiteral(" (Day %1)").arg(dayFilter);
    studentCountLabel->setText(
        QStringLiteral("Students: %1 (missing IDs: %2)%3")
            .arg(filteredStudentCount)
            .arg(missingIds)
            .arg(dayFilterText));
    choiceCountLabel->setText(
        QStringLiteral("Blank choices: %1").arg(blankChoices));
    activityCountLabel->setText(
        QStringLiteral("Unique activities: %1").arg(activities.size()));

    const bool hasData = filteredStudentCount > 0;
    runButton->setEnabled(hasData && !solverWatcher->isRunning());

    // Update capacity table with detected activities
    updateCapacityTable(activities);
  }

  void updateCapacityTable(const QSet<QString> &activities) {
    const int periodCount = periodCountSpin->value();
    const int choiceColumns = kMaxChoiceCount;
    const int periodStartCol = 1 + choiceColumns;

    // Store current capacities before clearing
    QMap<QString, QVector<int>> currentCapacities;
    for (int row = 0; row < capacityTable->rowCount(); ++row) {
      auto *activityItem = capacityTable->item(row, 0);
      if (!activityItem) {
        continue;
      }
      QVector<int> capacities(periodCount, defaultCapacitySpin->value());
      for (int periodIdx = 0; periodIdx < periodCount; ++periodIdx) {
        const int col = periodStartCol + periodIdx;
        auto *spinBox =
            qobject_cast<QSpinBox *>(capacityTable->cellWidget(row, col));
        if (spinBox) {
          capacities[periodIdx] = spinBox->value();
        }
      }
      currentCapacities[activityItem->text()] = capacities;
    }

    // Count how many students picked each activity as choice 1, 2, 3
    const QString dayFilter = dayFilterCombo->currentData().toString();
    QMap<QString, int> choice1Count;
    QMap<QString, int> choice2Count;
    QMap<QString, int> choice3Count;
    QMap<QString, int> choice4Count;
    QMap<QString, int> choice5Count;

    for (const auto &row : model->rows()) {
      // Apply day filter
      if (!dayFilter.isEmpty() && row.day != dayFilter) {
        continue;
      }

      // Skip students with invalid attendance values
      QString attendance = row.present.trimmed().toLower();
      if (attendance != "yes" && attendance != "no" && attendance != "maybe") {
        continue;
      }

      if (row.choices.size() > 0) {
        QString choice = row.choices[0].trimmed();
        if (!choice.isEmpty()) {
          choice1Count[choice]++;
        }
      }
      if (row.choices.size() > 1) {
        QString choice = row.choices[1].trimmed();
        if (!choice.isEmpty()) {
          choice2Count[choice]++;
        }
      }
      if (row.choices.size() > 2) {
        QString choice = row.choices[2].trimmed();
        if (!choice.isEmpty()) {
          choice3Count[choice]++;
        }
      }
      if (row.choices.size() > 3) {
        QString choice = row.choices[3].trimmed();
        if (!choice.isEmpty()) {
          choice4Count[choice]++;
        }
      }
      if (row.choices.size() > 4) {
        QString choice = row.choices[4].trimmed();
        if (!choice.isEmpty()) {
          choice5Count[choice]++;
        }
      }
    }

    // Rebuild table
    capacityTable->setRowCount(0);
    capacityTable->setColumnCount(periodStartCol + periodCount);
    QStringList headers = {
        QStringLiteral("Activity"), QStringLiteral("Choice 1"),
        QStringLiteral("Choice 2"), QStringLiteral("Choice 3"),
        QStringLiteral("Choice 4"), QStringLiteral("Choice 5")};
    for (int periodIdx = 0; periodIdx < periodCount; ++periodIdx) {
      headers.append(QStringLiteral("P%1").arg(periodIdx + 1));
    }
    capacityTable->setHorizontalHeaderLabels(headers);
    capacityTable->horizontalHeader()->setSectionResizeMode(
        0, QHeaderView::Stretch);
    for (int col = 1; col < capacityTable->columnCount(); ++col) {
      capacityTable->horizontalHeader()->setSectionResizeMode(
          col, QHeaderView::ResizeToContents);
    }
    QStringList sortedActivities = activities.values();
    sortedActivities.sort(Qt::CaseInsensitive);

    for (const QString &activity : sortedActivities) {
      int row = capacityTable->rowCount();
      capacityTable->insertRow(row);

      // Column 0: Activity name
      auto *activityItem = new QTableWidgetItem(activity);
      activityItem->setFlags(Qt::ItemIsEnabled);
      capacityTable->setItem(row, 0, activityItem);

      // Column 1: Choice 1 count
      auto *choice1Item = new QTableWidgetItem(
          QString::number(choice1Count.value(activity, 0)));
      choice1Item->setFlags(Qt::ItemIsEnabled);
      choice1Item->setTextAlignment(Qt::AlignCenter);
      capacityTable->setItem(row, 1, choice1Item);

      // Column 2: Choice 2 count
      auto *choice2Item = new QTableWidgetItem(
          QString::number(choice2Count.value(activity, 0)));
      choice2Item->setFlags(Qt::ItemIsEnabled);
      choice2Item->setTextAlignment(Qt::AlignCenter);
      capacityTable->setItem(row, 2, choice2Item);

      // Column 3: Choice 3 count
      auto *choice3Item = new QTableWidgetItem(
          QString::number(choice3Count.value(activity, 0)));
      choice3Item->setFlags(Qt::ItemIsEnabled);
      choice3Item->setTextAlignment(Qt::AlignCenter);
      capacityTable->setItem(row, 3, choice3Item);

        // Column 4: Choice 4 count
        auto *choice4Item = new QTableWidgetItem(
          QString::number(choice4Count.value(activity, 0)));
        choice4Item->setFlags(Qt::ItemIsEnabled);
        choice4Item->setTextAlignment(Qt::AlignCenter);
        capacityTable->setItem(row, 4, choice4Item);

        // Column 5: Choice 5 count
        auto *choice5Item = new QTableWidgetItem(
          QString::number(choice5Count.value(activity, 0)));
        choice5Item->setFlags(Qt::ItemIsEnabled);
        choice5Item->setTextAlignment(Qt::AlignCenter);
        capacityTable->setItem(row, 5, choice5Item);

      // Period capacity spinboxes
      const QVector<int> storedCaps = currentCapacities.value(
          activity, QVector<int>(periodCount, defaultCapacitySpin->value()));
      for (int periodIdx = 0; periodIdx < periodCount; ++periodIdx) {
        auto *spinBox = new QSpinBox(q_ptr);
        spinBox->setRange(0, 500);
        spinBox->setValue(
            storedCaps.value(periodIdx, defaultCapacitySpin->value()));

        // Connect spinbox to update total capacity when changed
        QObject::connect(spinBox, QOverload<int>::of(&QSpinBox::valueChanged),
                         q_ptr, [this]() { updateTotalCapacity(); });

        capacityTable->setCellWidget(row, periodStartCol + periodIdx, spinBox);
      }
    }

    updateTotalCapacity();
  }

  void updateTotalCapacity() {
    int totalCapacity = 0;
    const int periodStartCol = 1 + kMaxChoiceCount;
    for (int row = 0; row < capacityTable->rowCount(); ++row) {
      for (int col = periodStartCol; col < capacityTable->columnCount(); ++col) {
        auto *spinBox =
            qobject_cast<QSpinBox *>(capacityTable->cellWidget(row, col));
        if (spinBox) {
          totalCapacity += spinBox->value();
        }
      }
    }

    const int studentCount = countFilteredStudents();
    const int periodCount = periodCountSpin->value();
    const int requiredCapacity = studentCount * periodCount;
    bool sufficient = totalCapacity >= requiredCapacity;

    QString labelText =
        QStringLiteral("<b>Total Capacity:</b> %1 / %2 assignments")
            .arg(totalCapacity)
            .arg(requiredCapacity);

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

  void autoSetCapacity() {
    const int filteredStudentCount = countFilteredStudents();
    const int activityCount = capacityTable->rowCount();
    const int periodCount = periodCountSpin->value();
    if (activityCount > 0 && filteredStudentCount > 0 && periodCount > 0) {
      // Calculate: students / (activities * periods), rounded up
      const int divisor = activityCount * periodCount;
      int autoCapacity = (filteredStudentCount + divisor - 1) / divisor;
      defaultCapacitySpin->setValue(autoCapacity);
    }
  }

  void setAllCapacities() {
    int defaultValue = defaultCapacitySpin->value();
    const int periodStartCol = 1 + kMaxChoiceCount;
    for (int row = 0; row < capacityTable->rowCount(); ++row) {
      for (int col = periodStartCol; col < capacityTable->columnCount(); ++col) {
        auto *spinBox =
            qobject_cast<QSpinBox *>(capacityTable->cellWidget(row, col));
        if (spinBox) {
          spinBox->setValue(defaultValue);
        }
      }
    }
    // updateTotalCapacity() will be called automatically via valueChanged
    // signals
  }

  void openDataFile() {
    const QString path = QFileDialog::getOpenFileName(
        q_ptr, QStringLiteral("Open student preferences"), {},
        QStringLiteral("CSV Files (*.csv)"));
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

    if (table.rows.empty()) {
      QMessageBox::warning(
          q_ptr, QStringLiteral("Empty data"),
          QStringLiteral("The selected file contained no rows."));
      return false;
    }

    // Convert headers to QStringList
    QStringList columnHeaders;
    for (const auto &header : table.headers) {
      columnHeaders.append(QString::fromStdString(header));
    }

    // Show column mapping dialog
    ColumnMapDialog dialog(columnHeaders, q_ptr);
    if (dialog.exec() != QDialog::Accepted) {
      return false;
    }

    ColumnMapping mapping = dialog.getMapping();
    if (!mapping.isValid()) {
      QMessageBox::warning(q_ptr, QStringLiteral("Invalid mapping"),
                           QStringLiteral("Please map all required fields."));
      return false;
    }

    QStringList headers;
    auto rowsOpt = rowsFromTableWithMapping(table, mapping, headers);
    if (!rowsOpt || rowsOpt->empty()) {
      QMessageBox::warning(
          q_ptr, QStringLiteral("No data"),
          QStringLiteral("No valid student records found in the file."));
      return false;
    }

    diagnostics->clear();
    model->setHeaders(headers);
    model->setRows(std::move(*rowsOpt));
    lastDataPath = path;
    appendDiagnostic(QStringLiteral("Loaded %1 students from %2.")
                         .arg(model->rowCount())
                         .arg(QFileInfo(path).fileName()));

    // Switch to table view when data is loaded
    if (dataStack && model->rowCount() > 0) {
      dataStack->setCurrentIndex(1);
    }

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

  void saveProject() {
    if (currentProjectPath.isEmpty()) {
      saveProjectAs();
    } else {
      doSaveProject(currentProjectPath);
    }
  }

  void saveProjectAs() {
    const QString path = QFileDialog::getSaveFileName(
        q_ptr, QStringLiteral("Save Project As"),
        currentProjectPath.isEmpty() ? QStringLiteral("untitled.clicksort")
                                     : currentProjectPath,
        QStringLiteral("ClickSort Projects (*.clicksort)"));
    if (path.isEmpty()) {
      return;
    }
    doSaveProject(path);
  }

  void doSaveProject(const QString &path) {
    QJsonObject root;
    root["version"] = "1.0";

    // Save settings
    QJsonObject settings;
    settings["defaultCapacity"] = defaultCapacitySpin->value();
    settings["periodCount"] = periodCountSpin->value();
    settings["weightingEnabled"] = weightingEnabledCheck->isChecked();

    QJsonObject weights;
    weights["yes"] = weightYesSpin->value();
    weights["maybe"] = weightMaybeSpin->value();
    weights["no"] = weightNoSpin->value();
    settings["weights"] = weights;

    settings["dayFilter"] = dayFilterCombo->currentData().toString();

    // Save activity capacities per period
    QJsonObject activityPeriodCapacities;
    const int periodCount = periodCountSpin->value();
    const int periodStartCol = 1 + kMaxChoiceCount;
    for (int row = 0; row < capacityTable->rowCount(); ++row) {
      auto *activityItem = capacityTable->item(row, 0);
      if (!activityItem) {
        continue;
      }
      QJsonArray caps;
      for (int periodIdx = 0; periodIdx < periodCount; ++periodIdx) {
        const int col = periodStartCol + periodIdx;
        auto *spinBox =
            qobject_cast<QSpinBox *>(capacityTable->cellWidget(row, col));
        caps.append(spinBox ? spinBox->value() : defaultCapacitySpin->value());
      }
      activityPeriodCapacities[activityItem->text()] = caps;
    }
    settings["activityPeriodCapacities"] = activityPeriodCapacities;
    root["settings"] = settings;

    // Save student data
    QJsonArray students;
    for (const auto &row : model->rows()) {
      QJsonObject student;
      student["studentId"] = row.studentId;
      student["firstName"] = row.firstName;
      student["lastName"] = row.lastName;
      student["grade"] = row.grade;
      student["day"] = row.day;
      student["teacher"] = row.teacher;
      student["pathway"] = row.pathway;
      student["present"] = row.present;

      QJsonArray choices;
      for (const auto &choice : row.choices) {
        choices.append(choice);
      }
      student["choices"] = choices;

      students.append(student);
    }
    root["students"] = students;

    // Save solution if available
    if (hasResult && lastResult) {
      QJsonObject solution;
      solution["totalStudents"] = lastResult->totalStudents;
      solution["satisfiedStudents"] = lastResult->satisfiedStudents;
      solution["runtimeMs"] = static_cast<qint64>(lastResult->runtimeMs);

      QJsonArray assignments;
      for (const auto &assignment : lastResult->assignments) {
        QJsonObject assgn;
        assgn["studentId"] = QString::fromStdString(assignment.studentId);
        assgn["firstName"] = QString::fromStdString(assignment.firstName);
        assgn["lastName"] = QString::fromStdString(assignment.lastName);
        assgn["grade"] = QString::fromStdString(assignment.grade);
        assgn["day"] = QString::fromStdString(assignment.day);
        assgn["teacher"] = QString::fromStdString(assignment.teacher);
        assgn["pathway"] = QString::fromStdString(assignment.pathway);
        assgn["present"] = QString::fromStdString(assignment.present);
        assgn["activity"] = QString::fromStdString(assignment.activity);
        assgn["period"] = assignment.period;
        assgn["choiceRank"] = assignment.choiceRank;
        assgn["score"] = assignment.score;
        assignments.append(assgn);
      }
      solution["assignments"] = assignments;

      QJsonArray activitySummaries;
      for (const auto &summary : lastResult->activitySummary) {
        QJsonObject summ;
        summ["activity"] = QString::fromStdString(summary.activity);
        summ["assigned"] = summary.assigned;
        summ["capacity"] = summary.capacity;
        summ["presentYes"] = summary.presentYes;
        summ["presentMaybe"] = summary.presentMaybe;
        summ["presentNo"] = summary.presentNo;
        if (!summary.assignedPerPeriod.empty()) {
          QJsonArray assignedPerPeriod;
          for (int value : summary.assignedPerPeriod) {
            assignedPerPeriod.append(value);
          }
          summ["assignedPerPeriod"] = assignedPerPeriod;
        }
        if (!summary.capacityPerPeriod.empty()) {
          QJsonArray capacityPerPeriod;
          for (int value : summary.capacityPerPeriod) {
            capacityPerPeriod.append(value);
          }
          summ["capacityPerPeriod"] = capacityPerPeriod;
        }
        activitySummaries.append(summ);
      }
      solution["activitySummary"] = activitySummaries;

      root["solution"] = solution;
    }

    // Save metadata
    QJsonObject metadata;
    metadata["modified"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    root["metadata"] = metadata;

    // Write to file
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
      QMessageBox::warning(
          q_ptr, QStringLiteral("Unable to save"),
          QStringLiteral("Could not open file for writing:\n%1").arg(path));
      return;
    }

    QJsonDocument doc(root);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    currentProjectPath = path;
    addToRecentProjects(path);
    updateWindowTitle();
    appendDiagnostic(
        QStringLiteral("Saved project to %1").arg(QFileInfo(path).fileName()));
  }

  void loadProject() {
    const QString path = QFileDialog::getOpenFileName(
        q_ptr, QStringLiteral("Open Project"),
        currentProjectPath.isEmpty() ? QString()
                                     : QFileInfo(currentProjectPath).path(),
        QStringLiteral("ClickSort Projects (*.clicksort)"));
    if (path.isEmpty()) {
      return;
    }
    doLoadProject(path);
  }

  void doLoadProject(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
      QMessageBox::warning(
          q_ptr, QStringLiteral("Unable to open"),
          QStringLiteral("Could not open project file:\n%1").arg(path));
      return;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
      QMessageBox::warning(q_ptr, QStringLiteral("Invalid project file"),
                           QStringLiteral("Failed to parse JSON:\n%1")
                               .arg(parseError.errorString()));
      return;
    }

    QJsonObject root = doc.object();
    QString version = root["version"].toString();
    if (version != "1.0") {
      QMessageBox::warning(
          q_ptr, QStringLiteral("Unsupported version"),
          QStringLiteral("This project file version (%1) is not supported.")
              .arg(version));
      return;
    }

    // Load settings
    QJsonObject settings = root["settings"].toObject();
    defaultCapacitySpin->setValue(settings["defaultCapacity"].toInt());
    periodCountSpin->setValue(settings["periodCount"].toInt(3));
    weightingEnabledCheck->setChecked(settings["weightingEnabled"].toBool());

    QJsonObject weights = settings["weights"].toObject();
    weightYesSpin->setValue(weights["yes"].toInt());
    weightMaybeSpin->setValue(weights["maybe"].toInt());
    weightNoSpin->setValue(weights["no"].toInt());

    QString dayFilter = settings["dayFilter"].toString();
    int dayIndex = dayFilterCombo->findData(dayFilter);
    if (dayIndex >= 0) {
      dayFilterCombo->setCurrentIndex(dayIndex);
    }

    SolverOptions loadedOptions;
    loadedOptions.defaultCapacity = settings["defaultCapacity"].toInt();
    loadedOptions.periodCount = settings["periodCount"].toInt(3);
    if (settings["weightingEnabled"].toBool()) {
      loadedOptions.weightYes = weights["yes"].toInt();
      loadedOptions.weightMaybe = weights["maybe"].toInt();
      loadedOptions.weightNo = weights["no"].toInt();
    } else {
      loadedOptions.weightYes = 1;
      loadedOptions.weightMaybe = 1;
      loadedOptions.weightNo = 1;
    }
    lastOptions = loadedOptions;
    lastDayFilter = dayFilter;

    // Load student data
    QJsonArray students = root["students"].toArray();
    std::vector<StudentPreferenceRow> rows;
    for (const auto &studentValue : students) {
      QJsonObject student = studentValue.toObject();
      StudentPreferenceRow row;
      row.studentId = student["studentId"].toString();
      row.firstName = student["firstName"].toString();
      row.lastName = student["lastName"].toString();
      row.grade = student["grade"].toString();
      row.day = student["day"].toString();
      row.teacher = student["teacher"].toString();
      row.pathway = student["pathway"].toString();
      row.present = student["present"].toString();
      if (row.present.trimmed().isEmpty()) {
        row.present = QStringLiteral("yes");
      }

      QJsonArray choices = student["choices"].toArray();
      for (const auto &choiceValue : choices) {
        row.choices.push_back(choiceValue.toString());
      }

      rows.push_back(row);
    }

    diagnostics->clear();
    model->setHeaders(defaultHeaders());
    model->setRows(std::move(rows));

    // Restore activity capacities after updating summary
    QJsonObject activityPeriodCapacities =
        settings["activityPeriodCapacities"].toObject();
    if (activityPeriodCapacities.isEmpty() &&
        settings.contains("activityCapacities")) {
      QJsonObject legacyCaps = settings["activityCapacities"].toObject();
      const int periodCount = periodCountSpin->value();
      for (auto it = legacyCaps.begin(); it != legacyCaps.end(); ++it) {
        QJsonArray caps;
        for (int periodIdx = 0; periodIdx < periodCount; ++periodIdx) {
          caps.append(it.value().toInt());
        }
        activityPeriodCapacities[it.key()] = caps;
      }
    }
    if (lastOptions) {
      lastOptions->activityPeriodCapacities.clear();
      for (auto it = activityPeriodCapacities.begin();
           it != activityPeriodCapacities.end(); ++it) {
        const auto key = it.key().toStdString();
        std::vector<int> caps;
        const QJsonArray values = it.value().toArray();
        caps.reserve(values.size());
        for (const auto &value : values) {
          caps.push_back(value.toInt());
        }
        lastOptions->activityPeriodCapacities[key] = std::move(caps);
      }
    }
    const int periodCount = periodCountSpin->value();
    const int periodStartCol = 1 + kMaxChoiceCount;
    for (int row = 0; row < capacityTable->rowCount(); ++row) {
      auto *activityItem = capacityTable->item(row, 0);
      if (!activityItem) {
        continue;
      }
      QString activity = activityItem->text();
      if (!activityPeriodCapacities.contains(activity)) {
        continue;
      }
      QJsonArray caps = activityPeriodCapacities[activity].toArray();
      for (int periodIdx = 0; periodIdx < periodCount; ++periodIdx) {
        const int col = periodStartCol + periodIdx;
        auto *spinBox =
            qobject_cast<QSpinBox *>(capacityTable->cellWidget(row, col));
        if (spinBox && periodIdx < caps.size()) {
          spinBox->setValue(caps[periodIdx].toInt());
        }
      }
    }

    // Load solution if available
    if (root.contains("solution")) {
      QJsonObject solution = root["solution"].toObject();
      SolverResult result;
      result.totalStudents = solution["totalStudents"].toInt();
      result.satisfiedStudents = solution["satisfiedStudents"].toInt();
      result.runtimeMs = solution["runtimeMs"].toInteger();

      QJsonArray assignments = solution["assignments"].toArray();
      for (const auto &assgnValue : assignments) {
        QJsonObject assgn = assgnValue.toObject();
        StudentAssignment assignment;
        assignment.studentId = assgn["studentId"].toString().toStdString();
        assignment.firstName = assgn["firstName"].toString().toStdString();
        assignment.lastName = assgn["lastName"].toString().toStdString();
        assignment.grade = assgn["grade"].toString().toStdString();
        assignment.day = assgn["day"].toString().toStdString();
        assignment.teacher = assgn["teacher"].toString().toStdString();
        assignment.pathway = assgn["pathway"].toString().toStdString();
        assignment.present = assgn["present"].toString().toStdString();
        assignment.activity = assgn["activity"].toString().toStdString();
        assignment.period = assgn["period"].toInt(-1);
        assignment.choiceRank = assgn["choiceRank"].toInt();
        assignment.score = assgn["score"].toDouble();
        result.assignments.push_back(assignment);
      }

      // Load activity summary
      QJsonArray activitySummaries = solution["activitySummary"].toArray();
      for (const auto &summValue : activitySummaries) {
        QJsonObject summ = summValue.toObject();
        ActivitySummaryRow summary;
        summary.activity = summ["activity"].toString().toStdString();
        summary.assigned = summ["assigned"].toInt();
        summary.capacity = summ["capacity"].toInt();
        summary.presentYes = summ["presentYes"].toInt();
        summary.presentMaybe = summ["presentMaybe"].toInt();
        summary.presentNo = summ["presentNo"].toInt();
        if (summ.contains("assignedPerPeriod")) {
          QJsonArray assignedPerPeriod = summ["assignedPerPeriod"].toArray();
          summary.assignedPerPeriod.reserve(assignedPerPeriod.size());
          for (const auto &value : assignedPerPeriod) {
            summary.assignedPerPeriod.push_back(value.toInt());
          }
        }
        if (summ.contains("capacityPerPeriod")) {
          QJsonArray capacityPerPeriod = summ["capacityPerPeriod"].toArray();
          summary.capacityPerPeriod.reserve(capacityPerPeriod.size());
          for (const auto &value : capacityPerPeriod) {
            summary.capacityPerPeriod.push_back(value.toInt());
          }
        }
        result.activitySummary.push_back(summary);
      }

      lastResult = result;
      hasResult = true;
      populateResults(result);
      refreshResultExports();
    }

    currentProjectPath = path;
    addToRecentProjects(path);
    updateWindowTitle();
    appendDiagnostic(QStringLiteral("Loaded project from %1")
                         .arg(QFileInfo(path).fileName()));

    // Switch to table view when project is loaded
    if (dataStack && model->rowCount() > 0) {
      dataStack->setCurrentIndex(1);
    }
  }

  void updateWindowTitle() {
    QString title = QStringLiteral("ClickSort");
    if (!currentProjectPath.isEmpty()) {
      title += QStringLiteral(" - ") + QFileInfo(currentProjectPath).fileName();
    }
    q_ptr->setWindowTitle(title);
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

    // Filter rows by selected day
    const QString dayFilter = dayFilterCombo->currentData().toString();
    const auto &allRows = model->rows();
    std::vector<StudentPreferenceRow> filteredRows;

    for (const auto &row : allRows) {
      if (dayFilter.isEmpty() || row.day == dayFilter) {
        filteredRows.push_back(row);
      }
    }

    if (filteredRows.empty()) {
      QMessageBox::information(
          q_ptr, QStringLiteral("No students"),
          QStringLiteral("No students match the selected day filter."));
      return;
    }

    diagnostics->clear();

    SolverOptions options;
    options.defaultCapacity = defaultCapacitySpin->value();
    options.periodCount = periodCountSpin->value();

    // Check total capacity against filtered students
    int totalCapacity = 0;
    const int periodStartCol = 1 + kMaxChoiceCount;
    for (int row = 0; row < capacityTable->rowCount(); ++row) {
      for (int col = periodStartCol; col < capacityTable->columnCount(); ++col) {
        auto *spinBox =
            qobject_cast<QSpinBox *>(capacityTable->cellWidget(row, col));
        if (spinBox) {
          totalCapacity += spinBox->value();
        }
      }
    }

    const int studentCount = static_cast<int>(filteredRows.size());
    const int requiredCapacity = studentCount * options.periodCount;
    if (totalCapacity < requiredCapacity) {
      auto reply = QMessageBox::warning(
          q_ptr, QStringLiteral("Insufficient Capacity"),
          QStringLiteral(
              "Total capacity (%1) is less than number of assignments (%2).\n\n"
              "The solver will fail to assign all students.\n\n"
              "Do you want to proceed anyway?")
              .arg(totalCapacity)
              .arg(requiredCapacity),
          QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

      if (reply != QMessageBox::Yes) {
        return;
      }
    }

    // If weighting is disabled, set all weights to 1
    if (!weightingEnabledCheck->isChecked()) {
      options.weightYes = 1;
      options.weightMaybe = 1;
      options.weightNo = 1;
    } else {
      options.weightYes = weightYesSpin->value();
      options.weightMaybe = weightMaybeSpin->value();
      options.weightNo = weightNoSpin->value();
    }

    // Collect per-activity capacities from table
    for (int row = 0; row < capacityTable->rowCount(); ++row) {
      auto *activityItem = capacityTable->item(row, 0);
      if (!activityItem) {
        continue;
      }
      std::string activityName = activityItem->text().toStdString();
      std::vector<int> capacities;
      capacities.reserve(options.periodCount);
      for (int periodIdx = 0; periodIdx < options.periodCount; ++periodIdx) {
        const int col = periodStartCol + periodIdx;
        auto *spinBox =
            qobject_cast<QSpinBox *>(capacityTable->cellWidget(row, col));
        capacities.push_back(spinBox ? spinBox->value()
                                     : options.defaultCapacity);
      }
      options.activityPeriodCapacities[activityName] = std::move(capacities);
    }

    // Store options and day filter for report generation
    lastOptions = options;
    lastDayFilter = dayFilter;

    QString filterMsg = dayFilter.isEmpty()
                            ? QStringLiteral("all students")
                            : QStringLiteral("Day %1 students").arg(dayFilter);
    appendDiagnostic(
        QStringLiteral(
            "Launching solver for %1 with per-activity capacities...")
            .arg(filterMsg));
    progressDialog->setLabelText(QStringLiteral("Calculating assignments..."));
    progressDialog->show();
    runButton->setEnabled(false);

    auto future =
        QtConcurrent::run([rows = std::move(filteredRows), options]() {
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
      const auto firstName = toQString(assignment.firstName);
      const auto lastName = toQString(assignment.lastName);
      const auto grade = toQString(assignment.grade);
      const auto day = toQString(assignment.day);
      const auto teacher = toQString(assignment.teacher);
      const auto pathway = toQString(assignment.pathway);
      const auto present = toQString(assignment.present);
      const auto activity = toQString(assignment.activity);
      const QString period =
          assignment.period >= 0
              ? QStringLiteral("P%1").arg(assignment.period + 1)
              : QStringLiteral("-");
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
      resultsTable->setItem(rowIndex, 1, makeItem(firstName));
      resultsTable->setItem(rowIndex, 2, makeItem(lastName));
      resultsTable->setItem(rowIndex, 3, makeItem(grade));
      resultsTable->setItem(rowIndex, 4, makeItem(day));
      resultsTable->setItem(rowIndex, 5, makeItem(teacher));
      resultsTable->setItem(rowIndex, 6, makeItem(pathway));
      resultsTable->setItem(rowIndex, 7, makeItem(present));
      resultsTable->setItem(rowIndex, 8, makeItem(activity));
      resultsTable->setItem(rowIndex, 9, makeItem(period));
      resultsTable->setItem(rowIndex, 10, makeItem(rank));
      resultsTable->setItem(rowIndex, 11, makeItem(score));
      ++rowIndex;
    }

    // Populate satisfaction summary using helper function
    const auto summary = countChoiceSatisfactionByStudent(result);
    const auto &counts = summary.counts;
    const int totalStudents = summary.studentCount;

    auto makeSatisfactionItem = [](const QString &text, bool center = false) {
      auto *item = new QTableWidgetItem(text);
      item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
      if (center) {
        item->setTextAlignment(Qt::AlignCenter);
      }
      return item;
    };

    satisfactionSummary->setRowCount(6);

    // Row 0: 1st Choice
    satisfactionSummary->setItem(
      0, 0, makeSatisfactionItem(QStringLiteral("1st Choice")));
    satisfactionSummary->setItem(
      0, 1, makeSatisfactionItem(QString::number(counts.choice1Total), true));
    satisfactionSummary->setItem(
      0, 2,
      makeSatisfactionItem(
        totalStudents > 0
          ? QStringLiteral("%1%").arg(
              counts.choice1Total * 100.0 / totalStudents, 0, 'f', 1)
          : QStringLiteral("0%"),
        true));
    satisfactionSummary->setItem(
      0, 3, makeSatisfactionItem(QString::number(counts.choice1Yes), true));
    satisfactionSummary->setItem(
      0, 4, makeSatisfactionItem(QString::number(counts.choice1Maybe), true));
    satisfactionSummary->setItem(
      0, 5, makeSatisfactionItem(QString::number(counts.choice1No), true));

    // Row 1: 2nd Choice
    satisfactionSummary->setItem(
      1, 0, makeSatisfactionItem(QStringLiteral("2nd Choice")));
    satisfactionSummary->setItem(
      1, 1, makeSatisfactionItem(QString::number(counts.choice2Total), true));
    satisfactionSummary->setItem(
      1, 2,
      makeSatisfactionItem(
        totalStudents > 0
          ? QStringLiteral("%1%").arg(
              counts.choice2Total * 100.0 / totalStudents, 0, 'f', 1)
          : QStringLiteral("0%"),
        true));
    satisfactionSummary->setItem(
      1, 3, makeSatisfactionItem(QString::number(counts.choice2Yes), true));
    satisfactionSummary->setItem(
      1, 4, makeSatisfactionItem(QString::number(counts.choice2Maybe), true));
    satisfactionSummary->setItem(
      1, 5, makeSatisfactionItem(QString::number(counts.choice2No), true));

    // Row 2: 3rd Choice
    satisfactionSummary->setItem(
      2, 0, makeSatisfactionItem(QStringLiteral("3rd Choice")));
    satisfactionSummary->setItem(
      2, 1, makeSatisfactionItem(QString::number(counts.choice3Total), true));
    satisfactionSummary->setItem(
      2, 2,
      makeSatisfactionItem(
        totalStudents > 0
          ? QStringLiteral("%1%").arg(
              counts.choice3Total * 100.0 / totalStudents, 0, 'f', 1)
          : QStringLiteral("0%"),
        true));
    satisfactionSummary->setItem(
      2, 3, makeSatisfactionItem(QString::number(counts.choice3Yes), true));
    satisfactionSummary->setItem(
      2, 4, makeSatisfactionItem(QString::number(counts.choice3Maybe), true));
    satisfactionSummary->setItem(
      2, 5, makeSatisfactionItem(QString::number(counts.choice3No), true));

    // Row 3: 4th Choice
    satisfactionSummary->setItem(
      3, 0, makeSatisfactionItem(QStringLiteral("4th Choice")));
    satisfactionSummary->setItem(
      3, 1, makeSatisfactionItem(QString::number(counts.choice4Total), true));
    satisfactionSummary->setItem(
      3, 2,
      makeSatisfactionItem(
        totalStudents > 0
          ? QStringLiteral("%1%").arg(
              counts.choice4Total * 100.0 / totalStudents, 0, 'f', 1)
          : QStringLiteral("0%"),
        true));
    satisfactionSummary->setItem(
      3, 3, makeSatisfactionItem(QString::number(counts.choice4Yes), true));
    satisfactionSummary->setItem(
      3, 4, makeSatisfactionItem(QString::number(counts.choice4Maybe), true));
    satisfactionSummary->setItem(
      3, 5, makeSatisfactionItem(QString::number(counts.choice4No), true));

    // Row 4: 5th Choice
    satisfactionSummary->setItem(
      4, 0, makeSatisfactionItem(QStringLiteral("5th Choice")));
    satisfactionSummary->setItem(
      4, 1, makeSatisfactionItem(QString::number(counts.choice5Total), true));
    satisfactionSummary->setItem(
      4, 2,
      makeSatisfactionItem(
        totalStudents > 0
          ? QStringLiteral("%1%").arg(
              counts.choice5Total * 100.0 / totalStudents, 0, 'f', 1)
          : QStringLiteral("0%"),
        true));
    satisfactionSummary->setItem(
      4, 3, makeSatisfactionItem(QString::number(counts.choice5Yes), true));
    satisfactionSummary->setItem(
      4, 4, makeSatisfactionItem(QString::number(counts.choice5Maybe), true));
    satisfactionSummary->setItem(
      4, 5, makeSatisfactionItem(QString::number(counts.choice5No), true));

    // Row 5: Not Satisfied (fallback)
    satisfactionSummary->setItem(
      5, 0, makeSatisfactionItem(QStringLiteral("Not Satisfied")));
    satisfactionSummary->setItem(
      5, 1,
      makeSatisfactionItem(QString::number(counts.notSatisfiedTotal), true));
    satisfactionSummary->setItem(
      5, 2,
      makeSatisfactionItem(totalStudents > 0 ? QStringLiteral("%1%").arg(
                             counts.notSatisfiedTotal *
                               100.0 / totalStudents,
                             0, 'f', 1)
                           : QStringLiteral("0%"),
                 true));
    satisfactionSummary->setItem(
      5, 3,
      makeSatisfactionItem(QString::number(counts.notSatisfiedYes), true));
    satisfactionSummary->setItem(
      5, 4,
      makeSatisfactionItem(QString::number(counts.notSatisfiedMaybe), true));
    satisfactionSummary->setItem(
      5, 5,
      makeSatisfactionItem(QString::number(counts.notSatisfiedNo), true));

    activitySummary->clear();
    int periodCount = 0;
    if (!result.activitySummary.empty()) {
      periodCount = static_cast<int>(
          result.activitySummary.front().capacityPerPeriod.size());
    }
    QStringList activityHeaders = {QStringLiteral("Activity"),
                                   QStringLiteral("Assigned"),
                                   QStringLiteral("Capacity")};
    for (int periodIdx = 0; periodIdx < periodCount; ++periodIdx) {
      activityHeaders.append(QStringLiteral("P%1").arg(periodIdx + 1));
    }
    activityHeaders.append(QStringLiteral("Yes"));
    activityHeaders.append(QStringLiteral("Maybe"));
    activityHeaders.append(QStringLiteral("No"));
    activityHeaders.append(QStringLiteral("Expected Util."));
    activitySummary->setColumnCount(activityHeaders.size());
    activitySummary->setHeaderLabels(activityHeaders);
    for (const auto &summary : result.activitySummary) {
      auto *item = new QTreeWidgetItem(activitySummary);
      item->setText(0, toQString(summary.activity));
      item->setText(1, QString::number(summary.assigned));
      item->setText(2, QString::number(summary.capacity));
      int col = 3;
      for (int periodIdx = 0;
          periodIdx < static_cast<int>(summary.assignedPerPeriod.size());
          ++periodIdx) {
        const int assigned = summary.assignedPerPeriod[periodIdx];
        const int capacity = summary.capacityPerPeriod[periodIdx];
        item->setText(col++,
                 QStringLiteral("%1/%2").arg(assigned).arg(capacity));
      }
      item->setText(col++, QString::number(summary.presentYes));
      item->setText(col++, QString::number(summary.presentMaybe));
      item->setText(col++, QString::number(summary.presentNo));

      // Calculate expected utilization: yes=1.0, maybe=0.5, no=0.0
      const double expectedAttendance =
          summary.presentYes + (summary.presentMaybe * 0.5);
      const double expectedUtil =
          summary.capacity == 0
              ? 0.0
              : expectedAttendance / static_cast<double>(summary.capacity);
      item->setText(col,
                    QStringLiteral("%1%").arg(expectedUtil * 100.0, 0, 'f', 1));
    }
    activitySummary->resizeColumnToContents(0);

    resultsStatus->setText(QStringLiteral("Satisfied %1/%2 students in %3 ms")
                               .arg(result.satisfiedStudents)
                               .arg(result.totalStudents)
                               .arg(result.runtimeMs));

#if HAVE_QT_CHARTS
    // Create pie charts for choice satisfaction breakdown by attendance group
    auto createPieChart = [](const QString &title, int choice1, int choice2,
                 int choice3, int choice4, int choice5,
                 int other) -> QChart * {
      auto *series = new QPieSeries();

      if (choice1 > 0) {
        auto *slice1 = series->append(QStringLiteral("1st Choice"), choice1);
        slice1->setBrush(QColor(76, 175, 80)); // Green
        slice1->setLabelVisible(false);
      }
      if (choice2 > 0) {
        auto *slice2 = series->append(QStringLiteral("2nd Choice"), choice2);
        slice2->setBrush(QColor(255, 193, 7)); // Amber
        slice2->setLabelVisible(false);
      }
      if (choice3 > 0) {
        auto *slice3 = series->append(QStringLiteral("3rd Choice"), choice3);
        slice3->setBrush(QColor(255, 152, 0)); // Orange
        slice3->setLabelVisible(false);
      }
      if (choice4 > 0) {
        auto *slice4 = series->append(QStringLiteral("4th Choice"), choice4);
        slice4->setBrush(QColor(33, 150, 243)); // Blue
        slice4->setLabelVisible(false);
      }
      if (choice5 > 0) {
        auto *slice5 = series->append(QStringLiteral("5th Choice"), choice5);
        slice5->setBrush(QColor(0, 150, 136)); // Teal
        slice5->setLabelVisible(false);
      }
      if (other > 0) {
        auto *sliceOther = series->append(QStringLiteral("Other"), other);
        sliceOther->setBrush(QColor(158, 158, 158)); // Grey
        sliceOther->setLabelVisible(false);
      }

      auto *chart = new QChart();
      chart->addSeries(series);
      chart->setTitle(title);
      chart->legend()->setVisible(true);
      chart->legend()->setAlignment(Qt::AlignRight);

      // Reduce spacing between pie and legend
      chart->legend()->setContentsMargins(-50, 0, 0,
                                          0); // Remove internal layout margins
      chart->setBackgroundRoundness(0);       // Optional: square corners

      return chart;
    };

    overallChart->setChart(createPieChart(
      QStringLiteral("Overall"), counts.choice1Total, counts.choice2Total,
      counts.choice3Total, counts.choice4Total, counts.choice5Total,
      counts.notSatisfiedTotal));
    yesChart->setChart(createPieChart(
      QStringLiteral("'Yes' Attendees"), counts.choice1Yes, counts.choice2Yes,
      counts.choice3Yes, counts.choice4Yes, counts.choice5Yes,
      counts.notSatisfiedYes));
    maybeChart->setChart(createPieChart(
      QStringLiteral("'Maybe' Attendees"), counts.choice1Maybe,
      counts.choice2Maybe, counts.choice3Maybe, counts.choice4Maybe,
      counts.choice5Maybe, counts.notSatisfiedMaybe));
    noChart->setChart(createPieChart(QStringLiteral("'No' Attendees"),
                     counts.choice1No, counts.choice2No,
                     counts.choice3No, counts.choice4No,
                     counts.choice5No, counts.notSatisfiedNo));
#endif
  }

  void refreshResultExports() {
    const bool enabled = hasResult;
    exportResultsCsvButton->setEnabled(enabled);
  }

  void exportSummaryReport(const QString &filePath) {
    if (!hasResult || !lastResult || !lastOptions) {
      return;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
      appendDiagnostic(
          QStringLiteral("Warning: Could not create summary report"));
      return;
    }

    QTextStream out(&file);
    out << "CLICKSORT - SUMMARY REPORT\n";
    out << "==========================================\n\n";

    // Solver Options
    out << "SOLVER OPTIONS\n";
    out << "--------------\n";
    out << "Day Filter: "
        << (lastDayFilter.isEmpty() ? "All Days" : lastDayFilter) << "\n";
    out << "Default Capacity: " << lastOptions->defaultCapacity << "\n";
    out << "Periods: " << lastOptions->periodCount << "\n";

    bool weightingEnabled =
        (lastOptions->weightYes != 1 || lastOptions->weightMaybe != 1 ||
         lastOptions->weightNo != 1) &&
        (lastOptions->weightYes != lastOptions->weightMaybe ||
         lastOptions->weightYes != lastOptions->weightNo);
    out << "Attendance Weighting: "
        << (weightingEnabled ? "Enabled" : "Disabled") << "\n";

    if (weightingEnabled) {
      out << "  Yes Weight: " << lastOptions->weightYes << " (×1.00)\n";
      double normMaybe = lastOptions->weightYes > 0
                             ? static_cast<double>(lastOptions->weightMaybe) /
                                   lastOptions->weightYes
                             : 0.0;
      double normNo = lastOptions->weightYes > 0
                          ? static_cast<double>(lastOptions->weightNo) /
                                lastOptions->weightYes
                          : 0.0;
      out << "  Maybe Weight: " << lastOptions->weightMaybe << " (×"
          << QString::number(normMaybe, 'f', 2) << ")\n";
      out << "  No Weight: " << lastOptions->weightNo << " (×"
          << QString::number(normNo, 'f', 2) << ")\n";
    }
    out << "\n";

    // Overall Results
    out << "OVERALL RESULTS\n";
    out << "---------------\n";
    out << "Total Students: " << lastResult->totalStudents << "\n";
    out << "Satisfied Students: " << lastResult->satisfiedStudents << "\n";
    double satisfactionRate = lastResult->totalStudents > 0
                                  ? (lastResult->satisfiedStudents * 100.0 /
                                     lastResult->totalStudents)
                                  : 0.0;
    out << "Satisfaction Rate: " << QString::number(satisfactionRate, 'f', 1)
        << "%\n";
    out << "Runtime: " << lastResult->runtimeMs << " ms\n";
    out << "\n";

    // Choice Satisfaction Summary
    out << "CHOICE SATISFACTION SUMMARY\n";
    out << "---------------------------\n";

    // Use helper function to count choice satisfaction
    const auto summary = countChoiceSatisfactionByStudent(*lastResult);
    const auto &counts = summary.counts;

    const int totalStudents = summary.studentCount;
    auto formatRow = [&out, totalStudents](const QString &rank, int total,
                                           int yes, int maybe, int no) {
      double pct = totalStudents > 0 ? (total * 100.0 / totalStudents) : 0.0;
      out << qSetFieldWidth(15) << Qt::left << rank << qSetFieldWidth(8)
          << Qt::right << total << qSetFieldWidth(10) << Qt::right
          << QString::number(pct, 'f', 1) + "%" << qSetFieldWidth(8)
          << Qt::right << yes << qSetFieldWidth(8) << Qt::right << maybe
          << qSetFieldWidth(8) << Qt::right << no << qSetFieldWidth(0) << "\n";
    };

    out << qSetFieldWidth(15) << Qt::left << "Rank" << qSetFieldWidth(8)
        << Qt::right << "Total" << qSetFieldWidth(10) << Qt::right << "Total %"
        << qSetFieldWidth(8) << Qt::right << "Yes" << qSetFieldWidth(8)
        << Qt::right << "Maybe" << qSetFieldWidth(8) << Qt::right << "No"
        << qSetFieldWidth(0) << "\n";
    out << QString(63, '-') << "\n";

    formatRow("1st Choice", counts.choice1Total, counts.choice1Yes,
              counts.choice1Maybe, counts.choice1No);
    formatRow("2nd Choice", counts.choice2Total, counts.choice2Yes,
              counts.choice2Maybe, counts.choice2No);
    formatRow("3rd Choice", counts.choice3Total, counts.choice3Yes,
              counts.choice3Maybe, counts.choice3No);
    formatRow("4th Choice", counts.choice4Total, counts.choice4Yes,
          counts.choice4Maybe, counts.choice4No);
    formatRow("5th Choice", counts.choice5Total, counts.choice5Yes,
          counts.choice5Maybe, counts.choice5No);
    formatRow("Not Satisfied", counts.notSatisfiedTotal, counts.notSatisfiedYes,
              counts.notSatisfiedMaybe, counts.notSatisfiedNo);
    out << "\n";

    // Activity Summary
    out << "ACTIVITY SUMMARY\n";
    out << "----------------\n";
    out << qSetFieldWidth(30) << Qt::left << "Activity" << qSetFieldWidth(10)
        << Qt::right << "Assigned" << qSetFieldWidth(10) << Qt::right
        << "Capacity" << qSetFieldWidth(8) << Qt::right << "Yes"
        << qSetFieldWidth(8) << Qt::right << "Maybe" << qSetFieldWidth(8)
        << Qt::right << "No" << qSetFieldWidth(12) << Qt::right << "Exp. Util."
        << qSetFieldWidth(0) << "\n";
    out << QString(86, '-') << "\n";

    for (const auto &summary : lastResult->activitySummary) {
      const QString activityName =
        toQString(summary.activity).left(20);
      const double expectedAttendance =
          summary.presentYes + (summary.presentMaybe * 0.5);
      const double expectedUtil =
          summary.capacity == 0
              ? 0.0
              : expectedAttendance / static_cast<double>(summary.capacity);

      out << qSetFieldWidth(30) << Qt::left << activityName
          << qSetFieldWidth(10) << Qt::right << summary.assigned
          << qSetFieldWidth(10) << Qt::right << summary.capacity
          << qSetFieldWidth(8) << Qt::right << summary.presentYes
          << qSetFieldWidth(8) << Qt::right << summary.presentMaybe
          << qSetFieldWidth(8) << Qt::right << summary.presentNo
          << qSetFieldWidth(12) << Qt::right
          << QString::number(expectedUtil * 100.0, 'f', 1) + "%"
          << qSetFieldWidth(0) << "\n";
    }

    file.close();
  }

  int generateActivityRosterPdfs(const QDir &dir) {
    if (!lastResult) {
      return 0;
    }

    int filesCreated = 0;
    for (const auto &activitySum : lastResult->activitySummary) {
      const QString activity = toQString(activitySum.activity);

      // Collect students for this activity
      std::vector<StudentInfo> students;
      for (const auto &assignment : lastResult->assignments) {
        if (assignment.activity == activitySum.activity) {
          StudentInfo info;
          info.lastName = toQString(assignment.lastName);
          info.firstName = toQString(assignment.firstName);
          info.studentId = toQString(assignment.studentId);
          info.pathway = toQString(assignment.pathway);
          info.grade = toQString(assignment.grade);
          info.present = toQString(assignment.present);
          info.period = assignment.period >= 0
                            ? QStringLiteral("P%1").arg(assignment.period + 1)
                            : QStringLiteral("-");
          students.push_back(info);
        }
      }

      // Sort by period, then last name, then first name
      std::sort(
          students.begin(), students.end(),
          [](const StudentInfo &a, const StudentInfo &b) {
            auto periodOrder = [](const QString &period) {
              if (period.startsWith('P')) {
                bool ok = false;
                const int value = period.mid(1).toInt(&ok);
                if (ok) {
                  return value;
                }
              }
              return std::numeric_limits<int>::max();
            };

            const int periodA = periodOrder(a.period);
            const int periodB = periodOrder(b.period);
            if (periodA != periodB) {
              return periodA < periodB;
            }
            if (a.lastName != b.lastName) {
              return a.lastName.compare(b.lastName, Qt::CaseInsensitive) < 0;
            }
            return a.firstName.compare(b.firstName, Qt::CaseInsensitive) < 0;
          });

      // Create sanitized filename
      QString filename = activity;
      filename.replace(QRegularExpression(QStringLiteral("[/\\\\:*?\"<>|]")),
                       QStringLiteral("_"));
      filename = dir.filePath(filename + QStringLiteral(".pdf"));

      QPdfWriter pdfWriter(filename);
      pdfWriter.setPageSize(QPageSize::Letter);
      pdfWriter.setPageMargins(QMarginsF(15, 15, 15, 15));

      QPainter painter(&pdfWriter);
      if (!painter.isActive()) {
        appendDiagnostic(
            QStringLiteral("Warning: Could not create %1").arg(filename));
        continue;
      }

      // Set up fonts
      QFont titleFont("Arial", 16, QFont::Bold);
      QFont headerFont("Arial", 10, QFont::Bold);
      QFont normalFont("Arial", 9);

      int y = 0;
      const int pageWidth = painter.device()->width();
      const int lineHeight = 300;
      const int headerHeight = 300;

      // Draw title
      painter.setFont(titleFont);
      painter.drawText(0, y, activity);
      y += headerHeight * 2;

      // Draw enrollment info
      painter.setFont(normalFont);
      painter.drawText(0, y,
                       QString("Enrollment: %1    Capacity: %2")
                           .arg(activitySum.assigned)
                           .arg(activitySum.capacity));
      y += headerHeight;

      // Table setup
      const int col1Width = pageWidth / 7;  // Last Name
      const int col2Width = pageWidth / 7;  // First Name
      const int col3Width = pageWidth / 5;  // Student ID
      const int col4Width = pageWidth / 5;  // Pathway
      const int col5Width = pageWidth / 10; // Grade
      const int col6Width = pageWidth / 10; // Period
      const int col7Width = pageWidth / 10; // Present
      Q_UNUSED(col7Width);

      // Draw table header
      painter.setFont(headerFont);
      painter.fillRect(0, y, pageWidth, headerHeight, QColor(76, 175, 80));
      painter.setPen(Qt::white);

      int x = 50;
      painter.drawText(x, y + 225, "Last Name");
      x += col1Width;
      painter.drawText(x, y + 225, "First Name");
      x += col2Width;
      painter.drawText(x, y + 225, "Student ID");
      x += col3Width;
      painter.drawText(x, y + 225, "Pathway");
      x += col4Width;
      painter.drawText(x, y + 225, "Grade");
      x += col5Width;
      painter.drawText(x, y + 225, "Period");
      x += col6Width;
      painter.drawText(x, y + 225, "Present");

      y += headerHeight;
      painter.setPen(Qt::black);
      painter.setFont(normalFont);

      // Draw table rows
      bool alternateRow = false;
      for (const auto &student : students) {
        // Check if we need a new page
        if (y + lineHeight > painter.device()->height() - 1000) {
          pdfWriter.newPage();
          y = 0;
        }

        // Alternate row background
        if (alternateRow) {
          painter.fillRect(0, y, pageWidth, lineHeight, QColor(242, 242, 242));
        }
        alternateRow = !alternateRow;

        // Draw cell borders and text
        x = 50;
        painter.drawText(x, y + 225, student.lastName);
        x += col1Width;
        painter.drawText(x, y + 225, student.firstName);
        x += col2Width;
        painter.drawText(x, y + 225, student.studentId);
        x += col3Width;
        painter.drawText(x, y + 225, student.pathway);
        x += col4Width;
        painter.drawText(x, y + 225, student.grade);
        x += col5Width;
        painter.drawText(x, y + 225, student.period);
        x += col6Width;
        painter.drawText(x, y + 225, student.present);

        y += lineHeight;
      }

      painter.end();
      filesCreated++;
    }

    return filesCreated;
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

    // Export activity roster PDFs
    const int filesCreated = generateActivityRosterPdfs(dir);

    // Export summary report
    const QString summaryPath = dir.filePath(QStringLiteral("summary.txt"));
    exportSummaryReport(summaryPath);

    appendDiagnostic(QStringLiteral("Exported results.csv, summary.txt, and %1 "
                                    "activity roster PDF files to %2")
                         .arg(filesCreated)
                         .arg(folderPath));

    QMessageBox::information(
        q_ptr, QStringLiteral("Export Successful"),
        QStringLiteral("Successfully exported results.csv, summary.txt, and %1 "
                       "activity roster PDF files to:\n%2")
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
  QSpinBox *periodCountSpin;
  QPushButton *setAllCapacitiesButton;
  QPushButton *autoCapacityButton;
  QCheckBox *weightingEnabledCheck;
  QSpinBox *weightYesSpin;
  QSpinBox *weightMaybeSpin;
  QSpinBox *weightNoSpin;
  QLabel *weightYesNormLabel;
  QLabel *weightMaybeNormLabel;
  QLabel *weightNoNormLabel;
  QComboBox *dayFilterCombo;
  QPushButton *runButton;
  QLabel *resultsStatus;
  QTableWidget *resultsTable;
  QTableWidget *satisfactionSummary;
  QTreeWidget *activitySummary;
  QPushButton *exportResultsCsvButton;
#if HAVE_QT_CHARTS
  QChartView *overallChart;
  QChartView *yesChart;
  QChartView *maybeChart;
  QChartView *noChart;
#endif
  QFutureWatcher<SolverResult> *solverWatcher;
  QProgressDialog *progressDialog;
  QString lastDataPath;
  QString currentProjectPath;
  QWidget *welcomeWidget;
  QStackedWidget *dataStack;
  QListWidget *recentProjectsList;
  std::optional<SolverResult> lastResult;
  std::optional<SolverOptions> lastOptions;
  QString lastDayFilter;
  bool hasResult = false;
};

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_impl(std::make_unique<Impl>(this)) {
  setWindowTitle(QStringLiteral("ClickSort"));
  resize(1100, 760);
}

MainWindow::~MainWindow() = default;