#include "utils/SpreadsheetBridge.hpp"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QStringList>
#include <QTextStream>

namespace {

const char *kXlsxImportScript = R"PY(
import json
import sys
import xml.etree.ElementTree as ET
import zipfile

NS = "{http://schemas.openxmlformats.org/spreadsheetml/2006/main}"

def col_to_index(ref):
    col = 0
    for char in ref:
        if char.isalpha():
            col = col * 26 + (ord(char.upper()) - 64)
        else:
            break
    return col - 1

def read_shared_strings(z):
    if "xl/sharedStrings.xml" not in z.namelist():
        return []
    root = ET.fromstring(z.read("xl/sharedStrings.xml"))
    strings = []
    for si in root.findall(f"{NS}si"):
        text = ""
        t = si.find(f"{NS}t")
        if t is not None and t.text is not None:
            text = t.text
        else:
            parts = []
            for run in si.findall(f"{NS}r"):
                part = run.find(f"{NS}t")
                if part is not None and part.text:
                    parts.append(part.text)
            text = "".join(parts)
        strings.append(text)
    return strings

def parse_sheet(z, shared):
    if "xl/worksheets/sheet1.xml" not in z.namelist():
        raise RuntimeError("Missing sheet1.xml in workbook")
    sheet = ET.fromstring(z.read("xl/worksheets/sheet1.xml"))
    data = []
    max_cols = 0
    for row in sheet.findall(f"{NS}sheetData/{NS}row"):
        cells = []
        cursor = 0
        for cell in row.findall(f"{NS}c"):
            ref = cell.attrib.get("r", "")
            idx = cursor
            if ref:
                idx = col_to_index(ref)
            while len(cells) < idx:
                cells.append("")
            cell_type = cell.attrib.get("t")
            value = ""
            if cell_type == "s":
                v = cell.find(f"{NS}v")
                if v is not None and v.text is not None:
                    shared_index = int(v.text)
                    if 0 <= shared_index < len(shared):
                        value = shared[shared_index]
            elif cell_type == "inlineStr":
                t = cell.find(f"{NS}is/{NS}t")
                value = t.text if t is not None and t.text else ""
            else:
                v = cell.find(f"{NS}v")
                value = v.text if v is not None and v.text else ""
            cells.append(value)
            cursor = idx + 1
        max_cols = max(max_cols, len(cells))
        data.append(cells)
    for row in data:
        if len(row) < max_cols:
            row.extend([""] * (max_cols - len(row)))
    return data

def main():
    workbook = sys.argv[1]
    with zipfile.ZipFile(workbook, "r") as z:
        shared = read_shared_strings(z)
        rows = parse_sheet(z, shared)
    print(json.dumps({"rows": rows}, ensure_ascii=False))

if __name__ == "__main__":
    main()
)PY";

const char *kXlsxExportScript = R"PY(
import json
import sys
import zipfile

NS = "http://schemas.openxmlformats.org/spreadsheetml/2006/main"

letters = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"

def index_to_col(idx):
    result = ""
    idx += 1
    while idx:
        idx, rem = divmod(idx - 1, 26)
        result = letters[rem] + result
    return result

def sheet_xml(rows, shared_map):
    xml = [f'<worksheet xmlns="{NS}"><sheetData>']
    for r_idx, row in enumerate(rows, start=1):
        cells = []
        for c_idx, value in enumerate(row):
            if not value:
                continue
            key = value
            if key not in shared_map:
                shared_map[key] = len(shared_map)
            ref = f"{index_to_col(c_idx)}{r_idx}"
            cells.append(f'<c r="{ref}" t="s"><v>{shared_map[key]}</v></c>')
        cell_blob = "".join(cells)
        xml.append(f"<row r=\"{r_idx}\">{cell_blob}</row>")
    xml.append("</sheetData></worksheet>")
    return "".join(xml)

def shared_strings_xml(shared_map):
    items = sorted(shared_map.items(), key=lambda kv: kv[1])
    body = "".join([
        f'<si><t>{value}</t></si>' for value, _ in items
    ])
    return f'<sst xmlns="{NS}" count="{len(items)}" uniqueCount="{len(items)}">{body}</sst>'

def workbook_xml():
    return ('<workbook xmlns="{0}"><sheets>'
            '<sheet name="Sheet1" sheetId="1" r:id="rId1" '
            'xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships" />'
            '</sheets></workbook>').format(NS)

def root_rels():
  return ('<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
      '<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="xl/workbook.xml"/>'
      '</Relationships>')

def workbook_rels():
  return ('<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
      '<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet1.xml"/>'
      '<Relationship Id="rId2" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/sharedStrings" Target="sharedStrings.xml"/>'
      '</Relationships>')

def content_types():
    return ('<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">'
            '<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>'
            '<Default Extension="xml" ContentType="application/xml"/>'
            '<Override PartName="/xl/workbook.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml"/>'
            '<Override PartName="/xl/worksheets/sheet1.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"/>'
            '<Override PartName="/xl/sharedStrings.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sharedStrings+xml"/>'
            '</Types>')

def main():
    output_path = sys.argv[1]
    table = json.load(sys.stdin)
    rows = table.get("rows", [])
    shared = {}
    sheet = sheet_xml(rows, shared)
    shared_xml = shared_strings_xml(shared)
    with zipfile.ZipFile(output_path, "w", compression=zipfile.ZIP_DEFLATED) as z:
  z.writestr("[Content_Types].xml", content_types())
  z.writestr("_rels/.rels", root_rels())
        z.writestr("xl/workbook.xml", workbook_xml())
        z.writestr("xl/_rels/workbook.xml.rels", workbook_rels())
        z.writestr("xl/sharedStrings.xml", shared_xml)
        z.writestr("xl/worksheets/sheet1.xml", sheet)

if __name__ == "__main__":
    main()
)PY";

QString pythonExecutable() {
#ifdef Q_OS_WIN
  return QStringLiteral("python");
#else
  return QStringLiteral("python3");
#endif
}

bool runPython(const QString &script, const QStringList &args,
               const QByteArray &input, QByteArray &stdoutData,
               std::string *errorMessage) {
  QProcess process;
  QStringList fullArgs;
  fullArgs << QStringLiteral("-c") << script;
  fullArgs << args;
  process.start(pythonExecutable(), fullArgs);
  if (!process.waitForStarted()) {
    if (errorMessage) {
      *errorMessage = process.errorString().toStdString();
    }
    return false;
  }
  if (!input.isEmpty()) {
    process.write(input);
  }
  process.closeWriteChannel();
  if (!process.waitForFinished()) {
    if (errorMessage) {
      *errorMessage = process.errorString().toStdString();
    }
    return false;
  }
  if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
    if (errorMessage) {
      const auto combined = process.readAllStandardError();
      *errorMessage = combined.toStdString();
    }
    return false;
  }
  stdoutData = process.readAllStandardOutput();
  return true;
}

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

  if (qPath.endsWith(QStringLiteral(".xlsx"), Qt::CaseInsensitive)) {
    QByteArray stdoutData;
    if (!runPython(QString::fromUtf8(kXlsxImportScript), {qPath}, {},
                   stdoutData, errorMessage)) {
      return false;
    }
    const auto doc = QJsonDocument::fromJson(stdoutData);
    if (!doc.isObject()) {
      if (errorMessage) {
        *errorMessage = "Invalid XLSX JSON payload";
      }
      return false;
    }
    const auto rowsValue = doc.object().value(QStringLiteral("rows"));
    if (!rowsValue.isArray()) {
      if (errorMessage) {
        *errorMessage = "XLSX payload missing rows";
      }
      return false;
    }
    const auto rowsArray = rowsValue.toArray();
    bool headerParsed = false;
    for (const auto &rowValue : rowsArray) {
      const auto rowArray = rowValue.toArray();
      QStringList columns;
      columns.reserve(rowArray.size());
      for (const auto &cell : rowArray) {
        columns.append(cell.toString());
      }
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

bool WriteXlsx(const std::string &path, const SpreadsheetTable &table,
               std::string *errorMessage) {
  QJsonObject root;
  QJsonArray rows;
  auto appendRow = [&rows](const std::vector<std::string> &row) {
    QJsonArray jsonRow;
    for (const auto &cell : row) {
      jsonRow.append(QString::fromStdString(cell));
    }
    rows.append(jsonRow);
  };
  appendRow(table.headers);
  for (const auto &row : table.rows) {
    appendRow(row);
  }
  root.insert(QStringLiteral("rows"), rows);
  const auto payload = QJsonDocument(root).toJson(QJsonDocument::Compact);
  QByteArray stdoutData;
  if (!runPython(QString::fromUtf8(kXlsxExportScript),
                 {QString::fromStdString(path)}, payload, stdoutData,
                 errorMessage)) {
    return false;
  }
  Q_UNUSED(stdoutData);
  return true;
}

} // namespace SpreadsheetBridge
