cmake_minimum_required(VERSION 3.0)

project(r1-modeflow)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_FORTIFY_SOURCE=2 -g -O2 -Wl,--no-as-needed -lpthread -lrt -ladk-config -lpthread -pg ")

AUX_SOURCE_DIRECTORY(controller DIR_CONTROLLER)
AUX_SOURCE_DIRECTORY(actuator DIR_ACTUATOR)
AUX_SOURCE_DIRECTORY(sensor DIR_SENSOR)
AUX_SOURCE_DIRECTORY(libmodeflow DIR_LIBMODEFLOW)
AUX_SOURCE_DIRECTORY(charging DIR_CHARGING)
AUX_SOURCE_DIRECTORY(. DIR_SRC)
AUX_SOURCE_DIRECTORY(x86 DIR_X86)
add_executable (
  ${PROJECT_NAME}
  ${DIR_SRC}
  ${DIR_X86}
  ${DIR_CHARGING}
  ${DIR_CONTROLLER}
  ${DIR_ACTUATOR}
  ${DIR_SENSOR}
  ${DIR_LIBMODEFLOW}
)

set(CONFIG_MODEFLOW_DB "adk.modeflow.db" CACHE STRING "./")
add_definitions(-DCONFIG_FILENAME="${CONFIG_MODEFLOW_DB}")

target_link_libraries (
  ${PROJECT_NAME}
  systemd
)

target_include_directories(
  ${PROJECT_NAME} PUBLIC
  include
  libmodeflow
  .
)

target_compile_definitions(
${PROJECT_NAME} PUBLIC
X86_DEBUG
TEST
MULTI_THREAD
)

target_compile_options(
${PROJECT_NAME} PUBLIC
${GST_CFLAGS_OTHER} -Wno-unused-result -fpermissive
)

