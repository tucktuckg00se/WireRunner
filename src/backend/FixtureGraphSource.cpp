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
  out.remoteName = text(root, "remoteName");
  out.remoteVersion = text(root, "remoteVersion");
  for (const auto value : root.value("nodes").toArray()) {
    const auto o = value.toObject();
    out.nodes.push_back({.id = id(o,"id"), .name = text(o,"name"),
      .technicalName = text(o,"technicalName"), .stableId = text(o,"stableId"),
      .mediaClass = text(o,"mediaClass"), .state = text(o,"state"), .media = media(o),
      .role = NodeRole::Processor, .clientId = std::nullopt, .deviceId = std::nullopt,
      .x = 0.0, .y = 0.0});
  }
  for (const auto value : root.value("ports").toArray()) {
    const auto o = value.toObject();
    out.ports.push_back({id(o,"id"), id(o,"nodeId"), text(o,"name"), text(o,"channel"),
      text(o,"direction") == "output" ? PortDirection::Output : PortDirection::Input, media(o)});
  }
  for (const auto value : root.value("links").toArray()) {
    const auto o = value.toObject();
    out.links.push_back({id(o,"id"), id(o,"outputNodeId"), id(o,"outputPortId"),
      id(o,"inputNodeId"), id(o,"inputPortId"), text(o,"state"), media(o)});
  }
  layoutGraph(out);
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
} // namespace wirerunner
