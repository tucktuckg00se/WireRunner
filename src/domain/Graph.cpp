// SPDX-License-Identifier: GPL-3.0-or-later
#include "domain/Graph.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <deque>
#include <map>
#include <set>
#include <tuple>
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

double volumeToPercent(double linearGain) {
  return linearGain <= 0.0 ? 0.0 : std::cbrt(linearGain) * 100.0;
}

double percentToVolume(double percent) {
  const auto normalized = std::max(0.0, percent) / 100.0;
  return normalized * normalized * normalized;
}

double volumeToDecibels(double linearGain) {
  return linearGain <= 0.0 ? -INFINITY : 20.0 * std::log10(linearGain);
}

std::string stableNodeKey(const GraphSnapshot &snapshot, const GraphNode &node) {
  if (node.deviceId) {
    const auto device = std::ranges::find(snapshot.devices, *node.deviceId, &GraphDevice::id);
    if (device != snapshot.devices.end() && !device->stableId.empty()) return "device:" + device->stableId;
    return "runtime-device:" + std::to_string(*node.deviceId);
  }
  if (node.clientId) {
    const auto client = std::ranges::find(snapshot.clients, *node.clientId, &GraphClient::id);
    if (client != snapshot.clients.end() && !client->stableId.empty()) return "client:" + client->stableId;
    return "runtime-client:" + std::to_string(*node.clientId);
  }
  if (!node.stableId.empty()) return "node:" + node.stableId;
  return "runtime-node:" + std::to_string(node.id);
}

void classifyNodeRoles(GraphSnapshot &snapshot) {
  std::unordered_map<GlobalId, std::pair<int, int>> directions;
  for (const auto &port : snapshot.ports) {
    auto &counts = directions[port.nodeId];
    port.direction == PortDirection::Input ? ++counts.first : ++counts.second;
  }
  for (auto &node : snapshot.nodes) {
    const auto [inputs, outputs] = directions[node.id];
    if (outputs > 0 && inputs == 0) node.role = NodeRole::Source;
    else if (inputs > 0 && outputs == 0) node.role = NodeRole::Destination;
    else node.role = NodeRole::Processor;
  }
}

ComposedGraph composeGraph(const GraphSnapshot &snapshot) {
  ComposedGraph result;
  std::unordered_map<std::string, std::size_t> indices;
  for (const auto &node : snapshot.nodes) {
    const auto key = stableNodeKey(snapshot, node);
    auto found = indices.find(key);
    if (found == indices.end()) {
      GraphCard card;
      card.key = key;
      card.persistent = !key.starts_with("runtime-");
      card.title = node.name;
      card.subtitle = node.mediaClass;
      card.kind = "node";
      if (node.deviceId) {
        const auto device = std::ranges::find(snapshot.devices, *node.deviceId, &GraphDevice::id);
        if (device != snapshot.devices.end()) {
          card.title = device->name;
          card.subtitle = device->mediaClass.empty() ? "Device" : device->mediaClass;
          card.kind = "device";
        }
      } else if (node.clientId) {
        const auto client = std::ranges::find(snapshot.clients, *node.clientId, &GraphClient::id);
        if (client != snapshot.clients.end()) {
          card.title = client->name;
          card.subtitle = "Application";
          card.kind = "client";
        }
      }
      found = indices.emplace(key, result.cards.size()).first;
      result.cards.push_back(std::move(card));
    }
    auto &card = result.cards[found->second];
    card.nodeIds.push_back(node.id);
    result.nodeCards[node.id] = key;
  }

  for (const auto &port : snapshot.ports) {
    const auto cardKey = result.nodeCards.find(port.nodeId);
    if (cardKey == result.nodeCards.end()) continue;
    auto &card = result.cards[indices.at(cardKey->second)];
    (port.direction == PortDirection::Input ? card.inputs : card.outputs).push_back(port);
  }
  const auto portOrder = [](const GraphPort &left, const GraphPort &right) {
    return std::tuple{left.media, left.nodeId, left.channel, left.name, left.id} <
      std::tuple{right.media, right.nodeId, right.channel, right.name, right.id};
  };
  for (auto &card : result.cards) {
    std::ranges::sort(card.inputs, portOrder);
    std::ranges::sort(card.outputs, portOrder);
  }
  std::ranges::sort(result.cards, {}, &GraphCard::key);
  return result;
}

std::unordered_map<std::string, GraphPosition> layoutGraph(
    const ComposedGraph &graph, const std::vector<GraphLink> &links) {
  std::unordered_map<std::string, std::set<std::string>> outgoing;
  std::unordered_map<std::string, int> indegree;
  std::unordered_map<std::string, int> rank;
  for (const auto &card : graph.cards) indegree[card.key] = 0;
  for (const auto &link : links) {
    const auto output = graph.nodeCards.find(link.outputNodeId);
    const auto input = graph.nodeCards.find(link.inputNodeId);
    if (output == graph.nodeCards.end() || input == graph.nodeCards.end() || output->second == input->second) continue;
    if (outgoing[output->second].insert(input->second).second) ++indegree[input->second];
  }

  std::deque<std::string> ready;
  for (const auto &[key, degree] : indegree) if (degree == 0) ready.push_back(key);
  std::ranges::sort(ready);
  while (!ready.empty()) {
    const auto key = ready.front();
    ready.pop_front();
    for (const auto &next : outgoing[key]) {
      rank[next] = std::max(rank[next], rank[key] + 1);
      if (--indegree[next] == 0) ready.push_back(next);
    }
    std::ranges::sort(ready);
  }
  for (const auto &[key, degree] : indegree) if (degree > 0) rank[key] = std::max(rank[key], 1);

  std::map<int, std::vector<const GraphCard *>> lanes;
  for (const auto &card : graph.cards) lanes[rank[card.key]].push_back(&card);
  for (auto &[lane, cards] : lanes) {
    static_cast<void>(lane);
    std::ranges::sort(cards, [](const GraphCard *left, const GraphCard *right) {
      return std::tuple{left->title, left->key} < std::tuple{right->title, right->key};
    });
  }
  std::unordered_map<std::string, std::set<std::string>> incoming;
  for (const auto &[source, targets] : outgoing) for (const auto &target : targets) incoming[target].insert(source);
  const auto rowIndices = [&] {
    std::unordered_map<std::string, double> rows;
    for (const auto &[lane, cards] : lanes) {
      static_cast<void>(lane);
      for (std::size_t row = 0; row < cards.size(); ++row) rows[cards[row]->key] = static_cast<double>(row);
    }
    return rows;
  };
  for (int pass = 0; pass < 4 && lanes.size() > 1; ++pass) {
    auto rows = rowIndices();
    for (auto lane = std::next(lanes.begin()); lane != lanes.end(); ++lane) {
      std::ranges::stable_sort(lane->second, [&](const GraphCard *left, const GraphCard *right) {
        const auto score = [&](const GraphCard *card) {
          double total = 0.0; int count = 0;
          for (const auto &key : incoming[card->key]) if (rank[key] < lane->first) { total += rows[key]; ++count; }
          return count == 0 ? rows[card->key] : total / static_cast<double>(count);
        };
        return std::tuple{score(left), left->title, left->key} < std::tuple{score(right), right->title, right->key};
      });
      rows = rowIndices();
    }
    rows = rowIndices();
    for (auto lane = std::next(lanes.rbegin()); lane != lanes.rend(); ++lane) {
      std::ranges::stable_sort(lane->second, [&](const GraphCard *left, const GraphCard *right) {
        const auto score = [&](const GraphCard *card) {
          double total = 0.0; int count = 0;
          for (const auto &key : outgoing[card->key]) if (rank[key] > lane->first) { total += rows[key]; ++count; }
          return count == 0 ? rows[card->key] : total / static_cast<double>(count);
        };
        return std::tuple{score(left), left->title, left->key} < std::tuple{score(right), right->title, right->key};
      });
      rows = rowIndices();
    }
  }
  std::unordered_map<std::string, GraphPosition> positions;
  for (auto &[lane, cards] : lanes) {
    for (std::size_t row = 0; row < cards.size(); ++row) {
      positions[cards[row]->key] = {64.0 + static_cast<double>(lane) * 360.0,
        64.0 + static_cast<double>(row) * 178.0};
    }
  }
  return positions;
}

} // namespace wirerunner
