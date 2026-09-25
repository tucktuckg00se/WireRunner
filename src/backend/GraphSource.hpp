// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "domain/Graph.hpp"

#include <functional>
#include <memory>
#include <string>

namespace wirerunner {

enum class SourceState { Connecting, Ready, Disconnected, Error };
struct SourceStatus { SourceState state{SourceState::Connecting}; std::string message; };

class GraphSource {
public:
  using SnapshotCallback = std::function<void(std::shared_ptr<const GraphSnapshot>)>;
  using StatusCallback = std::function<void(SourceStatus)>;
  virtual ~GraphSource() = default;
  virtual void start(SnapshotCallback snapshot, StatusCallback status) = 0;
  virtual void stop() = 0;
};

} // namespace wirerunner
