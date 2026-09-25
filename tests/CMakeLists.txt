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
