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
  void createLink(CreateLinkRequest request, CommandCallback callback) override;
  void destroyLink(DestroyLinkRequest request, CommandCallback callback) override;
  void setNodeAudio(SetNodeAudioRequest request, CommandCallback callback) override;
  void setDeviceRouteAudio(SetDeviceRouteAudioRequest request, CommandCallback callback) override;
  void setDefault(SetDefaultRequest request, CommandCallback callback) override;
  void setDeviceProfile(SetDeviceProfileRequest request, CommandCallback callback) override;
  void setDeviceRoute(SetDeviceRouteRequest request, CommandCallback callback) override;
  static GraphSnapshot load(const QString &path);
private:
  QString path_;
};
}
