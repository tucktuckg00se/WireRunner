// SPDX-License-Identifier: GPL-3.0-or-later
#include "backend/FixtureGraphSource.hpp"
#include "domain/Graph.hpp"
#include "presentation/GraphController.hpp"
#include "presentation/LayoutStore.hpp"
#include "presentation/StableListModel.hpp"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

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
private:
  SnapshotCallback snapshot_;
  StatusCallback status_;
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

  void assignsRolesAndLanes() {
    GraphSnapshot graph;
    graph.nodes = {
      {.id = 1, .name = "Mic", .technicalName = {}, .stableId = {}, .mediaClass = {},
        .state = {}, .media = MediaType::Unknown, .role = NodeRole::Processor,
        .clientId = std::nullopt, .deviceId = std::nullopt},
      {.id = 2, .name = "Filter", .technicalName = {}, .stableId = {}, .mediaClass = {},
        .state = {}, .media = MediaType::Unknown, .role = NodeRole::Processor,
        .clientId = std::nullopt, .deviceId = std::nullopt},
      {.id = 3, .name = "Speakers", .technicalName = {}, .stableId = {}, .mediaClass = {},
        .state = {}, .media = MediaType::Unknown, .role = NodeRole::Processor,
        .clientId = std::nullopt, .deviceId = std::nullopt},
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
          .role = NodeRole::Processor, .clientId = std::nullopt, .deviceId = std::nullopt});
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
        .role = NodeRole::Processor, .clientId = std::nullopt, .deviceId = std::nullopt});
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

QTEST_APPLESS_MAIN(GraphTests)
#include "graph_tests.moc"
