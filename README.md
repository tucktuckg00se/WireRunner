# WireRunner

WireRunner is a native PipeWire and WirePlumber control surface for Linux. It
presents the real multimedia graph as one understandable workspace for audio,
video, and MIDI.

The current milestone is a read-only but fully navigable graph foundation built
with C++23 and Qt Quick 6.

WireRunner reads clients, devices, nodes, ports, and links directly from
libwireplumber on a dedicated GLib thread. It composes related objects into one
card, expands real ports in place, supports pan, zoom, search, path focus, and
card arrangement, and remembers stable layout without confusing it with media
state. The UI receives immutable snapshots, leaving the backend ready for the
validated command layer planned for later milestones.

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
