#include "core/Project.h"
#include "io/ProjectIO.h"
#include "pcb/Geometry.h"
#include "pcb/LayoutService.h"
#include "pcb/NetlistImporter.h"
#include "pcb/RoutingService.h"

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

bool saveProject(const PcbProject& project, const fs::path& path)
{
    std::string error;
    if (!ProjectIO::saveToFile(project, path.string(), error)) {
        std::cerr << "Save failed: " << path.u8string() << ": " << error << '\n';
        return false;
    }
    std::cout << "Saved: " << path.u8string() << '\n';
    return true;
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
    const fs::path samples = root / "samples";

    PcbProject layoutProject;
    const NetlistImportResult imported =
        NetlistImporter::importFiles(netlistPath.string(), libraryPath.string(), layoutProject);
    if (!imported.ok) {
        std::cerr << imported.summary() << '\n';
        return 1;
    }
    const LayoutResult layout = LayoutService::autoPlace(layoutProject.board, 3.0, 2.0, 0.5);
    if (!layout.ok) {
        std::cerr << "Layout failed: " << layout.message << '\n';
        return 1;
    }
    if (!saveProject(layoutProject, samples / "01-layout.pcbproj")) {
        return 1;
    }

    PcbProject manualProject = layoutProject;
    if (const Net* signalInput = manualProject.board.findNetByName("SIG_IN")) {
        const auto pads = collectPads(manualProject.board, signalInput->id);
        if (pads.size() >= 2) {
            const ManualRouteResult manual = RoutingService::addManualTrace(
                manualProject.board, signalInput->id, "F.Cu", 0.30,
                {pads[0].position, Point{25.0, 14.0}, pads[1].position}, 0.25);
            std::cout << "Manual route: " << manual.message << '\n';
        }
    }
    if (const Net* vcc = manualProject.board.findNetByName("VCC")) {
        std::string error;
        if (!RoutingService::placeVia(
                manualProject.board, vcc->id, Point{5.0, 35.0},
                "F.Cu", "B.Cu", 0.6, 0.3, error)) {
            std::cerr << "Manual via failed: " << error << '\n';
        }
    }
    if (!saveProject(manualProject, samples / "02-manual-routing.pcbproj")) {
        return 1;
    }

    PcbProject routedProject = layoutProject;
    const RouteResult routing = RoutingService::autoRoute(routedProject.board, 0.25);
    std::cout << "Automatic routing: " << routing.message
              << " (" << routing.traceCount << " traces, "
              << routing.viaCount << " vias)\n";
    if (!routing.ok) {
        return 1;
    }
    if (!saveProject(routedProject, samples / "03-auto-routing.pcbproj")) {
        return 1;
    }

    PcbProject completeProject = routedProject;
    const Net* ground = completeProject.board.findNetByName("GND");
    if (!ground) {
        std::cerr << "GND net is missing.\n";
        return 1;
    }
    std::string pourError;
    if (!RoutingService::addGroundPour(
            completeProject.board, ground->id, "B.Cu", 1.0, 0.30, pourError)) {
        std::cerr << "Copper pour failed: " << pourError << '\n';
        return 1;
    }
    const ValidationResult validation =
        RoutingService::validate(completeProject.board, 0.25);
    std::cout << "Validation: " << validation.summary() << '\n';
    if (!validation.ok) {
        return 1;
    }
    if (!saveProject(completeProject, samples / "04-complete.pcbproj") ||
        !saveProject(completeProject, samples / "c-member-demo.pcbproj")) {
        return 1;
    }

    PcbProject reloaded;
    const LoadResult loaded =
        ProjectIO::loadFromFile((samples / "04-complete.pcbproj").string(), reloaded);
    if (!loaded.ok || reloaded != completeProject) {
        std::cerr << "Round-trip verification failed.\n";
        return 1;
    }
    std::cout << "Round-trip verification: OK\n";
    std::cout << "Sample generation: PASS\n";
    return 0;
}
