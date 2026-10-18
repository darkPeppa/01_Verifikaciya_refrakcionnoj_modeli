cmake_minimum_required(VERSION 3.16)
project(XrayCRLGeant4 VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Batch mode is the primary mode; UI/vis drivers are optional.
option(XRCL_WITH_GEANT4_UIVIS "Request Geant4 UI and visualisation drivers" OFF)
if(XRCL_WITH_GEANT4_UIVIS)
  find_package(Geant4 11.0 REQUIRED ui_all vis_all)
else()
  find_package(Geant4 11.0 REQUIRED)
endif()
find_package(nlohmann_json 3.2 REQUIRED)
message(STATUS "Geant4 version: ${Geant4_VERSION}")

file(GLOB XRCL_SOURCES CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/src/*.cc)
add_library(xrcl_core STATIC ${XRCL_SOURCES})
target_include_directories(xrcl_core PUBLIC ${PROJECT_SOURCE_DIR}/include ${Geant4_INCLUDE_DIRS})
target_compile_definitions(xrcl_core PUBLIC ${Geant4_DEFINITIONS})
target_link_libraries(xrcl_core PUBLIC ${Geant4_LIBRARIES} nlohmann_json::nlohmann_json)
target_compile_options(xrcl_core PRIVATE -Wall -Wextra -Wshadow)

add_executable(xraycrl xraycrl.cc)
target_link_libraries(xraycrl PRIVATE xrcl_core)

# ---------------- tests ----------------
enable_testing()
add_executable(unit_tests tests/unit/unit_tests.cc)
target_link_libraries(unit_tests PRIVATE xrcl_core)
add_test(NAME unit_tests COMMAND unit_tests)

find_package(Python3 COMPONENTS Interpreter)
if(Python3_FOUND)
  add_test(NAME validation
           COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/run_validation.py
                   --app $<TARGET_FILE:xraycrl> --root ${PROJECT_SOURCE_DIR}
                   --out ${PROJECT_BINARY_DIR}/validation)
  set_tests_properties(validation PROPERTIES TIMEOUT 7200)
endif()
