#pragma once

#include <string>
#include <vector>

struct SpreadsheetTable {
  std::vector<std::string> headers;
  std::vector<std::vector<std::string>> rows;
};

namespace SpreadsheetBridge {

bool ReadFile(const std::string &path, SpreadsheetTable &out,
              std::string *errorMessage = nullptr);

bool WriteCsv(const std::string &path, const SpreadsheetTable &table,
              std::string *errorMessage = nullptr);

} // namespace SpreadsheetBridge
