// SPDX-License-Identifier: GPL-3.0-or-later
#include "presentation/GraphController.hpp"

#include <QMetaObject>
#include <QVariantMap>
#include <unordered_map>

namespace wirerunner {
namespace {
QString text(std::string_view value) {
  return QString::fromLatin1(value.data(), static_cast<qsizetype>(value.size()));
}
} // namespace

GraphController::GraphController(std::unique_ptr<GraphSource> source, QObject *parent)
  : QObject(parent), source_(std::move(source)) {}

GraphController::~GraphController() { source_->stop(); }

void GraphController::start() {
  source_->start(
    [this](std::shared_ptr<const GraphSnapshot> snapshot) {
      QMetaObject::invokeMethod(this, [this, snapshot = std::move(snapshot)] { applySnapshot(snapshot); }, Qt::QueuedConnection);
    },
    [this](SourceStatus status) {
      QMetaObject::invokeMethod(this, [this, status = std::move(status)] { applyStatus(status); }, Qt::QueuedConnection);
    });
}

void GraphController::applySnapshot(std::shared_ptr<const GraphSnapshot> snapshot) {
  QVariantList nodes;
  std::unordered_map<GlobalId, std::string> names;
  for (const auto &node : snapshot->nodes) {
    names[node.id] = node.name;
    QVariantMap item;
    item["id"] = node.id;
    item["name"] = QString::fromStdString(node.name);
    item["technicalName"] = QString::fromStdString(node.technicalName);
    item["stableId"] = QString::fromStdString(node.stableId);
    item["mediaClass"] = QString::fromStdString(node.mediaClass);
    item["media"] = text(mediaTypeName(node.media));
    item["role"] = text(nodeRoleName(node.role));
    item["state"] = QString::fromStdString(node.state);
    item["x"] = node.x;
    item["y"] = node.y;
    nodes.push_back(item);
  }
  QVariantList links;
  for (const auto &link : snapshot->links) {
    QVariantMap item;
    item["id"] = link.id;
    item["outputNodeId"] = link.outputNodeId;
    item["inputNodeId"] = link.inputNodeId;
    item["fromName"] = QString::fromStdString(names[link.outputNodeId]);
    item["toName"] = QString::fromStdString(names[link.inputNodeId]);
    item["media"] = text(mediaTypeName(link.media));
    item["state"] = QString::fromStdString(link.state);
    links.push_back(item);
  }
  nodes_ = std::move(nodes);
  links_ = std::move(links);
  remoteSummary_ = QString::fromStdString(snapshot->remoteName + " · " + snapshot->remoteVersion);
  emit graphChanged();
}

void GraphController::applyStatus(SourceStatus status) {
  statusText_ = QString::fromStdString(status.message);
  connected_ = status.state == SourceState::Ready;
  emit statusChanged();
}
} // namespace wirerunner
