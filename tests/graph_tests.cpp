// SPDX-License-Identifier: GPL-3.0-or-later
#include "backend/FixtureGraphSource.hpp"
#include "domain/Graph.hpp"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

using namespace wirerunner;

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
        .clientId = std::nullopt, .deviceId = std::nullopt, .x = 0.0, .y = 0.0},
      {.id = 2, .name = "Filter", .technicalName = {}, .stableId = {}, .mediaClass = {},
        .state = {}, .media = MediaType::Unknown, .role = NodeRole::Processor,
        .clientId = std::nullopt, .deviceId = std::nullopt, .x = 0.0, .y = 0.0},
      {.id = 3, .name = "Speakers", .technicalName = {}, .stableId = {}, .mediaClass = {},
        .state = {}, .media = MediaType::Unknown, .role = NodeRole::Processor,
        .clientId = std::nullopt, .deviceId = std::nullopt, .x = 0.0, .y = 0.0},
    };
    graph.ports = {
      {11, 1, "out", {}, PortDirection::Output},
      {21, 2, "in", {}, PortDirection::Input},
      {22, 2, "out", {}, PortDirection::Output},
      {31, 3, "in", {}, PortDirection::Input},
    };

    layoutGraph(graph);

    const auto node = [&](GlobalId id) -> const GraphNode & {
      return *std::ranges::find(graph.nodes, id, &GraphNode::id);
    };
    QCOMPARE(node(1).role, NodeRole::Source);
    QCOMPARE(node(2).role, NodeRole::Processor);
    QCOMPARE(node(3).role, NodeRole::Destination);
    QVERIFY(node(1).x < node(2).x);
    QVERIFY(node(2).x < node(3).x);
  }

  void loadsVersionedFixture() {
    const auto path = QStringLiteral(WIRERUNNER_SOURCE_DIR "/resources/fixtures/demo-graph.json");
    const auto graph = FixtureGraphSource::load(path);
    QCOMPARE(graph.remoteName, std::string("pipewire-0"));
    QVERIFY(graph.nodes.size() >= 6);
    QVERIFY(graph.links.size() >= 4);
    QVERIFY(std::ranges::any_of(graph.nodes, [](const GraphNode &node) { return node.media == MediaType::Video; }));
    QVERIFY(std::ranges::any_of(graph.nodes, [](const GraphNode &node) { return node.media == MediaType::Midi; }));
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
