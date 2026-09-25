// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace wirerunner {

using GlobalId = std::uint32_t;

enum class MediaType { Audio, Video, Midi, Unknown };
enum class PortDirection { Input, Output };
enum class NodeRole { Source, Processor, Destination };

struct GraphClient { GlobalId id{}; std::string name; std::string stableId; };
struct GraphDevice { GlobalId id{}; std::string name; std::string stableId; std::string mediaClass; };

struct GraphNode {
  GlobalId id{};
  std::string name;
  std::string technicalName;
  std::string stableId;
  std::string mediaClass;
  std::string state;
  MediaType media{MediaType::Unknown};
  NodeRole role{NodeRole::Processor};
  std::optional<GlobalId> clientId;
  std::optional<GlobalId> deviceId;
  double x{};
  double y{};
};

struct GraphPort {
  GlobalId id{};
  GlobalId nodeId{};
  std::string name;
  std::string channel;
  PortDirection direction{PortDirection::Input};
  MediaType media{MediaType::Unknown};
};

struct GraphLink {
  GlobalId id{};
  GlobalId outputNodeId{};
  GlobalId outputPortId{};
  GlobalId inputNodeId{};
  GlobalId inputPortId{};
  std::string state;
  MediaType media{MediaType::Unknown};
};

struct GraphSnapshot {
  std::vector<GraphClient> clients;
  std::vector<GraphDevice> devices;
  std::vector<GraphNode> nodes;
  std::vector<GraphPort> ports;
  std::vector<GraphLink> links;
  std::string remoteName;
  std::string remoteVersion;
};

MediaType classifyMedia(std::string_view mediaClass, std::string_view format = {});
std::string_view mediaTypeName(MediaType type);
std::string_view nodeRoleName(NodeRole role);
void layoutGraph(GraphSnapshot &snapshot);

} // namespace wirerunner
