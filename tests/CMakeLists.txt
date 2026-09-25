qt_add_executable(wirerunner_tests
  graph_tests.cpp
)
target_link_libraries(wirerunner_tests PRIVATE wirerunner_core Qt6::Test)
target_compile_definitions(wirerunner_tests PRIVATE
  WIRERUNNER_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)

add_test(NAME unit COMMAND wirerunner_tests)
add_test(NAME qml_demo_smoke COMMAND wirerunner --demo --quit-after 250)
add_test(NAME qml_backend_startup_smoke COMMAND wirerunner --quit-after 250)
set_tests_properties(qml_demo_smoke qml_backend_startup_smoke PROPERTIES
  ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_FATAL_WARNINGS=1"
  TIMEOUT 10
)

find_program(WIRERUNNER_QT6_QMLTESTRUNNER qmltestrunner
  HINTS /usr/lib/qt6/bin /usr/lib64/qt6/bin /usr/local/lib/qt6/bin
  NO_DEFAULT_PATH REQUIRED)
add_test(NAME qml_interactions
  COMMAND ${WIRERUNNER_QT6_QMLTESTRUNNER} -input ${CMAKE_CURRENT_SOURCE_DIR}/qml
)
set_tests_properties(qml_interactions PROPERTIES
  ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
  TIMEOUT 15
)
