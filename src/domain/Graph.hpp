// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace wirerunner {

using GlobalId = std::uint32_t;

enum class MediaType { Audio, Video, Midi, Unknown };
enum class PortDirection { Input, Output };
enum class NodeRole { Source, Processor, Destination };

struct GraphClient { GlobalId id{}; std::string name; std::string stableId; };
struct GraphDevice { GlobalId id{}; std::string name; std::string stableId; std::string mediaClass; };

struct NodeAudioControl {
  float volume{1.0F};
  std::vector<float> channelVolumes;
  float minimumVolume{};
  float maximumVolume{1.0F};
  bool muted{};
  bool hasVolume{};
  bool hasMute{};
  bool writable{};
};

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
  std::optional<NodeAudioControl> audio;
};

struct GraphPort {
  GlobalId id{};
  GlobalId nodeId{};
  std::string name;
  std::string channel;
  PortDirection direction{PortDirection::Input};
  MediaType media{MediaType::Unknown};
  std::string format{};
  std::uint32_t permissions{};
};

struct GraphLink {
  GlobalId id{};
  GlobalId outputNodeId{};
  GlobalId outputPortId{};
  GlobalId inputNodeId{};
  GlobalId inputPortId{};
  std::string state;
  MediaType media{MediaType::Unknown};
  bool feedback{};
  bool linger{};
  bool createdByWireRunner{};
  std::uint32_t permissions{};
};

struct GraphSnapshot {
  std::vector<GraphClient> clients;
  std::vector<GraphDevice> devices;
  std::vector<GraphNode> nodes;
  std::vector<GraphPort> ports;
  std::vector<GraphLink> links;
  std::string remoteName;
  std::string remoteVersion;
  std::uint64_t revision{};
};

MediaType classifyMedia(std::string_view mediaClass, std::string_view format = {});
std::string_view mediaTypeName(MediaType type);
std::string_view nodeRoleName(NodeRole role);
double volumeToPercent(double linearGain);
double percentToVolume(double percent);
double volumeToDecibels(double linearGain);

struct GraphCard {
  std::string key;
  std::string title;
  std::string subtitle;
  std::string kind;
  bool persistent{};
  std::vector<GlobalId> nodeIds;
  std::vector<GraphPort> inputs;
  std::vector<GraphPort> outputs;
};

struct ComposedGraph {
  std::vector<GraphCard> cards;
  std::unordered_map<GlobalId, std::string> nodeCards;
};

struct GraphPosition { double x{}; double y{}; };

std::string stableNodeKey(const GraphSnapshot &snapshot, const GraphNode &node);
void classifyNodeRoles(GraphSnapshot &snapshot);
ComposedGraph composeGraph(const GraphSnapshot &snapshot);
std::unordered_map<std::string, GraphPosition> layoutGraph(
  const ComposedGraph &graph, const std::vector<GraphLink> &links);

} // namespace wirerunner
