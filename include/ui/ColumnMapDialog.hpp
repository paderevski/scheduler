#pragma once

#include <QDialog>
#include <QComboBox>
#include <QLabel>
#include <QStringList>
#include <optional>

struct ColumnMapping {
  int studentIdColumn = -1;
  int firstNameColumn = -1;
  int lastNameColumn = -1;
  int gradeColumn = -1;
  int dayColumn = -1;
  int teacherColumn = -1;
  int pathwayColumn = -1;
  int presentColumn = -1;
  int choice1Column = -1;
  int choice2Column = -1;
  int choice3Column = -1;
  int choice4Column = -1;
  int choice5Column = -1;

  bool isValid() const {
    return studentIdColumn >= 0 &&
           firstNameColumn >= 0 &&
           lastNameColumn >= 0 &&
           choice1Column >= 0 &&
           choice2Column >= 0 &&
           choice3Column >= 0;
  }
};

class ColumnMapDialog : public QDialog {
  Q_OBJECT

public:
  explicit ColumnMapDialog(const QStringList &columnHeaders,
                          QWidget *parent = nullptr);

  ColumnMapping getMapping() const;

private:
  void setupUi();
  void tryAutoMap();
  QComboBox* createColumnCombo();

  QStringList m_columnHeaders;
  QComboBox *m_studentIdCombo;
  QComboBox *m_firstNameCombo;
  QComboBox *m_lastNameCombo;
  QComboBox *m_gradeCombo;
  QComboBox *m_dayCombo;
  QComboBox *m_teacherCombo;
  QComboBox *m_pathwayCombo;
  QComboBox *m_presentCombo;
  QComboBox *m_choice1Combo;
  QComboBox *m_choice2Combo;
  QComboBox *m_choice3Combo;
  QComboBox *m_choice4Combo;
  QComboBox *m_choice5Combo;
};
