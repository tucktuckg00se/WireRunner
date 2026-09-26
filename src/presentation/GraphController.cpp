// SPDX-License-Identifier: GPL-3.0-or-later
#include "presentation/GraphController.hpp"
#include "domain/Routing.hpp"

#include <QDateTime>
#include <QLineF>
#include <QMetaObject>
#include <QThread>
#include <algorithm>
#include <cmath>
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

QString availabilityText(Availability value) {
  return value == Availability::Unavailable ? QStringLiteral("unavailable")
    : value == Availability::Available ? QStringLiteral("available") : QStringLiteral("unknown");
}

QString defaultKindText(DefaultKind kind) {
  if (kind == DefaultKind::AudioSink) return QStringLiteral("audioSink");
  if (kind == DefaultKind::AudioSource) return QStringLiteral("audioSource");
  return QStringLiteral("videoSource");
}

QString channelPositionName(std::uint32_t position) {
  switch (position) {
  case 2: return QStringLiteral("Mono");
  case 3: return QStringLiteral("Front left");
  case 4: return QStringLiteral("Front right");
  case 5: return QStringLiteral("Front center");
  case 6: return QStringLiteral("LFE");
  case 7: return QStringLiteral("Side left");
  case 8: return QStringLiteral("Side right");
  case 12: return QStringLiteral("Rear left");
  case 13: return QStringLiteral("Rear right");
  default:
    if (position >= 0x1000 && position < 0x1040) return QStringLiteral("Aux %1").arg(position - 0x1000 + 1);
    return {};
  }
}

QString channelName(const GraphCard &card, const NodeAudioControl &audio, qsizetype index) {
  if (index < static_cast<qsizetype>(audio.channelMap.size())) {
    const auto mapped = channelPositionName(audio.channelMap.at(static_cast<std::size_t>(index)));
    if (!mapped.isEmpty()) return mapped;
  }
  const auto append = [&](const std::vector<GraphPort> &ports, QStringList &names) {
    for (const auto &port : ports) {
      if (port.media != MediaType::Audio) continue;
      auto name = qtext(port.channel);
      if (name.isEmpty()) name = qtext(port.name);
      if (!name.isEmpty() && !name.contains(QLatin1Char(',')) && !names.contains(name)) names.push_back(name);
    }
  };
  QStringList names;
  append(card.inputs, names); append(card.outputs, names);
  return index < names.size() ? names.at(index) : QStringLiteral("Channel %1").arg(index + 1);
}

QVariantMap audioMap(const GraphCard &card, const NodeAudioControl &audio, QString name,
                     const std::vector<float> *overrideVolumes = nullptr,
                     std::optional<bool> overrideMuted = std::nullopt, bool firstPair = false) {
  const auto &volumes = overrideVolumes && !overrideVolumes->empty() ? *overrideVolumes : audio.channelVolumes;
  const auto pairCount = firstPair ? std::min<std::size_t>(2, volumes.size()) : volumes.size();
  const auto linear = volumes.empty() ? audio.volume
    : *std::max_element(volumes.begin(), volumes.begin() + static_cast<std::ptrdiff_t>(pairCount));
  QVariantList channels;
  for (qsizetype index = 0; index < static_cast<qsizetype>(volumes.size()); ++index) {
    channels.push_back(QVariantMap{{QStringLiteral("index"), index},
      {QStringLiteral("name"), channelName(card, audio, index)},
      {QStringLiteral("volume"), volumeToPercent(volumes.at(static_cast<std::size_t>(index)))},
      {QStringLiteral("soft"), index < static_cast<qsizetype>(audio.softVolumes.size())},
      {QStringLiteral("minimum"), volumeToPercent(audio.minimumVolume)},
      {QStringLiteral("maximum"), volumeToPercent(std::max(audio.maximumVolume, linear))}});
  }
  return {{QStringLiteral("name"), std::move(name)},
    {QStringLiteral("volume"), volumeToPercent(linear)},
    {QStringLiteral("minimum"), volumeToPercent(audio.minimumVolume)},
    {QStringLiteral("maximum"), volumeToPercent(std::max(audio.maximumVolume, linear))},
    {QStringLiteral("muted"), overrideMuted.value_or(audio.muted)},
    {QStringLiteral("hasVolume"), audio.hasVolume}, {QStringLiteral("hasMute"), audio.hasMute},
    {QStringLiteral("writable"), audio.writable}, {QStringLiteral("boosted"), linear > 1.0F},
    {QStringLiteral("channels"), channels}, {QStringLiteral("pairCount"), std::min<qsizetype>(2, volumes.size())}};
}
} // namespace

GraphController::GraphController(std::unique_ptr<GraphSource> source, QString layoutPath, QObject *parent)
  : QObject(parent), source_(std::move(source)), layoutStore_(std::move(layoutPath)), cards_(this), links_(this) {
  commandTimer_.setSingleShot(true);
  commandTimer_.setInterval(4000);
  connect(&commandTimer_, &QTimer::timeout, this, [this] {
    if (pending_) failPending(QStringLiteral("PipeWire did not confirm the graph change"));
    else if (pendingSetting_) {
      const auto message = pendingSetting_->kind == SettingKind::Default
        ? QStringLiteral("WirePlumber kept a different default. Session policy may have higher priority.")
        : pendingSetting_->kind == SettingKind::Profile
          ? QStringLiteral("WirePlumber did not activate that device mode.")
          : QStringLiteral("WirePlumber did not activate that device port.");
      pendingSetting_.reset();
      setNotice(message);
      emit commandPendingChanged();
      rebuildPresentation();
    }
  });
  audioTimer_.setInterval(200);
  connect(&audioTimer_, &QTimer::timeout, this, [this] {
    const auto now = QDateTime::currentMSecsSinceEpoch();
    bool expired = false;
    for (auto it = pendingAudio_.begin(); it != pendingAudio_.end();) {
      if (it->deadline <= now) { it = pendingAudio_.erase(it); expired = true; }
      else ++it;
    }
    for (auto it = pendingRouteAudio_.begin(); it != pendingRouteAudio_.end();) {
      if (it->deadline <= now) { it = pendingRouteAudio_.erase(it); expired = true; }
      else ++it;
    }
    if (pendingAudio_.isEmpty() && pendingRouteAudio_.isEmpty()) audioTimer_.stop();
    if (expired) {
      setNotice(QStringLiteral("PipeWire did not confirm the audio change."));
      rebuildPresentation();
    }
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
    pendingAudio_.clear(); audioTimer_.stop();
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
  resolveAudioPending();
  resolveSettingPending();

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
    if (pendingSetting_) {
      pendingSetting_.reset();
      commandTimer_.stop();
      setNotice(QStringLiteral("The PipeWire connection changed before the setting was confirmed."));
      emit commandPendingChanged();
    }
    if (!pendingAudio_.isEmpty() || !pendingRouteAudio_.isEmpty()) {
      pendingAudio_.clear();
      pendingRouteAudio_.clear();
      audioTimer_.stop();
      setNotice(QStringLiteral("The PipeWire connection changed before the audio edit completed."));
      rebuildPresentation();
    }
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
    QVariantList members;
    QVariantList audioControls;
    QVariantMap inlineAudio;
    const GraphDevice *cardDevice = nullptr;
    if (card.deviceId) {
      const auto device = std::ranges::find(snapshot_->devices, *card.deviceId, &GraphDevice::id);
      if (device != snapshot_->devices.end()) cardDevice = &*device;
    }
    for (const auto nodeId : card.nodeIds) {
      if (cardDevice) break;
      const auto node = std::ranges::find(snapshot_->nodes, nodeId, &GraphNode::id);
      if (node == snapshot_->nodes.end() || !node->deviceId) continue;
      const auto device = std::ranges::find(snapshot_->devices, *node->deviceId, &GraphDevice::id);
      if (device != snapshot_->devices.end()) { cardDevice = &*device; break; }
    }
    if (cardDevice) {
      QSet<int> preferredDevices;
      for (const auto nodeId : card.nodeIds) {
        const auto node = std::ranges::find(snapshot_->nodes, nodeId, &GraphNode::id);
        if (node != snapshot_->nodes.end() && node->profileDeviceId) preferredDevices.insert(*node->profileDeviceId);
      }
      for (const auto &route : cardDevice->routes) {
        if (!route.active || (!route.audio.hasVolume && !route.audio.hasMute)) continue;
        if (!preferredDevices.isEmpty() && route.deviceIndex >= 0 && !preferredDevices.contains(route.deviceIndex)) continue;
        const auto routeKey = QStringLiteral("%1:%2").arg(cardDevice->id).arg(route.index);
        const auto pending = pendingRouteAudio_.constFind(routeKey);
        const auto pendingVolumes = pending == pendingRouteAudio_.cend() ? nullptr : &pending->channelVolumes;
        const auto pendingMuted = pending == pendingRouteAudio_.cend() ? std::optional<bool>{} : pending->muted;
        const auto routeLabel = route.description.empty() ? route.name : route.description;
        auto control = audioMap(card, route.audio,
          routeLabel.empty() ? (route.direction == PortDirection::Output ? QStringLiteral("Outputs") : QStringLiteral("Inputs"))
                             : qtext(routeLabel),
          pendingVolumes, pendingMuted, true);
        QVariantList channelMap;
        for (const auto position : route.audio.channelMap) channelMap.push_back(position);
        control.insert(QStringLiteral("targetKind"), QStringLiteral("route"));
        control.insert(QStringLiteral("deviceId"), cardDevice->id);
        control.insert(QStringLiteral("routeIndex"), route.index);
        control.insert(QStringLiteral("routeDeviceId"), route.deviceIndex);
        control.insert(QStringLiteral("direction"), route.direction == PortDirection::Output ? QStringLiteral("playback") : QStringLiteral("capture"));
        control.insert(QStringLiteral("channelMap"), channelMap);
        control.insert(QStringLiteral("pending"), pending != pendingRouteAudio_.cend());
        audioControls.push_back(control);
        if (inlineAudio.isEmpty() || (route.direction == PortDirection::Output && inlineAudio.value(QStringLiteral("direction")) != QStringLiteral("playback")))
          inlineAudio = control;
      }
    }
    const bool authoritativeDeviceAudio = !audioControls.isEmpty();
    const double portTop = inlineAudio.isEmpty() ? cardTop : 148.0;
    const double height = std::max(portTop + 26.0, portTop + 18.0 + static_cast<double>(visibleRows) * portStep);
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
      if (node->audio && !authoritativeDeviceAudio) {
        const auto pendingAudio = pendingAudio_.constFind(node->id);
        const auto linearVolume = pendingAudio != pendingAudio_.cend() && pendingAudio->volume
          ? static_cast<double>(*pendingAudio->volume) : static_cast<double>(node->audio->volume);
        const auto muted = pendingAudio != pendingAudio_.cend() && pendingAudio->muted
          ? *pendingAudio->muted : node->audio->muted;
        const auto decibels = volumeToDecibels(linearVolume);
        auto control = audioMap(card, *node->audio, qtext(node->name));
        control.insert(QStringLiteral("targetKind"), QStringLiteral("node"));
        control.insert(QStringLiteral("nodeId"), node->id);
        control.insert(QStringLiteral("volume"), volumeToPercent(linearVolume));
        control.insert(QStringLiteral("decibels"), std::isfinite(decibels) ? QVariant(decibels) : QVariant());
        control.insert(QStringLiteral("muted"), muted);
        control.insert(QStringLiteral("pending"), pendingAudio != pendingAudio_.cend());
        audioControls.push_back(control);
      }
    }
    for (const auto &link : snapshot_->links) {
      if (std::ranges::find(card.nodeIds, link.outputNodeId) != card.nodeIds.end() ||
          std::ranges::find(card.nodeIds, link.inputNodeId) != card.nodeIds.end()) ++connectionCount;
    }
    QVariantList profiles;
    QVariantList routeOptions;
    if (cardDevice) {
      if (mediaTypes.isEmpty()) {
        const auto media = classifyMedia(cardDevice->mediaClass);
        if (media != MediaType::Unknown) mediaTypes.push_back(text(mediaTypeName(media)));
      }
      for (const auto &profile : cardDevice->profiles) {
        profiles.push_back(QVariantMap{{QStringLiteral("index"), profile.index},
          {QStringLiteral("name"), qtext(profile.description.empty() ? profile.name : profile.description)},
          {QStringLiteral("technicalName"), qtext(profile.name)}, {QStringLiteral("active"), profile.active},
          {QStringLiteral("availability"), availabilityText(profile.availability)},
          {QStringLiteral("enabled"), cardDevice->writable && profile.availability != Availability::Unavailable}});
      }
      for (const auto &route : cardDevice->routes) {
        routeOptions.push_back(QVariantMap{{QStringLiteral("index"), route.index},
          {QStringLiteral("deviceIndex"), route.deviceIndex},
          {QStringLiteral("name"), qtext(route.description.empty() ? route.name : route.description)},
          {QStringLiteral("technicalName"), qtext(route.name)}, {QStringLiteral("active"), route.active},
          {QStringLiteral("direction"), route.direction == PortDirection::Output ? QStringLiteral("playback") : QStringLiteral("capture")},
          {QStringLiteral("availability"), availabilityText(route.availability)},
          {QStringLiteral("enabled"), cardDevice->writable && route.availability != Availability::Unavailable}});
      }
    }
    QVariantList defaultActions;
    QStringList defaultBadges;
    const auto appendDefault = [&](const GraphNode &node, DefaultKind kind, const QString &label) {
      const auto target = std::ranges::find(snapshot_->defaults, kind, &DefaultTarget::kind);
      const auto effective = target != snapshot_->defaults.end() && target->effectiveName == node.technicalName;
      const auto configured = target != snapshot_->defaults.end() && target->configuredName == node.technicalName;
      defaultActions.push_back(QVariantMap{{QStringLiteral("kind"), defaultKindText(kind)},
        {QStringLiteral("label"), label}, {QStringLiteral("nodeId"), node.id},
        {QStringLiteral("nodeName"), qtext(node.technicalName)}, {QStringLiteral("effective"), effective},
        {QStringLiteral("configured"), configured}});
      if (effective && !defaultBadges.contains(label)) defaultBadges.push_back(label);
    };
    for (const auto nodeId : card.nodeIds) {
      const auto node = std::ranges::find(snapshot_->nodes, nodeId, &GraphNode::id);
      if (node == snapshot_->nodes.end() || node->mediaClass.starts_with("Stream/")) continue;
      if (node->media == MediaType::Audio && node->mediaClass.find("Sink") != std::string::npos)
        appendDefault(*node, DefaultKind::AudioSink, QStringLiteral("Default output"));
      if (node->media == MediaType::Audio && node->mediaClass.find("Source") != std::string::npos)
        appendDefault(*node, DefaultKind::AudioSource, QStringLiteral("Default input"));
      if (node->media == MediaType::Video && node->mediaClass.find("Source") != std::string::npos)
        appendDefault(*node, DefaultKind::VideoSource, QStringLiteral("Default camera"));
    }
    const bool visible = mediaFilter_ == QStringLiteral("all") || mediaTypes.contains(mediaFilter_);
    QVariantMap item{{QStringLiteral("key"), key}, {QStringLiteral("title"), qtext(card.title)},
      {QStringLiteral("subtitle"), qtext(card.subtitle)}, {QStringLiteral("kind"), qtext(card.kind)},
      {QStringLiteral("persistent"), card.persistent}, {QStringLiteral("x"), state.position.x()},
      {QStringLiteral("y"), state.position.y()}, {QStringLiteral("width"), cardWidth},
      {QStringLiteral("height"), height}, {QStringLiteral("expanded"), state.expanded},
      {QStringLiteral("media"), primaryMedia(card)}, {QStringLiteral("mediaTypes"), mediaTypes},
      {QStringLiteral("state"), stateLabel}, {QStringLiteral("members"), members},
      {QStringLiteral("audioControls"), audioControls},
      {QStringLiteral("deviceId"), cardDevice ? QVariant::fromValue(cardDevice->id) : QVariant{}},
      {QStringLiteral("deviceWritable"), cardDevice && cardDevice->writable},
      {QStringLiteral("profiles"), profiles}, {QStringLiteral("routeOptions"), routeOptions},
      {QStringLiteral("defaultActions"), defaultActions}, {QStringLiteral("defaultBadges"), defaultBadges},
      {QStringLiteral("inlineAudio"), inlineAudio}, {QStringLiteral("portTop"), portTop},
      {QStringLiteral("technicalNames"), technicalNames}, {QStringLiteral("inputs"), inputs},
      {QStringLiteral("outputs"), outputs}, {QStringLiteral("inputGroups"), inputGroups},
      {QStringLiteral("outputGroups"), outputGroups}, {QStringLiteral("nodeCount"), static_cast<int>(card.nodeIds.size())},
      {QStringLiteral("inputCount"), static_cast<int>(card.inputs.size())},
      {QStringLiteral("outputCount"), static_cast<int>(card.outputs.size())}, {QStringLiteral("visible"), visible},
      {QStringLiteral("connectionCount"), connectionCount},
      {QStringLiteral("focused"), focusCards_.isEmpty() || focusCards_.contains(key)}};
    cards.push_back(item);
    cardMaps.insert(key, item);
    if (visible) cardRects.push_back(QVariantMap{{QStringLiteral("key"), key}, {QStringLiteral("x"), state.position.x()},
      {QStringLiteral("y"), state.position.y()}, {QStringLiteral("width"), cardWidth},
      {QStringLiteral("height"), height}});
    maximumX = std::max(maximumX, state.position.x() + cardWidth + 80.0);
    maximumY = std::max(maximumY, state.position.y() + height + 80.0);

    const auto addAnchors = [&](const QVariantList &ports, PortDirection direction) {
      for (qsizetype row = 0; row < ports.size(); ++row) {
        const auto port = ports.at(row).toMap();
        anchors.push_back(QVariantMap{{QStringLiteral("key"), port.value(QStringLiteral("key"))},
          {QStringLiteral("x"), state.position.x() + (direction == PortDirection::Input ? 0.0 : cardWidth)},
          {QStringLiteral("y"), state.position.y() + portTop + 9.0 + static_cast<double>(row) * portStep},
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
          {QStringLiteral("y"), state.position.y() + portTop + 9.0 + static_cast<double>(row) * portStep},
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

bool GraphController::canUndo() const { return !pending_ && !pendingSetting_ && historyCursor_ > 0; }
bool GraphController::canRedo() const { return !pending_ && !pendingSetting_ && historyCursor_ < history_.size(); }

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

void GraphController::applyAudioResult(GlobalId nodeId, CommandResult result) {
  const auto found = pendingAudio_.find(nodeId);
  if (found == pendingAudio_.end() || found->commandId != result.commandId) return;
  if (result.accepted) {
    setNotice(qtext(result.message));
    return;
  }
  pendingAudio_.erase(found);
  if (pendingAudio_.isEmpty()) audioTimer_.stop();
  setNotice(qtext(result.message));
  rebuildPresentation();
}

void GraphController::applyRouteAudioResult(const QString &key, CommandResult result) {
  const auto found = pendingRouteAudio_.find(key);
  if (found == pendingRouteAudio_.end() || found->commandId != result.commandId) return;
  if (result.accepted) { setNotice(qtext(result.message)); return; }
  pendingRouteAudio_.erase(found);
  if (pendingAudio_.isEmpty() && pendingRouteAudio_.isEmpty()) audioTimer_.stop();
  setNotice(qtext(result.message));
  rebuildPresentation();
}

void GraphController::applySettingResult(CommandResult result) {
  if (!pendingSetting_ || pendingSetting_->commandId != result.commandId) return;
  if (result.accepted) { setNotice(qtext(result.message)); return; }
  commandTimer_.stop();
  pendingSetting_.reset();
  setNotice(qtext(result.message));
  emit commandPendingChanged();
  rebuildPresentation();
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

void GraphController::resolveAudioPending() {
  if (!snapshot_) return;
  for (auto it = pendingAudio_.begin(); it != pendingAudio_.end();) {
    const auto node = std::ranges::find(snapshot_->nodes, it.key(), &GraphNode::id);
    bool confirmed = node == snapshot_->nodes.end() || !node->audio;
    if (!confirmed && it->volume)
      confirmed = std::abs(node->audio->volume - *it->volume) <= 0.002F;
    if (!confirmed && it->muted) confirmed = node->audio->muted == *it->muted;
    if (it->volume && it->muted && node != snapshot_->nodes.end() && node->audio)
      confirmed = std::abs(node->audio->volume - *it->volume) <= 0.002F && node->audio->muted == *it->muted;
    if (confirmed) it = pendingAudio_.erase(it); else ++it;
  }
  for (auto it = pendingRouteAudio_.begin(); it != pendingRouteAudio_.end();) {
    const auto device = std::ranges::find(snapshot_->devices, it->deviceId, &GraphDevice::id);
    const DeviceRoute *route = nullptr;
    if (device != snapshot_->devices.end()) {
      const auto found = std::ranges::find(device->routes, it->routeIndex, &DeviceRoute::index);
      if (found != device->routes.end()) route = &*found;
    }
    bool confirmed = !route;
    if (route && !it->channelVolumes.empty()) {
      confirmed = route->audio.channelVolumes.size() == it->channelVolumes.size();
      for (std::size_t i = 0; confirmed && i < it->channelVolumes.size(); ++i)
        confirmed = std::abs(route->audio.channelVolumes[i] - it->channelVolumes[i]) <= 0.002F;
    }
    if (route && it->muted) confirmed = confirmed || route->audio.muted == *it->muted;
    if (confirmed) it = pendingRouteAudio_.erase(it); else ++it;
  }
  if (pendingAudio_.isEmpty() && pendingRouteAudio_.isEmpty()) audioTimer_.stop();
}

void GraphController::resolveSettingPending() {
  if (!snapshot_ || !pendingSetting_) return;
  bool confirmed = false;
  if (pendingSetting_->kind == SettingKind::Default) {
    const auto target = std::ranges::find(snapshot_->defaults, pendingSetting_->defaultKind, &DefaultTarget::kind);
    confirmed = target != snapshot_->defaults.end() && (pendingSetting_->targetName.empty()
      ? target->configuredName.empty() : target->effectiveName == pendingSetting_->targetName);
  } else {
    const auto device = std::ranges::find(snapshot_->devices, pendingSetting_->deviceId, &GraphDevice::id);
    if (device != snapshot_->devices.end() && pendingSetting_->kind == SettingKind::Profile)
      confirmed = std::ranges::any_of(device->profiles, [&](const DeviceProfile &profile) {
        return profile.index == pendingSetting_->targetIndex && profile.active;
      });
    if (device != snapshot_->devices.end() && pendingSetting_->kind == SettingKind::Route)
      confirmed = std::ranges::any_of(device->routes, [&](const DeviceRoute &route) {
        return route.index == pendingSetting_->targetIndex && route.deviceIndex == pendingSetting_->routeDeviceId && route.active;
      });
  }
  if (!confirmed) return;
  const auto setting = *pendingSetting_;
  commandTimer_.stop();
  pendingSetting_.reset();
  if (setting.intent == OperationIntent::Normal &&
      (setting.kind == SettingKind::Default || setting.previousIndex >= 0)) {
    while (history_.size() > historyCursor_) history_.removeLast();
    HistoryAction action;
    action.kind = setting.kind == SettingKind::Default ? HistoryKind::DefaultChanged
      : setting.kind == SettingKind::Profile ? HistoryKind::ProfileChanged : HistoryKind::RouteChanged;
    action.defaultKind = setting.defaultKind;
    action.previousName = setting.previousName;
    action.targetName = setting.targetName;
    action.deviceId = setting.deviceId;
    action.previousIndex = setting.previousIndex;
    action.targetIndex = setting.targetIndex;
    action.previousRouteDeviceId = setting.previousRouteDeviceId;
    action.targetRouteDeviceId = setting.routeDeviceId;
    action.recordedAt = QDateTime::currentMSecsSinceEpoch();
    history_.push_back(std::move(action));
    if (history_.size() > 50) history_.removeFirst();
    historyCursor_ = history_.size();
  } else if (setting.intent == OperationIntent::Undo) {
    historyCursor_ = std::max<qsizetype>(0, historyCursor_ - 1);
  } else if (setting.intent == OperationIntent::Redo) {
    historyCursor_ = std::min(history_.size(), historyCursor_ + 1);
  }
  setNotice(QStringLiteral("WirePlumber confirmed the setting change."));
  emit commandPendingChanged(); emit historyChanged();
}

void GraphController::completePending(const GraphLink &observed) {
  if (!pending_) return;
  const auto operation = *pending_;
  commandTimer_.stop(); pending_.reset();
  if (operation.intent == OperationIntent::Normal) {
    while (history_.size() > historyCursor_) history_.removeLast();
    HistoryAction action;
    action.kind = operation.creating ? HistoryKind::Created : HistoryKind::Destroyed;
    action.link = observed;
    action.recordedAt = QDateTime::currentMSecsSinceEpoch();
    history_.push_back(std::move(action));
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

void GraphController::setNodeVolume(quint32 nodeId, double percent) {
  if (!snapshot_) return;
  const auto node = std::ranges::find(snapshot_->nodes, static_cast<GlobalId>(nodeId), &GraphNode::id);
  if (node == snapshot_->nodes.end() || !node->audio || !node->audio->hasVolume) {
    setNotice(QStringLiteral("This node does not expose a master volume control."));
    return;
  }
  if (!node->audio->writable) {
    setNotice(QStringLiteral("PipeWire does not allow this volume to be changed."));
    return;
  }
  const auto minimum = volumeToPercent(node->audio->minimumVolume);
  const auto maximum = volumeToPercent(node->audio->maximumVolume);
  const auto requested = static_cast<float>(percentToVolume(std::clamp(percent, minimum, maximum)));
  SetNodeAudioRequest request{.commandId = nextCommandId_++, .snapshotRevision = snapshot_->revision,
    .nodeId = static_cast<GlobalId>(nodeId), .volume = std::nullopt, .channelVolumes = {}, .muted = std::nullopt};
  if (node->audio->channelVolumes.empty()) {
    request.volume = requested;
  } else {
    const auto current = node->audio->volume;
    request.channelVolumes.reserve(node->audio->channelVolumes.size());
    for (const auto channel : node->audio->channelVolumes)
      request.channelVolumes.push_back(current > 0.000001F ? channel * requested / current : requested);
  }
  pendingAudio_.insert(nodeId, PendingAudio{request.commandId, requested, std::nullopt,
    QDateTime::currentMSecsSinceEpoch() + 4000});
  if (!audioTimer_.isActive()) audioTimer_.start();
  source_->setNodeAudio(std::move(request), [this, nodeId](CommandResult result) {
    QMetaObject::invokeMethod(this, [this, nodeId, result = std::move(result)] {
      applyAudioResult(nodeId, result);
    }, Qt::QueuedConnection);
  });
}

void GraphController::setNodeMuted(quint32 nodeId, bool muted) {
  if (!snapshot_) return;
  const auto node = std::ranges::find(snapshot_->nodes, static_cast<GlobalId>(nodeId), &GraphNode::id);
  if (node == snapshot_->nodes.end() || !node->audio || !node->audio->hasMute) {
    setNotice(QStringLiteral("This node does not expose a mute control."));
    return;
  }
  if (!node->audio->writable) {
    setNotice(QStringLiteral("PipeWire does not allow this mute control to be changed."));
    return;
  }
  SetNodeAudioRequest request{.commandId = nextCommandId_++, .snapshotRevision = snapshot_->revision,
    .nodeId = static_cast<GlobalId>(nodeId), .volume = std::nullopt, .channelVolumes = {}, .muted = muted};
  pendingAudio_.insert(nodeId, PendingAudio{request.commandId, std::nullopt, muted,
    QDateTime::currentMSecsSinceEpoch() + 4000});
  if (!audioTimer_.isActive()) audioTimer_.start();
  source_->setNodeAudio(std::move(request), [this, nodeId](CommandResult result) {
    QMetaObject::invokeMethod(this, [this, nodeId, result = std::move(result)] {
      applyAudioResult(nodeId, result);
    }, Qt::QueuedConnection);
  });
}

void GraphController::setAudioVolume(const QVariantMap &control, double percent) {
  if (control.value(QStringLiteral("targetKind")) == QStringLiteral("node")) {
    setNodeVolume(control.value(QStringLiteral("nodeId")).toUInt(), percent);
    return;
  }
  if (!snapshot_ || control.value(QStringLiteral("targetKind")) != QStringLiteral("route")) return;
  const auto deviceId = control.value(QStringLiteral("deviceId")).toUInt();
  const auto routeIndex = control.value(QStringLiteral("routeIndex")).toInt();
  const auto device = std::ranges::find(snapshot_->devices, deviceId, &GraphDevice::id);
  if (device == snapshot_->devices.end()) return;
  const auto route = std::ranges::find(device->routes, routeIndex, &DeviceRoute::index);
  if (route == device->routes.end() || !route->audio.writable || route->audio.channelVolumes.empty()) return;
  auto volumes = route->audio.channelVolumes;
  const auto pairCount = std::min<std::size_t>(2, volumes.size());
  const auto current = *std::max_element(volumes.begin(), volumes.begin() + static_cast<std::ptrdiff_t>(pairCount));
  const auto requested = static_cast<float>(percentToVolume(std::clamp(percent,
    volumeToPercent(route->audio.minimumVolume), volumeToPercent(route->audio.maximumVolume))));
  for (std::size_t index = 0; index < pairCount; ++index)
    volumes[index] = current > 0.000001F ? volumes[index] * requested / current : requested;
  const auto key = QStringLiteral("%1:%2").arg(deviceId).arg(routeIndex);
  SetDeviceRouteAudioRequest request{.commandId = nextCommandId_++, .snapshotRevision = snapshot_->revision,
    .deviceId = deviceId, .routeIndex = routeIndex, .routeDeviceId = route->deviceIndex,
    .channelVolumes = volumes, .channelMap = route->audio.channelMap, .muted = std::nullopt};
  pendingRouteAudio_.insert(key, PendingRouteAudio{request.commandId, deviceId, routeIndex, volumes,
    std::nullopt, QDateTime::currentMSecsSinceEpoch() + 4000});
  if (!audioTimer_.isActive()) audioTimer_.start();
  source_->setDeviceRouteAudio(std::move(request), [this, key](CommandResult result) {
    QMetaObject::invokeMethod(this, [this, key, result = std::move(result)] { applyRouteAudioResult(key, result); },
      Qt::QueuedConnection);
  });
  rebuildPresentation();
}

void GraphController::setAudioChannelVolume(const QVariantMap &control, int channel, double percent) {
  if (!snapshot_ || channel < 0) return;
  if (control.value(QStringLiteral("targetKind")) == QStringLiteral("node")) {
    const auto nodeId = control.value(QStringLiteral("nodeId")).toUInt();
    const auto node = std::ranges::find(snapshot_->nodes, nodeId, &GraphNode::id);
    if (node == snapshot_->nodes.end() || !node->audio || !node->audio->writable ||
        channel >= static_cast<int>(node->audio->channelVolumes.size())) return;
    auto volumes = node->audio->channelVolumes;
    volumes[static_cast<std::size_t>(channel)] = static_cast<float>(percentToVolume(percent));
    SetNodeAudioRequest request{.commandId = nextCommandId_++, .snapshotRevision = snapshot_->revision,
      .nodeId = nodeId, .volume = std::nullopt, .channelVolumes = volumes, .muted = std::nullopt};
    pendingAudio_.insert(nodeId, PendingAudio{request.commandId,
      *std::ranges::max_element(volumes), std::nullopt, QDateTime::currentMSecsSinceEpoch() + 4000});
    if (!audioTimer_.isActive()) audioTimer_.start();
    source_->setNodeAudio(std::move(request), [this, nodeId](CommandResult result) {
      QMetaObject::invokeMethod(this, [this, nodeId, result = std::move(result)] { applyAudioResult(nodeId, result); },
        Qt::QueuedConnection);
    });
    return;
  }
  const auto deviceId = control.value(QStringLiteral("deviceId")).toUInt();
  const auto routeIndex = control.value(QStringLiteral("routeIndex")).toInt();
  const auto device = std::ranges::find(snapshot_->devices, deviceId, &GraphDevice::id);
  if (device == snapshot_->devices.end()) return;
  const auto route = std::ranges::find(device->routes, routeIndex, &DeviceRoute::index);
  if (route == device->routes.end() || !route->audio.writable ||
      channel >= static_cast<int>(route->audio.channelVolumes.size())) return;
  auto volumes = route->audio.channelVolumes;
  volumes[static_cast<std::size_t>(channel)] = static_cast<float>(percentToVolume(percent));
  const auto key = QStringLiteral("%1:%2").arg(deviceId).arg(routeIndex);
  SetDeviceRouteAudioRequest request{.commandId = nextCommandId_++, .snapshotRevision = snapshot_->revision,
    .deviceId = deviceId, .routeIndex = routeIndex, .routeDeviceId = route->deviceIndex,
    .channelVolumes = volumes, .channelMap = route->audio.channelMap, .muted = std::nullopt};
  pendingRouteAudio_.insert(key, PendingRouteAudio{request.commandId, deviceId, routeIndex, volumes,
    std::nullopt, QDateTime::currentMSecsSinceEpoch() + 4000});
  if (!audioTimer_.isActive()) audioTimer_.start();
  source_->setDeviceRouteAudio(std::move(request), [this, key](CommandResult result) {
    QMetaObject::invokeMethod(this, [this, key, result = std::move(result)] { applyRouteAudioResult(key, result); }, Qt::QueuedConnection);
  });
  rebuildPresentation();
}

void GraphController::setAudioMuted(const QVariantMap &control, bool muted) {
  if (control.value(QStringLiteral("targetKind")) == QStringLiteral("node")) {
    setNodeMuted(control.value(QStringLiteral("nodeId")).toUInt(), muted);
    return;
  }
  if (!snapshot_) return;
  const auto deviceId = control.value(QStringLiteral("deviceId")).toUInt();
  const auto routeIndex = control.value(QStringLiteral("routeIndex")).toInt();
  const auto device = std::ranges::find(snapshot_->devices, deviceId, &GraphDevice::id);
  if (device == snapshot_->devices.end()) return;
  const auto route = std::ranges::find(device->routes, routeIndex, &DeviceRoute::index);
  if (route == device->routes.end() || !route->audio.writable || !route->audio.hasMute) return;
  const auto key = QStringLiteral("%1:%2").arg(deviceId).arg(routeIndex);
  SetDeviceRouteAudioRequest request{.commandId = nextCommandId_++, .snapshotRevision = snapshot_->revision,
    .deviceId = deviceId, .routeIndex = routeIndex, .routeDeviceId = route->deviceIndex,
    .channelVolumes = {}, .channelMap = route->audio.channelMap, .muted = muted};
  pendingRouteAudio_.insert(key, PendingRouteAudio{request.commandId, deviceId, routeIndex, {}, muted,
    QDateTime::currentMSecsSinceEpoch() + 4000});
  if (!audioTimer_.isActive()) audioTimer_.start();
  source_->setDeviceRouteAudio(std::move(request), [this, key](CommandResult result) {
    QMetaObject::invokeMethod(this, [this, key, result = std::move(result)] { applyRouteAudioResult(key, result); }, Qt::QueuedConnection);
  });
  rebuildPresentation();
}

void GraphController::setDefaultTarget(const QString &kind, quint32 nodeId) {
  if (!snapshot_ || pending_ || pendingSetting_) return;
  const auto node = std::ranges::find(snapshot_->nodes, static_cast<GlobalId>(nodeId), &GraphNode::id);
  if (node == snapshot_->nodes.end() || node->technicalName.empty()) {
    setNotice(QStringLiteral("The selected default target is no longer available."));
    return;
  }
  const auto targetKind = kind == QStringLiteral("audioSink") ? DefaultKind::AudioSink
    : kind == QStringLiteral("audioSource") ? DefaultKind::AudioSource : DefaultKind::VideoSource;
  SetDefaultRequest request{.commandId = nextCommandId_++, .snapshotRevision = snapshot_->revision,
    .kind = targetKind, .nodeName = node->technicalName};
  const auto previous = std::ranges::find(snapshot_->defaults, targetKind, &DefaultTarget::kind);
  pendingSetting_ = PendingSetting{.commandId = request.commandId, .kind = SettingKind::Default,
    .defaultKind = targetKind, .targetName = node->technicalName,
    .previousName = previous == snapshot_->defaults.end() ? std::string{} : previous->configuredName,
    .deadline = QDateTime::currentMSecsSinceEpoch() + 4000};
  commandTimer_.start();
  emit commandPendingChanged();
  setNotice(QStringLiteral("Asking WirePlumber to change the default…"));
  source_->setDefault(std::move(request), [this](CommandResult result) {
    QMetaObject::invokeMethod(this, [this, result = std::move(result)] { applySettingResult(result); }, Qt::QueuedConnection);
  });
  rebuildPresentation();
}

void GraphController::setDeviceProfile(quint32 deviceId, int profileIndex) {
  if (!snapshot_ || pending_ || pendingSetting_) return;
  const auto device = std::ranges::find(snapshot_->devices, static_cast<GlobalId>(deviceId), &GraphDevice::id);
  if (device == snapshot_->devices.end() || !device->writable) {
    setNotice(QStringLiteral("This device mode cannot be changed.")); return;
  }
  const auto profile = std::ranges::find(device->profiles, profileIndex, &DeviceProfile::index);
  if (profile == device->profiles.end() || profile->availability == Availability::Unavailable) {
    setNotice(QStringLiteral("That device mode is unavailable.")); return;
  }
  SetDeviceProfileRequest request{.commandId = nextCommandId_++, .snapshotRevision = snapshot_->revision,
    .deviceId = static_cast<GlobalId>(deviceId), .profileIndex = profileIndex};
  const auto active = std::ranges::find(device->profiles, true, &DeviceProfile::active);
  pendingSetting_ = PendingSetting{.commandId = request.commandId, .kind = SettingKind::Profile,
    .targetName = {}, .deviceId = static_cast<GlobalId>(deviceId), .targetIndex = profileIndex,
    .previousName = {},
    .previousIndex = active == device->profiles.end() ? -1 : active->index,
    .deadline = QDateTime::currentMSecsSinceEpoch() + 4000};
  commandTimer_.start(); emit commandPendingChanged();
  setNotice(QStringLiteral("Changing device mode…"));
  source_->setDeviceProfile(std::move(request), [this](CommandResult result) {
    QMetaObject::invokeMethod(this, [this, result = std::move(result)] { applySettingResult(result); }, Qt::QueuedConnection);
  });
  rebuildPresentation();
}

void GraphController::setDeviceRoute(quint32 deviceId, int routeIndex, int routeDeviceId) {
  if (!snapshot_ || pending_ || pendingSetting_) return;
  const auto device = std::ranges::find(snapshot_->devices, static_cast<GlobalId>(deviceId), &GraphDevice::id);
  if (device == snapshot_->devices.end() || !device->writable) {
    setNotice(QStringLiteral("This device port cannot be changed.")); return;
  }
  const auto route = std::ranges::find_if(device->routes, [&](const DeviceRoute &candidate) {
    return candidate.index == routeIndex && candidate.deviceIndex == routeDeviceId;
  });
  if (route == device->routes.end() || route->availability == Availability::Unavailable) {
    setNotice(QStringLiteral("That device port is unavailable.")); return;
  }
  SetDeviceRouteRequest request{.commandId = nextCommandId_++, .snapshotRevision = snapshot_->revision,
    .deviceId = static_cast<GlobalId>(deviceId), .routeIndex = routeIndex, .routeDeviceId = routeDeviceId};
  const auto active = std::ranges::find_if(device->routes, [&](const DeviceRoute &candidate) {
    return candidate.active && candidate.direction == route->direction;
  });
  pendingSetting_ = PendingSetting{.commandId = request.commandId, .kind = SettingKind::Route,
    .targetName = {}, .deviceId = static_cast<GlobalId>(deviceId), .targetIndex = routeIndex, .routeDeviceId = routeDeviceId,
    .previousName = {},
    .previousIndex = active == device->routes.end() ? -1 : active->index,
    .previousRouteDeviceId = active == device->routes.end() ? -1 : active->deviceIndex,
    .deadline = QDateTime::currentMSecsSinceEpoch() + 4000};
  commandTimer_.start(); emit commandPendingChanged();
  setNotice(QStringLiteral("Changing device port…"));
  source_->setDeviceRoute(std::move(request), [this](CommandResult result) {
    QMetaObject::invokeMethod(this, [this, result = std::move(result)] { applySettingResult(result); }, Qt::QueuedConnection);
  });
  rebuildPresentation();
}

void GraphController::submitHistorySetting(const HistoryAction &action, OperationIntent intent) {
  if (!snapshot_ || pending_ || pendingSetting_) return;
  const bool undoing = intent == OperationIntent::Undo;
  const auto commandId = nextCommandId_++;
  PendingSetting pending;
  pending.commandId = commandId;
  pending.intent = intent;
  pending.deadline = QDateTime::currentMSecsSinceEpoch() + 4000;
  if (action.kind == HistoryKind::DefaultChanged) {
    pending.kind = SettingKind::Default;
    pending.defaultKind = action.defaultKind;
    pending.targetName = undoing ? action.previousName : action.targetName;
    pending.previousName = undoing ? action.targetName : action.previousName;
    pendingSetting_ = pending;
    source_->setDefault({commandId, snapshot_->revision, action.defaultKind, pending.targetName}, [this](CommandResult result) {
      QMetaObject::invokeMethod(this, [this, result = std::move(result)] { applySettingResult(result); }, Qt::QueuedConnection);
    });
  } else if (action.kind == HistoryKind::ProfileChanged) {
    pending.kind = SettingKind::Profile;
    pending.deviceId = action.deviceId;
    pending.targetIndex = undoing ? action.previousIndex : action.targetIndex;
    pending.previousIndex = undoing ? action.targetIndex : action.previousIndex;
    pendingSetting_ = pending;
    source_->setDeviceProfile({commandId, snapshot_->revision, action.deviceId, pending.targetIndex}, [this](CommandResult result) {
      QMetaObject::invokeMethod(this, [this, result = std::move(result)] { applySettingResult(result); }, Qt::QueuedConnection);
    });
  } else if (action.kind == HistoryKind::RouteChanged) {
    pending.kind = SettingKind::Route;
    pending.deviceId = action.deviceId;
    pending.targetIndex = undoing ? action.previousIndex : action.targetIndex;
    pending.routeDeviceId = undoing ? action.previousRouteDeviceId : action.targetRouteDeviceId;
    pending.previousIndex = undoing ? action.targetIndex : action.previousIndex;
    pending.previousRouteDeviceId = undoing ? action.targetRouteDeviceId : action.previousRouteDeviceId;
    pendingSetting_ = pending;
    source_->setDeviceRoute({commandId, snapshot_->revision, action.deviceId, pending.targetIndex, pending.routeDeviceId}, [this](CommandResult result) {
      QMetaObject::invokeMethod(this, [this, result = std::move(result)] { applySettingResult(result); }, Qt::QueuedConnection);
    });
  } else return;
  commandTimer_.start();
  emit commandPendingChanged();
  setNotice(undoing ? QStringLiteral("Restoring the previous setting…") : QStringLiteral("Reapplying the setting…"));
  rebuildPresentation();
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
  } else if (action.kind == HistoryKind::Destroyed) {
    submitCreate(action.link.outputPortId, action.link.inputPortId, action.link.feedback, OperationIntent::Undo);
  } else submitHistorySetting(action, OperationIntent::Undo);
}

void GraphController::redo() {
  if (!canRedo() || !snapshot_) return;
  const auto &action = history_.at(historyCursor_);
  if (action.kind == HistoryKind::Created) {
    submitCreate(action.link.outputPortId, action.link.inputPortId, action.link.feedback, OperationIntent::Redo);
  } else if (action.kind == HistoryKind::Destroyed) {
    const auto found = std::ranges::find_if(snapshot_->links, [&](const GraphLink &link) {
      return link.outputPortId == action.link.outputPortId && link.inputPortId == action.link.inputPortId;
    });
    if (found == snapshot_->links.end()) {
      ++historyCursor_; setNotice(QStringLiteral("That link is already gone.")); emit historyChanged(); return;
    }
    submitDestroy(*found, OperationIntent::Redo);
  } else submitHistorySetting(action, OperationIntent::Redo);
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
