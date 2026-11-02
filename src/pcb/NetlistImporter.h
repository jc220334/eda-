#pragma once

#include "core/Project.h"

#include <string>
#include <vector>

namespace pcb {

struct ImportDiagnostic {
    std::string code;
    std::string message;
    bool blocking = true;
};

struct NetlistImportResult {
    bool ok = false;
    std::size_t componentCount = 0;
    std::size_t netCount = 0;
    std::size_t matchedPinCount = 0;
    std::vector<ImportDiagnostic> diagnostics;

    [[nodiscard]] std::string summary() const;
};

struct NetlistImportOptions {
    double boardWidth = 60.0;
    double boardHeight = 40.0;
    double firstX = 12.0;
    double firstY = 10.0;
    double columnSpacing = 12.0;
    double rowSpacing = 10.0;
    int columns = 3;
};

class NetlistImporter {
public:
    static NetlistImportResult importFiles(
        const std::string& netlistPath,
        const std::string& libraryPath,
        PcbProject& output,
        const NetlistImportOptions& options = {});

    static NetlistImportResult importStrings(
        const std::string& netlistText,
        const std::string& libraryText,
        PcbProject& output,
        const NetlistImportOptions& options = {});
};

} // namespace pcb
