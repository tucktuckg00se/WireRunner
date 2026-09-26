# WireRunner

WireRunner is a native PipeWire and WirePlumber control surface for Linux. It
presents the real multimedia graph as one understandable workspace for audio,
video, and MIDI.

The current milestone is a live routing and audio-control workspace built with
C++23 and Qt Quick 6.

WireRunner reads clients, devices, nodes, ports, and links directly from
libwireplumber on a dedicated GLib thread. It composes related objects into one
card, expands real ports in place, supports pan, zoom, search, path focus, and
card arrangement, and remembers stable layout without confusing it with media
state. Drag an exact output port to an input port to add a route without
replacing existing links. Selected wires can be disconnected individually, and
successful routing changes support session-local undo and redo. Probable cycles
require explicit confirmation and are created as PipeWire feedback links.
Device cards expose a compact first-pair volume and mute control. The inspector
shows every channel exposed by the active WirePlumber device Route, including
separate playback and capture routes, while application cards retain their
per-node controls. Device changes are written back as remembered Route state;
node changes are reconciled with the observed PipeWire state. The Fit action now
lives with the zoom controls, Ctrl+mouse wheel zooms around the pointer, and
wires follow cards continuously while they move.

Volume fields accept exact percentages or explicit dB values, and a double
click on a slider handle returns it to 0 dB. The wheel pans the graph, while
Ctrl+wheel and trackpad pinch zoom around the pointer. Device inspectors expose
remembered WirePlumber profiles and ports, and eligible audio and video nodes
can be selected as defaults without rewriting their visible links.

The demonstration graph remains read-only. Live mutations use libwireplumber's
link factory on the dedicated GLib backend thread and are considered complete
only after the resulting graph snapshot confirms them.

## Documentation

- [Product requirements](docs/PRD.md)
- [Functional inventory](docs/PRODUCT_BRIEF.md)
- [UX reference prototype](docs/prototype/index.html)

## Building

Install a C++23 compiler, CMake 3.28 or newer, Ninja, Qt 6.8 or newer with Qt
Quick Controls, pkg-config, and the WirePlumber 0.5 development files. Then run:

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

Run against the current desktop PipeWire session:

```sh
./build/dev/src/wirerunner
```

Run with deterministic demonstration data:

```sh
./build/dev/src/wirerunner --demo
```

The demonstration graph includes audio fan-out, an intermediate virtual device,
a video route, and a MIDI route. It is also used by the QML smoke test.

## License

WireRunner is licensed under GPL-3.0-or-later.
