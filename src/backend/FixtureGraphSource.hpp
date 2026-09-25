// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "backend/GraphSource.hpp"
#include <QString>

namespace wirerunner {
class FixtureGraphSource final : public GraphSource {
public:
  explicit FixtureGraphSource(QString path);
  void start(SnapshotCallback snapshot, StatusCallback status) override;
  void stop() override {}
  static GraphSnapshot load(const QString &path);
private:
  QString path_;
};
}
