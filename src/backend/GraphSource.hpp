// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "domain/Graph.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <cstdint>

namespace wirerunner {

enum class SourceState { Connecting, Ready, Disconnected, Error };
struct SourceStatus { SourceState state{SourceState::Connecting}; std::string message; };

using CommandId = std::uint64_t;

struct CreateLinkRequest {
  CommandId commandId{};
  std::uint64_t snapshotRevision{};
  GlobalId outputNodeId{};
  GlobalId outputPortId{};
  GlobalId inputNodeId{};
  GlobalId inputPortId{};
  bool feedback{};
  bool linger{true};
};

struct DestroyLinkRequest {
  CommandId commandId{};
  std::uint64_t snapshotRevision{};
  GlobalId linkId{};
};

struct SetNodeAudioRequest {
  CommandId commandId{};
  std::uint64_t snapshotRevision{};
  GlobalId nodeId{};
  std::optional<float> volume;
  std::vector<float> channelVolumes;
  std::optional<bool> muted;
};

struct SetDeviceRouteAudioRequest {
  CommandId commandId{};
  std::uint64_t snapshotRevision{};
  GlobalId deviceId{};
  int routeIndex{-1};
  int routeDeviceId{-1};
  std::vector<float> channelVolumes;
  std::vector<std::uint32_t> channelMap;
  std::optional<bool> muted;
};

struct SetDefaultRequest {
  CommandId commandId{};
  std::uint64_t snapshotRevision{};
  DefaultKind kind{DefaultKind::AudioSink};
  std::string nodeName;
};

struct SetDeviceProfileRequest {
  CommandId commandId{};
  std::uint64_t snapshotRevision{};
  GlobalId deviceId{};
  int profileIndex{-1};
};

struct SetDeviceRouteRequest {
  CommandId commandId{};
  std::uint64_t snapshotRevision{};
  GlobalId deviceId{};
  int routeIndex{-1};
  int routeDeviceId{-1};
};

struct CommandResult {
  CommandId commandId{};
  bool accepted{};
  std::string message;
};

class GraphSource {
public:
  using SnapshotCallback = std::function<void(std::shared_ptr<const GraphSnapshot>)>;
  using StatusCallback = std::function<void(SourceStatus)>;
  using CommandCallback = std::function<void(CommandResult)>;
  virtual ~GraphSource() = default;
  virtual void start(SnapshotCallback snapshot, StatusCallback status) = 0;
  virtual void stop() = 0;
  virtual void createLink(CreateLinkRequest request, CommandCallback callback) = 0;
  virtual void destroyLink(DestroyLinkRequest request, CommandCallback callback) = 0;
  virtual void setNodeAudio(SetNodeAudioRequest request, CommandCallback callback) = 0;
  virtual void setDeviceRouteAudio(SetDeviceRouteAudioRequest request, CommandCallback callback) = 0;
  virtual void setDefault(SetDefaultRequest request, CommandCallback callback) = 0;
  virtual void setDeviceProfile(SetDeviceProfileRequest request, CommandCallback callback) = 0;
  virtual void setDeviceRoute(SetDeviceRouteRequest request, CommandCallback callback) = 0;
};

} // namespace wirerunner
