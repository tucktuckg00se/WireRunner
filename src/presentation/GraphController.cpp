// SPDX-License-Identifier: GPL-3.0-or-later
#include "presentation/GraphController.hpp"
#include "domain/Routing.hpp"

#include <QDateTime>
#include <QLineF>
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
constexpr std::uint32_t executePermission = 0100;

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
  std::map<MediaType, std::vector<const GraphPort *>> grouped;
  for (const auto &port : ports) grouped[port.media].push_back(&port);
  QVariantList result;
  for (const auto &[media, members] : grouped) {
    const auto count = static_cast<int>(members.size());
    const auto mediaName = text(mediaTypeName(media));
    const auto key = count == 1 ? portKey(members.front()->id) : groupKey(qtext(card.key), direction, media);
    result.push_back(QVariantMap{{QStringLiteral("key"), key},
      {QStringLiteral("media"), mediaName}, {QStringLiteral("count"), count},
      {QStringLiteral("id"), count == 1 ? QVariant::fromValue(members.front()->id) : QVariant{}},
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
  : QObject(parent), source_(std::move(source)), layoutStore_(std::move(layoutPath)), cards_(this), links_(this) {
  commandTimer_.setSingleShot(true);
  commandTimer_.setInterval(4000);
  connect(&commandTimer_, &QTimer::timeout, this, [this] {
    if (pending_) failPending(QStringLiteral("PipeWire did not confirm the graph change"));
  });
}

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
  const auto previousRemote = remoteName_;
  snapshot_ = std::move(snapshot);
  composed_ = composeGraph(*snapshot_);
  const auto nextRemote = qtext(snapshot_->remoteName.empty() ? std::string("pipewire-0") : snapshot_->remoteName);
  if (!remoteName_.isEmpty() && remoteName_ != nextRemote) {
    cardStates_.clear(); focusCards_.clear(); clearSelection(); cancelRoute();
    pending_.reset(); history_.clear(); historyCursor_ = 0;
    emit historyChanged(); emit commandPendingChanged();
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
  resolvePending();

  if (!previousRemote.isEmpty() && previousRemote == nextRemote && !history_.isEmpty() &&
      historyCursor_ == history_.size()) {
    const auto &last = history_.back();
    if (last.kind == HistoryKind::Destroyed &&
        QDateTime::currentMSecsSinceEpoch() - last.recordedAt < 2500 &&
        std::ranges::any_of(snapshot_->links, [&](const GraphLink &link) {
          return link.id != last.link.id && link.outputPortId == last.link.outputPortId &&
            link.inputPortId == last.link.inputPortId;
        })) {
      history_.removeLast();
      historyCursor_ = std::min(historyCursor_, history_.size());
      setNotice(QStringLiteral("This route was restored by session policy or another client."));
      emit historyChanged();
    }
  }
  rebuildPresentation();
}

void GraphController::applyStatus(SourceStatus status) {
  statusText_ = qtext(status.message);
  connected_ = status.state == SourceState::Ready;
  if (!connected_) {
    cancelRoute();
    if (pending_) failPending(QStringLiteral("The PipeWire connection changed before the edit completed"));
  }
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
          {QStringLiteral("y"), state.position.y() + cardTop + 9.0 + static_cast<double>(row) * portStep},
          {QStringLiteral("cardKey"), key}, {QStringLiteral("direction"), port.value(QStringLiteral("direction"))},
          {QStringLiteral("media"), port.value(QStringLiteral("media"))}, {QStringLiteral("count"), 1},
          {QStringLiteral("portId"), port.value(QStringLiteral("id"))}});
      }
    };
    const auto addGroupAnchors = [&](const QVariantList &groups, PortDirection direction) {
      for (qsizetype row = 0; row < groups.size(); ++row) {
        const auto group = groups.at(row).toMap();
        anchors.push_back(QVariantMap{{QStringLiteral("key"), group.value(QStringLiteral("key"))},
          {QStringLiteral("x"), state.position.x() + (direction == PortDirection::Input ? 0.0 : cardWidth)},
          {QStringLiteral("y"), state.position.y() + cardTop + 9.0 + static_cast<double>(row) * portStep},
          {QStringLiteral("cardKey"), key},
          {QStringLiteral("direction"), direction == PortDirection::Input ? QStringLiteral("input") : QStringLiteral("output")},
          {QStringLiteral("media"), group.value(QStringLiteral("media"))},
          {QStringLiteral("count"), group.value(QStringLiteral("count"))},
          {QStringLiteral("portId"), group.value(QStringLiteral("id"))}});
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
  const auto anchorKey = [&](const QString &cardKey, PortDirection direction, const GraphPort *port,
                             GlobalId portId, MediaType media) {
    if (cardStates_.value(cardKey).expanded) return portKey(portId);
    const auto card = std::ranges::find(composed_.cards, cardKey.toStdString(), &GraphCard::key);
    if (card != composed_.cards.end()) {
      const auto &members = direction == PortDirection::Input ? card->inputs : card->outputs;
      const auto count = std::ranges::count(members, media, &GraphPort::media);
      if (count == 1) return portKey(portId);
    }
    return groupKey(cardKey, direction, port ? port->media : media);
  };
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
      {QStringLiteral("outputAnchorKey"), anchorKey(outputCard, PortDirection::Output, outputPort, link.outputPortId, outputMedia)},
      {QStringLiteral("inputAnchorKey"), anchorKey(inputCard, PortDirection::Input, inputPort, link.inputPortId, inputMedia)},
      {QStringLiteral("fromName"), cardMaps.value(outputCard).value(QStringLiteral("title"))},
      {QStringLiteral("toName"), cardMaps.value(inputCard).value(QStringLiteral("title"))},
      {QStringLiteral("outputPortName"), outputPort ? qtext(outputPort->name) : QString{}},
      {QStringLiteral("inputPortName"), inputPort ? qtext(inputPort->name) : QString{}},
      {QStringLiteral("media"), text(mediaTypeName(link.media))}, {QStringLiteral("state"), qtext(link.state)},
      {QStringLiteral("feedback"), link.feedback}, {QStringLiteral("linger"), link.linger},
      {QStringLiteral("createdByWireRunner"), link.createdByWireRunner},
      {QStringLiteral("canDestroy"), (link.permissions & executePermission) != 0},
      {QStringLiteral("focused"), focusCards_.isEmpty() ||
        (focusCards_.contains(outputCard) && focusCards_.contains(inputCard))}};
    links.push_back(item);
  }

  if (pending_ && pending_->creating) {
    const auto &link = pending_->link;
    if (nodeCards.contains(link.outputNodeId) && nodeCards.contains(link.inputNodeId) &&
        ports.contains(link.outputPortId) && ports.contains(link.inputPortId)) {
      const auto outputCard = nodeCards.value(link.outputNodeId);
      const auto inputCard = nodeCards.value(link.inputNodeId);
      links.push_back(QVariantMap{{QStringLiteral("key"), QStringLiteral("pending:%1").arg(pending_->commandId)},
        {QStringLiteral("outputAnchorKey"), anchorKey(outputCard, PortDirection::Output, ports.at(link.outputPortId), link.outputPortId, link.media)},
        {QStringLiteral("inputAnchorKey"), anchorKey(inputCard, PortDirection::Input, ports.at(link.inputPortId), link.inputPortId, link.media)},
        {QStringLiteral("media"), text(mediaTypeName(link.media))}, {QStringLiteral("state"), QStringLiteral("pending")},
        {QStringLiteral("pending"), true}, {QStringLiteral("focused"), true}});
    }
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
  rebuildRouting();
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

bool GraphController::canUndo() const { return !pending_ && historyCursor_ > 0; }
bool GraphController::canRedo() const { return !pending_ && historyCursor_ < history_.size(); }

void GraphController::setNotice(QString message) {
  if (noticeText_ == message) return;
  noticeText_ = std::move(message);
  emit noticeChanged();
}

void GraphController::expandForRouting(const QString &cardKey) {
  auto found = cardStates_.find(cardKey);
  if (found == cardStates_.end() || found->expanded) return;
  found->expanded = true;
  saveCard(cardKey);
  setNotice(QStringLiteral("Choose an exact port to make this route."));
  rebuildPresentation();
}

void GraphController::beginRoute(quint32 outputPortId, double x, double y) {
  if (!snapshot_ || pending_) return;
  const auto *port = findPort(*snapshot_, outputPortId);
  if (!port || port->direction != PortDirection::Output) {
    setNotice(QStringLiteral("A route must start at an output port."));
    return;
  }
  routeOutputPort_ = outputPortId;
  routeTargetPort_.reset();
  routeCursor_ = {x, y};
  feedbackConfirmation_ = false;
  rebuildRouting();
}

void GraphController::updateRoute(double x, double y) {
  if (!snapshot_ || !routeOutputPort_ || feedbackConfirmation_) return;
  routeCursor_ = {x, y};
  routeTargetPort_.reset();
  QVariantMap closest;
  double best = 24.0;
  for (const auto &value : anchors_) {
    const auto anchor = value.toMap();
    if (anchor.value(QStringLiteral("direction")).toString() != QStringLiteral("input")) continue;
    const QLineF distance(routeCursor_, {anchor.value(QStringLiteral("x")).toDouble(),
      anchor.value(QStringLiteral("y")).toDouble()});
    if (distance.length() < best) { best = distance.length(); closest = anchor; }
  }
  if (!closest.isEmpty()) {
    if (closest.value(QStringLiteral("count")).toInt() > 1) {
      const auto cardKey = closest.value(QStringLiteral("cardKey")).toString();
      expandForRouting(cardKey);
      updateRoute(x, y);
      return;
    }
    const auto id = closest.value(QStringLiteral("portId")).toUInt();
    if (id != 0) routeTargetPort_ = id;
  }
  rebuildRouting();
}

void GraphController::finishRoute(double x, double y) {
  if (!routeOutputPort_ || feedbackConfirmation_) return;
  updateRoute(x, y);
  if (!routeTargetPort_) { cancelRoute(); return; }
  finishRouteToPort(*routeTargetPort_);
}

void GraphController::finishRouteToPort(quint32 inputPortId) {
  if (!snapshot_ || !routeOutputPort_ || pending_) return;
  routeTargetPort_ = inputPortId;
  const auto compatibility = assessLink(*snapshot_, *routeOutputPort_, inputPortId);
  if (!compatibility.compatible) {
    setNotice(qtext(compatibility.reason));
    rebuildRouting();
    return;
  }
  if (compatibility.probableCycle) {
    feedbackConfirmation_ = true;
    setNotice(QStringLiteral("This closes a media cycle. Confirm it as a feedback link."));
    rebuildRouting();
    emit feedbackConfirmationChanged();
    return;
  }
  const auto output = *routeOutputPort_;
  cancelRoute();
  submitCreate(output, inputPortId, false);
}

void GraphController::cancelRoute() {
  const bool routingChangedValue = routeOutputPort_.has_value() || routeTargetPort_.has_value();
  const bool confirmationChanged = feedbackConfirmation_;
  routeOutputPort_.reset(); routeTargetPort_.reset(); routing_.clear(); feedbackConfirmation_ = false;
  if (routingChangedValue) emit routingChanged();
  if (confirmationChanged) emit feedbackConfirmationChanged();
}

void GraphController::confirmFeedback() {
  if (!feedbackConfirmation_ || !routeOutputPort_ || !routeTargetPort_) return;
  const auto output = *routeOutputPort_;
  const auto input = *routeTargetPort_;
  cancelRoute();
  submitCreate(output, input, true);
}

void GraphController::cancelFeedback() { cancelRoute(); }

void GraphController::rebuildRouting() {
  QVariantMap next;
  if (snapshot_ && routeOutputPort_) {
    QVariantList compatibleInputs;
    QVariantMap incompatibleInputs;
    QVariantMap compatibleNotes;
    for (const auto &port : snapshot_->ports) {
      if (port.direction != PortDirection::Input) continue;
      const auto result = assessLink(*snapshot_, *routeOutputPort_, port.id);
      if (result.compatible) {
        compatibleInputs.push_back(port.id);
        if (!result.reason.empty()) compatibleNotes.insert(QString::number(port.id), qtext(result.reason));
      }
      else incompatibleInputs.insert(QString::number(port.id), qtext(result.reason));
    }
    QString reason;
    bool valid = false;
    bool cycle = false;
    if (routeTargetPort_) {
      const auto result = assessLink(*snapshot_, *routeOutputPort_, *routeTargetPort_);
      valid = result.compatible;
      cycle = result.probableCycle;
      reason = qtext(result.reason);
    }
    const auto *output = findPort(*snapshot_, *routeOutputPort_);
    next = {{QStringLiteral("active"), true}, {QStringLiteral("outputPortId"), *routeOutputPort_},
      {QStringLiteral("outputAnchorKey"), portKey(*routeOutputPort_)},
      {QStringLiteral("targetPortId"), routeTargetPort_.value_or(0)},
      {QStringLiteral("targetAnchorKey"), routeTargetPort_ ? portKey(*routeTargetPort_) : QString{}},
      {QStringLiteral("cursorX"), routeCursor_.x()}, {QStringLiteral("cursorY"), routeCursor_.y()},
      {QStringLiteral("media"), output ? text(mediaTypeName(output->media)) : QStringLiteral("unknown")},
      {QStringLiteral("compatibleInputs"), compatibleInputs},
      {QStringLiteral("incompatibleInputs"), incompatibleInputs},
      {QStringLiteral("compatibleNotes"), compatibleNotes}, {QStringLiteral("validTarget"), valid},
      {QStringLiteral("cycle"), cycle}, {QStringLiteral("reason"), reason}};
  }
  if (routing_ == next) return;
  routing_ = std::move(next);
  emit routingChanged();
}

void GraphController::submitCreate(GlobalId outputPortId, GlobalId inputPortId, bool feedback,
                                   OperationIntent intent) {
  if (!snapshot_ || pending_) return;
  const auto compatibility = assessLink(*snapshot_, outputPortId, inputPortId);
  if (!compatibility.compatible) { setNotice(qtext(compatibility.reason)); return; }
  const auto *output = findPort(*snapshot_, outputPortId);
  const auto *input = findPort(*snapshot_, inputPortId);
  if (!output || !input) return;
  GraphLink link{.outputNodeId = output->nodeId, .outputPortId = output->id,
    .inputNodeId = input->nodeId, .inputPortId = input->id, .state = "pending",
    .media = output->media, .feedback = feedback, .linger = true, .createdByWireRunner = true};
  const auto commandId = nextCommandId_++;
  pending_ = PendingOperation{commandId, true, link, intent};
  emit commandPendingChanged(); emit historyChanged();
  setNotice(feedback ? QStringLiteral("Creating feedback link…") : QStringLiteral("Creating link…"));
  commandTimer_.start();
  rebuildPresentation();
  CreateLinkRequest request{commandId, snapshot_->revision, output->nodeId, output->id,
    input->nodeId, input->id, feedback, true};
  source_->createLink(request, [this](CommandResult result) {
    QMetaObject::invokeMethod(this, [this, result = std::move(result)] { applyCommandResult(result); },
      Qt::QueuedConnection);
  });
}

void GraphController::submitDestroy(const GraphLink &link, OperationIntent intent) {
  if (!snapshot_ || pending_) return;
  if ((link.permissions & executePermission) == 0) {
    setNotice(QStringLiteral("PipeWire does not allow this link to be removed."));
    return;
  }
  const auto commandId = nextCommandId_++;
  pending_ = PendingOperation{commandId, false, link, intent};
  emit commandPendingChanged(); emit historyChanged();
  setNotice(QStringLiteral("Disconnecting link…"));
  commandTimer_.start();
  rebuildPresentation();
  source_->destroyLink({commandId, snapshot_->revision, link.id}, [this](CommandResult result) {
    QMetaObject::invokeMethod(this, [this, result = std::move(result)] { applyCommandResult(result); },
      Qt::QueuedConnection);
  });
}

void GraphController::applyCommandResult(CommandResult result) {
  if (!pending_ || pending_->commandId != result.commandId) return;
  if (!result.accepted) { failPending(qtext(result.message)); return; }
  setNotice(qtext(result.message));
}

void GraphController::resolvePending() {
  if (!snapshot_ || !pending_) return;
  if (pending_->creating) {
    const auto found = std::ranges::find_if(snapshot_->links, [&](const GraphLink &link) {
      return link.outputPortId == pending_->link.outputPortId && link.inputPortId == pending_->link.inputPortId;
    });
    if (found != snapshot_->links.end()) completePending(*found);
    return;
  }
  if (findLink(*snapshot_, pending_->link.id)) return;
  const auto replacement = std::ranges::find_if(snapshot_->links, [&](const GraphLink &link) {
    return link.outputPortId == pending_->link.outputPortId && link.inputPortId == pending_->link.inputPortId;
  });
  if (replacement != snapshot_->links.end()) {
    commandTimer_.stop(); pending_.reset();
    emit commandPendingChanged(); emit historyChanged();
    setNotice(QStringLiteral("This route was restored by session policy or another client."));
    return;
  }
  completePending(pending_->link);
}

void GraphController::completePending(const GraphLink &observed) {
  if (!pending_) return;
  const auto operation = *pending_;
  commandTimer_.stop(); pending_.reset();
  if (operation.intent == OperationIntent::Normal) {
    while (history_.size() > historyCursor_) history_.removeLast();
    history_.push_back({operation.creating ? HistoryKind::Created : HistoryKind::Destroyed,
      observed, QDateTime::currentMSecsSinceEpoch()});
    if (history_.size() > 50) history_.removeFirst();
    historyCursor_ = history_.size();
  } else if (operation.intent == OperationIntent::Undo) {
    historyCursor_ = std::max<qsizetype>(0, historyCursor_ - 1);
  } else {
    historyCursor_ = std::min(history_.size(), historyCursor_ + 1);
  }
  if (!operation.creating && selectedKind_ == QStringLiteral("link")) clearSelection();
  setNotice(operation.creating ? QStringLiteral("Link created. Undo is available.")
                               : QStringLiteral("Link disconnected. Undo is available."));
  emit commandPendingChanged(); emit historyChanged();
}

void GraphController::failPending(const QString &message) {
  if (!pending_) return;
  commandTimer_.stop(); pending_.reset();
  setNotice(message);
  emit commandPendingChanged(); emit historyChanged();
  rebuildPresentation();
}

void GraphController::disconnectSelected() {
  if (!snapshot_ || selectedKind_ != QStringLiteral("link") || pending_) return;
  const auto id = selected_.value(QStringLiteral("id")).toUInt();
  const auto *link = findLink(*snapshot_, id);
  if (!link) { setNotice(QStringLiteral("The selected link no longer exists.")); return; }
  submitDestroy(*link);
}

void GraphController::undo() {
  if (!canUndo() || !snapshot_) return;
  const auto &action = history_.at(historyCursor_ - 1);
  if (action.kind == HistoryKind::Created) {
    auto found = std::ranges::find(snapshot_->links, action.link.id, &GraphLink::id);
    if (found == snapshot_->links.end()) found = std::ranges::find_if(snapshot_->links, [&](const GraphLink &link) {
      return link.outputPortId == action.link.outputPortId && link.inputPortId == action.link.inputPortId;
    });
    if (found == snapshot_->links.end()) {
      --historyCursor_; setNotice(QStringLiteral("That link is already gone.")); emit historyChanged(); return;
    }
    submitDestroy(*found, OperationIntent::Undo);
  } else {
    submitCreate(action.link.outputPortId, action.link.inputPortId, action.link.feedback, OperationIntent::Undo);
  }
}

void GraphController::redo() {
  if (!canRedo() || !snapshot_) return;
  const auto &action = history_.at(historyCursor_);
  if (action.kind == HistoryKind::Created) {
    submitCreate(action.link.outputPortId, action.link.inputPortId, action.link.feedback, OperationIntent::Redo);
  } else {
    const auto found = std::ranges::find_if(snapshot_->links, [&](const GraphLink &link) {
      return link.outputPortId == action.link.outputPortId && link.inputPortId == action.link.inputPortId;
    });
    if (found == snapshot_->links.end()) {
      ++historyCursor_; setNotice(QStringLiteral("That link is already gone.")); emit historyChanged(); return;
    }
    submitDestroy(*found, OperationIntent::Redo);
  }
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
