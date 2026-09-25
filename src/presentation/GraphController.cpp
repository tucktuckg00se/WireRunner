// SPDX-License-Identifier: GPL-3.0-or-later
#include "presentation/GraphController.hpp"

#include <QMetaObject>
#include <QThread>
#include <algorithm>
#include <deque>
#include <map>
#include <unordered_map>

namespace wirerunner {
namespace {
constexpr double cardWidth = 272.0;
constexpr double cardTop = 100.0;
constexpr double portStep = 28.0;

QString text(std::string_view value) {
  return QString::fromLatin1(value.data(), static_cast<qsizetype>(value.size()));
}
QString qtext(const std::string &value) { return QString::fromStdString(value); }
QString portKey(GlobalId id) { return QStringLiteral("port:%1").arg(id); }
QString groupKey(const QString &card, PortDirection direction, MediaType media) {
  return QStringLiteral("group:%1:%2:%3").arg(card,
    direction == PortDirection::Input ? QStringLiteral("input") : QStringLiteral("output"),
    text(mediaTypeName(media)));
}

QVariantMap portMap(const GraphPort &port, const GraphSnapshot &snapshot) {
  const auto node = std::ranges::find(snapshot.nodes, port.nodeId, &GraphNode::id);
  return {{QStringLiteral("key"), portKey(port.id)}, {QStringLiteral("id"), port.id},
    {QStringLiteral("nodeId"), port.nodeId},
    {QStringLiteral("nodeName"), node == snapshot.nodes.end() ? QString{} : qtext(node->name)},
    {QStringLiteral("name"), qtext(port.name)}, {QStringLiteral("channel"), qtext(port.channel)},
    {QStringLiteral("direction"), port.direction == PortDirection::Input ? QStringLiteral("input") : QStringLiteral("output")},
    {QStringLiteral("media"), text(mediaTypeName(port.media))}, {QStringLiteral("format"), qtext(port.format)}};
}

QVariantList portGroups(const GraphCard &card, PortDirection direction) {
  const auto &ports = direction == PortDirection::Input ? card.inputs : card.outputs;
  std::map<MediaType, int> counts;
  for (const auto &port : ports) ++counts[port.media];
  QVariantList result;
  for (const auto &[media, count] : counts) {
    const auto mediaName = text(mediaTypeName(media));
    result.push_back(QVariantMap{{QStringLiteral("key"), groupKey(qtext(card.key), direction, media)},
      {QStringLiteral("media"), mediaName}, {QStringLiteral("count"), count},
      {QStringLiteral("label"), count == 1 ? mediaName : QStringLiteral("%1 %2 ports").arg(count).arg(mediaName)}});
  }
  return result;
}

QString primaryMedia(const GraphCard &card) {
  if (!card.outputs.empty()) return text(mediaTypeName(card.outputs.front().media));
  if (!card.inputs.empty()) return text(mediaTypeName(card.inputs.front().media));
  return QStringLiteral("unknown");
}
} // namespace

GraphController::GraphController(std::unique_ptr<GraphSource> source, QString layoutPath, QObject *parent)
  : QObject(parent), source_(std::move(source)), layoutStore_(std::move(layoutPath)), cards_(this), links_(this) {}

GraphController::~GraphController() { source_->stop(); }

void GraphController::start() {
  source_->start(
    [this](std::shared_ptr<const GraphSnapshot> snapshot) {
      if (QThread::currentThread() == thread()) { applySnapshot(std::move(snapshot)); return; }
      QMetaObject::invokeMethod(this, [this, snapshot = std::move(snapshot)] { applySnapshot(snapshot); }, Qt::QueuedConnection);
    },
    [this](SourceStatus status) {
      if (QThread::currentThread() == thread()) { applyStatus(std::move(status)); return; }
      QMetaObject::invokeMethod(this, [this, status = std::move(status)] { applyStatus(status); }, Qt::QueuedConnection);
    });
}

void GraphController::applySnapshot(std::shared_ptr<const GraphSnapshot> snapshot) {
  snapshot_ = std::move(snapshot);
  composed_ = composeGraph(*snapshot_);
  const auto nextRemote = qtext(snapshot_->remoteName.empty() ? std::string("pipewire-0") : snapshot_->remoteName);
  if (!remoteName_.isEmpty() && remoteName_ != nextRemote) {
    cardStates_.clear(); focusCards_.clear(); clearSelection();
  }
  remoteName_ = nextRemote;
  const auto automatic = layoutGraph(composed_, snapshot_->links);
  QSet<QString> present;
  for (const auto &card : composed_.cards) {
    const auto key = qtext(card.key);
    present.insert(key);
    if (cardStates_.contains(key)) continue;
    CardState state;
    state.persistent = card.persistent;
    if (card.persistent) {
      if (const auto saved = layoutStore_.card(remoteName_, key)) {
        state.position = saved->position;
        state.expanded = saved->expanded;
      } else if (const auto position = automatic.find(card.key); position != automatic.end()) {
        state.position = {position->second.x, position->second.y};
      }
    } else if (const auto position = automatic.find(card.key); position != automatic.end()) {
      state.position = {position->second.x, position->second.y};
    }
    cardStates_.insert(key, state);
  }
  if (!selectedKey_.isEmpty() && !present.contains(selectedKey_)) clearSelection();
  for (auto it = focusCards_.begin(); it != focusCards_.end();) {
    if (!present.contains(*it)) it = focusCards_.erase(it); else ++it;
  }
  remoteSummary_ = qtext(snapshot_->remoteName + "  /  " + snapshot_->remoteVersion);
  rebuildPresentation();
}

void GraphController::applyStatus(SourceStatus status) {
  statusText_ = qtext(status.message);
  connected_ = status.state == SourceState::Ready;
  emit statusChanged();
}

void GraphController::rebuildPresentation() {
  if (!snapshot_) return;
  QList<QVariantMap> cards;
  QVariantList anchors;
  QVariantList cardRects;
  QHash<GlobalId, QString> nodeCards;
  QHash<QString, QVariantMap> cardMaps;
  double maximumX = 1000.0;
  double maximumY = 650.0;
  for (const auto &[nodeId, key] : composed_.nodeCards) nodeCards.insert(nodeId, qtext(key));

  for (const auto &card : composed_.cards) {
    const auto key = qtext(card.key);
    const auto state = cardStates_.value(key);
    QVariantList inputs;
    QVariantList outputs;
    for (const auto &port : card.inputs) inputs.push_back(portMap(port, *snapshot_));
    for (const auto &port : card.outputs) outputs.push_back(portMap(port, *snapshot_));
    const auto inputGroups = portGroups(card, PortDirection::Input);
    const auto outputGroups = portGroups(card, PortDirection::Output);
    const auto visibleRows = state.expanded ? std::max(inputs.size(), outputs.size())
                                            : std::max(inputGroups.size(), outputGroups.size());
    const double height = std::max(126.0, cardTop + 18.0 + static_cast<double>(visibleRows) * portStep);
    QVariantList members;
    QStringList technicalNames;
    QString stateLabel = QStringLiteral("idle");
    QStringList mediaTypes;
    int connectionCount = 0;
    for (const auto nodeId : card.nodeIds) {
      const auto node = std::ranges::find(snapshot_->nodes, nodeId, &GraphNode::id);
      if (node == snapshot_->nodes.end()) continue;
      if (node->state == "running") stateLabel = QStringLiteral("running");
      const auto media = text(mediaTypeName(node->media));
      if (!mediaTypes.contains(media)) mediaTypes.push_back(media);
      technicalNames.push_back(qtext(node->technicalName));
      members.push_back(QVariantMap{{QStringLiteral("id"), node->id}, {QStringLiteral("name"), qtext(node->name)},
        {QStringLiteral("technicalName"), qtext(node->technicalName)}, {QStringLiteral("stableId"), qtext(node->stableId)},
        {QStringLiteral("mediaClass"), qtext(node->mediaClass)}, {QStringLiteral("state"), qtext(node->state)},
        {QStringLiteral("media"), media}, {QStringLiteral("role"), text(nodeRoleName(node->role))}});
    }
    for (const auto &link : snapshot_->links) {
      if (std::ranges::find(card.nodeIds, link.outputNodeId) != card.nodeIds.end() ||
          std::ranges::find(card.nodeIds, link.inputNodeId) != card.nodeIds.end()) ++connectionCount;
    }
    const bool visible = mediaFilter_ == QStringLiteral("all") || mediaTypes.contains(mediaFilter_);
    QVariantMap item{{QStringLiteral("key"), key}, {QStringLiteral("title"), qtext(card.title)},
      {QStringLiteral("subtitle"), qtext(card.subtitle)}, {QStringLiteral("kind"), qtext(card.kind)},
      {QStringLiteral("persistent"), card.persistent}, {QStringLiteral("x"), state.position.x()},
      {QStringLiteral("y"), state.position.y()}, {QStringLiteral("width"), cardWidth},
      {QStringLiteral("height"), height}, {QStringLiteral("expanded"), state.expanded},
      {QStringLiteral("media"), primaryMedia(card)}, {QStringLiteral("mediaTypes"), mediaTypes},
      {QStringLiteral("state"), stateLabel}, {QStringLiteral("members"), members},
      {QStringLiteral("technicalNames"), technicalNames}, {QStringLiteral("inputs"), inputs},
      {QStringLiteral("outputs"), outputs}, {QStringLiteral("inputGroups"), inputGroups},
      {QStringLiteral("outputGroups"), outputGroups}, {QStringLiteral("nodeCount"), static_cast<int>(card.nodeIds.size())},
      {QStringLiteral("inputCount"), static_cast<int>(card.inputs.size())},
      {QStringLiteral("outputCount"), static_cast<int>(card.outputs.size())}, {QStringLiteral("visible"), visible},
      {QStringLiteral("connectionCount"), connectionCount},
      {QStringLiteral("focused"), focusCards_.isEmpty() || focusCards_.contains(key)}};
    cards.push_back(item);
    cardMaps.insert(key, item);
    if (visible) cardRects.push_back(QVariantMap{{QStringLiteral("x"), state.position.x()},
      {QStringLiteral("y"), state.position.y()}, {QStringLiteral("width"), cardWidth},
      {QStringLiteral("height"), height}});
    maximumX = std::max(maximumX, state.position.x() + cardWidth + 80.0);
    maximumY = std::max(maximumY, state.position.y() + height + 80.0);

    const auto addAnchors = [&](const QVariantList &ports, PortDirection direction) {
      for (qsizetype row = 0; row < ports.size(); ++row) {
        const auto port = ports.at(row).toMap();
        anchors.push_back(QVariantMap{{QStringLiteral("key"), port.value(QStringLiteral("key"))},
          {QStringLiteral("x"), state.position.x() + (direction == PortDirection::Input ? 0.0 : cardWidth)},
          {QStringLiteral("y"), state.position.y() + cardTop + 9.0 + static_cast<double>(row) * portStep}});
      }
    };
    const auto addGroupAnchors = [&](const QVariantList &groups, PortDirection direction) {
      for (qsizetype row = 0; row < groups.size(); ++row) {
        const auto group = groups.at(row).toMap();
        anchors.push_back(QVariantMap{{QStringLiteral("key"), group.value(QStringLiteral("key"))},
          {QStringLiteral("x"), state.position.x() + (direction == PortDirection::Input ? 0.0 : cardWidth)},
          {QStringLiteral("y"), state.position.y() + cardTop + 9.0 + static_cast<double>(row) * portStep}});
      }
    };
    if (state.expanded) {
      addAnchors(inputs, PortDirection::Input); addAnchors(outputs, PortDirection::Output);
    } else {
      addGroupAnchors(inputGroups, PortDirection::Input); addGroupAnchors(outputGroups, PortDirection::Output);
    }
  }

  std::unordered_map<GlobalId, const GraphPort *> ports;
  for (const auto &port : snapshot_->ports) ports[port.id] = &port;
  QList<QVariantMap> links;
  for (const auto &link : snapshot_->links) {
    if (!nodeCards.contains(link.outputNodeId) || !nodeCards.contains(link.inputNodeId)) continue;
    const auto outputCard = nodeCards.value(link.outputNodeId);
    const auto inputCard = nodeCards.value(link.inputNodeId);
    const auto outputPort = ports.contains(link.outputPortId) ? ports.at(link.outputPortId) : nullptr;
    const auto inputPort = ports.contains(link.inputPortId) ? ports.at(link.inputPortId) : nullptr;
    const auto outputMedia = outputPort ? outputPort->media : link.media;
    const auto inputMedia = inputPort ? inputPort->media : link.media;
    QVariantMap item{{QStringLiteral("key"), QStringLiteral("link:%1").arg(link.id)}, {QStringLiteral("id"), link.id},
      {QStringLiteral("outputCardKey"), outputCard}, {QStringLiteral("inputCardKey"), inputCard},
      {QStringLiteral("outputNodeId"), link.outputNodeId}, {QStringLiteral("inputNodeId"), link.inputNodeId},
      {QStringLiteral("outputAnchorKey"), cardStates_.value(outputCard).expanded ? portKey(link.outputPortId)
        : groupKey(outputCard, PortDirection::Output, outputMedia)},
      {QStringLiteral("inputAnchorKey"), cardStates_.value(inputCard).expanded ? portKey(link.inputPortId)
        : groupKey(inputCard, PortDirection::Input, inputMedia)},
      {QStringLiteral("fromName"), cardMaps.value(outputCard).value(QStringLiteral("title"))},
      {QStringLiteral("toName"), cardMaps.value(inputCard).value(QStringLiteral("title"))},
      {QStringLiteral("outputPortName"), outputPort ? qtext(outputPort->name) : QString{}},
      {QStringLiteral("inputPortName"), inputPort ? qtext(inputPort->name) : QString{}},
      {QStringLiteral("media"), text(mediaTypeName(link.media))}, {QStringLiteral("state"), qtext(link.state)},
      {QStringLiteral("focused"), focusCards_.isEmpty() ||
        (focusCards_.contains(outputCard) && focusCards_.contains(inputCard))}};
    links.push_back(item);
  }

  cards_.setItems(std::move(cards));
  renderedLinks_.clear();
  renderedLinks_.reserve(links.size());
  for (const auto &link : links) renderedLinks_.push_back(link);
  links_.setItems(std::move(links));
  anchors_ = std::move(anchors);
  cardRects_ = std::move(cardRects);
  canvasWidth_ = maximumX;
  canvasHeight_ = maximumY;
  updateSelection();
  emit graphChanged();
}

void GraphController::setMediaFilter(QString value) {
  if (value == mediaFilter_) return;
  mediaFilter_ = std::move(value);
  emit mediaFilterChanged();
  rebuildPresentation();
}

void GraphController::moveCard(const QString &key, double x, double y) {
  auto found = cardStates_.find(key);
  if (found == cardStates_.end()) return;
  found->position = {std::max(20.0, x), std::max(20.0, y)};
  saveCard(key);
  rebuildPresentation();
}

void GraphController::toggleCard(const QString &key) {
  auto found = cardStates_.find(key);
  if (found == cardStates_.end()) return;
  found->expanded = !found->expanded;
  saveCard(key);
  rebuildPresentation();
}

void GraphController::selectCard(const QString &key) {
  selectedKind_ = QStringLiteral("card"); selectedKey_ = key; updateSelection();
}
void GraphController::selectLink(const QString &key) {
  selectedKind_ = QStringLiteral("link"); selectedKey_ = key; updateSelection();
}
void GraphController::clearSelection() {
  selectedKind_.clear(); selectedKey_.clear(); selected_.clear(); emit selectionChanged();
}

void GraphController::updateSelection() {
  QVariantMap next;
  auto *model = selectedKind_ == QStringLiteral("card") ? &cards_ : &links_;
  for (int row = 0; row < model->count(); ++row) {
    const auto item = model->get(row);
    if (item.value(QStringLiteral("key")).toString() == selectedKey_) {
      next = item; next.insert(QStringLiteral("selectionKind"), selectedKind_); break;
    }
  }
  if (next != selected_) {
    selected_ = std::move(next);
    if (selected_.isEmpty()) { selectedKind_.clear(); selectedKey_.clear(); }
    emit selectionChanged();
  }
}

void GraphController::focusSelected() {
  if (!snapshot_ || selectedKey_.isEmpty()) return;
  QSet<GlobalId> seeds;
  if (selectedKind_ == QStringLiteral("card")) {
    const auto card = std::ranges::find(composed_.cards, selectedKey_.toStdString(), &GraphCard::key);
    if (card != composed_.cards.end()) for (const auto nodeId : card->nodeIds) seeds.insert(nodeId);
  } else for (int row = 0; row < links_.count(); ++row) {
    const auto link = links_.get(row);
    if (link.value(QStringLiteral("key")).toString() == selectedKey_) {
      seeds.insert(link.value(QStringLiteral("outputNodeId")).toUInt());
      seeds.insert(link.value(QStringLiteral("inputNodeId")).toUInt());
    }
  }
  if (seeds.isEmpty()) return;
  focusCards_.clear();
  QHash<GlobalId, QSet<GlobalId>> adjacency;
  for (const auto &link : snapshot_->links) {
    const auto output = link.outputNodeId;
    const auto input = link.inputNodeId;
    adjacency[output].insert(input); adjacency[input].insert(output);
  }
  QSet<GlobalId> visited;
  std::deque<GlobalId> pending;
  for (const auto seed : seeds) { visited.insert(seed); pending.push_back(seed); }
  while (!pending.empty()) {
    const auto current = pending.front(); pending.pop_front();
    for (const auto next : adjacency.value(current)) if (!visited.contains(next)) {
      visited.insert(next); pending.push_back(next);
    }
  }
  for (const auto nodeId : visited) {
    const auto card = composed_.nodeCards.find(nodeId);
    if (card != composed_.nodeCards.end()) focusCards_.insert(qtext(card->second));
  }
  emit focusChanged(); rebuildPresentation();
}

void GraphController::clearFocus() {
  if (focusCards_.isEmpty()) return;
  focusCards_.clear(); emit focusChanged(); rebuildPresentation();
}

QVariantMap GraphController::findCard(const QString &query) {
  const auto term = query.trimmed();
  if (term.isEmpty()) return {};
  for (int row = 0; row < cards_.count(); ++row) {
    const auto item = cards_.get(row);
    const auto searchable = item.value(QStringLiteral("title")).toString() + QLatin1Char(' ') +
      item.value(QStringLiteral("subtitle")).toString() + QLatin1Char(' ') +
      item.value(QStringLiteral("technicalNames")).toStringList().join(QLatin1Char(' '));
    if (!searchable.contains(term, Qt::CaseInsensitive)) continue;
    selectCard(item.value(QStringLiteral("key")).toString());
    return item;
  }
  statusText_ = QStringLiteral("No object matches “%1”").arg(term); emit statusChanged(); return {};
}

void GraphController::saveCard(const QString &key) {
  const auto state = cardStates_.value(key);
  if (!state.persistent) return;
  layoutStore_.setCard(remoteName_, key, {state.position, state.expanded});
  QString error;
  if (!layoutStore_.save(&error)) {
    statusText_ = QStringLiteral("Could not save layout: %1").arg(error); emit statusChanged();
  }
}

} // namespace wirerunner
