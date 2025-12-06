#include "ui/ColumnMapDialog.hpp"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

ColumnMapDialog::ColumnMapDialog(const QStringList &columnHeaders,
                                 QWidget *parent)
    : QDialog(parent), m_columnHeaders(columnHeaders) {
  setupUi();
  tryAutoMap();
}

ColumnMapping ColumnMapDialog::getMapping() const {
  ColumnMapping mapping;
  mapping.studentIdColumn = m_studentIdCombo->currentIndex() - 1;
  mapping.firstNameColumn = m_firstNameCombo->currentIndex() - 1;
  mapping.lastNameColumn = m_lastNameCombo->currentIndex() - 1;
  mapping.gradeColumn = m_gradeCombo->currentIndex() - 1;
  mapping.dayColumn = m_dayCombo->currentIndex() - 1;
  mapping.teacherColumn = m_teacherCombo->currentIndex() - 1;
  mapping.pathwayColumn = m_pathwayCombo->currentIndex() - 1;
  mapping.presentColumn = m_presentCombo->currentIndex() - 1;
  mapping.choice1Column = m_choice1Combo->currentIndex() - 1;
  mapping.choice2Column = m_choice2Combo->currentIndex() - 1;
  mapping.choice3Column = m_choice3Combo->currentIndex() - 1;
  return mapping;
}

void ColumnMapDialog::setupUi() {
  setWindowTitle(QStringLiteral("Map CSV Columns"));
  setMinimumWidth(500);

  auto *layout = new QVBoxLayout(this);

  auto *instructionLabel = new QLabel(
      QStringLiteral("<b>Map CSV columns to required fields</b><br>"
                     "Select which column contains each piece of student data."),
      this);
  instructionLabel->setWordWrap(true);
  layout->addWidget(instructionLabel);

  auto *formLayout = new QFormLayout();

  // Required fields
  m_studentIdCombo = createColumnCombo();
  formLayout->addRow(QStringLiteral("Student ID (required):"), m_studentIdCombo);

  m_firstNameCombo = createColumnCombo();
  formLayout->addRow(QStringLiteral("First Name (required):"), m_firstNameCombo);

  m_lastNameCombo = createColumnCombo();
  formLayout->addRow(QStringLiteral("Last Name (required):"), m_lastNameCombo);

  m_choice1Combo = createColumnCombo();
  formLayout->addRow(QStringLiteral("Choice 1 (required):"), m_choice1Combo);

  m_choice2Combo = createColumnCombo();
  formLayout->addRow(QStringLiteral("Choice 2 (required):"), m_choice2Combo);

  m_choice3Combo = createColumnCombo();
  formLayout->addRow(QStringLiteral("Choice 3 (required):"), m_choice3Combo);

  // Optional fields
  auto *optionalLabel = new QLabel(QStringLiteral("<br><b>Optional fields</b>"), this);
  formLayout->addRow(optionalLabel);

  m_gradeCombo = createColumnCombo();
  formLayout->addRow(QStringLiteral("Grade (optional):"), m_gradeCombo);

  m_dayCombo = createColumnCombo();
  formLayout->addRow(QStringLiteral("Day (optional):"), m_dayCombo);

  m_teacherCombo = createColumnCombo();
  formLayout->addRow(QStringLiteral("Teacher (optional):"), m_teacherCombo);

  m_pathwayCombo = createColumnCombo();
  formLayout->addRow(QStringLiteral("Pathway (optional):"), m_pathwayCombo);

  m_presentCombo = createColumnCombo();
  formLayout->addRow(QStringLiteral("Present (optional):"), m_presentCombo);

  layout->addLayout(formLayout);

  auto *buttonBox = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
  layout->addWidget(buttonBox);

  // Validation
  auto *okButton = buttonBox->button(QDialogButtonBox::Ok);
  auto validateMapping = [this, okButton]() {
    bool valid = getMapping().isValid();
    okButton->setEnabled(valid);
  };

  connect(m_studentIdCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, validateMapping);
  connect(m_firstNameCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, validateMapping);
  connect(m_lastNameCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, validateMapping);
  connect(m_choice1Combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, validateMapping);
  connect(m_choice2Combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, validateMapping);
  connect(m_choice3Combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, validateMapping);

  validateMapping();
}

QComboBox* ColumnMapDialog::createColumnCombo() {
  auto *combo = new QComboBox(this);
  combo->addItem(QStringLiteral("(none)"));
  for (const QString &header : m_columnHeaders) {
    combo->addItem(header);
  }
  return combo;
}

void ColumnMapDialog::tryAutoMap() {
  // Try to automatically map columns based on common header names
  for (int i = 0; i < m_columnHeaders.size(); ++i) {
    QString header = m_columnHeaders[i].toLower().trimmed();

    // Student ID variations
    if (header.contains("student") && header.contains("id")) {
      m_studentIdCombo->setCurrentIndex(i + 1);
    }
    else if (header == "id" || header == "studentid") {
      m_studentIdCombo->setCurrentIndex(i + 1);
    }

    // First name variations
    else if (header == "first name" || header == "firstname" ||
             header == "first" || header == "fname") {
      m_firstNameCombo->setCurrentIndex(i + 1);
    }

    // Last name variations
    else if (header == "last name" || header == "lastname" ||
             header == "last" || header == "lname" || header == "surname") {
      m_lastNameCombo->setCurrentIndex(i + 1);
    }

    // Grade variations
    else if (header == "grade" || header == "year" || header == "level") {
      m_gradeCombo->setCurrentIndex(i + 1);
    }

    // Day variations
    else if (header == "day" || header == "session") {
      m_dayCombo->setCurrentIndex(i + 1);
    }

    // Teacher variations
    else if (header == "teacher" || header == "instructor") {
      m_teacherCombo->setCurrentIndex(i + 1);
    }

    // Pathway variations
    else if (header == "pathway" || header == "track" || header == "program") {
      m_pathwayCombo->setCurrentIndex(i + 1);
    }

    // Present variations
    else if (header == "present" || header == "attendance" || header == "attending") {
      m_presentCombo->setCurrentIndex(i + 1);
    }

    // Choice variations
    else if (header == "choice 1" || header == "choice1" ||
             header == "1st choice" || header == "first choice" ||
             header == "preference 1" || header == "preference1") {
      m_choice1Combo->setCurrentIndex(i + 1);
    }
    else if (header == "choice 2" || header == "choice2" ||
             header == "2nd choice" || header == "second choice" ||
             header == "preference 2" || header == "preference2") {
      m_choice2Combo->setCurrentIndex(i + 1);
    }
    else if (header == "choice 3" || header == "choice3" ||
             header == "3rd choice" || header == "third choice" ||
             header == "preference 3" || header == "preference3") {
      m_choice3Combo->setCurrentIndex(i + 1);
    }
  }
}
