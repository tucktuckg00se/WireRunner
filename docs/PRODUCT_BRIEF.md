# WirePlumber GUI: shared product brief

## Product goal

Design a modern Linux desktop control surface for PipeWire and WirePlumber that a beginner can use to answer three questions immediately:

1. Where is my sound playing?
2. Which microphone is being used?
3. How do I move or adjust it?

The same interface must let an experienced user inspect, patch, tune, save, and troubleshoot a complex audio graph without hiding the real system model.

This is an audio-first presentation of PipeWire's broader media graph. Cameras, screen capture, virtual cameras, video processing, and video links use the same object and routing model. Media filters may reduce initial complexity, but the architecture must not treat video as a separate application or an afterthought.

## UX contract

- Open on a useful overview, not an empty graph.
- Use human names first: “Built-in speakers,” “Firefox,” and “Scarlett 4i4.” Keep node IDs, internal names, media classes, and serials one reveal away.
- Make ordinary actions direct: change volume, mute, choose defaults, and move a stream in one interaction.
- Use one canonical workspace for live audio objects and links. Do not repeat the same objects in a browser, mixer, overview, and patchbay.
- Use progressive disclosure on the objects themselves rather than separate beginner and expert representations of the system.
- A live link has one canonical representation and one editing model. Convenience actions may focus or create that same visible link, but must not introduce a second destination or routing control.
- Preserve graph semantics: outputs may fan out to several inputs, inputs may receive several outputs, and virtual or processing nodes may sit anywhere in the path.
- Treat media type as a property of ports and links. Audio, video, and mixed-media clients share the workspace; compatibility guidance prevents invalid cross-media connections.
- Present WirePlumber policy alongside the graph without conflating it with the graph. Defaults, automatic policy, remembered rules, and current links must remain distinct.
- Preview the impact of disruptive changes such as profile switches, device disablement, or graph-wide restores.
- Distinguish live-session changes, remembered state, and generated configuration fragments everywhere they differ.
- Keep destructive and disruptive actions undoable when possible. Show an activity record with the previous value and a one-click undo.
- Update live when devices, Bluetooth links, applications, streams, ports, or policy state change. Preserve the user’s current context while the graph moves.
- Be fully operable by keyboard and screen reader; never encode channel, status, or routing only by color.

## Required functional surface

### 1. System overview and health

- Show WirePlumber and PipeWire connection/service health, versions, active WirePlumber profile, and reconnect state.
- Summarize current default output, default input, active playback/capture streams, muted devices, disconnected devices, and obvious faults.
- Show live level meters for the active output, input, devices, and streams, with peak/clip indication and an option to reduce meter activity.
- Provide fast global actions: mute all playback, mute all microphones, restore last working state, and open diagnostics.
- Surface recent changes such as “Headphones connected,” “Discord moved to headset,” or “Bluetooth switched to headset profile.”

### 2. Physical and network devices

- List audio interfaces, sound cards, USB devices, HDMI/DisplayPort endpoints, Bluetooth devices, network audio endpoints, and built-in audio.
- Show connection and availability state, bus, driver/backend, nickname/description, and stable identity.
- Rename devices locally without losing the original hardware name.
- Enable, disable, or hide a device from ordinary views.
- Select a device profile, including duplex/output/input/off profiles, with a clear explanation of what ports will appear or disappear.
- Select routes/ports such as speakers, headphones, line out, microphone, line in, or HDMI connector.
- Display unplugged or unavailable ports and explain why a route cannot be selected.
- Choose Bluetooth profile and codec when available; expose quality/latency preference and automatic headset switching.
- Show battery and link state for Bluetooth hardware when the backend exposes them.
- Choose the fallback/default output and input; clear a remembered default to return to automatic priority selection.
- Edit session priority and explain its interaction with an explicitly remembered default.

### 3. Output, input, and per-channel controls

- Adjust output/input volume with a configurable safe limit and optional values in percent or dB.
- Mute/unmute and provide a prominent microphone privacy control.
- Expose balance, fade, and individual channel trims; link/unlink channels.
- Show the active channel map and allow channel remapping where supported.
- Identify channels with a test tone or channel callout, with an obvious stop control and safe initial level.
- Show latency, sample format, sample rate, channel count, and current state (running, idle, suspended, error).
- Expose hardware vs software volume when known, including the current route’s saved volume.

### 4. Applications and streams

- Group streams by application while allowing expansion to individual streams/roles.
- Show icon, application name, media title/role, process identity, format, rate, channels, latency, target, and stream state.
- Adjust volume and mute per application or per stream; optionally apply a control to all streams from the same process.
- Connect or disconnect a live playback or recording stream from compatible outputs/inputs without assuming it has a single destination.
- Connect a stream to multiple outputs directly or through a virtual output, according to the actual graph structure.
- Set a stream as temporarily soloed; provide solo-in-place semantics that are clear about what becomes muted.
- Pin routing and volume as an application rule for future streams, with match criteria the user can inspect and edit.
- Choose whether new streams follow the system default or a saved application target.
- Surface recording applications and their microphone source prominently for privacy.

### 5. Routing and patchbay

- Provide a live graph of devices, nodes, ports, streams, filters, virtual endpoints, and links.
- Create and remove links through one consistent port-to-port interaction that supports pointer and keyboard input.
- Support one-to-one, one-to-many, many-to-one, and loopback routes.
- Validate media type, direction, channel format, and port compatibility before connecting; explain rejected links.
- Allow channel-level patching for multichannel interfaces.
- Collapse nodes into devices/apps, expand to ports, filter by media class, and search by human or internal name.
- Keep links readable with large graphs through grouping, focus/isolate, edge bundling or equivalent, and a minimap only if it proves useful.
- Mark links created by automatic policy versus the user and allow policy-managed links to be pinned or overridden.
- Make transient nodes and disappearing streams understandable rather than treating them as errors.
- Support selection, multi-selection, alignment/grouping, and keyboard traversal without turning the graph into a diagram editor for its own sake.

### 6. Virtual devices and processing

- Create virtual sinks, virtual sources, null sinks, loopbacks, monitor sources, combine/mirror outputs, remapped endpoints, and filter-chain endpoints.
- Use guided presets for common jobs: “Play to two devices,” “Record desktop audio,” “Send an app into a DAW,” “Create a clean microphone,” and “Make a streaming mix.”
- Let experts configure name/description, media class, channels, channel map, format/rate, latency, clock behavior, and module-specific properties.
- Insert, remove, reorder, bypass, and inspect processing nodes or filter chains when available.
- Clearly distinguish a sink’s monitor source from a physical microphone.
- Save a virtual device for future sessions or keep it temporary; show where persistence is implemented.
- Prevent feedback loops where detectable and warn before creating a route with a likely feedback path.

### 6a. Video and mixed-media graph

- Show cameras, screen-capture sources, application video streams, virtual cameras, video converters, filters, and consumers as first-class graph objects.
- Support one-to-one and one-to-many video links through the same port-to-port interaction used for audio.
- Expose resolution, pixel format, color space, frame rate, modifier, crop/orientation, latency, and negotiated versus requested format where available.
- Represent applications with both audio and video without duplicating the application across unrelated navigation systems. Their distinct PipeWire nodes and ports may be grouped under one client identity.
- Create and manage virtual cameras and video-processing chains while preserving every intermediate node in the visible graph.
- Show permission and portal-mediated capture state for cameras and screen sharing when available.
- Filter or focus the workspace by audio, video, active media, client, or selected signal path without creating separate copies of the graph.

### 7. Scenes, presets, and persistence

- Save a named scene containing selected defaults, device profiles/routes, volumes/mutes, virtual devices, links, application rules, and relevant policy settings.
- Let the user choose which parts a scene captures and restores.
- Compare the current graph with a saved scene before applying it; handle absent hardware gracefully.
- Offer undo after scene restore and retain a small scene history.
- Import/export scenes in a readable format, detect conflicts, and redact machine-specific identifiers when sharing.
- Explain persistence scope for every change: current stream, current session, remembered WirePlumber state, or configuration fragment.
- List and clear remembered defaults, routes, profiles, stream properties, and saved settings independently.

### 8. Policy and automation

- Browse WirePlumber settings with plain-language descriptions, current value, schema default, saved value, type, valid range, and source.
- Change, save, reset, or delete saved settings with the precedence made explicit.
- Cover device profile/route restoration, default target restoration, stream restoration, Bluetooth autoswitch and codec preference, linking behavior, suspend/idle behavior, and other installed schema settings.
- Build rules for matching devices, nodes, applications, media roles, names, properties, and connection events.
- Support actions such as rename, set priority, select target/profile/route, set properties, create links, or apply a scene.
- Provide a readable condition/action editor and an expert source view; validate before installation.
- Generate override fragments under the user configuration directory rather than editing distribution files.
- Show which rule or policy decision produced a target, profile, route, or link when that provenance is available.

### 9. Pro-audio engine controls

- Display current PipeWire graph clock, driver node, sample rate, quantum/buffer size, minimum/maximum quantum, force values, and effective latency.
- Change supported runtime metadata controls with presets for stable playback, recording, low-latency performance, and power saving.
- Show requested versus effective rate/quantum and which client constrains them.
- Identify resampling and rate mismatches; show resampler quality/configuration when exposed.
- Support pro-audio device profiles, multichannel port naming, and dense channel strips without sacrificing the beginner view.
- Display XRUN/dropout counts or equivalent diagnostics when available and timestamp changes that correlate with them.
- Avoid promising controls the backend cannot safely change; represent unsupported and read-only values accurately.

### 10. Monitoring and diagnostics

- Inspect every object’s properties, parameters, permissions, IDs, serials, factory/module/client ownership, and raw values.
- Search and filter the complete object inventory: clients, devices, nodes, ports, links, modules, factories, metadata, and endpoints.
- Show a timestamped event stream for additions, removals, state changes, route/profile changes, and link operations.
- Display service logs with level/topic filters and a copy/export flow that offers privacy redaction.
- Run guided checks for “no sound,” “microphone unavailable,” “Bluetooth quality dropped,” “app ignores routing,” and “audio crackles.”
- Explain likely causes and provide reversible fixes; never reduce diagnosis to a generic failure toast.
- Export a support bundle containing versions, status, graph snapshot, relevant settings/configuration, and selected logs.
- Provide a command preview or copyable equivalent for expert operations where a reliable CLI representation exists.

### 11. Configuration and lifecycle

- Discover user, host, and distribution configuration layers; show effective values and provenance.
- Create, enable/disable, reorder, edit, validate, and remove user configuration fragments.
- Back up before overwriting a user-managed fragment and retain recoverable history.
- Indicate whether a change is immediate, affects only new streams, requires reconnecting a device, or requires restarting WirePlumber/PipeWire.
- Restart the relevant user service with impact preview and reconnection progress.
- Detect unsupported WirePlumber/PipeWire versions and degrade controls honestly.
- Handle daemon disconnects, permission failures, partial state, stale object IDs, and races caused by a changing graph.

### 12. Interaction, safety, and accessibility

- Global search/command palette for devices, apps, settings, scenes, and actions.
- Keyboard shortcuts for mute, default selection, focus navigation, graph zoom, undo/redo, and command search.
- Context menus may accelerate expert work but no required action may exist only in a context menu.
- Use confirmation only for broad or disruptive operations; make routine routing and volume changes direct and undoable.
- Offer volume safety preferences, feedback-loop protection, test-tone limits, and microphone-use indicators.
- Meet WCAG 2.2 AA contrast and focus requirements, support 200% text scaling, reduced motion, screen readers, high contrast, and color-vision differences.
- Work from compact laptop windows through wide studio displays. Dense expert views must remain usable without tiny hit targets.
- Provide useful empty, loading, disconnected, permission-denied, unsupported, and partially available states.

## Core information architecture

The live media graph is the primary workspace. Each application, stream, device node, virtual endpoint, processing node, port, and link appears once. Human names, grouping, layout, media filtering, and progressive detail make this workspace approachable without substituting a smaller routing model.

- **Canvas:** the live system, including volume, mute, defaults, activity, nodes, ports, and links.
- **Media scope:** audio, video, and mixed-media views filter the same canvas and preserve selection and layout.
- **Selection inspector:** properties and actions belonging only to the selected object or link. It must not reproduce the object inventory or provide another routing mechanism.
- **Inline expansion:** profiles, routes, channels, and ports expand from their owning device or node while preserving context.
- **Policy and persistence:** defaults, automatic links, remembered rules, and ownership appear as annotations on the affected objects and links.
- **Saved setups:** scenes change the same visible workspace and provide a preview before application.
- **System controls:** engine, configuration, logs, and diagnostics remain secondary utilities because they do not represent alternative views of the live graph.

## Mandatory workflows to prototype

Each competing design must make these concrete and clickable:

1. A beginner switches laptop playback from HDMI to headphones and makes headphones the default.
2. The user connects Firefox to a USB interface, sees whether its existing output remains connected, deliberately keeps or removes that link, and saves the resulting behavior for future Firefox streams.
3. The user creates a combined output for speakers and a Bluetooth headset, with latency caveats explained.
4. A streamer creates a virtual microphone that mixes a physical mic with an application source while avoiding feedback.
5. An audio professional opens a 12-channel interface, renames channels, adjusts independent trims, and patches channels to a DAW.
6. The user diagnoses Bluetooth audio switching from high-quality playback to headset mode when an app records.
7. The user saves the current setup as a scene, previews a different scene, applies it despite one missing device, then undoes it.
8. The user changes graph quantum for low-latency work, sees the effective value and likely stability impact, then restores the prior value.
9. The user routes a camera through a video-processing node into two consumers, creates a virtual camera from that path, and inspects the negotiated format without leaving the graph.

## Design deliverables and judging criteria

Build a polished, responsive, interactive front-end prototype using realistic data. It may be a web prototype even if the eventual product becomes a native Linux app. Include enough interaction to demonstrate the mandatory workflows; do not submit static mockups alone.

Document the visual token system and a short rationale. Avoid generic dashboard cards, decorative gradients, gratuitous glass effects, and debugging-oriented object dumps. The primary graph should be composed, labeled, and useful immediately.

Concepts will be judged on:

- beginner time-to-success for defaults, volume, mute, and per-app routing;
- expert depth and information density;
- clarity of live versus persistent state;
- routing/patchbay legibility at small and large graph sizes;
- safety around feedback, high volume, disruptive profile changes, and restarts;
- accessibility and keyboard operation;
- visual identity grounded in audio signal flow and studio hardware without imitating a physical mixer unnecessarily;
- quality of error, empty, reconnecting, and missing-device states;
- implementation coherence and readiness to evolve into a real product.

## Technical truth the UI must respect

- WirePlumber is PipeWire’s session and policy manager. Devices, nodes, ports, links, clients, and metadata are related but not interchangeable.
- PipeWire transports video as well as audio. Video sources, sinks, filters, and links use the same graph primitives, with media-specific formats and negotiation.
- A default target affects new auto-connected streams; moving an existing stream is a separate action.
- Device profiles can add or remove nodes; routes select ports and may carry saved volume, mute, channel-map, and codec properties.
- Saved WirePlumber state can override configuration-file values. Resetting a runtime setting and deleting its saved value are different actions.
- Object IDs are transient. Designs should rely on and display stable identifying properties when persistence is promised.
- Some controls belong to PipeWire metadata or loaded modules rather than WirePlumber itself. The UI can unify them, but its explanations and failure states must remain accurate.

## Official references

- https://pipewire.pages.freedesktop.org/wireplumber/
- https://pipewire.pages.freedesktop.org/wireplumber/man/wpctl.html
- https://pipewire.pages.freedesktop.org/wireplumber/daemon/configuration/settings.html
- https://pipewire.pages.freedesktop.org/wireplumber/daemon/configuration/modifying_configuration.html
- https://docs.pipewire.org/page_access.html
