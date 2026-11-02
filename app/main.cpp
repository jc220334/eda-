#include "core/Project.h"
#include "io/ProjectIO.h"
#include "pcb/PcbWorkflow.h"

#include <filesystem>
#include <iostream>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

namespace fs = std::filesystem;
using namespace pcb;

fs::path findRoot(const fs::path& start)
{
    fs::path current = start;
    if (!current.is_absolute()) {
        current = fs::absolute(current);
    }
    for (int depth = 0; depth < 32; ++depth) {
        if (fs::exists(current / "examples" / "netlist-basic" / "sample.net.json") &&
            fs::exists(current / "CMakeLists.txt")) {
            return current;
        }
        if (!current.has_parent_path() || current.parent_path() == current) {
            break;
        }
        current = current.parent_path();
    }
    return {};
}

} // namespace

int main(int argc, char* argv[])
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    const fs::path root = argc > 1 ? findRoot(argv[1]) : findRoot(fs::current_path());
    if (root.empty()) {
        std::cerr << "Cannot locate repository root.\n";
        return 1;
    }

    const fs::path netlistPath =
        root / "examples" / "netlist-basic" / "sample.net.json";
    const fs::path libraryPath =
        root / "examples" / "netlist-basic" / "sample.lib.json";
    const fs::path outputPath = root / "samples" / "c-member-demo.pcbproj";

    PcbProject project;
    const WorkflowResult workflow = PcbWorkflow::buildSample(
        netlistPath.string(), libraryPath.string(), project);

    std::cout << "===== PCB member C complete workflow =====\n";
    for (const std::string& line : workflow.log) {
        std::cout << line << '\n';
    }

    if (!workflow.import.ok) {
        std::cerr << "Import failed.\n";
        return 1;
    }

    std::cout << "\nBoard summary\n"
              << "  layers       : " << project.board.layers.layers().size() << '\n'
              << "  nets         : " << project.board.nets.nets().size() << '\n'
              << "  components   : " << project.board.components.size() << '\n'
              << "  traces       : " << project.board.traces.size() << '\n'
              << "  vias         : " << project.board.vias.size() << '\n'
              << "  copper pours : " << project.board.copperPours.size() << '\n';

    std::string error;
    if (!ProjectIO::saveToFile(project, outputPath.string(), error)) {
        std::cerr << "Save failed: " << error << '\n';
        return 1;
    }
    std::cout << "\nSaved project: " << outputPath.u8string() << '\n';

    PcbProject reloaded;
    const LoadResult loaded = ProjectIO::loadFromFile(outputPath.string(), reloaded);
    if (!loaded.ok || reloaded != project) {
        std::cerr << "Saved project verification failed";
        if (!loaded.ok) {
            std::cerr << ": " << loaded.error;
        }
        std::cerr << '\n';
        return 1;
    }

    std::cout << "Round-trip verification: OK\n";
    std::cout << "Workflow result: "
              << (workflow.ok ? "PASS" : "CHECK VALIDATION ISSUES") << '\n';
    return workflow.ok ? 0 : 2;
}
