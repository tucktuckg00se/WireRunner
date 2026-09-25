# WireRunner: PipeWire and WirePlumber control surface

**Document:** Product requirements document  
**Status:** Draft for product and engineering review  
**Last updated:** September 24, 2026  
**Working name:** WireRunner

## 1. Product summary

WireRunner is a desktop control surface for PipeWire and WirePlumber. It makes the live multimedia graph understandable and directly editable without replacing PipeWire's model with a smaller, task-specific abstraction.

The primary workspace is a composed graph of applications, devices, streams, processing nodes, virtual endpoints, ports, and links. Each object appears once. Users change routing by manipulating the links they can see. Contextual controls expose volume, mute, profiles, routes, formats, policy, persistence, and technical properties on the object to which they belong.

WireRunner should feel calm to a person who only wants to change an output or microphone, while retaining the graph fidelity, channel control, virtual devices, automation, and inspection required by audio professionals, streamers, developers, and advanced Linux users. PipeWire video sources and consumers use the same workspace and interaction model.

## 2. Problem

PipeWire and WirePlumber provide a powerful multimedia graph and policy system, but users currently encounter it through fragmented tools:

- desktop settings expose a small subset of device and volume controls;
- volume-control applications often assume one stream has one destination;
- patchbays expose live links but provide little policy, persistence, device, or configuration context;
- low-level inspection tools expose the complete object model but are designed for diagnosis rather than intentional daily use;
- advanced settings, virtual devices, rules, and persistent behavior often require editing SPA-JSON configuration fragments by hand;
- existing comprehensive tools tend to divide one system across dashboards, mixers, device pages, patchbays, policy pages, and configuration pages.

These interfaces make simple actions feel uncertain and advanced actions feel disconnected. Users cannot easily tell the difference between a live link, a default target, an automatically created policy link, remembered state, and persistent configuration. Many interfaces also focus solely on audio even though PipeWire is a multimedia graph that handles video and MIDI data as well.

## 3. Product vision

A user opens WireRunner and immediately sees where active media comes from, where it goes, and which policy caused that state. The user can make a change by acting on the visible object or connection. As their needs become more sophisticated, the same objects reveal ports, channels, formats, rules, and raw properties without requiring them to learn a second application.

WireRunner should become the standard visual control surface for PipeWire and WirePlumber: suitable for ordinary desktop use, deep enough for production work, and accurate enough to teach users how the system actually behaves.

## 4. Product principles

### 4.1 One system, one workspace

Applications, streams, hardware, virtual devices, processing nodes, ports, and links appear on one canvas. Search, filters, focus, grouping, and zoom change what is emphasized; they do not create competing copies of the graph.

### 4.2 Simplicity through composition

WireRunner simplifies labels, grouping, layout, and disclosure. It does not simplify away valid graph structures. One output can connect to several inputs, one input can receive several outputs, and any path may include intermediate nodes.

### 4.3 Wires are routing

A visible link is the canonical representation of a live connection. The canonical editing action connects an output port to a compatible input port. Repeating the action creates fan-out. Selecting a link exposes its state, ownership, format, persistence, and removal action.

There must be no destination dropdown, secondary route list, or convenience control that silently describes or edits a different routing model. A shortcut may create or focus the same visible link if its effect is immediately shown on the canvas.

### 4.4 Policy is visible but distinct

A default device, automatic linking rule, saved application preference, and live connection are different things. WireRunner presents policy as annotations on the objects and links it affects. The user can inspect why a link exists and whether changing it affects the current session, future streams, or persistent configuration.

### 4.5 Beginners and experts share the same model

Human names, sensible initial layout, grouped stereo ports, direct volume controls, and short explanations make the initial view approachable. Inline expansion reveals channels, individual ports, formats, properties, and policy. There is no global basic/advanced mode.

### 4.6 Audio first, media complete

Audio receives the most polished initial workflows, but the product model is media-generic. Cameras, screen capture, video consumers, virtual cameras, and video processing use the same graph primitives. Audio, video, MIDI, and mixed-media filters operate on the same canvas.

### 4.7 Changes are understandable and recoverable

Routine changes happen immediately and support undo. Disruptive changes show their expected effect before application. WireRunner clearly labels changes that are live, remembered by WirePlumber, written to user configuration, or dependent on an external process.

## 5. Target users

### Desktop user

Wants to change playback devices, microphones, application volume, Bluetooth behavior, or camera routing without understanding PipeWire terminology.

### Streamer and creator

Builds mixes from microphones, desktop audio, applications, cameras, virtual cameras, and processing chains. Needs reliable persistence and feedback protection.

### Musician and audio professional

Uses multichannel interfaces, DAWs, low-latency settings, per-channel patching, MIDI, clock controls, and detailed format information.

### Linux enthusiast and administrator

Manages device profiles, priorities, network endpoints, rules, virtual devices, saved setups, and service configuration across varied hardware.

### Multimedia developer

Needs complete object inspection, negotiated formats, permissions, metadata, graph events, logs, and an accurate picture of transient behavior.

## 6. Core jobs

Users must be able to answer and act on these questions:

1. What is producing or consuming media right now?
2. Where is each signal going?
3. Why does this connection exist?
4. How do I connect, disconnect, adjust, or process it?
5. Will this change affect the current stream, future matching streams, or every session?
6. What will happen if a device disappears or a profile changes?
7. What technical state is PipeWire actually using?

## 7. Canonical product model

WireRunner must preserve the distinction between these entities:

- **Client:** an application or process connected to PipeWire.
- **Device:** hardware or a backend-managed device that may expose profiles and routes.
- **Node:** a producer, consumer, processor, adapter, stream, or virtual endpoint.
- **Port:** a typed input or output on a node, including channel and media information.
- **Link:** a live connection from an output port to an input port.
- **Parameter:** mutable object state such as volume, format, profile, route, or latency.
- **Metadata:** shared state including defaults, targets, and runtime settings.
- **Policy:** WirePlumber logic that creates, removes, selects, restores, or constrains graph state.
- **Rule:** persistent matching criteria and actions for future objects or events.
- **Scene:** a user-selected collection of graph, device, policy, and engine state.
- **Layout:** WireRunner-only presentation data. Layout changes never imply graph changes.

PipeWire object IDs are transient. WireRunner must use stable properties and explicit matching rules whenever it promises persistence.

## 8. Information architecture

### 8.1 Live media canvas

The canvas is the home screen and primary working surface. It includes:

- human-readable nodes arranged by signal flow;
- live links with visual distinctions for automatic, user-created, remembered, fallback, and inactive state;
- grouped ports that expand in place;
- volume, mute, activity, default, recording, permission, and unavailable indicators where relevant;
- media scope filters for audio, video, MIDI, active media, and all objects;
- search that locates and focuses existing objects;
- path focus that temporarily de-emphasizes unrelated objects;
- stable user layout that survives transient node disappearance and return;
- semantic zoom that changes the amount of detail without changing the underlying representation;
- viewport culling and link batching that keep large graphs responsive;
- spatial hit testing for nodes, ports, and links at every supported zoom level;
- an accessible graph structure whose focus and selection remain synchronized with the rendered canvas.

The default canvas should show active and important objects in a useful layout. It must never open as an undifferentiated dump of every registry object.

### 8.2 Contextual inspector

The inspector shows properties and actions for the current selection:

- node controls and properties for a selected node;
- port format and compatibility for a selected port;
- ownership, state, negotiated format, persistence, and removal for a selected link;
- profile and route controls for a selected device;
- policy explanation and matching details for a selected annotation.

The inspector must not reproduce the graph as a list or provide an alternative routing mechanism.

### 8.3 Secondary utilities

Scenes, rules, engine settings, logs, event history, configuration layers, and preferences may use dedicated dialogs or workspaces because they are not alternative representations of the live graph. Their effects must preview or resolve back onto the canvas.

## 9. Functional requirements

### 9.1 Discovery and live updates

- Connect to PipeWire and WirePlumber and reconstruct the live object graph.
- Update incrementally as clients, devices, nodes, ports, links, metadata, parameters, and permissions change.
- Preserve selection, viewport, layout, and pending edits during graph churn.
- Represent transient streams as expected lifecycle events rather than failures.
- Show disconnected, restarting, partially available, and permission-restricted states.
- Expose stable identity information used for remembered behavior.

### 9.2 Graph navigation

- Pan, zoom, fit, search, and keyboard-navigate the canvas.
- Filter by media type, activity, object type, client, hardware, or selected path.
- Collapse related nodes under a device or client identity while retaining accurate node and port boundaries on expansion.
- Bundle visually parallel channel links while allowing channel-level expansion.
- Allow users to arrange, align, and group objects without turning WireRunner into a general diagram editor.
- Retain positions for temporarily absent objects and saved setups.
- Avoid a persistent minimap unless usability testing shows it materially improves large-graph navigation.

### 9.3 Routing

- Create a link from an output port to a compatible input port using pointer or keyboard.
- Add additional destinations without replacing existing links.
- Support one-to-one, one-to-many, many-to-one, loopback, monitor, and intermediate processing paths.
- Remove an individual link without affecting sibling links.
- Validate media type, direction, channel layout, format compatibility, and permissions before linking.
- Explain incompatible targets directly at the attempted connection.
- Allow channel-level routing for multichannel nodes.
- Distinguish policy-created, user-created, remembered, fallback, and externally managed links.
- Show when WirePlumber recreates or supersedes a link and explain the responsible policy when possible.
- Detect probable audio feedback loops and unsafe video cycles before creation.

### 9.4 Audio controls

- Control volume and mute for devices, nodes, applications, streams, and individual channels where supported.
- Show percent and dB with a configurable safe-volume limit.
- Support balance, fade, linked and independent channel trims, and channel maps.
- Show live meters with peak and clipping state; provide a reduced-motion or reduced-refresh option.
- Select device profiles and routes with an impact preview.
- Control Bluetooth profile and codec where available.
- Identify channels using a safely limited test tone.
- Show hardware versus software volume when known.
- Expose sample rate, format, channel count, state, latency, and resampling information.

### 9.5 Video controls

- Show cameras, screen-capture sources, application streams, virtual cameras, processors, and consumers as first-class nodes.
- Route video through the same typed port-to-port interaction used by audio.
- Support one source feeding multiple consumers.
- Show requested and negotiated resolution, frame rate, pixel format, color space, modifier, orientation, crop, and latency where available.
- Expose camera controls supported by the backend without promising unavailable controls.
- Create virtual cameras and visible video processing chains.
- Display portal-mediated screen capture and permission state.
- Group an application's audio and video nodes under one client identity without conflating their ports or links.

### 9.6 Defaults, policy, and rules

- Mark current default audio input, audio output, and relevant video source metadata on their graph objects.
- Explain that defaults affect automatic selection and may not rewrite existing links.
- Show the source and precedence of effective policy when available.
- Create application and device rules from a selected live object using inspectable match criteria.
- Support match properties for devices, nodes, clients, applications, media roles, names, and connection events.
- Support actions including rename, priority, default preference, target, profile, route, properties, link creation, and scene application.
- Provide a structured rule editor and an expert source representation.
- Validate rules before installation and show which live objects they currently match.
- Allow saved WirePlumber state and generated configuration values to be reset independently.

### 9.7 Virtual devices and processing

- Create temporary or persistent virtual sinks, sources, cameras, null endpoints, monitors, loopbacks, combined outputs, remapped endpoints, buses, and submixes.
- Insert filters and processing nodes into an existing link while keeping the resulting path visible.
- Remove, reorder, bypass, and inspect processing nodes.
- Provide optional starting templates for common structures while showing the exact graph they will create before confirmation.
- Expose channels, maps, formats, latency, clock behavior, and module-specific properties.
- Label a sink monitor distinctly from a physical microphone.
- Show which mechanism owns a persistent virtual object: WirePlumber component, PipeWire module, user service, or external application.

### 9.8 Scenes and persistence

- Save a named scene containing user-selected defaults, profiles, routes, levels, virtual objects, links, rules, engine state, and layout.
- Preview graph additions, removals, and changes before recalling a scene.
- Handle absent hardware by retaining pending intent and showing what could not be applied.
- Undo a scene recall as one operation.
- Import and export a readable scene format with optional machine-identity redaction.
- Label persistence scope consistently as live only, remembered state, generated user configuration, or externally owned.
- Never present layout persistence as media-state persistence.

### 9.9 Engine and professional controls

- Show effective graph clock, driver node, sample rate, quantum, allowed ranges, and calculated latency.
- Show requested versus effective values and the client constraining them where discoverable.
- Apply runtime metadata changes and restore prior values.
- Provide clearly explained presets for stable playback, recording, low latency, and power saving.
- Show DSP load, XRUNs, rate mismatches, and resampling.
- Support pro-audio profiles and dense multichannel nodes with usable target sizes.

### 9.10 Inspection, history, and diagnostics

- Inspect object properties, parameters, permissions, IDs, serials, ownership, and raw values.
- Show a timestamped event history for graph and policy changes.
- Record WireRunner actions with their prior values and undo availability.
- Show service health and relevant logs as secondary utilities.
- Export a privacy-reviewed support bundle containing versions, graph state, selected configuration, and logs.
- Provide copyable command or configuration equivalents when the mapping is reliable.

### 9.11 Configuration lifecycle

- Discover distribution, system, and user configuration layers and explain effective precedence.
- Generate user overrides rather than modifying distribution-owned files.
- Validate generated fragments and retain recoverable history.
- Show whether a change is immediate, applies to new streams, requires device reconnection, or requires service restart.
- Preview disruption before restarting WirePlumber or PipeWire and show reconnection progress.
- Degrade controls honestly on unsupported versions or backends.

## 10. Required interaction behavior

### Connecting a stream to another output

The user drags from the stream's output port to the device input port. The new wire appears while all existing wires remain. If the user wants an exclusive route, they remove the other wire explicitly. WireRunner never interprets “connect here” as “disconnect everywhere else.”

### Making an output the default

The user selects the output and chooses **Make default** in its inspector. The default marker moves to that output. Current links remain visible and unchanged unless WirePlumber policy changes them; any such changes animate and receive policy provenance.

### Remembering a connection

The user selects a live wire and chooses **Remember for future streams**. WireRunner previews the stable match criteria and resulting action. Saving creates a rule attached to that same wire. The wire's appearance changes to indicate remembered policy.

### Inserting a processor

The user selects a link and chooses **Insert processor**. WireRunner previews the replacement path, then replaces the original link with source-to-processor and processor-to-destination links. The processor is a normal selectable node.

### Expanding a multichannel interface

The user expands the device node in place. Grouped ports become individual named channels without switching pages or losing the surrounding path. Collapsing restores the composed view while retaining all links.

## 11. Launch scope

### Minimum lovable product

- Live audio and video graph discovery.
- Composed automatic layout with saved user positions.
- Typed nodes, grouped ports, links, search, filtering, focus, pan, and zoom.
- Audio volume, mute, meters, defaults, profiles, routes, and basic channel expansion.
- Video source and consumer inspection, format display, and direct linking.
- Multi-destination link creation and individual link removal.
- Link ownership and live-versus-remembered explanation.
- Basic WirePlumber application routing rules.
- Creation of common virtual audio endpoints and loopbacks.
- Undo for WireRunner-initiated live changes.
- Disconnected, missing-device, permission, and incompatible-link states.
- Keyboard navigation and screen-reader foundations.

### Version 1.0

- Complete audio channel patching and pro-audio profiles.
- Virtual cameras and video processing chains.
- General rule editor with source view.
- Scenes with preview, partial restore, import, and export.
- Engine timing and performance controls.
- Processing insertion and filter chains.
- Configuration-layer management and restart workflow.
- Event history, logs, diagnostics, and support export.
- Network audio endpoints and discovery.

### Later opportunities

- Collaborative or remote graph management.
- Multiple PipeWire remotes and embedded systems.
- Shareable workflow templates.
- Plugin-host integration beyond configuration-managed filter chains.
- Mobile companion or read-only monitoring surfaces.

## 12. Non-goals

- Replacing PipeWire, WirePlumber, a DAW, video editor, compositor, or full plugin host.
- Hiding the graph behind fixed workflows that cannot express the underlying topology.
- Editing distribution-owned configuration files in place.
- Guaranteeing persistence for objects controlled entirely by transient external applications.
- Providing controls that the active backend cannot report or change safely.
- Acting primarily as an automated troubleshooting wizard.

## 13. Accessibility and input

- Meet WCAG 2.2 AA contrast and focus requirements.
- Support full keyboard traversal, link creation, selection, deletion, undo, search, and inspector operation.
- Provide a structured accessible representation of the graph without creating a competing visual object browser.
- Never use color alone for media type, ownership, state, warnings, or selection.
- Support 200% text scaling, high contrast, reduced motion, and reduced meter activity.
- Keep controls usable from compact laptop windows through large studio displays.
- Announce live graph changes without overwhelming screen-reader users; batch rapid transient updates.

## 14. Safety and trust

- Warn before likely feedback paths, dangerous test-tone levels, broad scene changes, and service restarts.
- Provide a configurable volume ceiling and safe test-tone default.
- Show microphone, camera, and screen-capture use prominently.
- Explain why an application can or cannot see a protected source.
- Require confirmation only for broad, destructive, privacy-sensitive, or disruptive operations.
- Make ordinary routing and level changes immediate and undoable.
- Never claim a change is persistent until WireRunner has verified the owning mechanism accepted it.

## 15. Technical requirements

### Selected implementation stack

WireRunner will use C++23 for the application, domain model, graph state, commands, validation, undo, policy, and persistence. CMake is the primary build system, with Ninja as the preferred local and continuous-integration generator.

The desktop interface will use Qt Quick 6 and QML. C++ presentation models expose immutable values, list models, properties, signals, and typed commands to QML through Qt's native `QObject` and model/view APIs. Qt Widgets are not part of the selected architecture.

The initial platform baseline is Qt 6.8 or newer. Linux packages and Flatpak manifests must declare the required Qt Base, Qt Declarative, Qt Quick Controls, development, graphics, GLib, PipeWire, and WirePlumber dependencies explicitly.

The domain model remains independent of Qt. Graph entities, commands, validation, persistence formats, policy logic, and undo history use standard C++ types and do not inherit from `QObject`. A narrow presentation layer converts domain snapshots and changes into Qt value types and models for QML. QML never owns live graph state or makes persistence decisions.

The graph canvas will use Qt Quick scene-graph rendering with viewport culling, batched link geometry, spatial hit testing, semantic zoom, and explicit accessibility semantics. Ordinary interface controls remain QML components. The implementation must not depend on Qt Widgets.

### Backend boundary

The product must maintain a backend model separate from the presentation layer. A dedicated backend thread owns a GLib main context and connects directly to the libwireplumber 0.5 and PipeWire C APIs. It publishes immutable graph snapshots and typed incremental changes to the application model. The application forwards coalesced presentation changes to the Qt event loop using queued Qt connections and registered value types.

Libwireplumber is the primary high-level interface for graph objects and WirePlumber state. The PipeWire C API is used only for lower-level capabilities that libwireplumber does not expose conveniently, such as profiler data or specific PipeWire parameters. All graph mutations pass through one C++ command layer so validation, provenance, undo, and error reporting remain consistent regardless of the underlying API.

Shell-command parsing may be useful during prototyping but must not become the sole source of truth for graph state in the production architecture.

### Ownership and memory safety

- Use value types, `std::unique_ptr`, `std::shared_ptr` only where ownership is genuinely shared, and explicit weak references for observers.
- Do not use raw owning pointers. Raw pointers may appear only as non-owning views whose lifetime is locally evident.
- Wrap every owned GObject, WirePlumber, PipeWire, SPA, file-descriptor, and subscription handle in an RAII type with the correct release operation.
- Keep Qt parent ownership inside the presentation layer and do not mix it with domain ownership.
- Never share mutable graph objects between the GLib backend thread and Qt UI thread. Cross-thread messages contain immutable values or owned command payloads.
- Disconnect callbacks and invalidate backend handles before destroying the objects they reference.
- Run AddressSanitizer and UndefinedBehaviorSanitizer in continuous integration. Run ThreadSanitizer on the backend and model test suites where supported.
- Apply Clang-Tidy and strict compiler warnings to production C++ targets.

### State layers

WireRunner must model four independent layers:

1. live PipeWire objects and links;
2. effective WirePlumber policy and remembered state;
3. persistent user configuration and WireRunner-managed helpers;
4. WireRunner presentation state such as layout, grouping, and preferences.

Every mutation must declare which layer it changes.

### Capability discovery

Controls must be generated from observed object capabilities, parameters, installed WirePlumber features, backend support, and version. Unsupported controls should be absent or clearly read-only rather than failing after interaction.

### Performance

- Render a typical desktop graph within one second after the backend snapshot is available.
- Keep direct manipulation responsive at 60 frames per second for ordinary graphs.
- Remain usable with at least 250 nodes, 1,000 ports, and 1,000 links through grouping and culling.
- Coalesce rapid meter and registry updates without losing meaningful state transitions.
- Avoid audio or video disruption caused by UI monitoring.

### Platform

- Target modern Linux desktops running PipeWire and WirePlumber.
- Require Qt 6.8 or newer.
- Integrate with common desktop themes and portals while retaining a distinct WireRunner identity.
- Package first for Flatpak and major distribution formats where API and permission requirements permit.
- Preserve a path to remote PipeWire instances without making remote operation a launch dependency.

## 16. Success measures

Usability studies should measure outcomes rather than feature discovery alone.

- At least 90% of first-time users can identify the active output and microphone without assistance.
- At least 85% can connect one application to a second output while preserving its first connection.
- At least 80% correctly predict whether changing a default affects an existing link.
- At least 80% can create a simple microphone-plus-application virtual mix from the canvas.
- Experienced users can reach an individual port on a 12-channel interface in no more than two deliberate expansions.
- Users can identify whether a selected link is automatic, manual, or remembered within five seconds.
- Fewer than 5% of tested routine routing actions produce an unintended disconnection.
- The production app adds negligible DSP load and does not cause measurable XRUNs under normal monitoring settings.

Product telemetry, if implemented, must be opt-in and must never collect graph names, application identities, device identities, or media metadata without explicit informed consent.

## 17. Validation scenarios

1. Change laptop playback from HDMI to headphones and make headphones the default without obscuring existing links.
2. Connect Firefox to a USB interface while retaining headphones, then remove only the headphones link.
3. Remember the Firefox-to-USB behavior for future matching streams and inspect the generated match.
4. Send one application to two physical outputs through direct fan-out and through a synchronized virtual output.
5. Mix a physical microphone and application audio into a virtual microphone for OBS while preventing feedback.
6. Expand a 12-channel interface, rename channels, adjust trims, and patch individual channels to a DAW.
7. Route one camera through a conversion or processing node into a video call and recorder simultaneously.
8. Create a virtual camera from a processed screen-capture path and inspect the negotiated output format.
9. Preview and recall a scene when one device is missing, then undo the recall.
10. Change graph quantum, observe the effective value and XRUN impact, then restore the prior value.
11. Explain a Bluetooth profile switch caused by capture activity and change the controlling preference.
12. Recover gracefully when WirePlumber restarts while a link is selected and an absent device later returns.

## 18. Risks and mitigations

### Large graphs become visually dense

Use composed automatic layout, client/device grouping, port grouping, path focus, media filters, semantic zoom, edge bundling, and viewport culling. Test these behaviors with real professional and mixed-media graphs before adding a minimap or secondary inventory.

### Policy provenance is incomplete

Show verified provenance where available and label inference honestly. Maintain an event history that correlates WirePlumber changes with resulting graph mutations.

### Persistence varies by object and application

Preview stable matching properties, identify the persistence owner, and distinguish intent waiting for an object from a currently active link.

### Configuration can become distribution-specific

Use capability discovery, generate user-owned overrides, preserve provenance, and validate against the running versions. Keep backend adapters isolated from the UI model.

### Beginner clarity conflicts with full fidelity

Measure comprehension of real graph behavior. Prefer grouping, naming, layout, and contextual explanation over alternative simplified controls.

### Video permissions and portal sessions are constrained

Represent permissions and portal-mediated objects explicitly. Do not imply that WireRunner can bypass compositor, portal, or application security decisions.

## 19. Open product questions

- Should WireRunner manage MIDI at launch or expose it initially as inspect-and-route only?
- Which WirePlumber policy decisions can provide authoritative provenance through current public APIs?
- Which persistent virtual objects should WireRunner host itself, and which should be generated as PipeWire or WirePlumber configuration?
- How should client grouping behave when one application creates many short-lived nodes or browser tabs?
- What automatic layout best preserves a user's mental map while nodes appear and disappear?
- How should an accessible nonvisual graph representation support routing without becoming a duplicate visual navigation system?
- Which video controls are sufficiently portable across V4L2, libcamera, portals, and application-provided nodes for the first release?
- What is the safest transaction and rollback model for profile changes, scene recall, and service restart?

## 20. Competitive position

WireRunner occupies the space between friendly but limited desktop controls and complete but specialist graph tools.

- [Helvum](https://flathub.org/apps/org.pipewire.Helvum) provides a clean PipeWire patchbay for audio, video, and MIDI routing.
- [qpwgraph](https://github.com/rncbc/qpwgraph) provides a mature Qt graph and patchbay workflow.
- [coppwr](https://github.com/dimtpap/coppwr) exposes low-level graph, object, metadata, module, profiling, and portal controls.
- [PipeWire Controller](https://github.com/knightinfected/PipeWireController) covers a broad audio configuration surface including virtual devices, policies, effects, and snapshots.
- [pwvucontrol](https://github.com/saivert/pwvucontrol) offers approachable conventional volume and device controls.

WireRunner's differentiator is the combination of a faithful multimedia graph, visible WirePlumber policy, direct manipulation, progressive object-level detail, and a calm experience built around one canonical workspace.

## 21. Reference prototype and source material

- Current UX prototype: [`prototype/index.html`](prototype/index.html)
- Prototype rationale: [`prototype/README.md`](prototype/README.md)
- Shared functional inventory and technical notes: [`PRODUCT_BRIEF.md`](PRODUCT_BRIEF.md)
- PipeWire documentation: <https://docs.pipewire.org/>
- WirePlumber documentation: <https://pipewire.pages.freedesktop.org/wireplumber/>
- WirePlumber linking policy: <https://pipewire.pages.freedesktop.org/wireplumber/policies/linking.html>
- WirePlumber video configuration: <https://pipewire.pages.freedesktop.org/wireplumber/daemon/configuration/video.html>
