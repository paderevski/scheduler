#include "utils/SpreadsheetBridge.hpp"

#include <QFile>
#include <QStringList>
#include <QTextStream>

namespace {

QStringList parseCsvRow(const QString &line) {
  QStringList columns;
  QString current;
  bool inQuotes = false;
  for (int i = 0; i < line.size(); ++i) {
    const QChar ch = line.at(i);
    if (ch == '\"') {
      if (inQuotes && i + 1 < line.size() && line.at(i + 1) == '\"') {
        current.append('\"');
        ++i;
      } else {
        inQuotes = !inQuotes;
      }
    } else if (ch == ',' && !inQuotes) {
      columns.append(current.trimmed());
      current.clear();
    } else {
      current.append(ch);
    }
  }
  columns.append(current.trimmed());
  return columns;
}

std::vector<std::string> toStdVector(const QStringList &list) {
  std::vector<std::string> result;
  result.reserve(list.size());
  for (const auto &item : list) {
    result.push_back(item.trimmed().toStdString());
  }
  return result;
}

QStringList toQStringList(const std::vector<std::string> &values) {
  QStringList list;
  for (const auto &value : values) {
    list.append(QString::fromStdString(value));
  }
  return list;
}

} // namespace

namespace SpreadsheetBridge {

bool ReadFile(const std::string &path, SpreadsheetTable &out,
              std::string *errorMessage) {
  out.headers.clear();
  out.rows.clear();
  const QString qPath = QString::fromStdString(path);
  if (qPath.endsWith(QStringLiteral(".csv"), Qt::CaseInsensitive)) {
    QFile file(qPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
      if (errorMessage) {
        *errorMessage = file.errorString().toStdString();
      }
      return false;
    }
    QTextStream stream(&file);
    bool headerParsed = false;
    while (!stream.atEnd()) {
      const QString line = stream.readLine();
      if (line.trimmed().isEmpty()) {
        continue;
      }
      const auto columns = parseCsvRow(line);
      if (!headerParsed) {
        out.headers = toStdVector(columns);
        headerParsed = true;
      } else {
        out.rows.push_back(toStdVector(columns));
      }
    }
    return headerParsed;
  }

  if (errorMessage) {
    *errorMessage = "Unsupported file format";
  }
  return false;
}

bool WriteCsv(const std::string &path, const SpreadsheetTable &table,
              std::string *errorMessage) {
  QFile file(QString::fromStdString(path));
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text |
                 QIODevice::Truncate)) {
    if (errorMessage) {
      *errorMessage = file.errorString().toStdString();
    }
    return false;
  }
  QTextStream stream(&file);
  auto writeRow = [&stream](const std::vector<std::string> &row) {
    QStringList columns;
    columns.reserve(static_cast<int>(row.size()));
    for (const auto &cell : row) {
      QString value = QString::fromStdString(cell);
      if (value.contains('"') || value.contains(',') || value.contains('\n')) {
        value.replace('"', QStringLiteral(""
                                          ""));
        value = QStringLiteral("\"") + value + QStringLiteral("\"");
      }
      columns.append(value);
    }
    stream << columns.join(',') << '\n';
  };
  writeRow(table.headers);
  for (const auto &row : table.rows) {
    writeRow(row);
  }
  stream.flush();
  return true;
}

} // namespace SpreadsheetBridge
