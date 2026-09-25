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
  void createLink(CreateLinkRequest request, CommandCallback callback) override;
  void destroyLink(DestroyLinkRequest request, CommandCallback callback) override;

private:
  void run(std::stop_token token);
  void publish();
  GraphSnapshot snapshot();
  void invoke(std::function<void()> task);

  SnapshotCallback snapshotCallback_;
  StatusCallback statusCallback_;
  std::jthread thread_;
  mutable std::mutex mutex_;
  _GMainContext *context_{};
  _GMainLoop *loop_{};
  _WpCore *core_{};
  _WpObjectManager *manager_{};
  std::uint64_t revision_{};
};

} // namespace wirerunner
