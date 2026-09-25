// SPDX-License-Identifier: GPL-3.0-or-later
#include "domain/Routing.hpp"

#include <algorithm>
#include <deque>
#include <unordered_map>
#include <unordered_set>

namespace wirerunner {
namespace { constexpr std::uint32_t executePermission = 0100; }

const GraphPort *findPort(const GraphSnapshot &graph, GlobalId id) {
  const auto found = std::ranges::find(graph.ports, id, &GraphPort::id);
  return found == graph.ports.end() ? nullptr : &*found;
}

const GraphLink *findLink(const GraphSnapshot &graph, GlobalId id) {
  const auto found = std::ranges::find(graph.links, id, &GraphLink::id);
  return found == graph.links.end() ? nullptr : &*found;
}

LinkCompatibility assessLink(const GraphSnapshot &graph, GlobalId outputPortId, GlobalId inputPortId) {
  const auto *output = findPort(graph, outputPortId);
  const auto *input = findPort(graph, inputPortId);
  if (!output || !input) return {false, false, "This port is no longer available"};
  if (output->direction != PortDirection::Output) return {false, false, "A route must start at an output port"};
  if (input->direction != PortDirection::Input) return {false, false, "A route must end at an input port"};
  if (output->media != MediaType::Unknown && input->media != MediaType::Unknown && output->media != input->media)
    return {false, false, "The ports carry different media types"};
  if (output->permissions != 0 && (output->permissions & executePermission) == 0)
    return {false, false, "PipeWire does not allow this output to be linked"};
  if (input->permissions != 0 && (input->permissions & executePermission) == 0)
    return {false, false, "PipeWire does not allow this input to be linked"};
  if (std::ranges::any_of(graph.links, [&](const GraphLink &link) {
        return link.outputPortId == outputPortId && link.inputPortId == inputPortId;
      })) return {false, false, "These exact ports are already connected"};

  std::unordered_map<GlobalId, std::vector<GlobalId>> adjacency;
  for (const auto &link : graph.links) {
    if (output->media != MediaType::Unknown && link.media != MediaType::Unknown && link.media != output->media) continue;
    adjacency[link.outputNodeId].push_back(link.inputNodeId);
  }
  std::deque<GlobalId> pending{input->nodeId};
  std::unordered_set<GlobalId> visited{input->nodeId};
  bool cycle = input->nodeId == output->nodeId;
  while (!cycle && !pending.empty()) {
    const auto current = pending.front();
    pending.pop_front();
    for (const auto next : adjacency[current]) {
      if (next == output->nodeId) { cycle = true; break; }
      if (visited.insert(next).second) pending.push_back(next);
    }
  }
  const bool unverified = output->media == MediaType::Unknown || input->media == MediaType::Unknown;
  return {true, cycle, cycle ? "This closes a feedback path"
    : unverified ? "PipeWire will verify compatibility when the link is created" : std::string{}};
}

} // namespace wirerunner
