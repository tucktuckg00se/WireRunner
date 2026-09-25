// SPDX-License-Identifier: GPL-3.0-or-later
#include "backend/WirePlumberGraphSource.hpp"

#include <wp/wp.h>

#include <charconv>
#include <unordered_map>

namespace wirerunner {
namespace {

std::string property(WpPipewireObject *object, const char *key) {
  const auto *value = wp_pipewire_object_get_property(object, key);
  return value ? value : "";
}

std::string firstProperty(WpPipewireObject *object, std::initializer_list<const char *> keys) {
  for (const auto *key : keys) {
    auto value = property(object, key);
    if (!value.empty()) return value;
  }
  return {};
}

std::optional<GlobalId> numericProperty(WpPipewireObject *object, const char *key) {
  const auto value = property(object, key);
  if (value.empty()) return std::nullopt;
  GlobalId result{};
  const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
  return parsed.ec == std::errc{} ? std::optional{result} : std::nullopt;
}

std::string nodeState(WpNode *node) {
  const char *error = nullptr;
  switch (wp_node_get_state(node, &error)) {
  case WP_NODE_STATE_ERROR: return error ? std::string("error: ") + error : "error";
  case WP_NODE_STATE_CREATING: return "creating";
  case WP_NODE_STATE_SUSPENDED: return "suspended";
  case WP_NODE_STATE_IDLE: return "idle";
  case WP_NODE_STATE_RUNNING: return "running";
  }
  return "unknown";
}

std::string linkState(WpLink *link) {
  const char *error = nullptr;
  switch (wp_link_get_state(link, &error)) {
  case WP_LINK_STATE_ERROR: return error ? std::string("error: ") + error : "error";
  case WP_LINK_STATE_UNLINKED: return "unlinked";
  case WP_LINK_STATE_INIT: return "initializing";
  case WP_LINK_STATE_NEGOTIATING: return "negotiating";
  case WP_LINK_STATE_ALLOCATING: return "allocating";
  case WP_LINK_STATE_PAUSED: return "paused";
  case WP_LINK_STATE_ACTIVE: return "active";
  }
  return "unknown";
}

template<typename Function>
void eachObject(WpObjectManager *manager, GType type, Function function) {
  WpIterator *iterator = wp_object_manager_new_filtered_iterator(manager, type, nullptr);
  GValue item = G_VALUE_INIT;
  while (wp_iterator_next(iterator, &item)) {
    function(static_cast<GObject *>(g_value_get_object(&item)));
    g_value_unset(&item);
  }
  wp_iterator_unref(iterator);
}

gboolean quitLoop(gpointer data) {
  g_main_loop_quit(static_cast<GMainLoop *>(data));
  return G_SOURCE_REMOVE;
}

} // namespace

WirePlumberGraphSource::~WirePlumberGraphSource() { stop(); }

void WirePlumberGraphSource::start(SnapshotCallback snapshotCallback, StatusCallback statusCallback) {
  stop();
  snapshotCallback_ = std::move(snapshotCallback);
  statusCallback_ = std::move(statusCallback);
  thread_ = std::jthread([this](std::stop_token token) { run(token); });
}

void WirePlumberGraphSource::stop() {
  if (!thread_.joinable()) return;
  thread_.request_stop();
  {
    std::scoped_lock lock(mutex_);
    if (context_ && loop_) g_main_context_invoke(context_, quitLoop, loop_);
  }
  thread_.join();
}

void WirePlumberGraphSource::run(std::stop_token token) {
  statusCallback_({SourceState::Connecting, "Connecting to PipeWire…"});
  auto *context = g_main_context_new();
  g_main_context_push_thread_default(context);
  auto *core = wp_core_new(context, nullptr, nullptr);
  if (!wp_core_connect(core)) {
    statusCallback_({SourceState::Disconnected, "Could not connect to the PipeWire session"});
    g_object_unref(core);
    g_main_context_pop_thread_default(context);
    g_main_context_unref(context);
    return;
  }

  auto *manager = wp_object_manager_new();
  wp_object_manager_add_interest(manager, WP_TYPE_CLIENT, nullptr);
  wp_object_manager_add_interest(manager, WP_TYPE_DEVICE, nullptr);
  wp_object_manager_add_interest(manager, WP_TYPE_NODE, nullptr);
  wp_object_manager_add_interest(manager, WP_TYPE_PORT, nullptr);
  wp_object_manager_add_interest(manager, WP_TYPE_LINK, nullptr);
  wp_object_manager_add_interest(manager, WP_TYPE_METADATA, nullptr);
  for (const auto type : {WP_TYPE_CLIENT, WP_TYPE_DEVICE, WP_TYPE_PORT, WP_TYPE_LINK, WP_TYPE_METADATA})
    wp_object_manager_request_object_features(manager, type, WP_PIPEWIRE_OBJECT_FEATURES_MINIMAL);
  wp_object_manager_request_object_features(manager, WP_TYPE_NODE,
    static_cast<WpObjectFeatures>(WP_PIPEWIRE_OBJECT_FEATURES_MINIMAL) |
      static_cast<WpObjectFeatures>(WP_NODE_FEATURE_PORTS));

  g_signal_connect(manager, "installed", G_CALLBACK(+[](WpObjectManager *, gpointer data) {
    auto *self = static_cast<WirePlumberGraphSource *>(data);
    self->publish();
    self->statusCallback_({SourceState::Ready, "Live PipeWire graph"});
  }), this);
  g_signal_connect(manager, "objects-changed", G_CALLBACK(+[](WpObjectManager *, gpointer data) {
    static_cast<WirePlumberGraphSource *>(data)->publish();
  }), this);

  auto *loop = g_main_loop_new(context, FALSE);
  {
    std::scoped_lock lock(mutex_);
    context_ = context;
    loop_ = loop;
    core_ = core;
    manager_ = manager;
  }
  wp_core_install_object_manager(core, manager);
  if (!token.stop_requested()) g_main_loop_run(loop);
  {
    std::scoped_lock lock(mutex_);
    context_ = nullptr;
    loop_ = nullptr;
    core_ = nullptr;
    manager_ = nullptr;
  }
  wp_core_disconnect(core);
  g_main_loop_unref(loop);
  g_object_unref(manager);
  g_object_unref(core);
  g_main_context_pop_thread_default(context);
  g_main_context_unref(context);
}

void WirePlumberGraphSource::publish() {
  if (snapshotCallback_) snapshotCallback_(std::make_shared<GraphSnapshot>(snapshot()));
}

GraphSnapshot WirePlumberGraphSource::snapshot() const {
  GraphSnapshot graph;
  graph.remoteName = wp_core_get_remote_name(core_) ? wp_core_get_remote_name(core_) : "pipewire-0";
  graph.remoteVersion = wp_core_get_remote_version(core_) ? wp_core_get_remote_version(core_) : "unknown";

  eachObject(manager_, WP_TYPE_CLIENT, [&](GObject *value) {
    auto *object = WP_PIPEWIRE_OBJECT(value);
    const auto objectId = wp_proxy_get_bound_id(WP_PROXY(value));
    auto name = firstProperty(object, {"application.name", "client.name"});
    if (name.empty()) name = "Client " + std::to_string(objectId);
    graph.clients.push_back({objectId, std::move(name), firstProperty(object, {"application.id", "application.process.binary", "client.name"})});
  });
  eachObject(manager_, WP_TYPE_DEVICE, [&](GObject *value) {
    auto *object = WP_PIPEWIRE_OBJECT(value);
    const auto objectId = wp_proxy_get_bound_id(WP_PROXY(value));
    auto name = firstProperty(object, {"device.description", "device.nick", "device.name"});
    if (name.empty()) name = "Device " + std::to_string(objectId);
    graph.devices.push_back({objectId, std::move(name), firstProperty(object, {"device.serial", "device.name"}), property(object, "media.class")});
  });
  eachObject(manager_, WP_TYPE_NODE, [&](GObject *value) {
    auto *object = WP_PIPEWIRE_OBJECT(value);
    const auto objectId = wp_proxy_get_bound_id(WP_PROXY(value));
    const auto technicalName = property(object, "node.name");
    auto name = firstProperty(object, {"node.description", "node.nick", "application.name", "node.name"});
    if (name.empty()) name = "Node " + std::to_string(objectId);
    const auto mediaClass = property(object, "media.class");
    graph.nodes.push_back({objectId, std::move(name), technicalName,
      firstProperty(object, {"node.name", "application.id", "device.serial", "object.serial"}),
      mediaClass, nodeState(WP_NODE(value)), classifyMedia(mediaClass), NodeRole::Processor,
      numericProperty(object, "client.id"), numericProperty(object, "device.id")});
  });

  std::unordered_map<GlobalId, MediaType> nodeMedia;
  for (const auto &node : graph.nodes) nodeMedia[node.id] = node.media;
  eachObject(manager_, WP_TYPE_PORT, [&](GObject *value) {
    auto *object = WP_PIPEWIRE_OBJECT(value);
    const auto objectId = wp_proxy_get_bound_id(WP_PROXY(value));
    const auto nodeId = numericProperty(object, "node.id").value_or(0);
    graph.ports.push_back({objectId, nodeId,
      firstProperty(object, {"port.alias", "port.name"}), property(object, "audio.channel"),
      wp_port_get_direction(WP_PORT(value)) == WP_DIRECTION_OUTPUT ? PortDirection::Output : PortDirection::Input,
      nodeMedia.contains(nodeId) ? nodeMedia[nodeId] : classifyMedia({}, property(object, "format.dsp")),
      property(object, "format.dsp")});
  });
  eachObject(manager_, WP_TYPE_LINK, [&](GObject *value) {
    guint32 outputNode{}, outputPort{}, inputNode{}, inputPort{};
    wp_link_get_linked_object_ids(WP_LINK(value), &outputNode, &outputPort, &inputNode, &inputPort);
    graph.links.push_back({wp_proxy_get_bound_id(WP_PROXY(value)), outputNode, outputPort, inputNode, inputPort,
      linkState(WP_LINK(value)), nodeMedia.contains(outputNode) ? nodeMedia[outputNode] : MediaType::Unknown});
  });
  classifyNodeRoles(graph);
  return graph;
}

} // namespace wirerunner
