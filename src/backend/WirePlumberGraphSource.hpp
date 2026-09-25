// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "backend/GraphSource.hpp"

#include <mutex>
#include <thread>

struct _GMainContext;
struct _GMainLoop;
struct _WpCore;
struct _WpObjectManager;

namespace wirerunner {

class WirePlumberGraphSource final : public GraphSource {
public:
  ~WirePlumberGraphSource() override;
  void start(SnapshotCallback snapshot, StatusCallback status) override;
  void stop() override;

private:
  void run(std::stop_token token);
  void publish();
  GraphSnapshot snapshot() const;

  SnapshotCallback snapshotCallback_;
  StatusCallback statusCallback_;
  std::jthread thread_;
  mutable std::mutex mutex_;
  _GMainContext *context_{};
  _GMainLoop *loop_{};
  _WpCore *core_{};
  _WpObjectManager *manager_{};
};

} // namespace wirerunner
