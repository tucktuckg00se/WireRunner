// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "domain/Graph.hpp"

#include <string>

namespace wirerunner {

struct LinkCompatibility {
  bool compatible{};
  bool probableCycle{};
  std::string reason;
};

const GraphPort *findPort(const GraphSnapshot &graph, GlobalId id);
const GraphLink *findLink(const GraphSnapshot &graph, GlobalId id);
LinkCompatibility assessLink(const GraphSnapshot &graph, GlobalId outputPortId, GlobalId inputPortId);

} // namespace wirerunner
