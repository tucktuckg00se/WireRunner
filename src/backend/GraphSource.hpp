// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "domain/Graph.hpp"

#include <functional>
#include <memory>
#include <string>
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
};

} // namespace wirerunner
