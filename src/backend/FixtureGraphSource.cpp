// SPDX-License-Identifier: GPL-3.0-or-later
#include "backend/FixtureGraphSource.hpp"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <stdexcept>

namespace wirerunner {
namespace {
std::string text(const QJsonObject &o, const char *key) { return o.value(key).toString().toStdString(); }
GlobalId id(const QJsonObject &o, const char *key) { return static_cast<GlobalId>(o.value(key).toInteger()); }
MediaType media(const QJsonObject &o) { return classifyMedia(text(o, "media")); }
std::optional<GlobalId> optionalId(const QJsonObject &o, const char *key) {
  return o.contains(key) ? std::optional{id(o, key)} : std::nullopt;
}
std::optional<NodeAudioControl> audioControl(const QJsonObject &o) {
  if (!o.value("audio").isObject()) return std::nullopt;
  const auto audio = o.value("audio").toObject();
  NodeAudioControl result;
  result.volume = static_cast<float>(audio.value("volume").toDouble(1.0));
  result.minimumVolume = static_cast<float>(audio.value("minimumVolume").toDouble(0.0));
  result.maximumVolume = static_cast<float>(audio.value("maximumVolume").toDouble(1.0));
  result.muted = audio.value("muted").toBool();
  result.hasVolume = audio.value("hasVolume").toBool(true);
  result.hasMute = audio.value("hasMute").toBool(true);
  result.writable = audio.value("writable").toBool(false);
  for (const auto value : audio.value("channelVolumes").toArray())
    result.channelVolumes.push_back(static_cast<float>(value.toDouble()));
  for (const auto value : audio.value("channelMap").toArray())
    result.channelMap.push_back(static_cast<std::uint32_t>(value.toInteger()));
  for (const auto value : audio.value("softVolumes").toArray())
    result.softVolumes.push_back(static_cast<float>(value.toDouble()));
  return result;
}

DeviceRoute deviceRoute(const QJsonObject &o) {
  return {.index = o.value("index").toInt(-1), .deviceIndex = o.value("deviceIndex").toInt(-1),
    .direction = o.value("direction").toString() == QStringLiteral("input")
      ? PortDirection::Input : PortDirection::Output,
    .name = text(o, "name"), .description = text(o, "description"),
    .audio = audioControl(QJsonObject{{QStringLiteral("audio"), o.value("audio")}}).value_or(NodeAudioControl{})};
}
}

FixtureGraphSource::FixtureGraphSource(QString path) : path_(std::move(path)) {}

GraphSnapshot FixtureGraphSource::load(const QString &path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) throw std::runtime_error("Cannot open fixture: " + path.toStdString());
  QJsonParseError error;
  const auto document = QJsonDocument::fromJson(file.readAll(), &error);
  if (error.error != QJsonParseError::NoError || !document.isObject())
    throw std::runtime_error("Invalid fixture JSON: " + error.errorString().toStdString());
  const auto root = document.object();
  if (root.value("version").toInt() != 1) throw std::runtime_error("Unsupported fixture version");

  GraphSnapshot out;
  out.revision = 1;
  out.remoteName = text(root, "remoteName");
  out.remoteVersion = text(root, "remoteVersion");
  for (const auto value : root.value("clients").toArray()) {
    const auto o = value.toObject();
    out.clients.push_back({id(o,"id"), text(o,"name"), text(o,"stableId")});
  }
  for (const auto value : root.value("devices").toArray()) {
    const auto o = value.toObject();
    GraphDevice device{.id = id(o,"id"), .name = text(o,"name"), .stableId = text(o,"stableId"),
      .mediaClass = text(o,"mediaClass"), .routes = {}};
    for (const auto route : o.value("routes").toArray()) device.routes.push_back(deviceRoute(route.toObject()));
    out.devices.push_back(std::move(device));
  }
  for (const auto value : root.value("nodes").toArray()) {
    const auto o = value.toObject();
    out.nodes.push_back({.id = id(o,"id"), .name = text(o,"name"),
      .technicalName = text(o,"technicalName"), .stableId = text(o,"stableId"),
      .mediaClass = text(o,"mediaClass"), .state = text(o,"state"), .media = media(o),
      .role = NodeRole::Processor, .clientId = optionalId(o,"clientId"),
      .deviceId = optionalId(o,"deviceId"),
      .profileDeviceId = o.contains("profileDeviceId") ? std::optional{o.value("profileDeviceId").toInt()} : std::nullopt,
      .audio = audioControl(o)});
  }
  for (const auto value : root.value("ports").toArray()) {
    const auto o = value.toObject();
    out.ports.push_back({id(o,"id"), id(o,"nodeId"), text(o,"name"), text(o,"channel"),
      text(o,"direction") == "output" ? PortDirection::Output : PortDirection::Input, media(o), text(o,"format")});
  }
  for (const auto value : root.value("links").toArray()) {
    const auto o = value.toObject();
    out.links.push_back({id(o,"id"), id(o,"outputNodeId"), id(o,"outputPortId"),
      id(o,"inputNodeId"), id(o,"inputPortId"), text(o,"state"), media(o)});
  }
  classifyNodeRoles(out);
  return out;
}

void FixtureGraphSource::start(SnapshotCallback snapshot, StatusCallback status) {
  try {
    auto graph = std::make_shared<GraphSnapshot>(load(path_));
    snapshot(std::move(graph));
    status({SourceState::Ready, "Demonstration graph"});
  } catch (const std::exception &error) {
    status({SourceState::Error, error.what()});
  }
}

void FixtureGraphSource::createLink(CreateLinkRequest request, CommandCallback callback) {
  callback({request.commandId, false, "The demonstration graph is read-only"});
}

void FixtureGraphSource::destroyLink(DestroyLinkRequest request, CommandCallback callback) {
  callback({request.commandId, false, "The demonstration graph is read-only"});
}

void FixtureGraphSource::setNodeAudio(SetNodeAudioRequest request, CommandCallback callback) {
  callback({request.commandId, false, "The demonstration graph is read-only"});
}
void FixtureGraphSource::setDeviceRouteAudio(SetDeviceRouteAudioRequest request, CommandCallback callback) {
  callback({request.commandId, false, "The demonstration graph is read-only"});
}
} // namespace wirerunner
