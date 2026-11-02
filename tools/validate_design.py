#!/usr/bin/env python3
"""Validate the v1 netlist design samples without external dependencies."""

from __future__ import annotations

import json
from collections import Counter
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
NETLIST_PATH = ROOT / "examples" / "netlist-basic" / "sample.net.json"
LIBRARY_PATH = ROOT / "examples" / "netlist-basic" / "sample.lib.json"
BOARD_PATH = ROOT / "examples" / "netlist-basic" / "sample.board.json"


class ValidationError(Exception):
    pass


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValidationError(message)


def load_object(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise ValidationError(f"{path.relative_to(ROOT)}: {exc}") from exc
    require(isinstance(value, dict), f"{path.relative_to(ROOT)}: root must be an object")
    return value


def require_unique(values: list[str], label: str) -> None:
    duplicates = sorted(value for value, count in Counter(values).items() if count > 1)
    require(not duplicates, f"{label} contains duplicates: {duplicates}")


def normalized(value: Any, label: str) -> str:
    require(isinstance(value, str) and value.strip(), f"{label} must be a non-empty string")
    return value.strip().upper()


def points(value: Any, label: str, minimum: int) -> list[list[int]]:
    require(isinstance(value, list) and len(value) >= minimum, f"{label} needs at least {minimum} points")
    for index, point in enumerate(value):
        require(
            isinstance(point, list)
            and len(point) == 2
            and all(isinstance(coordinate, int) and not isinstance(coordinate, bool) for coordinate in point),
            f"{label}[{index}] must be [int, int]",
        )
    return value


def point(value: Any, label: str) -> list[int]:
    require(
        isinstance(value, list)
        and len(value) == 2
        and all(isinstance(coordinate, int) and not isinstance(coordinate, bool) for coordinate in value),
        f"{label} must be [int, int]",
    )
    return value


def validate_library(library: dict[str, Any]) -> dict[str, set[str]]:
    require(library.get("format") == "eda-course-footprint-library", "library format mismatch")
    require(library.get("version") == 1, "library version mismatch")
    require(library.get("units") == "um", "library units must be um")

    footprints = library.get("footprints")
    require(isinstance(footprints, list) and footprints, "library footprints must be non-empty")
    footprint_ids = [footprint.get("id") for footprint in footprints]
    require(all(isinstance(value, str) and value.strip() for value in footprint_ids), "invalid footprint ID")
    require_unique(footprint_ids, "footprint IDs")

    pins_by_footprint: dict[str, set[str]] = {}
    for footprint in footprints:
        footprint_id = footprint["id"]
        point(footprint.get("origin"), f"{footprint_id}.origin")
        pads = footprint.get("pads")
        graphics = footprint.get("graphics")
        require(isinstance(pads, list), f"{footprint_id}.pads must be an array")
        require(isinstance(graphics, list), f"{footprint_id}.graphics must be an array")

        pad_ids: list[str] = []
        pins: set[str] = set()
        for index, pad in enumerate(pads):
            require(isinstance(pad, dict), f"{footprint_id}.pads[{index}] must be an object")
            pad_id = pad.get("id")
            require(isinstance(pad_id, str) and pad_id.strip(), f"{footprint_id}.pads[{index}].id")
            pad_ids.append(pad_id)
            point(pad.get("center"), f"{footprint_id}.{pad_id}.center")
            point(pad.get("size"), f"{footprint_id}.{pad_id}.size")
            layers = pad.get("layer_set")
            require(isinstance(layers, list) and layers, f"{footprint_id}.{pad_id}.layer_set")

            pin = pad.get("pin")
            if pin is not None:
                pins.add(normalized(pin, f"{footprint_id}.{pad_id}.pin"))

            mount = pad.get("mount")
            drill = pad.get("drill_um")
            require(mount in {"smd", "plated_through", "non_plated"}, f"{footprint_id}.{pad_id}.mount")
            require(isinstance(drill, int) and drill >= 0, f"{footprint_id}.{pad_id}.drill_um")
            if mount == "smd":
                require(drill == 0, f"{footprint_id}.{pad_id}: SMD drill must be zero")
            else:
                require(drill > 0, f"{footprint_id}.{pad_id}: through-hole drill must be positive")

        require_unique(pad_ids, f"pad IDs in {footprint_id}")
        pins_by_footprint[footprint_id] = pins

    return pins_by_footprint


def validate_netlist(
    netlist: dict[str, Any], pins_by_footprint: dict[str, set[str]]
) -> set[tuple[str, str, str]]:
    require(netlist.get("format") == "eda-course-netlist", "netlist format mismatch")
    require(netlist.get("version") == 1, "netlist version mismatch")

    components = netlist.get("components")
    nets = netlist.get("nets")
    require(isinstance(components, list) and components, "netlist components must be non-empty")
    require(isinstance(nets, list) and nets, "netlist nets must be non-empty")

    refs = [normalized(component.get("ref"), f"components[{index}].ref") for index, component in enumerate(components)]
    require_unique(refs, "component references")

    footprint_by_ref: dict[str, str] = {}
    for index, component in enumerate(components):
        footprint = component.get("footprint")
        require(isinstance(footprint, str) and footprint.strip(), f"components[{index}].footprint")
        require(footprint in pins_by_footprint, f"{refs[index]} references unknown footprint {footprint}")
        footprint_by_ref[refs[index]] = footprint

    net_names = [value.get("name") for value in nets]
    require(all(isinstance(name, str) and name.strip() for name in net_names), "invalid net name")
    require_unique(net_names, "net names")

    expected_connections: set[tuple[str, str, str]] = set()
    pin_owner: dict[tuple[str, str], str] = {}
    for net_index, net in enumerate(nets):
        connections = net.get("connections")
        require(
            isinstance(connections, list) and len(connections) >= 2,
            f"nets[{net_index}] needs at least two connections",
        )
        for connection_index, connection in enumerate(connections):
            ref = normalized(connection.get("ref"), f"nets[{net_index}].connections[{connection_index}].ref")
            pin = normalized(connection.get("pin"), f"nets[{net_index}].connections[{connection_index}].pin")
            require(ref in footprint_by_ref, f"unknown component {ref} in net {net['name']}")
            require(
                pin in pins_by_footprint[footprint_by_ref[ref]],
                f"component {ref} has no pin {pin}",
            )
            key = (ref, pin)
            previous_net = pin_owner.get(key)
            require(previous_net is None, f"pin {ref}.{pin} appears in {previous_net} and {net['name']}")
            pin_owner[key] = net["name"]
            expected_connections.add((ref, pin, net["name"]))

    require(
        len(expected_connections) == sum(len(net["connections"]) for net in nets),
        "duplicate connection inside a net",
    )
    return expected_connections


def validate_board(
    board: dict[str, Any],
    pins_by_footprint: dict[str, set[str]],
    expected_connections: set[tuple[str, str, str]],
) -> None:
    require(board.get("format") == "eda-course-board", "board format mismatch")
    require(board.get("version") == 1, "board version mismatch")
    require(board.get("units") == "um", "board units must be um")

    outline = points(board.get("outline"), "board.outline", 4)
    require(outline[0] == outline[-1], "board outline must repeat its first point at the end")

    layers = board.get("layers")
    require(isinstance(layers, list) and layers, "board layers must be non-empty")
    layer_ids = [layer.get("id") for layer in layers]
    require(all(isinstance(value, str) and value for value in layer_ids), "invalid layer ID")
    require_unique(layer_ids, "layer IDs")

    components = board.get("components")
    nets = board.get("nets")
    connections = board.get("connections")
    require(isinstance(components, list), "board components must be an array")
    require(isinstance(nets, list), "board nets must be an array")
    require(isinstance(connections, list), "board connections must be an array")

    component_ids = [component.get("id") for component in components]
    refs = [normalized(component.get("ref"), "board component ref") for component in components]
    require(all(isinstance(value, str) and value for value in component_ids), "invalid component ID")
    require_unique(component_ids, "component instance IDs")
    require_unique(refs, "component references")

    component_by_id = {component["id"]: component for component in components}
    ref_by_component_id = {
        component["id"]: normalized(component["ref"], "board component ref")
        for component in components
    }
    for component in components:
        require(
            component.get("footprint") in pins_by_footprint,
            f"{component['id']} references unknown footprint {component.get('footprint')}",
        )
        require(
            component.get("rotation_mdeg") in {0, 90000, 180000, 270000},
            f"{component['id']} has unsupported rotation",
        )

    net_ids = [net.get("id") for net in nets]
    require(all(isinstance(value, str) and value for value in net_ids), "invalid net ID")
    require_unique(net_ids, "net IDs")
    net_name_by_id = {net["id"]: net.get("name") for net in nets}

    actual_connections: set[tuple[str, str, str]] = set()
    occupied_pins: set[tuple[str, str]] = set()
    for index, connection in enumerate(connections):
        component_id = connection.get("component")
        require(component_id in component_by_id, f"connections[{index}] has unknown component")
        pin = normalized(connection.get("pin"), f"connections[{index}].pin")
        ref = ref_by_component_id[component_id]
        require(
            pin in pins_by_footprint[component_by_id[component_id]["footprint"]],
            f"connection {ref}.{pin} references an unknown pin",
        )
        net_id = connection.get("net")
        require(net_id in net_name_by_id, f"connections[{index}] has unknown net")
        require((component_id, pin) not in occupied_pins, f"pin {ref}.{pin} is connected more than once")
        occupied_pins.add((component_id, pin))
        actual_connections.add((ref, pin, net_name_by_id[net_id]))

    require(
        actual_connections == expected_connections,
        "board connections do not match the netlist",
    )

    require(isinstance(board.get("tracks"), list), "board tracks must be an array")
    require(isinstance(board.get("vias"), list), "board vias must be an array")
    require(isinstance(board.get("copperZones"), list), "board copperZones must be an array")


def main() -> None:
    library = load_object(LIBRARY_PATH)
    netlist = load_object(NETLIST_PATH)
    board = load_object(BOARD_PATH)

    for schema_name in ("netlist", "footprint-library", "board"):
        schema = load_object(ROOT / "schemas" / f"{schema_name}.schema.json")
        require(schema.get("$schema") == "https://json-schema.org/draft/2020-12/schema", f"{schema_name} schema draft")

    pins_by_footprint = validate_library(library)
    expected_connections = validate_netlist(netlist, pins_by_footprint)
    validate_board(board, pins_by_footprint, expected_connections)

    print(f"Library footprints: {len(library['footprints'])}")
    print(f"Netlist components: {len(netlist['components'])}")
    print(f"Netlist nets: {len(netlist['nets'])}")
    print(f"Matched pin connections: {len(expected_connections)}")
    print("Design samples: OK")


if __name__ == "__main__":
    try:
        main()
    except ValidationError as exc:
        raise SystemExit(f"ERROR: {exc}") from exc
