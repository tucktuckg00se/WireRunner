// SPDX-License-Identifier: GPL-3.0-or-later
#include "backend/WirePlumberGraphSource.hpp"

#include <wp/wp.h>
#include <pipewire/permission.h>
#include <spa/param/props.h>
#include <spa/param/profile.h>
#include <spa/param/route.h>
#include <spa/pod/iter.h>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <memory>
#include <unordered_map>
#include <QJsonDocument>
#include <QJsonObject>

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

std::optional<int> integerProperty(WpPipewireObject *object, const char *key) {
  const auto value = property(object, key);
  if (value.empty()) return std::nullopt;
  int result{};
  const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
  return parsed.ec == std::errc{} ? std::optional{result} : std::nullopt;
}

bool booleanProperty(WpPipewireObject *object, const char *key) {
  const auto value = property(object, key);
  return value == "true" || value == "1";
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

GObject *findObject(WpObjectManager *manager, GType type, GlobalId id) {
  GObject *result = nullptr;
  eachObject(manager, type, [&](GObject *object) {
    if (!result && wp_proxy_get_bound_id(WP_PROXY(object)) == id) result = object;
  });
  return result;
}

bool podFloat(const spa_pod *pod, float &value) {
  if (spa_pod_is_float(pod)) return spa_pod_get_float(pod, &value) >= 0;
  if (!spa_pod_is_choice(pod) || SPA_POD_CHOICE_VALUE_TYPE(pod) != SPA_TYPE_Float ||
      SPA_POD_CHOICE_N_VALUES(pod) == 0) return false;
  value = static_cast<const float *>(SPA_POD_CHOICE_VALUES(pod))[0];
  return true;
}

void readVolumeRange(const spa_pod *pod, NodeAudioControl &audio) {
  if (!spa_pod_is_choice(pod) || SPA_POD_CHOICE_VALUE_TYPE(pod) != SPA_TYPE_Float ||
      SPA_POD_CHOICE_TYPE(pod) != SPA_CHOICE_Range || SPA_POD_CHOICE_N_VALUES(pod) < 3) return;
  const auto *values = static_cast<const float *>(SPA_POD_CHOICE_VALUES(pod));
  if (std::isfinite(values[1]) && std::isfinite(values[2]) && values[1] >= 0.0F && values[2] > values[1]) {
    audio.minimumVolume = values[1];
    audio.maximumVolume = values[2];
  }
}

void readAudioProperties(const spa_pod *pod, NodeAudioControl &audio) {
  if (!pod || !spa_pod_is_object(pod)) return;
  const auto *object = reinterpret_cast<const spa_pod_object *>(pod);
  const spa_pod_prop *property = nullptr;
  SPA_POD_OBJECT_FOREACH(object, property) {
        if (property->key == SPA_PROP_volume) {
          float volume{};
          if (podFloat(&property->value, volume) && std::isfinite(volume) && volume >= 0.0F) {
            audio.volume = volume;
            audio.hasVolume = true;
            readVolumeRange(&property->value, audio);
          }
        } else if (property->key == SPA_PROP_channelVolumes && spa_pod_is_array(&property->value) &&
                   SPA_POD_ARRAY_VALUE_TYPE(&property->value) == SPA_TYPE_Float) {
          const auto count = SPA_POD_ARRAY_N_VALUES(&property->value);
          const auto *values = static_cast<const float *>(SPA_POD_ARRAY_VALUES(&property->value));
          audio.channelVolumes.assign(values, values + count);
          if (!audio.channelVolumes.empty()) {
            audio.volume = *std::ranges::max_element(audio.channelVolumes);
            audio.hasVolume = true;
          }
        } else if (property->key == SPA_PROP_channelMap && spa_pod_is_array(&property->value) &&
                   SPA_POD_ARRAY_VALUE_TYPE(&property->value) == SPA_TYPE_Id) {
          const auto count = SPA_POD_ARRAY_N_VALUES(&property->value);
          const auto *values = static_cast<const std::uint32_t *>(SPA_POD_ARRAY_VALUES(&property->value));
          audio.channelMap.assign(values, values + count);
        } else if (property->key == SPA_PROP_softVolumes && spa_pod_is_array(&property->value) &&
                   SPA_POD_ARRAY_VALUE_TYPE(&property->value) == SPA_TYPE_Float) {
          const auto count = SPA_POD_ARRAY_N_VALUES(&property->value);
          const auto *values = static_cast<const float *>(SPA_POD_ARRAY_VALUES(&property->value));
          audio.softVolumes.assign(values, values + count);
        } else if (property->key == SPA_PROP_mute) {
          bool muted{};
          if (spa_pod_get_bool(&property->value, &muted) >= 0) {
            audio.muted = muted;
            audio.hasMute = true;
          }
        }
  }
}

void finishAudioProperties(NodeAudioControl &audio) {
  if (audio.channelVolumes.empty() && !audio.softVolumes.empty()) audio.channelVolumes = audio.softVolumes;
  if (!audio.channelVolumes.empty()) {
    audio.volume = *std::ranges::max_element(audio.channelVolumes);
    audio.hasVolume = true;
  }
  if (audio.hasVolume) audio.maximumVolume = std::max(audio.maximumVolume, audio.volume);
}

std::optional<NodeAudioControl> nodeAudioControl(WpNode *node) {
  auto *iterator = wp_pipewire_object_enum_params_sync(WP_PIPEWIRE_OBJECT(node), "Props", nullptr);
  if (!iterator) return std::nullopt;
  NodeAudioControl audio;
  GValue item = G_VALUE_INIT;
  while (wp_iterator_next(iterator, &item)) {
    auto *wrapped = static_cast<WpSpaPod *>(g_value_get_boxed(&item));
    readAudioProperties(wrapped ? wp_spa_pod_get_spa_pod(wrapped) : nullptr, audio);
    g_value_unset(&item);
  }
  wp_iterator_unref(iterator);
  finishAudioProperties(audio);
  if (!audio.hasVolume && !audio.hasMute) return std::nullopt;
  const auto permissions = wp_global_proxy_get_permissions(WP_GLOBAL_PROXY(node));
  audio.writable = (permissions & PW_PERM_W) != 0 && (permissions & PW_PERM_X) != 0;
  return audio;
}

Availability availability(std::uint32_t value) {
  if (value == SPA_PARAM_AVAILABILITY_yes) return Availability::Available;
  if (value == SPA_PARAM_AVAILABILITY_no) return Availability::Unavailable;
  return Availability::Unknown;
}

DeviceRoute parseRoute(const spa_pod *pod, bool active, bool writable) {
  DeviceRoute route;
  route.active = active;
  if (!pod || !spa_pod_is_object(pod)) return route;
  const auto *object = reinterpret_cast<const spa_pod_object *>(pod);
  const spa_pod_prop *prop = nullptr;
  SPA_POD_OBJECT_FOREACH(object, prop) {
    if (prop->key == SPA_PARAM_ROUTE_index) spa_pod_get_int(&prop->value, &route.index);
    else if (prop->key == SPA_PARAM_ROUTE_device) spa_pod_get_int(&prop->value, &route.deviceIndex);
    else if (prop->key == SPA_PARAM_ROUTE_priority) spa_pod_get_int(&prop->value, &route.priority);
    else if (prop->key == SPA_PARAM_ROUTE_available) {
      std::uint32_t value{};
      if (spa_pod_get_id(&prop->value, &value) >= 0) route.availability = availability(value);
    } else if (prop->key == SPA_PARAM_ROUTE_direction) {
      std::uint32_t direction{};
      if (spa_pod_get_id(&prop->value, &direction) >= 0)
        route.direction = direction == SPA_DIRECTION_INPUT ? PortDirection::Input : PortDirection::Output;
    } else if (prop->key == SPA_PARAM_ROUTE_name) {
      const char *name = nullptr;
      if (spa_pod_get_string(&prop->value, &name) >= 0 && name) route.name = name;
    } else if (prop->key == SPA_PARAM_ROUTE_description) {
      const char *description = nullptr;
      if (spa_pod_get_string(&prop->value, &description) >= 0 && description) route.description = description;
    } else if (prop->key == SPA_PARAM_ROUTE_props) readAudioProperties(&prop->value, route.audio);
  }
  finishAudioProperties(route.audio);
  route.audio.writable = writable;
  return route;
}

std::vector<DeviceRoute> deviceRoutes(WpDevice *device) {
  std::vector<DeviceRoute> routes;
  const auto permissions = wp_global_proxy_get_permissions(WP_GLOBAL_PROXY(device));
  const bool writable = (permissions & PW_PERM_W) != 0 && (permissions & PW_PERM_X) != 0;
  auto *iterator = wp_pipewire_object_enum_params_sync(WP_PIPEWIRE_OBJECT(device), "EnumRoute", nullptr);
  if (iterator) {
    GValue item = G_VALUE_INIT;
    while (wp_iterator_next(iterator, &item)) {
      auto *wrapped = static_cast<WpSpaPod *>(g_value_get_boxed(&item));
      auto route = parseRoute(wrapped ? wp_spa_pod_get_spa_pod(wrapped) : nullptr, false, writable);
      if (route.index >= 0) routes.push_back(std::move(route));
      g_value_unset(&item);
    }
    wp_iterator_unref(iterator);
  }
  iterator = wp_pipewire_object_enum_params_sync(WP_PIPEWIRE_OBJECT(device), "Route", nullptr);
  if (!iterator) return routes;
  GValue item = G_VALUE_INIT;
  while (wp_iterator_next(iterator, &item)) {
    auto *wrapped = static_cast<WpSpaPod *>(g_value_get_boxed(&item));
    auto active = parseRoute(wrapped ? wp_spa_pod_get_spa_pod(wrapped) : nullptr, true, writable);
    auto existing = std::ranges::find_if(routes, [&](const DeviceRoute &route) {
      return route.index == active.index && (route.deviceIndex < 0 || active.deviceIndex < 0 || route.deviceIndex == active.deviceIndex);
    });
    if (existing == routes.end()) routes.push_back(std::move(active));
    else {
      existing->active = true;
      existing->deviceIndex = active.deviceIndex;
      if (active.audio.hasVolume || active.audio.hasMute) existing->audio = std::move(active.audio);
    }
    g_value_unset(&item);
  }
  wp_iterator_unref(iterator);
  return routes;
}

std::vector<DeviceProfile> deviceProfiles(WpDevice *device) {
  std::vector<DeviceProfile> profiles;
  auto parse = [](const spa_pod *pod, bool active) {
    DeviceProfile profile;
    profile.active = active;
    if (!pod || !spa_pod_is_object(pod)) return profile;
    const auto *object = reinterpret_cast<const spa_pod_object *>(pod);
    const spa_pod_prop *prop = nullptr;
    SPA_POD_OBJECT_FOREACH(object, prop) {
      if (prop->key == SPA_PARAM_PROFILE_index) spa_pod_get_int(&prop->value, &profile.index);
      else if (prop->key == SPA_PARAM_PROFILE_priority) spa_pod_get_int(&prop->value, &profile.priority);
      else if (prop->key == SPA_PARAM_PROFILE_available) {
        std::uint32_t value{};
        if (spa_pod_get_id(&prop->value, &value) >= 0) profile.availability = availability(value);
      } else if (prop->key == SPA_PARAM_PROFILE_name) {
        const char *name = nullptr;
        if (spa_pod_get_string(&prop->value, &name) >= 0 && name) profile.name = name;
      } else if (prop->key == SPA_PARAM_PROFILE_description) {
        const char *description = nullptr;
        if (spa_pod_get_string(&prop->value, &description) >= 0 && description) profile.description = description;
      }
    }
    return profile;
  };
  for (const auto *param : {"EnumProfile", "Profile"}) {
    auto *iterator = wp_pipewire_object_enum_params_sync(WP_PIPEWIRE_OBJECT(device), param, nullptr);
    if (!iterator) continue;
    GValue item = G_VALUE_INIT;
    while (wp_iterator_next(iterator, &item)) {
      auto *wrapped = static_cast<WpSpaPod *>(g_value_get_boxed(&item));
      auto value = parse(wrapped ? wp_spa_pod_get_spa_pod(wrapped) : nullptr, std::string_view(param) == "Profile");
      auto existing = std::ranges::find(profiles, value.index, &DeviceProfile::index);
      if (existing == profiles.end()) profiles.push_back(std::move(value));
      else if (value.active) existing->active = true;
      g_value_unset(&item);
    }
    wp_iterator_unref(iterator);
  }
  std::ranges::sort(profiles, std::greater{}, &DeviceProfile::priority);
  return profiles;
}

std::string metadataNodeName(WpMetadata *metadata, const char *key) {
  const char *type = nullptr;
  const auto *raw = wp_metadata_find(metadata, 0, key, &type);
  if (!raw) return {};
  const auto document = QJsonDocument::fromJson(QByteArray(raw));
  return document.isObject() ? document.object().value(QStringLiteral("name")).toString().toStdString() : std::string{};
}

struct ActivationData {
  GraphSource::CommandCallback callback;
  CommandId commandId{};
  WpLink *link{};
};

void linkActivated(GObject *source, GAsyncResult *result, gpointer data) {
  std::unique_ptr<ActivationData> activation(static_cast<ActivationData *>(data));
  GError *error = nullptr;
  const bool accepted = wp_object_activate_finish(WP_OBJECT(source), result, &error);
  activation->callback({activation->commandId, accepted,
    accepted ? "Link submitted to PipeWire" : (error ? error->message : "PipeWire rejected the link")});
  if (error) g_error_free(error);
  g_object_unref(activation->link);
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

void WirePlumberGraphSource::invoke(std::function<void()> task) {
  GMainContext *context = nullptr;
  {
    std::scoped_lock lock(mutex_);
    if (context_) context = g_main_context_ref(context_);
  }
  if (!context) return;
  auto *owned = new std::function<void()>(std::move(task));
  g_main_context_invoke_full(context, G_PRIORITY_DEFAULT, +[](gpointer data) -> gboolean {
    (*static_cast<std::function<void()> *>(data))();
    return G_SOURCE_REMOVE;
  }, owned, +[](gpointer data) { delete static_cast<std::function<void()> *>(data); });
  g_main_context_unref(context);
}

void WirePlumberGraphSource::createLink(CreateLinkRequest request, CommandCallback callback) {
  {
    std::scoped_lock lock(mutex_);
    if (!context_) {
      callback({request.commandId, false, "PipeWire is not connected"});
      return;
    }
  }
  invoke([this, request, callback = std::move(callback)]() mutable {
    auto *output = findObject(manager_, WP_TYPE_PORT, request.outputPortId);
    auto *input = findObject(manager_, WP_TYPE_PORT, request.inputPortId);
    if (!output || !input || wp_port_get_direction(WP_PORT(output)) != WP_DIRECTION_OUTPUT ||
        wp_port_get_direction(WP_PORT(input)) != WP_DIRECTION_INPUT ||
        numericProperty(WP_PIPEWIRE_OBJECT(output), "node.id") != request.outputNodeId ||
        numericProperty(WP_PIPEWIRE_OBJECT(input), "node.id") != request.inputNodeId) {
      callback({request.commandId, false, "One of the selected ports is no longer available"});
      return;
    }
    const auto outputNode = std::to_string(request.outputNodeId);
    const auto outputPort = std::to_string(request.outputPortId);
    const auto inputNode = std::to_string(request.inputNodeId);
    const auto inputPort = std::to_string(request.inputPortId);
    auto *properties = wp_properties_new_empty();
    wp_properties_set(properties, "link.output.node", outputNode.c_str());
    wp_properties_set(properties, "link.output.port", outputPort.c_str());
    wp_properties_set(properties, "link.input.node", inputNode.c_str());
    wp_properties_set(properties, "link.input.port", inputPort.c_str());
    wp_properties_set(properties, "object.linger", request.linger ? "true" : "false");
    wp_properties_set(properties, "wirerunner.created", "true");
    if (request.feedback) wp_properties_set(properties, "link.feedback", "true");
    auto *link = wp_link_new_from_factory(core_, "link-factory", properties);
    if (!link) {
      callback({request.commandId, false, "PipeWire could not create a link proxy"});
      return;
    }
    auto *activation = new ActivationData{std::move(callback), request.commandId, link};
    wp_object_activate(WP_OBJECT(link), WP_PROXY_FEATURE_BOUND, nullptr, linkActivated, activation);
  });
}

void WirePlumberGraphSource::destroyLink(DestroyLinkRequest request, CommandCallback callback) {
  {
    std::scoped_lock lock(mutex_);
    if (!context_) {
      callback({request.commandId, false, "PipeWire is not connected"});
      return;
    }
  }
  invoke([this, request, callback = std::move(callback)]() mutable {
    auto *object = findObject(manager_, WP_TYPE_LINK, request.linkId);
    if (!object) {
      callback({request.commandId, false, "The selected link no longer exists"});
      return;
    }
    auto *link = WP_GLOBAL_PROXY(object);
    if ((wp_global_proxy_get_permissions(link) & PW_PERM_X) == 0) {
      callback({request.commandId, false, "PipeWire does not allow this link to be removed"});
      return;
    }
    wp_global_proxy_request_destroy(link);
    callback({request.commandId, true, "Disconnect submitted to PipeWire"});
  });
}

void WirePlumberGraphSource::setNodeAudio(SetNodeAudioRequest request, CommandCallback callback) {
  {
    std::scoped_lock lock(mutex_);
    if (!context_) {
      callback({request.commandId, false, "PipeWire is not connected"});
      return;
    }
  }
  invoke([this, request = std::move(request), callback = std::move(callback)]() mutable {
    auto *object = findObject(manager_, WP_TYPE_NODE, request.nodeId);
    if (!object) {
      callback({request.commandId, false, "The selected audio node is no longer available"});
      return;
    }
    auto *node = WP_PIPEWIRE_OBJECT(object);
    const auto permissions = wp_global_proxy_get_permissions(WP_GLOBAL_PROXY(object));
    if ((permissions & PW_PERM_W) == 0 || (permissions & PW_PERM_X) == 0) {
      callback({request.commandId, false, "PipeWire does not allow this audio control to be changed"});
      return;
    }

    auto *builder = wp_spa_pod_builder_new_object("Spa:Pod:Object:Param:Props", "Props");
    if (request.volume) {
      wp_spa_pod_builder_add_property_id(builder, SPA_PROP_volume);
      wp_spa_pod_builder_add_float(builder, *request.volume);
    }
    if (!request.channelVolumes.empty()) {
      auto *arrayBuilder = wp_spa_pod_builder_new_array();
      for (const auto value : request.channelVolumes) wp_spa_pod_builder_add_float(arrayBuilder, value);
      auto *array = wp_spa_pod_builder_end(arrayBuilder);
      wp_spa_pod_builder_unref(arrayBuilder);
      wp_spa_pod_builder_add_property_id(builder, SPA_PROP_channelVolumes);
      wp_spa_pod_builder_add_pod(builder, array);
      wp_spa_pod_unref(array);
    }
    if (request.muted) {
      wp_spa_pod_builder_add_property_id(builder, SPA_PROP_mute);
      wp_spa_pod_builder_add_boolean(builder, *request.muted);
    }
    auto *props = wp_spa_pod_builder_end(builder);
    wp_spa_pod_builder_unref(builder);
    const bool accepted = props && wp_pipewire_object_set_param(node, "Props", 0, props);
    if (props) wp_spa_pod_unref(props);
    callback({request.commandId, accepted,
      accepted ? "Audio change submitted to PipeWire" : "PipeWire rejected the audio change"});
  });
}

void WirePlumberGraphSource::setDeviceRouteAudio(SetDeviceRouteAudioRequest request, CommandCallback callback) {
  {
    std::scoped_lock lock(mutex_);
    if (!context_) {
      callback({request.commandId, false, "PipeWire is not connected"});
      return;
    }
  }
  invoke([this, request = std::move(request), callback = std::move(callback)]() mutable {
    auto *object = findObject(manager_, WP_TYPE_DEVICE, request.deviceId);
    if (!object) {
      callback({request.commandId, false, "The selected device is no longer available"});
      return;
    }
    const auto permissions = wp_global_proxy_get_permissions(WP_GLOBAL_PROXY(object));
    if ((permissions & PW_PERM_W) == 0 || (permissions & PW_PERM_X) == 0) {
      callback({request.commandId, false, "PipeWire does not allow this device route to be changed"});
      return;
    }
    auto *propsBuilder = wp_spa_pod_builder_new_object("Spa:Pod:Object:Param:Props", "Props");
    if (!request.channelVolumes.empty()) {
      auto *arrayBuilder = wp_spa_pod_builder_new_array();
      for (const auto value : request.channelVolumes) wp_spa_pod_builder_add_float(arrayBuilder, value);
      auto *array = wp_spa_pod_builder_end(arrayBuilder);
      wp_spa_pod_builder_unref(arrayBuilder);
      wp_spa_pod_builder_add_property_id(propsBuilder, SPA_PROP_channelVolumes);
      wp_spa_pod_builder_add_pod(propsBuilder, array);
      wp_spa_pod_unref(array);
    }
    if (!request.channelMap.empty()) {
      auto *arrayBuilder = wp_spa_pod_builder_new_array();
      for (const auto value : request.channelMap) wp_spa_pod_builder_add_id(arrayBuilder, value);
      auto *array = wp_spa_pod_builder_end(arrayBuilder);
      wp_spa_pod_builder_unref(arrayBuilder);
      wp_spa_pod_builder_add_property_id(propsBuilder, SPA_PROP_channelMap);
      wp_spa_pod_builder_add_pod(propsBuilder, array);
      wp_spa_pod_unref(array);
    }
    if (request.muted) {
      wp_spa_pod_builder_add_property_id(propsBuilder, SPA_PROP_mute);
      wp_spa_pod_builder_add_boolean(propsBuilder, *request.muted);
    }
    auto *props = wp_spa_pod_builder_end(propsBuilder);
    wp_spa_pod_builder_unref(propsBuilder);

    auto *routeBuilder = wp_spa_pod_builder_new_object("Spa:Pod:Object:Param:Route", "Route");
    wp_spa_pod_builder_add_property_id(routeBuilder, SPA_PARAM_ROUTE_index);
    wp_spa_pod_builder_add_int(routeBuilder, request.routeIndex);
    wp_spa_pod_builder_add_property_id(routeBuilder, SPA_PARAM_ROUTE_device);
    wp_spa_pod_builder_add_int(routeBuilder, request.routeDeviceId);
    wp_spa_pod_builder_add_property_id(routeBuilder, SPA_PARAM_ROUTE_props);
    wp_spa_pod_builder_add_pod(routeBuilder, props);
    wp_spa_pod_builder_add_property_id(routeBuilder, SPA_PARAM_ROUTE_save);
    wp_spa_pod_builder_add_boolean(routeBuilder, true);
    auto *route = wp_spa_pod_builder_end(routeBuilder);
    wp_spa_pod_builder_unref(routeBuilder);
    wp_spa_pod_unref(props);
    const bool accepted = route && wp_pipewire_object_set_param(WP_PIPEWIRE_OBJECT(object), "Route", 0, route);
    if (route) wp_spa_pod_unref(route);
    callback({request.commandId, accepted,
      accepted ? "Device volume saved through WirePlumber" : "WirePlumber rejected the device volume change"});
  });
}

void WirePlumberGraphSource::setDefault(SetDefaultRequest request, CommandCallback callback) {
  {
    std::scoped_lock lock(mutex_);
    if (!context_) { callback({request.commandId, false, "PipeWire is not connected"}); return; }
  }
  invoke([this, request = std::move(request), callback = std::move(callback)]() mutable {
    WpMetadata *metadata = nullptr;
    eachObject(manager_, WP_TYPE_METADATA, [&](GObject *value) {
      if (!metadata && property(WP_PIPEWIRE_OBJECT(value), "metadata.name") == "default") metadata = WP_METADATA(value);
    });
    if (!metadata) { callback({request.commandId, false, "WirePlumber default metadata is unavailable"}); return; }
    const char *suffix = request.kind == DefaultKind::AudioSink ? "audio.sink"
      : request.kind == DefaultKind::AudioSource ? "audio.source" : "video.source";
    const auto key = std::string("default.configured.") + suffix;
    if (request.nodeName.empty()) wp_metadata_set(metadata, 0, key.c_str(), nullptr, nullptr);
    else {
      const auto json = QJsonDocument(QJsonObject{{QStringLiteral("name"), QString::fromStdString(request.nodeName)}})
        .toJson(QJsonDocument::Compact);
      wp_metadata_set(metadata, 0, key.c_str(), "Spa:String:JSON", json.constData());
    }
    callback({request.commandId, true, "Default preference submitted to WirePlumber"});
  });
}

void WirePlumberGraphSource::setDeviceProfile(SetDeviceProfileRequest request, CommandCallback callback) {
  {
    std::scoped_lock lock(mutex_);
    if (!context_) { callback({request.commandId, false, "PipeWire is not connected"}); return; }
  }
  invoke([this, request, callback = std::move(callback)]() mutable {
    auto *object = findObject(manager_, WP_TYPE_DEVICE, request.deviceId);
    if (!object) { callback({request.commandId, false, "The selected device is no longer available"}); return; }
    auto *builder = wp_spa_pod_builder_new_object("Spa:Pod:Object:Param:Profile", "Profile");
    wp_spa_pod_builder_add_property_id(builder, SPA_PARAM_PROFILE_index);
    wp_spa_pod_builder_add_int(builder, request.profileIndex);
    wp_spa_pod_builder_add_property_id(builder, SPA_PARAM_PROFILE_save);
    wp_spa_pod_builder_add_boolean(builder, true);
    auto *profile = wp_spa_pod_builder_end(builder);
    wp_spa_pod_builder_unref(builder);
    const bool accepted = profile && wp_pipewire_object_set_param(WP_PIPEWIRE_OBJECT(object), "Profile", 0, profile);
    if (profile) wp_spa_pod_unref(profile);
    callback({request.commandId, accepted, accepted ? "Device mode submitted to WirePlumber" : "WirePlumber rejected the device mode"});
  });
}

void WirePlumberGraphSource::setDeviceRoute(SetDeviceRouteRequest request, CommandCallback callback) {
  {
    std::scoped_lock lock(mutex_);
    if (!context_) { callback({request.commandId, false, "PipeWire is not connected"}); return; }
  }
  invoke([this, request, callback = std::move(callback)]() mutable {
    auto *object = findObject(manager_, WP_TYPE_DEVICE, request.deviceId);
    if (!object) { callback({request.commandId, false, "The selected device is no longer available"}); return; }
    auto *builder = wp_spa_pod_builder_new_object("Spa:Pod:Object:Param:Route", "Route");
    wp_spa_pod_builder_add_property_id(builder, SPA_PARAM_ROUTE_index);
    wp_spa_pod_builder_add_int(builder, request.routeIndex);
    wp_spa_pod_builder_add_property_id(builder, SPA_PARAM_ROUTE_device);
    wp_spa_pod_builder_add_int(builder, request.routeDeviceId);
    wp_spa_pod_builder_add_property_id(builder, SPA_PARAM_ROUTE_save);
    wp_spa_pod_builder_add_boolean(builder, true);
    auto *route = wp_spa_pod_builder_end(builder);
    wp_spa_pod_builder_unref(builder);
    const bool accepted = route && wp_pipewire_object_set_param(WP_PIPEWIRE_OBJECT(object), "Route", 0, route);
    if (route) wp_spa_pod_unref(route);
    callback({request.commandId, accepted, accepted ? "Device port submitted to WirePlumber" : "WirePlumber rejected the device port"});
  });
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
  for (const auto type : {WP_TYPE_CLIENT, WP_TYPE_PORT, WP_TYPE_LINK})
    wp_object_manager_request_object_features(manager, type, WP_PIPEWIRE_OBJECT_FEATURES_MINIMAL);
  wp_object_manager_request_object_features(manager, WP_TYPE_METADATA,
    static_cast<WpObjectFeatures>(WP_PIPEWIRE_OBJECT_FEATURES_MINIMAL) |
      static_cast<WpObjectFeatures>(WP_METADATA_FEATURE_DATA));
  wp_object_manager_request_object_features(manager, WP_TYPE_DEVICE,
    static_cast<WpObjectFeatures>(WP_PIPEWIRE_OBJECT_FEATURES_MINIMAL) |
      static_cast<WpObjectFeatures>(WP_PIPEWIRE_OBJECT_FEATURE_PARAM_ROUTE) |
      static_cast<WpObjectFeatures>(WP_PIPEWIRE_OBJECT_FEATURE_PARAM_PROFILE));
  wp_object_manager_request_object_features(manager, WP_TYPE_NODE,
    static_cast<WpObjectFeatures>(WP_PIPEWIRE_OBJECT_FEATURES_MINIMAL) |
      static_cast<WpObjectFeatures>(WP_NODE_FEATURE_PORTS) |
      static_cast<WpObjectFeatures>(WP_PIPEWIRE_OBJECT_FEATURE_PARAM_PROPS));

  g_signal_connect(manager, "installed", G_CALLBACK(+[](WpObjectManager *, gpointer data) {
    auto *self = static_cast<WirePlumberGraphSource *>(data);
    eachObject(self->manager_, WP_TYPE_METADATA, [&](GObject *value) {
      g_signal_connect(value, "changed", G_CALLBACK(+[](WpMetadata *, guint, gchar *, gchar *, gchar *, gpointer owner) {
        static_cast<WirePlumberGraphSource *>(owner)->publish();
      }), self);
    });
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

GraphSnapshot WirePlumberGraphSource::snapshot() {
  GraphSnapshot graph;
  graph.revision = ++revision_;
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
    const auto permissions = wp_global_proxy_get_permissions(WP_GLOBAL_PROXY(value));
    graph.devices.push_back({.id = objectId, .name = std::move(name),
      .stableId = firstProperty(object, {"device.serial", "device.name"}),
      .mediaClass = property(object, "media.class"), .routes = deviceRoutes(WP_DEVICE(value)),
      .profiles = deviceProfiles(WP_DEVICE(value)),
      .writable = (permissions & PW_PERM_W) != 0 && (permissions & PW_PERM_X) != 0});
  });
  eachObject(manager_, WP_TYPE_NODE, [&](GObject *value) {
    auto *object = WP_PIPEWIRE_OBJECT(value);
    const auto objectId = wp_proxy_get_bound_id(WP_PROXY(value));
    const auto technicalName = property(object, "node.name");
    auto name = firstProperty(object, {"node.description", "node.nick", "application.name", "node.name"});
    if (name.empty()) name = "Node " + std::to_string(objectId);
    const auto mediaClass = property(object, "media.class");
    auto audio = classifyMedia(mediaClass) == MediaType::Audio ? nodeAudioControl(WP_NODE(value)) : std::nullopt;
    graph.nodes.push_back({objectId, std::move(name), technicalName,
      firstProperty(object, {"node.name", "application.id", "device.serial", "object.serial"}),
      mediaClass, nodeState(WP_NODE(value)), classifyMedia(mediaClass), NodeRole::Processor,
      numericProperty(object, "client.id"), numericProperty(object, "device.id"),
      integerProperty(object, "card.profile.device"), std::move(audio)});
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
      property(object, "format.dsp"), wp_global_proxy_get_permissions(WP_GLOBAL_PROXY(value))});
  });
  eachObject(manager_, WP_TYPE_LINK, [&](GObject *value) {
    guint32 outputNode{}, outputPort{}, inputNode{}, inputPort{};
    wp_link_get_linked_object_ids(WP_LINK(value), &outputNode, &outputPort, &inputNode, &inputPort);
    auto *object = WP_PIPEWIRE_OBJECT(value);
    graph.links.push_back({wp_proxy_get_bound_id(WP_PROXY(value)), outputNode, outputPort, inputNode, inputPort,
      linkState(WP_LINK(value)), nodeMedia.contains(outputNode) ? nodeMedia[outputNode] : MediaType::Unknown,
      booleanProperty(object, "link.feedback"), booleanProperty(object, "object.linger"),
      booleanProperty(object, "wirerunner.created"), wp_global_proxy_get_permissions(WP_GLOBAL_PROXY(value))});
  });
  eachObject(manager_, WP_TYPE_METADATA, [&](GObject *value) {
    auto *object = WP_PIPEWIRE_OBJECT(value);
    if (property(object, "metadata.name") != "default") return;
    auto *metadata = WP_METADATA(value);
    for (const auto &[kind, suffix] : std::initializer_list<std::pair<DefaultKind, const char *>>{
           {DefaultKind::AudioSink, "audio.sink"}, {DefaultKind::AudioSource, "audio.source"},
           {DefaultKind::VideoSource, "video.source"}}) {
      graph.defaults.push_back({kind,
        metadataNodeName(metadata, (std::string("default.configured.") + suffix).c_str()),
        metadataNodeName(metadata, (std::string("default.") + suffix).c_str())});
    }
  });
  classifyNodeRoles(graph);
  return graph;
}

} // namespace wirerunner
