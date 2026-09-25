# WireRunner U — one honest workspace

This revision treats the live PipeWire graph and WirePlumber policy as the product model rather than translating them into a smaller routing workflow.

The interaction model is media-generic. Audio is the initial presentation, while video sources, consumers, virtual cameras, and processing chains belong on the same canvas with media-typed ports and links.

- Every node appears once, on the canvas.
- Wires are the only representation and editing mechanism for live links.
- Repeating the same port-to-port gesture creates fan-out without replacing existing links.
- Virtual devices and processing are ordinary graph nodes.
- The inspector edits the selected node or link; it does not offer a second routing control.
- Defaults and remembered behavior are policy attached to graph objects, visibly distinct from live links.
- Ports expand on their owning node rather than opening a separate Ports page.

Open `index.html` directly, or serve the directory with any static web server.
