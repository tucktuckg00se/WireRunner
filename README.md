# WireRunner

WireRunner is a native PipeWire and WirePlumber control surface for Linux. It
presents the real multimedia graph as one understandable workspace for audio,
video, and MIDI.

The project is in its first implementation milestone: a read-only live graph
viewer built with C++23 and Qt Quick 6.

## Documentation

- [Product requirements](docs/PRD.md)
- [Functional inventory](docs/PRODUCT_BRIEF.md)
- [UX reference prototype](docs/prototype/index.html)

## Building

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

## License

WireRunner is licensed under GPL-3.0-or-later.
