#pragma once

#include <QAbstractTableModel>
#include <QModelIndex>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <optional>
#include <vector>

struct StudentPreferenceRow {
  QString studentId;
  QString studentName;
  QStringList choices; // ordered by priority (choice1, choice2, ...)
};

struct PreferenceSummary {
  int studentCount = 0;
  int maxChoices = 0;
  int missingStudentIds = 0;
  int missingChoices = 0;
};

class PreferenceModel : public QAbstractTableModel {
  Q_OBJECT

public:
  explicit PreferenceModel(QObject *parent = nullptr);

  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  int columnCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override;
  QVariant headerData(int section, Qt::Orientation orientation,
                      int role = Qt::DisplayRole) const override;
  Qt::ItemFlags flags(const QModelIndex &index) const override;
  bool setData(const QModelIndex &index, const QVariant &value,
               int role = Qt::EditRole) override;

  void setHeaders(QStringList headers);
  void setRows(std::vector<StudentPreferenceRow> rows);
  const std::vector<StudentPreferenceRow> &rows() const noexcept;
  const QStringList &headers() const noexcept;

  [[nodiscard]] PreferenceSummary summarize() const;
  [[nodiscard]] bool isDirty() const noexcept;
  void setDirty(bool dirty) noexcept;

private:
  QStringList m_headers;
  std::vector<StudentPreferenceRow> m_rows;
  bool m_dirty = false;
  mutable bool m_validationDirty = true;
  mutable std::vector<int> m_duplicateIdCounts;

  void invalidateValidationCache() const;
  void rebuildValidationCache() const;
};
