/**
 * \file serialization/component_codec.cpp
 **/
#include "serialization/component_codec.hpp"

#include <algorithm>

#include "script/behavior_fields.hpp"
#include "script/script_object.hpp"
#include "script/scripting_environment.hpp"

#include "object/animation_component.hpp"
#include "object/audio_listener_component.hpp"
#include "object/audio_source_component.hpp"
#include "object/camera_component.hpp"
#include "object/grid_component.hpp"
#include "object/light_component.hpp"
#include "object/network_component.hpp"
#include "object/network_settings_component.hpp"
#include "object/physics_component.hpp"
#include "object/physics_joint_component.hpp"
#include "object/render_component.hpp"
#include "object/script_component.hpp"
#include "object/transform.hpp"

namespace other {
  namespace serialization {
    namespace {

      constexpr std::string_view kBehaviorsFieldName = "behaviors";
      constexpr std::string_view kDisabledFieldName = "disabled";
      constexpr std::string_view kFieldsFieldName = "fields";
      /// a field that names another object, or an asset, is a table of one key in a text scene
      constexpr std::string_view kObjectKey = "object";
      constexpr std::string_view kAssetKey = "asset";

      /// what a script component's payload holds
      struct script_payload {
        ostd::vector<std::string> behaviors = {};
        /// the behaviors whose switch is off
        ostd::vector<std::string> disabled = {};
        /// a behavior's name and its saved fields (script/behavior_fields.hpp), for each
        ///  behavior that has any
        ostd::vector<std::pair<std::string, ostd::vector<uint8_t>>> fields = {};
      };

      script_payload decode_script_payload(std::span<const uint8_t> payload) {
        script_payload contents = {};
        ostd::map<natural_t, detail::payload_field> fields = {};
        if (!detail::index_payload(payload, fields)) {
          CORE_LOG_ERROR("truncated script component payload");
          return contents;
        }

        if (const auto it = fields.find(FNV(std::string{ kBehaviorsFieldName })); it != fields.end()) {
          (void)field_codec::decode_body(it->second.tag, it->second.body, contents.behaviors);
        }
        if (const auto it = fields.find(FNV(std::string{ kDisabledFieldName })); it != fields.end()) {
          (void)field_codec::decode_body(it->second.tag, it->second.body, contents.disabled);
        }

        const auto it = fields.find(FNV(std::string{ kFieldsFieldName }));
        if (it == fields.end()) {
          return contents;
        }
        const std::span<const uint8_t> body = it->second.body;
        size_t offset = 0;
        uint32_t count = 0;
        if (!field_codec::read_raw(body, offset, count)) {
          return contents;
        }
        for (uint32_t i = 0; i < count; ++i) {
          std::string name = "";
          uint32_t size = 0;
          if (!field_codec::read_sized_string(body, offset, name) || !field_codec::read_raw(body, offset, size) || offset + size > body.size()) {
            CORE_LOG_ERROR("truncated script component fields");
            break;
          }
          contents.fields.emplace_back(std::move(name), ostd::vector<uint8_t>{ body.begin() + offset, body.begin() + offset + size });
          offset += size;
        }
        return contents;
      }

      ostd::vector<uint8_t> encode_script_payload(const script_payload& contents) {
        ostd::vector<uint8_t> payload = {};
        if (contents.behaviors.empty()) {
          return payload;  /// empty payload -> implicit component record is omitted
        }
        field_codec::write_raw<natural_t>(FNV(std::string{ kBehaviorsFieldName }), payload);
        field_codec::encode_value(contents.behaviors, payload);
        if (!contents.disabled.empty()) {
          field_codec::write_raw<natural_t>(FNV(std::string{ kDisabledFieldName }), payload);
          field_codec::encode_value(contents.disabled, payload);
        }
        if (contents.fields.empty()) {
          return payload;
        }

        ostd::vector<uint8_t> body = {};
        field_codec::write_raw<uint32_t>(static_cast<uint32_t>(contents.fields.size()), body);
        for (const auto& [name, stream] : contents.fields) {
          field_codec::write_sized_string(name, body);
          field_codec::write_raw<uint32_t>(static_cast<uint32_t>(stream.size()), body);
          body.insert(body.end(), stream.begin(), stream.end());
        }
        /// one frame an older reader skips by its size
        field_codec::write_raw<natural_t>(FNV(std::string{ kFieldsFieldName }), payload);
        field_codec::write_raw<uint8_t>(static_cast<uint8_t>(value_type::USER_TYPE), payload);
        field_codec::write_raw<uint32_t>(static_cast<uint32_t>(body.size()), payload);
        payload.insert(payload.end(), body.begin(), body.end());
        return payload;
      }

      bool has_fields(std::span<const uint8_t> stream) {
        uint32_t count = 0;
        size_t offset = 0;
        return field_codec::read_raw(stream, offset, count) && count > 0;
      }

      /// a behavior's fields as the keys of the table the writer is in. a struct is a table
      ///  under it, written after the keys as toml wants
      void fields_to_toml(std::span<const uint8_t> stream, toml_writer& w, const std::string& table_path) {
        ostd::vector<behavior_fields::field> fields = {};
        if (!behavior_fields::read(stream, fields)) {
          return;
        }

        for (const behavior_fields::field& f : fields) {
          const std::string key{ f.name };
          switch (f.kind) {
            case behavior_field_kind::BOOL: {
              uint8_t v = 0;
              if (f.value(v)) {
                w.key(key, v != 0);
              }
            } break;

            case behavior_field_kind::INT32: {
              int32_t v = 0;
              if (f.value(v)) {
                w.key(key, static_cast<int64_t>(v));
              }
            } break;

            case behavior_field_kind::UINT32: {
              uint32_t v = 0;
              if (f.value(v)) {
                w.key(key, static_cast<int64_t>(v));
              }
            } break;

            case behavior_field_kind::INT64: {
              int64_t v = 0;
              if (f.value(v)) {
                w.key(key, v);
              }
            } break;

            case behavior_field_kind::UINT64: {
              /// toml has no unsigned 64 bit integer: the same bits, read back the same way
              uint64_t v = 0;
              if (f.value(v)) {
                w.key(key, static_cast<int64_t>(v));
              }
            } break;

            case behavior_field_kind::FLOAT: {
              float v = 0.f;
              if (f.value(v)) {
                w.key(key, v);
              }
            } break;

            case behavior_field_kind::DOUBLE: {
              double v = 0.0;
              if (f.value(v)) {
                w.key(key, v);
              }
            } break;

            case behavior_field_kind::TEXT:
            case behavior_field_kind::ENUM: w.key(key, f.text()); break;

            case behavior_field_kind::ASSET: {
              const std::string_view path = f.text();
              w.key_inline_table(key, [&path](toml_writer::inline_table& table) {
                table.key(kAssetKey, path);
              });
            } break;

            case behavior_field_kind::OBJECT: {
              natural_t id = 0;
              if (f.value(id)) {
                w.key_inline_table(key, [id](toml_writer::inline_table& table) {
                  table.key(kObjectKey, id);
                });
              }
            } break;

            case behavior_field_kind::VEC2:
            case behavior_field_kind::VEC3:
            case behavior_field_kind::VEC4:
            case behavior_field_kind::QUAT: {
              ostd::vector<float> components(f.body.size() / sizeof(float));
              std::memcpy(components.data(), f.body.data(), components.size() * sizeof(float));
              w.key_array(key, components);
            } break;

            default: break;
          }
        }

        for (const behavior_fields::field& f : fields) {
          if (f.kind != behavior_field_kind::STRUCT) {
            continue;
          }
          const std::string nested_path = std::format("{}.{}", table_path, toml_writer::path_segment(f.name));
          w.table(nested_path);
          fields_to_toml(f.body, w, nested_path);
        }
      }

      /// a text scene does not say what kind a number is, so a field read from one is the
      ///  widest of its sort and the script takes it as the kind its field is
      ostd::vector<uint8_t> fields_from_toml(const toml::table& table, const std::string& where, ostd::vector<std::string>& warnings) {
        behavior_fields::writer out = {};
        for (const auto& [key, node] : table) {
          const std::string_view name = key.str();
          if (node.is_boolean()) {
            out.add_value<uint8_t>(name, behavior_field_kind::BOOL, node.value_or(false) ? 1 : 0);
          } else if (node.is_integer()) {
            out.add_value<int64_t>(name, behavior_field_kind::INT64, node.value_or<int64_t>(0));
          } else if (node.is_floating_point()) {
            out.add_value<double>(name, behavior_field_kind::DOUBLE, node.value_or(0.0));
          } else if (node.is_string()) {
            const std::string text = node.value_or<std::string>("");
            out.add_text(name, behavior_field_kind::TEXT, text);
          } else if (const toml::array* array = node.as_array(); array != nullptr) {
            ostd::vector<float> components = {};
            for (const toml::node& element : *array) {
              if (element.is_number()) {
                components.push_back(static_cast<float>(element.value_or(0.0)));
              }
            }
            if (components.size() != array->size() || components.size() < 2 || components.size() > 4) {
              warnings.push_back(std::format("script field '{}.{}' is an array that is not two to four numbers; ignored", where, name));
              continue;
            }
            constexpr behavior_field_kind kKinds[] = { behavior_field_kind::VEC2, behavior_field_kind::VEC3, behavior_field_kind::VEC4 };
            out.add(name, kKinds[components.size() - 2], std::span<const uint8_t>{ reinterpret_cast<const uint8_t*>(components.data()), components.size() * sizeof(float) });
          } else if (const toml::table* nested = node.as_table(); nested != nullptr) {
            const toml::node* object = nested->size() == 1 ? nested->get(kObjectKey) : nullptr;
            const toml::node* asset = nested->size() == 1 ? nested->get(kAssetKey) : nullptr;
            if (object != nullptr && object->is_integer()) {
              out.add_value<natural_t>(name, behavior_field_kind::OBJECT, static_cast<natural_t>(object->value_or<int64_t>(0)));
            } else if (asset != nullptr && asset->is_string()) {
              const std::string path = asset->value_or<std::string>("");
              out.add_text(name, behavior_field_kind::ASSET, path);
            } else {
              const ostd::vector<uint8_t> stream = fields_from_toml(*nested, std::format("{}.{}", where, name), warnings);
              out.add(name, behavior_field_kind::STRUCT, stream);
            }
          } else {
            warnings.push_back(std::format("script field '{}.{}' is of a kind a script field cannot be; ignored", where, name));
          }
        }
        return out.take();
      }

      /// script_component's persistent state (which behaviors it has, which are switched off
      ///  and what their fields hold) lives in the scripting environment's script_object, not
      ///  the component struct - hence a custom codec, same payload discipline
      component_codec make_script_codec() {
        component_codec codec = {
          .key = "script",
          .display_name = "Script",
          .key_hash = FNV(std::string{ "script" }),
          .implicit = true,
        };

        codec.has = [](scene& s, scene_object* object) -> bool {
          return s.has_component<script_component>(object);
        };

        codec.capture = [](scene& s, scene_object* object, const codec_services&) -> ostd::vector<uint8_t> {
          const script_component* component = s.get_component<script_component>(object);
          OTHER_ASSERT(component != nullptr, "script capture called for an object without a script component");
          if (component->script_object_id < 0) {
            return {};
          }

          auto* script_env = subsystem<scripting_environment>::get();
          OTHER_ASSERT(script_env != nullptr, "scripting environment unavailable during scene capture");
          script_object* script_obj = script_env->get_object(component->script_object_id);
          if (script_obj == nullptr) {
            return {};
          }

          script_payload contents = {};
          for (const auto& handle : script_obj->behavior_handles) {
            contents.behaviors.push_back(handle.type_name);
            if (!handle.enabled) {
              contents.disabled.push_back(handle.type_name);
            }
            ostd::vector<uint8_t> stream = script_env->capture_behavior_fields(component->script_object_id, handle.type_name);
            if (has_fields(stream)) {
              contents.fields.emplace_back(handle.type_name, std::move(stream));
            }
          }
          return encode_script_payload(contents);
        };

        codec.apply = [](scene& s, scene_object* object, std::span<const uint8_t> payload, const codec_services&) {
          script_component* component = s.get_component<script_component>(object);
          OTHER_ASSERT(component != nullptr, "script apply called for an object without a script component");
          const script_payload contents = decode_script_payload(payload);
          for (const std::string& name : contents.behaviors) {
            /// attach is idempotent,  behaviors surviving a play-stop restore are kept as-is
            component->add_behavior(name);
          }

          /// reconcile: behaviors on the live object absent from the document are genuine removals
          ///  (e.g. added during play); objects with an empty captured list emit no record, so those escape this prune
          auto* script_env = subsystem<scripting_environment>::get();
          if (script_env == nullptr || component->script_object_id < 0) {
            return;
          }
          script_object* script_obj = script_env->get_object(component->script_object_id);
          if (script_obj == nullptr) {
            return;
          }
          ostd::vector<std::string> extras = {};
          for (const auto& handle : script_obj->behavior_handles) {
            if (std::find(contents.behaviors.begin(), contents.behaviors.end(), handle.type_name) == contents.behaviors.end()) {
              extras.push_back(handle.type_name);
            }
          }
          for (const std::string& extra : extras) {
            component->remove_behavior(extra);
          }

          /// the switch is the document's: one that play, or a throw, switched off is on again
          for (const std::string& name : contents.behaviors) {
            const auto handle = std::ranges::find(script_obj->behavior_handles, name, &script_object::behavior_handle::type_name);
            const bool enabled = !std::ranges::contains(contents.disabled, name);
            if (handle != script_obj->behavior_handles.end() && handle->enabled != enabled) {
              script_env->set_behavior_enabled(component->script_object_id, name, enabled);
            }
          }
        };

        /// fields wait for every object of the document: one may name an object after its own
        codec.finish = [](scene& s, scene_object* object, std::span<const uint8_t> payload, const codec_services&, const ostd::map<natural_t, natural_t>& ids) {
          const script_component* component = s.get_component<script_component>(object);
          auto* script_env = subsystem<scripting_environment>::get();
          if (component == nullptr || script_env == nullptr || component->script_object_id < 0) {
            return;
          }
          const script_payload contents = decode_script_payload(payload);
          if (contents.fields.empty()) {
            return;
          }

          const behavior_object_resolver resolve = [&s, script_env](natural_t object_id) -> void* {
            scene_object* named = s.find_object(object_id);
            if (named == nullptr || !s.has_component<script_component>(named)) {
              return nullptr;
            }
            return script_env->get_object_instance(s.get_component<script_component>(named)->script_object_id);
          };
          for (const auto& [name, stream] : contents.fields) {
            script_env->apply_behavior_fields(component->script_object_id, name, behavior_fields::remap_objects(stream, ids), resolve);
          }
        };

        codec.payload_to_toml = [](std::span<const uint8_t> payload, toml_writer& w, const std::string& table_path) {
          const script_payload contents = decode_script_payload(payload);
          if (contents.behaviors.empty()) {
            return;
          }
          w.key_array(kBehaviorsFieldName, contents.behaviors);
          if (!contents.disabled.empty()) {
            w.key_array(kDisabledFieldName, contents.disabled);
          }
          for (const auto& [name, stream] : contents.fields) {
            const std::string fields_path = std::format("{}.{}.{}", table_path, kFieldsFieldName, toml_writer::path_segment(name));
            w.table(fields_path);
            fields_to_toml(stream, w, fields_path);
          }
        };

        codec.payload_from_toml = [](const toml::table& table, ostd::vector<std::string>& warnings) -> ostd::vector<uint8_t> {
          script_payload contents = {};
          if (const toml::node* node = table.get(kBehaviorsFieldName); node != nullptr) {
            if (!detail::read_toml_value(*node, contents.behaviors, warnings)) {
              warnings.push_back("script component 'behaviors' must be an array of strings");
            }
          }
          if (const toml::node* node = table.get(kDisabledFieldName); node != nullptr) {
            if (!detail::read_toml_value(*node, contents.disabled, warnings)) {
              warnings.push_back("script component 'disabled' must be an array of strings");
            }
          }
          if (const toml::table* fields = table.get_as<toml::table>(kFieldsFieldName); fields != nullptr) {
            for (const auto& [key, node] : *fields) {
              const toml::table* behavior_fields_table = node.as_table();
              if (behavior_fields_table == nullptr) {
                warnings.push_back(std::format("script component 'fields.{}' must be a table", std::string{ key.str() }));
                continue;
              }
              ostd::vector<uint8_t> stream = fields_from_toml(*behavior_fields_table, std::string{ key.str() }, warnings);
              if (has_fields(stream)) {
                contents.fields.emplace_back(std::string{ key.str() }, std::move(stream));
              }
            }
          }
          return encode_script_payload(contents);
        };

        codec.collect_asset_refs = [](std::span<const uint8_t> payload, ostd::vector<component_asset_ref>& out) {
          const script_payload contents = decode_script_payload(payload);
          ostd::vector<std::string> paths = {};
          for (const auto& [name, stream] : contents.fields) {
            behavior_fields::collect_assets(stream, paths);
          }
          /// a script's field does not say what kind of asset it holds: its extension does
          for (std::string& path : paths) {
            out.push_back({ .path = std::move(path), .type = asset::EMPTY });
          }
        };

        codec.rewrite_asset_refs = [](std::span<const uint8_t> payload, const asset_path_rewrite_fn& rewrite) -> ostd::vector<uint8_t> {
          script_payload contents = decode_script_payload(payload);
          if (contents.fields.empty()) {
            return ostd::vector<uint8_t>{ payload.begin(), payload.end() };
          }
          for (auto& [name, stream] : contents.fields) {
            stream = behavior_fields::rewrite_assets(stream, rewrite);
          }
          return encode_script_payload(contents);
        };

        return codec;
      }

      /// authored camera tables usually give only position/direction; the view matrix reads the basis,
      ///  so canonicalize it (look()'s degenerate-up fallback) after the generic field apply
      component_codec make_camera_codec() {
        component_codec codec = make_generic_codec<camera_component>("camera", "Camera", /*implicit=*/false);
        auto generic_apply = codec.apply;
        codec.apply = [generic_apply](scene& s, scene_object* object, std::span<const uint8_t> payload, const codec_services& services) {
          generic_apply(s, object, payload, services);
          camera_component* component = s.get_component<camera_component>(object);
          OTHER_ASSERT(component != nullptr, "camera apply left the object without a camera component");
          component->camera.look(component->camera.position, component->camera.position + component->camera.direction);
        };
        return codec;
      }

      ostd::vector<component_codec> build_builtin_codecs() {
        ostd::vector<component_codec> codecs = {};
        codecs.push_back(make_generic_codec<transform>("transform", "Transform", /*implicit=*/true));
        codecs.push_back(make_script_codec());
        codecs.push_back(make_generic_codec<render_component>("render", "Graphics Object", /*implicit=*/false));
        codecs.push_back(make_generic_codec<physics_component>("physics", "Physics Object", /*implicit=*/false));
        codecs.push_back(make_generic_codec<physics_joint_component>("physics-joint", "Physics Joint", /*implicit=*/false));
        codecs.push_back(make_camera_codec());
        codecs.push_back(make_generic_codec<grid_component>("grid", "Grid", /*implicit=*/false));
        codecs.push_back(make_generic_codec<point_light_component>("point-light", "Point Light", /*implicit=*/false));
        codecs.push_back(make_generic_codec<direction_light_component>("direction-light", "Directional Light", /*implicit=*/false));
        codecs.push_back(make_generic_codec<animation_component>("animation", "Animation", /*implicit=*/false));
        codecs.push_back(make_generic_codec<audio_source_component>("audio-source", "Audio Source", /*implicit=*/false));
        codecs.push_back(make_generic_codec<audio_listener_component>("audio-listener", "Audio Listener", /*implicit=*/false));
        codecs.push_back(make_generic_codec<network_component>("network", "Network", /*implicit=*/false));
        codecs.push_back(make_generic_codec<network_settings_component>("network-settings", "Network Settings", /*implicit=*/false));

        for (size_t i = 0; i < codecs.size(); ++i) {
          for (size_t j = i + 1; j < codecs.size(); ++j) {
            OTHER_ASSERT(codecs[i].key_hash != codecs[j].key_hash, "component codec key collision: '{}' vs '{}'", codecs[i].key, codecs[j].key);
          }
        }
        return codecs;
      }

    }  // namespace

    const ostd::vector<component_codec>& component_codecs() {
      /// leaked on purpose: a static's destructor runs after arena shutdown (UAF)
      static const auto* codecs = new ostd::vector<component_codec>(build_builtin_codecs());
      return *codecs;
    }

    const component_codec* find_component_codec(natural_t key_hash) {
      for (const component_codec& codec : component_codecs()) {
        if (codec.key_hash == key_hash) {
          return &codec;
        }
      }
      return nullptr;
    }

    const component_codec* find_component_codec(std::string_view key) {
      for (const component_codec& codec : component_codecs()) {
        if (codec.key == key) {
          return &codec;
        }
      }
      return nullptr;
    }

    codec_services& default_codec_services() {
      static codec_services services = {};
      return services;
    }

  }  // namespace serialization
}  // namespace other
