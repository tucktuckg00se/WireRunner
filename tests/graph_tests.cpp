// SPDX-License-Identifier: GPL-3.0-or-later
#include "backend/FixtureGraphSource.hpp"
#include "domain/Graph.hpp"
#include "domain/Routing.hpp"
#include "presentation/GraphController.hpp"
#include "presentation/LayoutStore.hpp"
#include "presentation/StableListModel.hpp"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>
#include <cmath>

using namespace wirerunner;

class ControllableGraphSource final : public GraphSource {
public:
  void start(SnapshotCallback snapshot, StatusCallback status) override {
    snapshot_ = std::move(snapshot);
    status_ = std::move(status);
    status_({SourceState::Ready, "Test graph"});
  }
  void stop() override {}
  void publish(GraphSnapshot value) { snapshot_(std::make_shared<GraphSnapshot>(std::move(value))); }
  void createLink(CreateLinkRequest request, CommandCallback callback) override {
    lastCreate = request; command_ = std::move(callback);
  }
  void destroyLink(DestroyLinkRequest request, CommandCallback callback) override {
    lastDestroy = request; command_ = std::move(callback);
  }
  void setNodeAudio(SetNodeAudioRequest request, CommandCallback callback) override {
    lastAudio = std::move(request); audioCommand_ = std::move(callback);
  }
  void setDeviceRouteAudio(SetDeviceRouteAudioRequest request, CommandCallback callback) override {
    lastRouteAudio = std::move(request); audioCommand_ = std::move(callback);
  }
  void setDefault(SetDefaultRequest request, CommandCallback callback) override {
    lastDefault = std::move(request); command_ = std::move(callback);
  }
  void setDeviceProfile(SetDeviceProfileRequest request, CommandCallback callback) override {
    lastProfile = std::move(request); command_ = std::move(callback);
  }
  void setDeviceRoute(SetDeviceRouteRequest request, CommandCallback callback) override {
    lastRoute = std::move(request); command_ = std::move(callback);
  }
  void respond(bool accepted, std::string message = {}) {
    QVERIFY(command_);
    const auto id = lastCreate.commandId != 0 && lastCreate.commandId >= lastDestroy.commandId
      ? lastCreate.commandId : lastDestroy.commandId;
    auto callback = std::move(command_);
    callback({id, accepted, std::move(message)});
  }
  void respondAudio(bool accepted, std::string message = {}) {
    QVERIFY(audioCommand_);
    auto callback = std::move(audioCommand_);
    callback({lastAudio.commandId, accepted, std::move(message)});
  }
  void respondRouteAudio(bool accepted, std::string message = {}) {
    QVERIFY(audioCommand_);
    auto callback = std::move(audioCommand_);
    callback({lastRouteAudio.commandId, accepted, std::move(message)});
  }
  void respondDefault(bool accepted, std::string message = {}) {
    QVERIFY(command_); auto callback = std::move(command_);
    callback({lastDefault.commandId, accepted, std::move(message)});
  }
  void respondProfile(bool accepted, std::string message = {}) {
    QVERIFY(command_); auto callback = std::move(command_);
    callback({lastProfile.commandId, accepted, std::move(message)});
  }
  void respondRoute(bool accepted, std::string message = {}) {
    QVERIFY(command_); auto callback = std::move(command_);
    callback({lastRoute.commandId, accepted, std::move(message)});
  }
  CreateLinkRequest lastCreate;
  DestroyLinkRequest lastDestroy;
  SetNodeAudioRequest lastAudio;
  SetDeviceRouteAudioRequest lastRouteAudio;
  SetDefaultRequest lastDefault;
  SetDeviceProfileRequest lastProfile;
  SetDeviceRouteRequest lastRoute;
private:
  SnapshotCallback snapshot_;
  StatusCallback status_;
  CommandCallback command_;
  CommandCallback audioCommand_;
};

class GraphTests final : public QObject {
  Q_OBJECT

private slots:
  void classifiesMedia() {
    QCOMPARE(classifyMedia("Audio/Source"), MediaType::Audio);
    QCOMPARE(classifyMedia("Video/Source"), MediaType::Video);
    QCOMPARE(classifyMedia("Midi/Bridge"), MediaType::Midi);
    QCOMPARE(classifyMedia("Stream/Input/Unknown", "8 bit raw video"), MediaType::Video);
  }

  void convertsPipeWireVolumeForPeople() {
    QCOMPARE(volumeToPercent(1.0), 100.0);
    QCOMPARE(volumeToPercent(0.125), 50.0);
    QCOMPARE(percentToVolume(50.0), 0.125);
    QVERIFY(std::abs(volumeToDecibels(0.5) - (-6.0205999)) < 0.0001);
    QVERIFY(std::isinf(volumeToDecibels(0.0)));
  }

  void assignsRolesAndLanes() {
    GraphSnapshot graph;
    graph.nodes = {
      {.id = 1, .name = "Mic", .technicalName = {}, .stableId = {}, .mediaClass = {},
        .state = {}, .media = MediaType::Unknown, .role = NodeRole::Processor,
        .clientId = std::nullopt, .deviceId = std::nullopt, .audio = std::nullopt},
      {.id = 2, .name = "Filter", .technicalName = {}, .stableId = {}, .mediaClass = {},
        .state = {}, .media = MediaType::Unknown, .role = NodeRole::Processor,
        .clientId = std::nullopt, .deviceId = std::nullopt, .audio = std::nullopt},
      {.id = 3, .name = "Speakers", .technicalName = {}, .stableId = {}, .mediaClass = {},
        .state = {}, .media = MediaType::Unknown, .role = NodeRole::Processor,
        .clientId = std::nullopt, .deviceId = std::nullopt, .audio = std::nullopt},
    };
    graph.ports = {
      {11, 1, "out", {}, PortDirection::Output},
      {21, 2, "in", {}, PortDirection::Input},
      {22, 2, "out", {}, PortDirection::Output},
      {31, 3, "in", {}, PortDirection::Input},
    };
    graph.links = {
      {101, 1, 11, 2, 21, "active", MediaType::Audio},
      {102, 2, 22, 3, 31, "active", MediaType::Audio},
    };

    classifyNodeRoles(graph);
    const auto composed = composeGraph(graph);
    const auto positions = layoutGraph(composed, graph.links);

    const auto node = [&](GlobalId id) -> const GraphNode & {
      return *std::ranges::find(graph.nodes, id, &GraphNode::id);
    };
    QCOMPARE(node(1).role, NodeRole::Source);
    QCOMPARE(node(2).role, NodeRole::Processor);
    QCOMPARE(node(3).role, NodeRole::Destination);
    QVERIFY(positions.at("runtime-node:1").x < positions.at("runtime-node:2").x);
    QVERIFY(positions.at("runtime-node:2").x < positions.at("runtime-node:3").x);
  }

  void loadsVersionedFixture() {
    const auto path = QStringLiteral(WIRERUNNER_SOURCE_DIR "/resources/fixtures/demo-graph.json");
    const auto graph = FixtureGraphSource::load(path);
    QCOMPARE(graph.remoteName, std::string("pipewire-0"));
    QVERIFY(graph.nodes.size() >= 6);
    QVERIFY(graph.links.size() >= 4);
    QVERIFY(std::ranges::any_of(graph.nodes, [](const GraphNode &node) { return node.media == MediaType::Video; }));
    QVERIFY(std::ranges::any_of(graph.nodes, [](const GraphNode &node) { return node.media == MediaType::Midi; }));
    const auto composed = composeGraph(graph);
    const auto obs = std::ranges::find(composed.cards, std::string("client:com.obsproject.Studio"), &GraphCard::key);
    QVERIFY(obs != composed.cards.end());
    QCOMPARE(obs->nodeIds.size(), std::size_t{2});
    QCOMPARE(obs->inputs.size(), std::size_t{2});
  }

  void controllerResolvesCollapsedAnchors() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto fixture = QStringLiteral(WIRERUNNER_SOURCE_DIR "/resources/fixtures/demo-graph.json");
    GraphController controller(std::make_unique<FixtureGraphSource>(fixture), directory.filePath("layout.json"));
    QSignalSpy changed(&controller, &GraphController::graphChanged);
    controller.start();
    QTRY_VERIFY_WITH_TIMEOUT(!changed.isEmpty(), 1000);
    QVERIFY(controller.cards()->count() >= 8);
    QVERIFY(controller.links()->count() >= 7);
    QSet<QString> anchors;
    for (const auto &value : controller.anchors()) anchors.insert(value.toMap().value("key").toString());
    for (int row = 0; row < controller.links()->count(); ++row) {
      const auto link = controller.links()->get(row);
      QVERIFY2(anchors.contains(link.value("outputAnchorKey").toString()), qPrintable(link.value("outputAnchorKey").toString()));
      QVERIFY2(anchors.contains(link.value("inputAnchorKey").toString()), qPrintable(link.value("inputAnchorKey").toString()));
    }

    controller.toggleCard(QStringLiteral("client:org.mozilla.firefox"));
    const auto firefoxLink = controller.links()->get(0);
    QCOMPARE(firefoxLink.value("outputAnchorKey").toString(), QStringLiteral("port:100"));
    anchors.clear();
    for (const auto &value : controller.anchors()) anchors.insert(value.toMap().value("key").toString());
    QVERIFY(anchors.contains(QStringLiteral("port:100")));

    controller.setMediaFilter(QStringLiteral("video"));
    int visibleCards = 0;
    for (int row = 0; row < controller.cards()->count(); ++row)
      if (controller.cards()->get(row).value("visible").toBool()) ++visibleCards;
    QCOMPARE(visibleCards, 2);
    QCOMPARE(controller.cardRects().size(), 2);
  }

  void validatesRoutesAndDetectsCycles() {
    GraphSnapshot graph;
    graph.nodes = {
      {1, "", "", "", "", "", MediaType::Audio, NodeRole::Processor, std::nullopt, std::nullopt, std::nullopt},
      {2, "", "", "", "", "", MediaType::Audio, NodeRole::Processor, std::nullopt, std::nullopt, std::nullopt},
      {3, "", "", "", "", "", MediaType::Video, NodeRole::Processor, std::nullopt, std::nullopt, std::nullopt}
    };
    graph.ports = {
      {11, 1, "", "", PortDirection::Output, MediaType::Audio, "", 0},
      {21, 2, "", "", PortDirection::Input, MediaType::Audio, "", 0},
      {22, 2, "", "", PortDirection::Output, MediaType::Audio, "", 0},
      {12, 1, "", "", PortDirection::Input, MediaType::Audio, "", 0},
      {31, 3, "", "", PortDirection::Input, MediaType::Video, "", 0}
    };
    auto result = assessLink(graph, 11, 21);
    QVERIFY(result.compatible);
    QVERIFY(!result.probableCycle);
    QVERIFY(!assessLink(graph, 11, 31).compatible);

    graph.links.push_back({101, 1, 11, 2, 21, "active", MediaType::Audio});
    QVERIFY(!assessLink(graph, 11, 21).compatible);
    result = assessLink(graph, 22, 12);
    QVERIFY(result.compatible);
    QVERIFY(result.probableCycle);
  }

  void routesAndUndoesOnlyAfterObservedSnapshots() {
    QTemporaryDir directory;
    auto source = std::make_unique<ControllableGraphSource>();
    auto *backend = source.get();
    GraphController controller(std::move(source), directory.filePath("layout.json"));
    controller.start();
    GraphSnapshot graph;
    graph.revision = 1;
    graph.remoteName = "pipewire-0";
    graph.nodes = {
      {1, "Source", "", "source", "", "", MediaType::Audio, NodeRole::Processor, std::nullopt, std::nullopt, std::nullopt},
      {2, "Sink", "", "sink", "", "", MediaType::Audio, NodeRole::Processor, std::nullopt, std::nullopt, std::nullopt}
    };
    graph.ports = {
      {11, 1, "out", "", PortDirection::Output, MediaType::Audio, "", 0},
      {21, 2, "in", "", PortDirection::Input, MediaType::Audio, "", 0}
    };
    backend->publish(graph);
    controller.beginRoute(11, 100, 100);
    controller.finishRouteToPort(21);
    QVERIFY(controller.commandPending());
    QVERIFY(!controller.canUndo());
    QCOMPARE(backend->lastCreate.outputPortId, GlobalId{11});
    QCOMPARE(backend->lastCreate.inputPortId, GlobalId{21});
    QVERIFY(backend->lastCreate.linger);
    backend->respond(true, "accepted");
    graph.revision = 2;
    graph.links.push_back({101, 1, 11, 2, 21, "active", MediaType::Audio, false, true, true, 0100});
    backend->publish(graph);
    QTRY_VERIFY(controller.canUndo());

    controller.undo();
    QCOMPARE(backend->lastDestroy.linkId, GlobalId{101});
    backend->respond(true, "accepted");
    graph.revision = 3;
    graph.links.clear();
    backend->publish(graph);
    QTRY_VERIFY(controller.canRedo());
  }

  void requiresConfirmationForFeedbackLinks() {
    QTemporaryDir directory;
    auto source = std::make_unique<ControllableGraphSource>();
    auto *backend = source.get();
    GraphController controller(std::move(source), directory.filePath("layout.json"));
    controller.start();
    GraphSnapshot graph;
    graph.revision = 1;
    graph.remoteName = "pipewire-0";
    graph.nodes = {
      {1, "First", "", "first", "", "", MediaType::Audio, NodeRole::Processor, std::nullopt, std::nullopt, std::nullopt},
      {2, "Second", "", "second", "", "", MediaType::Audio, NodeRole::Processor, std::nullopt, std::nullopt, std::nullopt}
    };
    graph.ports = {
      {11, 1, "out", "", PortDirection::Output, MediaType::Audio, "", 0100},
      {12, 1, "in", "", PortDirection::Input, MediaType::Audio, "", 0100},
      {21, 2, "in", "", PortDirection::Input, MediaType::Audio, "", 0100},
      {22, 2, "out", "", PortDirection::Output, MediaType::Audio, "", 0100}
    };
    graph.links.push_back({101, 1, 11, 2, 21, "active", MediaType::Audio, false, true, false, 0100});
    backend->publish(graph);
    controller.beginRoute(22, 100, 100);
    controller.finishRouteToPort(12);
    QVERIFY(controller.feedbackConfirmation());
    QVERIFY(!controller.commandPending());
    QCOMPARE(backend->lastCreate.commandId, CommandId{0});
    controller.confirmFeedback();
    QVERIFY(controller.commandPending());
    QVERIFY(backend->lastCreate.feedback);
  }

  void controlsEachAudioNodeInAGroupedCard() {
    QTemporaryDir directory;
    auto source = std::make_unique<ControllableGraphSource>();
    auto *backend = source.get();
    GraphController controller(std::move(source), directory.filePath("layout.json"));
    controller.start();
    GraphSnapshot graph;
    graph.revision = 1;
    graph.remoteName = "pipewire-0";
    graph.clients = {{7, "Browser", "org.browser"}};
    graph.nodes = {
      {.id = 1, .name = "Music tab", .technicalName = "music", .stableId = "music",
        .mediaClass = "Stream/Output/Audio", .state = "running", .media = MediaType::Audio,
        .role = NodeRole::Source, .clientId = 7, .deviceId = std::nullopt,
        .audio = NodeAudioControl{.volume = 0.729F, .channelVolumes = {0.729F, 0.3645F},
          .minimumVolume = 0.0F, .maximumVolume = 1.5F, .muted = false,
          .hasVolume = true, .hasMute = true, .writable = true}},
      {.id = 2, .name = "Meeting tab", .technicalName = "meeting", .stableId = "meeting",
        .mediaClass = "Stream/Output/Audio", .state = "running", .media = MediaType::Audio,
        .role = NodeRole::Source, .clientId = 7, .deviceId = std::nullopt,
        .audio = NodeAudioControl{.volume = 1.0F, .channelVolumes = {}, .minimumVolume = 0.0F, .maximumVolume = 1.0F,
          .muted = false, .hasVolume = true, .hasMute = true, .writable = true}}
    };
    backend->publish(graph);
    controller.selectCard(QStringLiteral("client:org.browser"));
    QCOMPARE(controller.selected().value("audioControls").toList().size(), 2);

    controller.setNodeVolume(1, 50.0);
    QCOMPARE(backend->lastAudio.nodeId, GlobalId{1});
    QVERIFY(!backend->lastAudio.volume.has_value());
    QCOMPARE(backend->lastAudio.channelVolumes.size(), std::size_t{2});
    QVERIFY(std::abs(backend->lastAudio.channelVolumes[0] - 0.125F) < 0.0001F);
    QVERIFY(std::abs(backend->lastAudio.channelVolumes[1] - 0.0625F) < 0.0001F);
    backend->respondAudio(true, "accepted");
    graph.revision = 2;
    graph.nodes[0].audio->volume = 0.125F;
    graph.nodes[0].audio->channelVolumes = {0.125F, 0.0625F};
    backend->publish(graph);
    QTRY_COMPARE(controller.selected().value("audioControls").toList().at(0).toMap().value("volume").toInt(), 50);

    controller.setNodeMuted(2, true);
    QCOMPARE(backend->lastAudio.nodeId, GlobalId{2});
    QCOMPARE(backend->lastAudio.muted, std::optional<bool>{true});
    backend->respondAudio(false, "denied");
    QTRY_VERIFY(controller.noticeText().contains(QStringLiteral("denied")));
    QCOMPARE(controller.selected().value("audioControls").toList().at(1).toMap().value("muted").toBool(), false);
  }

  void controlsDeviceRoutesWithoutFlatteningChannels() {
    QTemporaryDir directory;
    auto source = std::make_unique<ControllableGraphSource>();
    auto *backend = source.get();
    GraphController controller(std::move(source), directory.filePath("layout.json"));
    controller.start();
    GraphSnapshot graph;
    graph.revision = 1;
    graph.remoteName = "pipewire-0";
    NodeAudioControl routeAudio{.volume = 0.25F, .channelVolumes = {0.25F, 0.125F, 0.064F},
      .channelMap = {3, 4, 11}, .softVolumes = {0.25F, 0.125F, 0.064F},
      .minimumVolume = 0.0F, .maximumVolume = 1.0F, .muted = false,
      .hasVolume = true, .hasMute = true, .writable = true};
    graph.devices = {{50, "Studio interface", "usb-focusrite", "Audio/Device",
      {{.index = 7, .deviceIndex = 0, .direction = PortDirection::Output,
        .name = "analog-output", .description = "Playback", .active = true, .audio = routeAudio}}}};
    graph.nodes = {{.id = 5, .name = "Studio playback", .technicalName = "alsa_output.test",
      .stableId = "playback", .mediaClass = "Audio/Sink", .state = "running", .media = MediaType::Audio,
      .role = NodeRole::Destination, .clientId = std::nullopt, .deviceId = 50, .profileDeviceId = 0,
      .audio = NodeAudioControl{.volume = 1.0F, .hasVolume = true, .writable = true}}};
    graph.ports = {{51, 5, "playback_FL", "FL", PortDirection::Input, MediaType::Audio},
      {52, 5, "playback_FR", "FR", PortDirection::Input, MediaType::Audio}};
    backend->publish(graph);
    controller.selectCard(QStringLiteral("device:usb-focusrite"));
    const auto selected = controller.selected();
    QCOMPARE(selected.value("audioControls").toList().size(), 1);
    QCOMPARE(selected.value("portTop").toInt(), 148);
    const auto control = selected.value("inlineAudio").toMap();
    QCOMPARE(control.value("targetKind").toString(), QStringLiteral("route"));
    QCOMPARE(control.value("channels").toList().size(), 3);

    controller.setAudioVolume(control, 80.0);
    QCOMPARE(backend->lastRouteAudio.deviceId, GlobalId{50});
    QCOMPARE(backend->lastRouteAudio.routeIndex, 7);
    QCOMPARE(backend->lastRouteAudio.routeDeviceId, 0);
    QCOMPARE(backend->lastRouteAudio.channelVolumes.size(), std::size_t{3});
    QVERIFY(std::abs(backend->lastRouteAudio.channelVolumes[0] - 0.512F) < 0.0001F);
    QVERIFY(std::abs(backend->lastRouteAudio.channelVolumes[1] - 0.256F) < 0.0001F);
    QVERIFY(std::abs(backend->lastRouteAudio.channelVolumes[2] - 0.064F) < 0.0001F);
    backend->respondRouteAudio(true, "saved");

    controller.setAudioChannelVolume(control, 2, 50.0);
    QCOMPARE(backend->lastRouteAudio.channelVolumes[0], 0.25F);
    QCOMPARE(backend->lastRouteAudio.channelVolumes[1], 0.125F);
    QCOMPARE(backend->lastRouteAudio.channelVolumes[2], 0.125F);
  }

  void confirmsDefaultsProfilesAndRoutesFromSnapshots() {
    QTemporaryDir directory;
    auto source = std::make_unique<ControllableGraphSource>();
    auto *backend = source.get();
    GraphController controller(std::move(source), directory.filePath("layout.json"));
    controller.start();
    GraphSnapshot graph;
    graph.revision = 1; graph.remoteName = "pipewire-0";
    graph.devices = {{.id = 50, .name = "Interface", .stableId = "usb-interface", .mediaClass = "Audio/Device",
      .routes = {
        {.index = 1, .deviceIndex = 0, .direction = PortDirection::Output, .name = "speakers",
          .description = "Speakers", .availability = Availability::Available, .active = true},
        {.index = 2, .deviceIndex = 0, .direction = PortDirection::Output, .name = "headphones",
          .description = "Headphones", .availability = Availability::Available, .active = false}},
      .profiles = {
        {.index = 10, .name = "stereo", .description = "Stereo", .availability = Availability::Available, .active = true},
        {.index = 20, .name = "pro-audio", .description = "Pro Audio", .availability = Availability::Available, .active = false}},
      .writable = true}};
    graph.nodes = {{.id = 5, .name = "Interface output", .technicalName = "alsa_output.interface",
      .stableId = "output", .mediaClass = "Audio/Sink", .state = "running", .media = MediaType::Audio,
      .role = NodeRole::Destination, .deviceId = 50, .profileDeviceId = 0}};
    graph.ports = {{51, 5, "playback", "FL,FR", PortDirection::Input, MediaType::Audio}};
    graph.defaults = {{DefaultKind::AudioSink, "old.output", "old.output"}};
    backend->publish(graph);
    controller.selectCard(QStringLiteral("device:usb-interface"));
    QCOMPARE(controller.selected().value("profiles").toList().size(), 2);
    QCOMPARE(controller.selected().value("routeOptions").toList().size(), 2);
    QCOMPARE(controller.selected().value("defaultActions").toList().size(), 1);

    controller.setDefaultTarget(QStringLiteral("audioSink"), 5);
    QCOMPARE(backend->lastDefault.nodeName, std::string("alsa_output.interface"));
    backend->respondDefault(true, "accepted");
    graph.revision = 2;
    graph.defaults[0] = {DefaultKind::AudioSink, "alsa_output.interface", "alsa_output.interface"};
    backend->publish(graph);
    QVERIFY(controller.canUndo());

    controller.setDeviceProfile(50, 20);
    QCOMPARE(backend->lastProfile.profileIndex, 20);
    backend->respondProfile(true, "accepted");
    const auto nodes = graph.nodes;
    const auto ports = graph.ports;
    graph.revision = 3; graph.nodes.clear(); graph.ports.clear();
    backend->publish(graph);
    QCOMPARE(controller.selectedKey(), QStringLiteral("device:usb-interface"));
    graph.revision = 4; graph.nodes = nodes; graph.ports = ports;
    graph.devices[0].profiles[0].active = false; graph.devices[0].profiles[1].active = true;
    backend->publish(graph);

    controller.setDeviceRoute(50, 2, 0);
    QCOMPARE(backend->lastRoute.routeIndex, 2);
    backend->respondRoute(true, "accepted");
    graph.revision = 5; graph.devices[0].routes[0].active = false; graph.devices[0].routes[1].active = true;
    backend->publish(graph);
    controller.undo();
    QCOMPARE(backend->lastRoute.routeIndex, 1);
    QCOMPARE(backend->lastRoute.routeDeviceId, 0);
  }

  void reportsPolicyRestoredRoutesWithoutFightingThem() {
    QTemporaryDir directory;
    auto source = std::make_unique<ControllableGraphSource>();
    auto *backend = source.get();
    GraphController controller(std::move(source), directory.filePath("layout.json"));
    controller.start();
    GraphSnapshot graph;
    graph.revision = 1;
    graph.remoteName = "pipewire-0";
    graph.nodes = {
      {1, "Source", "", "source", "", "", MediaType::Audio, NodeRole::Processor, std::nullopt, std::nullopt, std::nullopt},
      {2, "Sink", "", "sink", "", "", MediaType::Audio, NodeRole::Processor, std::nullopt, std::nullopt, std::nullopt}
    };
    graph.ports = {
      {11, 1, "out", "", PortDirection::Output, MediaType::Audio, "", 0100},
      {21, 2, "in", "", PortDirection::Input, MediaType::Audio, "", 0100}
    };
    graph.links.push_back({101, 1, 11, 2, 21, "active", MediaType::Audio, false, true, false, 0100});
    backend->publish(graph);
    controller.selectLink(QStringLiteral("link:101"));
    controller.disconnectSelected();
    QCOMPARE(backend->lastDestroy.linkId, GlobalId{101});
    backend->respond(true, "accepted");
    graph.revision = 2;
    graph.links = {{102, 1, 11, 2, 21, "active", MediaType::Audio, false, true, false, 0100}};
    backend->publish(graph);
    QTRY_VERIFY(!controller.commandPending());
    QVERIFY(!controller.canUndo());
    QVERIFY(controller.noticeText().contains(QStringLiteral("restored")));
  }

  void persistsStableCardLayout() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto fixture = QStringLiteral(WIRERUNNER_SOURCE_DIR "/resources/fixtures/demo-graph.json");
    const auto layout = directory.filePath("layout.json");
    {
      GraphController controller(std::make_unique<FixtureGraphSource>(fixture), layout);
      controller.start();
      controller.moveCard(QStringLiteral("client:org.mozilla.firefox"), 777.0, 333.0);
      controller.toggleCard(QStringLiteral("client:org.mozilla.firefox"));
    }
    GraphController restored(std::make_unique<FixtureGraphSource>(fixture), layout);
    restored.start();
    QVariantMap firefox;
    for (int row = 0; row < restored.cards()->count(); ++row) {
      const auto item = restored.cards()->get(row);
      if (item.value("key") == QStringLiteral("client:org.mozilla.firefox")) firefox = item;
    }
    QVERIFY(!firefox.isEmpty());
    QCOMPARE(firefox.value("x").toDouble(), 777.0);
    QCOMPARE(firefox.value("y").toDouble(), 333.0);
    QVERIFY(firefox.value("expanded").toBool());
  }

  void scopesAndRecoversLayoutDocuments() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = directory.filePath("layout.json");
    LayoutStore store(path);
    store.setCard(QStringLiteral("studio"), QStringLiteral("node:a"), {{12.0, 34.0}, true});
    QVERIFY(store.save());
    LayoutStore restored(path);
    QVERIFY(restored.card(QStringLiteral("studio"), QStringLiteral("node:a")).has_value());
    QVERIFY(!restored.card(QStringLiteral("other"), QStringLiteral("node:a")).has_value());

    QFile broken(path);
    QVERIFY(broken.open(QIODevice::WriteOnly | QIODevice::Truncate));
    broken.write("not json");
    broken.close();
    LayoutStore recovered(path);
    QVERIFY(!recovered.card(QStringLiteral("studio"), QStringLiteral("node:a")).has_value());
  }

  void focusesAConnectedComponentWithoutHidingOthers() {
    QTemporaryDir directory;
    const auto fixture = QStringLiteral(WIRERUNNER_SOURCE_DIR "/resources/fixtures/demo-graph.json");
    GraphController controller(std::make_unique<FixtureGraphSource>(fixture), directory.filePath("layout.json"));
    controller.start();
    controller.selectCard(QStringLiteral("client:org.mozilla.firefox"));
    controller.focusSelected();
    QVERIFY(controller.focusActive());
    bool midiFocused = true;
    bool cameraFocused = true;
    bool firefoxFocused = false;
    for (int row = 0; row < controller.cards()->count(); ++row) {
      const auto item = controller.cards()->get(row);
      if (item.value("key") == QStringLiteral("device:usb-midi-keyboard")) midiFocused = item.value("focused").toBool();
      if (item.value("key") == QStringLiteral("device:usb-camera-01")) cameraFocused = item.value("focused").toBool();
      if (item.value("key") == QStringLiteral("client:org.mozilla.firefox")) firefoxFocused = item.value("focused").toBool();
    }
    QVERIFY(firefoxFocused);
    QVERIFY(!midiFocused);
    QVERIFY(!cameraFocused);
    controller.clearFocus();
    QVERIFY(!controller.focusActive());
  }

  void preservesContextThroughGraphChurn() {
    QTemporaryDir directory;
    auto source = std::make_unique<ControllableGraphSource>();
    auto *publisher = source.get();
    GraphController controller(std::move(source), directory.filePath("layout.json"));
    controller.start();
    const auto makeGraph = [](std::string remote, std::vector<std::pair<GlobalId, std::string>> identities) {
      GraphSnapshot graph;
      graph.remoteName = std::move(remote);
      for (const auto &[id, stable] : identities) {
        graph.nodes.push_back({.id = id, .name = "Node " + stable, .technicalName = "node." + stable,
          .stableId = stable, .mediaClass = "Audio/Node", .state = "running", .media = MediaType::Audio,
          .role = NodeRole::Processor, .clientId = std::nullopt, .deviceId = std::nullopt,
          .audio = std::nullopt});
      }
      return graph;
    };
    publisher->publish(makeGraph("pipewire-0", {{1, "a"}, {2, "b"}}));
    controller.moveCard(QStringLiteral("node:a"), 500.0, 240.0);
    controller.selectCard(QStringLiteral("node:a"));

    publisher->publish(makeGraph("pipewire-0", {{20, "b"}, {10, "a"}, {30, "new"}}));
    QCOMPARE(controller.selectedKey(), QStringLiteral("node:a"));
    QVariantMap cardA;
    for (int row = 0; row < controller.cards()->count(); ++row) {
      const auto item = controller.cards()->get(row);
      if (item.value("key") == QStringLiteral("node:a")) cardA = item;
    }
    QCOMPARE(cardA.value("x").toDouble(), 500.0);
    QCOMPARE(cardA.value("y").toDouble(), 240.0);

    publisher->publish(makeGraph("pipewire-0", {{20, "b"}}));
    QVERIFY(controller.selectedKey().isEmpty());
    publisher->publish(makeGraph("pipewire-0", {{99, "a"}}));
    QCOMPARE(controller.cards()->get(0).value("x").toDouble(), 500.0);

    publisher->publish(makeGraph("other-remote", {{99, "a"}}));
    QVERIFY(controller.cards()->get(0).value("x").toDouble() != 500.0);
  }

  void composesLargeGraphs() {
    GraphSnapshot graph;
    graph.nodes.reserve(250);
    graph.ports.reserve(500);
    graph.links.reserve(1000);
    for (GlobalId index = 1; index <= 250; ++index) {
      graph.nodes.push_back({.id = index, .name = "Node " + std::to_string(index),
        .technicalName = "node." + std::to_string(index), .stableId = "stable." + std::to_string(index),
        .mediaClass = "Audio/Node", .state = "running", .media = MediaType::Audio,
        .role = NodeRole::Processor, .clientId = std::nullopt, .deviceId = std::nullopt,
        .audio = std::nullopt});
      graph.ports.push_back({index * 10, index, "input", "FL", PortDirection::Input, MediaType::Audio});
      graph.ports.push_back({index * 10 + 1, index, "output", "FL", PortDirection::Output, MediaType::Audio});
    }
    GlobalId linkId = 1;
    for (GlobalId index = 1; index < 250; ++index) {
      for (int parallel = 0; parallel < 4 && graph.links.size() < 1000; ++parallel) {
        graph.links.push_back({linkId++, index, index * 10 + 1, index + 1, (index + 1) * 10,
          "active", MediaType::Audio});
      }
    }
    while (graph.links.size() < 1000) {
      graph.links.push_back({linkId++, 250, 2501, 1, 10, "active", MediaType::Audio});
    }
    const auto composed = composeGraph(graph);
    const auto positions = layoutGraph(composed, graph.links);
    QCOMPARE(composed.cards.size(), std::size_t{250});
    QCOMPARE(graph.ports.size(), std::size_t{500});
    QCOMPARE(graph.links.size(), std::size_t{1000});
    QCOMPARE(positions.size(), std::size_t{250});
  }

  void rejectsUnsupportedFixture() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QFile file(directory.filePath(QStringLiteral("future.json")));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(R"({"version":2,"nodes":[],"ports":[],"links":[]})");
    file.close();
    QVERIFY_EXCEPTION_THROWN(FixtureGraphSource::load(file.fileName()), std::runtime_error);
  }
};

QTEST_GUILESS_MAIN(GraphTests)
#include "graph_tests.moc"
