#include "models/PreferenceModel.hpp"

#include <QColor>
#include <QHash>

PreferenceModel::PreferenceModel(QObject *parent)
    : QAbstractTableModel(parent) {}

int PreferenceModel::rowCount(const QModelIndex &parent) const {
  if (parent.isValid()) {
    return 0;
  }
  return static_cast<int>(m_rows.size());
}

int PreferenceModel::columnCount(const QModelIndex &parent) const {
  if (parent.isValid()) {
    return 0;
  }
  return m_headers.isEmpty() ? 0 : m_headers.size();
}

QVariant PreferenceModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || index.row() >= static_cast<int>(m_rows.size())) {
    return {};
  }

  const auto &row = m_rows[static_cast<std::size_t>(index.row())];

  if (role == Qt::DisplayRole || role == Qt::EditRole) {
    if (index.column() == 0) {
      return row.studentId;
    }
    if (index.column() == 1) {
      return row.studentName;
    }
    const auto choiceIndex = index.column() - 2;
    if (choiceIndex >= 0 && choiceIndex < row.choices.size()) {
      return row.choices[choiceIndex];
    }
    return {};
  }

  if (role == Qt::BackgroundRole) {
    rebuildValidationCache();
    if (index.column() == 0) {
      if (row.studentId.trimmed().isEmpty()) {
        return QColor(255, 245, 204);
      }
      if (index.row() < static_cast<int>(m_duplicateIdCounts.size()) &&
          m_duplicateIdCounts[static_cast<std::size_t>(index.row())] > 1) {
        return QColor(255, 210, 210);
      }
    } else if (index.column() >= 2) {
      const auto choiceIndex = index.column() - 2;
      QString choice;
      if (choiceIndex >= 0 && choiceIndex < row.choices.size()) {
        choice = row.choices[choiceIndex];
      }
      if (choice.trimmed().isEmpty()) {
        return QColor(255, 250, 205);
      }
    }
  }

  return {};
}

QVariant PreferenceModel::headerData(int section, Qt::Orientation orientation,
                                     int role) const {
  if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
    if (section >= 0 && section < m_headers.size()) {
      return m_headers[section];
    }
  }
  return QAbstractTableModel::headerData(section, orientation, role);
}

Qt::ItemFlags PreferenceModel::flags(const QModelIndex &index) const {
  if (!index.isValid()) {
    return Qt::NoItemFlags;
  }
  return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
}

bool PreferenceModel::setData(const QModelIndex &index, const QVariant &value,
                              int role) {
  if (!index.isValid() || role != Qt::EditRole ||
      index.row() >= static_cast<int>(m_rows.size())) {
    return false;
  }

  auto &row = m_rows[static_cast<std::size_t>(index.row())];
  const auto stringValue = value.toString();

  if (index.column() == 0) {
    row.studentId = stringValue;
  } else if (index.column() == 1) {
    row.studentName = stringValue;
  } else {
    const auto choiceIndex = index.column() - 2;
    if (choiceIndex >= 0) {
      if (choiceIndex >= row.choices.size()) {
        row.choices.reserve(choiceIndex + 1);
        while (row.choices.size() <= choiceIndex) {
          row.choices.append(QString());
        }
      }
      row.choices[choiceIndex] = stringValue;
    }
  }

  m_dirty = true;
  invalidateValidationCache();
  emit dataChanged(index, index, {role});
  return true;
}

void PreferenceModel::setHeaders(QStringList headers) {
  beginResetModel();
  m_headers = std::move(headers);
  invalidateValidationCache();
  endResetModel();
}

void PreferenceModel::setRows(std::vector<StudentPreferenceRow> rows) {
  beginResetModel();
  m_rows = std::move(rows);
  m_dirty = false;
  invalidateValidationCache();
  endResetModel();
}

const std::vector<StudentPreferenceRow> &
PreferenceModel::rows() const noexcept {
  return m_rows;
}

const QStringList &PreferenceModel::headers() const noexcept {
  return m_headers;
}

PreferenceSummary PreferenceModel::summarize() const {
  PreferenceSummary summary;
  summary.studentCount = static_cast<int>(m_rows.size());

  for (const auto &row : m_rows) {
    if (row.studentId.trimmed().isEmpty()) {
      summary.missingStudentIds++;
    }
    summary.maxChoices =
        std::max(summary.maxChoices, static_cast<int>(row.choices.size()));
    for (const auto &choice : row.choices) {
      if (choice.trimmed().isEmpty()) {
        summary.missingChoices++;
      }
    }
  }

  return summary;
}

bool PreferenceModel::isDirty() const noexcept { return m_dirty; }

void PreferenceModel::setDirty(bool dirty) noexcept { m_dirty = dirty; }

void PreferenceModel::invalidateValidationCache() const {
  m_validationDirty = true;
}

void PreferenceModel::rebuildValidationCache() const {
  if (!m_validationDirty) {
    return;
  }

  m_duplicateIdCounts.assign(m_rows.size(), 0);
  QHash<QString, int> occurrence;
  for (const auto &row : m_rows) {
    const auto id = row.studentId.trimmed();
    if (id.isEmpty()) {
      continue;
    }
    occurrence[id] += 1;
  }

  for (int i = 0; i < static_cast<int>(m_rows.size()); ++i) {
    const auto id = m_rows[static_cast<std::size_t>(i)].studentId.trimmed();
    if (id.isEmpty()) {
      continue;
    }
    m_duplicateIdCounts[static_cast<std::size_t>(i)] = occurrence.value(id);
  }

  m_validationDirty = false;
}
