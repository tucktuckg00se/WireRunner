// SPDX-License-Identifier: GPL-3.0-or-later
#include "domain/Graph.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <unordered_map>

namespace wirerunner {
namespace {
std::string lower(std::string_view value) {
  std::string result(value);
  std::ranges::transform(result, result.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return result;
}
}

MediaType classifyMedia(std::string_view mediaClass, std::string_view format) {
  const auto value = lower(std::string(mediaClass) + " " + std::string(format));
  if (value.contains("audio")) return MediaType::Audio;
  if (value.contains("video") || value.contains("camera")) return MediaType::Video;
  if (value.contains("midi")) return MediaType::Midi;
  return MediaType::Unknown;
}

std::string_view mediaTypeName(MediaType type) {
  switch (type) {
  case MediaType::Audio: return "audio";
  case MediaType::Video: return "video";
  case MediaType::Midi: return "midi";
  case MediaType::Unknown: return "unknown";
  }
  return "unknown";
}

std::string_view nodeRoleName(NodeRole role) {
  switch (role) {
  case NodeRole::Source: return "source";
  case NodeRole::Processor: return "processor";
  case NodeRole::Destination: return "destination";
  }
  return "processor";
}

void layoutGraph(GraphSnapshot &snapshot) {
  std::unordered_map<GlobalId, std::pair<int, int>> directions;
  for (const auto &port : snapshot.ports) {
    auto &counts = directions[port.nodeId];
    port.direction == PortDirection::Input ? ++counts.first : ++counts.second;
  }

  std::ranges::sort(snapshot.nodes, {}, [](const GraphNode &node) {
    return std::pair{node.media, node.name};
  });
  std::array<int, 3> rows{};
  for (auto &node : snapshot.nodes) {
    const auto [inputs, outputs] = directions[node.id];
    if (outputs > 0 && inputs == 0) node.role = NodeRole::Source;
    else if (inputs > 0 && outputs == 0) node.role = NodeRole::Destination;
    else node.role = NodeRole::Processor;
    const auto lane = static_cast<std::size_t>(node.role);
    node.x = 56.0 + static_cast<double>(lane) * 390.0;
    node.y = 76.0 + static_cast<double>(rows[lane]++) * 154.0;
  }
}

} // namespace wirerunner
