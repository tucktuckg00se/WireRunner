add_library(wirerunner_core STATIC
  domain/Graph.cpp
  domain/Routing.cpp
  backend/FixtureGraphSource.cpp
  presentation/LayoutStore.cpp
  presentation/StableListModel.cpp
  presentation/GraphController.cpp
)
target_include_directories(wirerunner_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(wirerunner_core PUBLIC Qt6::Core)

qt_add_executable(wirerunner
  main.cpp
  backend/WirePlumberGraphSource.cpp
  presentation/LinkLayer.cpp
)

target_include_directories(wirerunner PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(wirerunner PRIVATE
  wirerunner_core
  Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2
  PkgConfig::WirePlumber
)

qt_add_qml_module(wirerunner
  URI WireRunner
  VERSION 1.0
  QML_FILES qml/Main.qml qml/GraphCard.qml qml/GraphText.qml qml/InspectorPane.qml qml/LevelControl.qml
)

qt_add_resources(wirerunner wirerunner_resources
  PREFIX "/"
  BASE "../resources"
  FILES ../resources/fixtures/demo-graph.json
)

install(TARGETS wirerunner RUNTIME DESTINATION bin)
