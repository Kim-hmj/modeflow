cmake_minimum_required(VERSION 3.0)

project(r1-modeflow)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_FORTIFY_SOURCE=2 -g -O0 -Wl,--no-as-needed -lpthread -lrt -pthread")

include(GNUInstallDirs)
include(FindProtobuf)
find_package(Protobuf REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

pkg_check_modules(DBUSCPP dbus-cpp>=5.0 REQUIRED)
pkg_check_modules(ADK_IPC adk-ipc REQUIRED)
pkg_check_modules(ADK_CONFIG adk-config REQUIRED)
pkg_check_modules(ADK_MESSAGE_SERVICE adk-message-service REQUIRED)
pkg_check_modules(ADK_MESSAGES adk-messages REQUIRED)

#pkg_check_modules(GST REQUIRED gstreamer-1.0)
#pkg_check_modules(GST_APP REQUIRED gstreamer-app-1.0)
#pkg_check_modules(GST_AUD REQUIRED gstreamer-audio-1.0)

add_definitions(-DCONFIG_FILENAME="/data/adk.modeflow.db")

link_directories(
  ${ADK_IPC_LIBRARY_DIRS}
  ${ADK_CONFIG_LIBRARY_DIRS}
  ${DBUSCPP_LIBRARY_DIRS}
  ${ADK_MESSAGES_LIBRARIES}
)

AUX_SOURCE_DIRECTORY(src/controller DIR_CONTROLLER)
AUX_SOURCE_DIRECTORY(src/actuator DIR_ACTUATOR)
AUX_SOURCE_DIRECTORY(src/sensor DIR_SENSOR)
AUX_SOURCE_DIRECTORY(src/libmodeflow DIR_LIBMODEFLOW)
AUX_SOURCE_DIRECTORY(src/charging DIR_CHARGING)
AUX_SOURCE_DIRECTORY(src/ DIR_SRC)
AUX_SOURCE_DIRECTORY(src/arm DIR_ARM)
add_executable (
  ${PROJECT_NAME}
  ${DIR_SRC}
  ${DIR_ARM}
  ${DIR_CHARGING}
  ${DIR_CONTROLLER}
  ${DIR_ACTUATOR}
  ${DIR_SENSOR}
  ${DIR_LIBMODEFLOW}
)

target_link_libraries (
  ${PROJECT_NAME}
  ${CMAKE_THREAD_LIBS_INIT}
  ${ADK_IPC_LIBRARIES}
  ${ADK_MESSAGES_LIBRARIES}
  ${ADK_MESSAGE_SERVICE_LIBRARIES}
  ${ADK_CONFIG_LIBRARIES}
  ${DBUSCPP_LIBRARIES}
  ${PROTOBUF_LIBRARY}
  ${GST_LIBRARIES}
  ${GST_APP_LIBRARIES}
  ${GST_AUD_LIBRARIES}
  systemd
)

target_include_directories(
  ${PROJECT_NAME} PUBLIC
  ${ADK_IPC_INCLUDE_DIRS}
  ${ADK_MESSAGES_INCLUDE_DIRS}
  ${ADK_MESSAGE_SERVICE_INCLUDE_DIRS}
  ${ADK_CONFIG_INCLUDE_DIRS}
  ${DBUSCPP_INCLUDE_DIRS}
  ${GST_INCLUDE_DIRS}
  ${CMAKE_SOURCE_DIR}/include
  src/include
  src/libmodeflow
  src/charging/libmodeflow
  src
)

target_compile_definitions(
${PROJECT_NAME} PUBLIC
MULTI_THREAD
)

target_compile_options(
${PROJECT_NAME} PUBLIC
${GST_CFLAGS_OTHER} -Wno-unused-result -fpermissive
)

install(
  TARGETS ${PROJECT_NAME} RUNTIME
  DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(
  FILES src/monitormfl.sh
  DESTINATION ${CMAKE_INSTALL_BINDIR}
  PERMISSIONS WORLD_EXECUTE
)
