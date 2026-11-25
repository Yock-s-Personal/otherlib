/**
 * \file render/pipeline_declarations.cpp
 **/
#include "render/pipeline_declarations.hpp"

#include <magic_enum/magic_enum.hpp>

#include "core/fnv.hpp"
#include "core/logger.hpp"
#include "core/profiler.hpp"

#include "render/material_declarations.hpp"
#include "vm/command_bus.hpp"
#include "vm/command_files/ocmd_toolchain.hpp"
#include "vm/diagnostics/diagnostic_engine.hpp"

namespace other {
  namespace {

    /// what went wrong while one footprint was read; all of it is logged together, so a
    ///  pipeline with three faults is not mended one run at a time
    struct decoding {
      ostd::vector<std::string> errors;

      template <typename... Args>
      void fail(std::format_string<Args...> format, Args&&... args) {
        errors.push_back(std::format(format, std::forward<Args>(args)...));
      }
    };

    std::string label(const footprint_node& node, const std::string_view kind) {
      return node.name.empty() ? std::format("a {}", kind) : std::format("{} '{}'", kind, node.name);
    }

    const asset_value* value_of(const footprint_node& node, const std::string_view field, const asset_value_kind kind) {
      const footprint_field* found = node.field(asset_hash(field));
      return found != nullptr && found->value.kind == kind ? &found->value : nullptr;
    }

    /// a field that is not there is what the renderer's own definition starts as
    std::string word_of(const footprint_node& node, const std::string_view field, const std::string_view otherwise = "") {
      const asset_value* found = value_of(node, field, ASSET_VALUE_WORD);
      return found != nullptr ? found->text : std::string{ otherwise };
    }

    std::string text_of(const footprint_node& node, const std::string_view field) {
      const asset_value* found = value_of(node, field, ASSET_VALUE_TEXT);
      return found != nullptr ? found->text : std::string{};
    }

    int64_t integer_of(const footprint_node& node, const std::string_view field, const int64_t otherwise = 0) {
      const asset_value* found = value_of(node, field, ASSET_VALUE_INTEGER);
      return found != nullptr ? found->integer : otherwise;
    }

    bool truth_of(const footprint_node& node, const std::string_view field, const bool otherwise) {
      const asset_value* found = value_of(node, field, ASSET_VALUE_BOOL);
      return found != nullptr ? found->integer != 0 : otherwise;
    }

    std::span<const asset_value> list_of(const footprint_node& node, const std::string_view field) {
      const asset_value* found = value_of(node, field, ASSET_VALUE_LIST);
      return found != nullptr ? std::span<const asset_value>{ found->items } : std::span<const asset_value>{};
    }

    bool is_number(const asset_value& value) {
      return value.kind == ASSET_VALUE_INTEGER || value.kind == ASSET_VALUE_DECIMAL;
    }

    float number_of(const asset_value& value) {
      return value.kind == ASSET_VALUE_DECIMAL ? static_cast<float>(value.decimal) : static_cast<float>(value.integer);
    }

    /// the parts of one kind, in the order the footprint has them
    auto parts_of(const footprint_node& node, const std::string_view kind) {
      return node.parts | std::views::filter([hash = asset_hash(kind)](const footprint_node& part) { return part.kind == hash; });
    }

    template <typename T>
    struct named {
      std::string_view word;
      T value;
    };

    /// the engine's value for a word of the language; a word it has none for is a fault of the footprint
    template <typename T, size_t N>
    opt<T> from_word(const named<T> (&table)[N], const std::string_view word) {
      const auto found = std::ranges::find(table, word, &named<T>::word);
      return found != std::end(table) ? opt<T>{ found->value } : std::nullopt;
    }

    constexpr named<gpu_buffer::buf_type> kBufferTypes[] = {
      { "uniform", gpu_buffer::UNIFORM_BUFFER },
      { "storage", gpu_buffer::STORAGE_BUFFER },
      { "draw-indirect", gpu_buffer::DRAW_INDIRECT_BUFFER },
    };
    constexpr named<binding_type> kBindingTypes[] = {
      { "uniform", binding_type::UNIFORM_BUFFER },
      { "storage", binding_type::STORAGE_BUFFER },
      { "draw-indirect", binding_type::DRAW_INDIRECT_BUFFER },
    };
    constexpr named<gpu_buffer::usage> kBufferUsages[] = {
      { "static", gpu_buffer::STATIC },
      { "dynamic", gpu_buffer::DYNAMIC },
      { "stream", gpu_buffer::STREAM },
    };
    constexpr named<texture::tex_type> kTextureTypes[] = {
      { "line", texture::TEXTURE_1D },
      { "flat", texture::TEXTURE_2D },
      { "volume", texture::TEXTURE_3D },
      { "cube", texture::TEXTURE_CUBE },
    };
    constexpr named<render_pass::type> kPassTypes[] = {
      { "render", render_pass::RENDER_PASS },
      { "compute", render_pass::COMPUTE_PASS },
    };
    constexpr named<framebuffer::attachment_type> kAttachments[] = {
      { "color", framebuffer::COLOR },
      { "depth", framebuffer::DEPTH },
      { "stencil", framebuffer::STENCIL },
      { "depth-stencil", framebuffer::DEPTH_STENCIL },
    };
    constexpr named<access_flags> kAccesses[] = {
      { "read", access_flags::READ },
      { "write", access_flags::WRITE },
      { "read-write", access_flags::READ_WRITE },
      { "sample", access_flags::SAMPLE },
    };
    constexpr named<framebuffer::clear_mask_bit> kClearBits[] = {
      { "color", framebuffer::COLOR_BIT },
      { "depth", framebuffer::DEPTH_BIT },
      { "stencil", framebuffer::STENCIL_BIT },
      { "all", framebuffer::ALL_BITS },
    };
    /// the word a binding's name starts with; see binding_label
    struct scope_words {
      binding_scope scope;
      std::string_view prefix;
    };
    constexpr named<scope_words> kBindingScopes[] = {
      { "per-pipeline", { binding_scope::PER_PIPELINE, "per_pipeline" } },
      { "per-frame", { binding_scope::PER_FRAME, "per_frame" } },
      { "per-pass", { binding_scope::PER_PASS, "per_pass" } },
      { "per-draw", { binding_scope::PER_DRAW_CALL, "per_draw" } },
      { "per-instance", { binding_scope::PER_INSTANCE, "per_instance" } },
    };
    constexpr named<material_value::kind> kParamTypes[] = {
      { "float", material_value::kind::F32 },
      { "vec2", material_value::kind::VEC2 },
      { "vec3", material_value::kind::VEC3 },
      { "vec4", material_value::kind::VEC4 },
      { "int", material_value::kind::I32 },
      { "bool", material_value::kind::B32 },
    };

    /// a format is the engine's own name for it in small letters
    opt<texture::format> format_from_word(const std::string_view word) {
      std::string name{ word };
      std::ranges::transform(name, name.begin(), [](const unsigned char c) { return static_cast<char>(std::toupper(c)); });
      const auto found = magic_enum::enum_cast<texture::format>(name);
      return found.has_value() && *found != texture::NUM_FORMATS ? opt<texture::format>{ *found } : std::nullopt;
    }

    /// the engine names a binding for its logs. the declaration names it by its tag, and this
    ///  is the name the TOML gave the same binding
    std::string binding_label(const std::string_view scope_prefix, const std::string_view tag) {
      return std::format("{}.{}_buffer", scope_prefix, tag);
    }

    void decode_resources(decoding& state, const footprint_node& pipeline, pipeline_definition& def) {
      for (const footprint_node& part : parts_of(pipeline, "buffer")) {
        const opt<gpu_buffer::buf_type> type = from_word(kBufferTypes, word_of(part, "type"));
        const opt<gpu_buffer::usage> usage = from_word(kBufferUsages, word_of(part, "usage", "dynamic"));
        const std::string tag = word_of(part, "tag");
        if (!type.has_value() || !usage.has_value() || tag.empty()) {
          state.fail("{} needs a type, a usage and a tag the engine knows; it has '{}', '{}' and '{}'", label(part, "buffer"), word_of(part, "type"), word_of(part, "usage", "dynamic"), tag);
          continue;
        }
        def.buffers.push_back({ .name = part.name, .type = *type, .usage = *usage, .tag = resource_tag_from_string(tag) });
        /// a buffer the engine fills is one the pipeline cannot be without
        def.required_tags.push_back(resource_tag_from_string(tag));
      }

      for (const footprint_node& part : parts_of(pipeline, "texture")) {
        const opt<texture::tex_type> type = from_word(kTextureTypes, word_of(part, "type"));
        const opt<texture::format> format = format_from_word(word_of(part, "format"));
        const std::span<const asset_value> size = list_of(part, "size");
        const bool whole = std::ranges::all_of(size, [](const asset_value& side) { return side.kind == ASSET_VALUE_INTEGER && side.integer > 0; });
        if (!type.has_value() || !format.has_value() || size.size() == 1 || size.size() > 3 || !whole) {
          state.fail("{} needs a type and a format the engine knows, and a size of none, two or three whole numbers; it has '{}', '{}' and {} number(s)", label(part, "texture"),
                     word_of(part, "type"), word_of(part, "format"), size.size());
          continue;
        }

        pipeline_texture_definition& made = def.textures.emplace_back();
        made.name = part.name;
        made.type = *type;
        made.format = *format;
        /// none of its own is the window's
        made.use_window_size = size.empty();
        if (!size.empty()) {
          made.fixed_size = { static_cast<int32_t>(size[0].integer), static_cast<int32_t>(size[1].integer) };
          made.depth = size.size() == 3 ? static_cast<uint32_t>(size[2].integer) : 0;
        }
        if (const std::string seed = text_of(part, "seed"); !seed.empty()) {
          made.seed_texture_path = seed;
        }
      }

      for (const footprint_node& part : parts_of(pipeline, "shader")) {
        const std::string compute = text_of(part, "compute");
        const std::string vertex = text_of(part, "vertex");
        const std::string fragment = text_of(part, "fragment");
        const std::string geometry = text_of(part, "geometry");
        if (!compute.empty()) {
          def.shaders.push_back({ .name = part.name, .compute_path = compute });
        } else if (!vertex.empty() && !fragment.empty()) {
          pipeline_shader_definition& made = def.shaders.emplace_back();
          made.name = part.name;
          made.vertex_path = vertex;
          made.fragment_path = fragment;
          if (!geometry.empty()) {
            made.geometry_path = geometry;
          }
        } else {
          state.fail("{} needs a compute stage, or a vertex and a fragment stage", label(part, "shader"));
        }
      }
    }

    opt<material_value> material_default(const asset_value* given, const material_value::kind kind) {
      if (given == nullptr) {
        material_value zero = {};
        zero.value_kind = kind;
        return zero;
      }
      switch (kind) {
        case material_value::kind::F32: return is_number(*given) ? opt<material_value>{ material_value::from(number_of(*given)) } : std::nullopt;
        case material_value::kind::I32: return given->kind == ASSET_VALUE_INTEGER ? opt<material_value>{ material_value::from(static_cast<int32_t>(given->integer)) } : std::nullopt;
        case material_value::kind::B32: return given->kind == ASSET_VALUE_BOOL ? opt<material_value>{ material_value::from(given->integer != 0) } : std::nullopt;
        default: {
          const size_t expected = kind == material_value::kind::VEC2 ? 2 : kind == material_value::kind::VEC3 ? 3 :
                                                                                                                4;
          if (given->kind != ASSET_VALUE_LIST || given->items.size() != expected || !std::ranges::all_of(given->items, is_number)) {
            return std::nullopt;
          }
          material_value made = {};
          made.value_kind = kind;
          for (size_t i = 0; i < expected; ++i) {
            made.data[static_cast<glm::length_t>(i)] = number_of(given->items[i]);
          }
          return made;
        }
      }
    }

    void decode_material_layout(decoding& state, const footprint_node& pipeline, pipeline_definition& def) {
      auto layouts = parts_of(pipeline, "material-layout");
      if (layouts.empty()) {
        /// none declared: this pipeline draws without materials
        return;
      }

      const footprint_node& declared = layouts.front();
      material_layout layout;
      for (const footprint_node& part : parts_of(declared, "param")) {
        const opt<material_value::kind> kind = from_word(kParamTypes, word_of(part, "type"));
        const footprint_field* given = part.field(asset_hash("default"));
        const opt<material_value> initial = kind.has_value() ? material_default(given != nullptr ? &given->value : nullptr, *kind) : std::nullopt;
        if (!initial.has_value()) {
          state.fail("{} of the material layout needs a type the engine knows and a default of that type; its type is '{}'", label(part, "param"), word_of(part, "type"));
          continue;
        }
        material_layout::param& param = layout.params.emplace_back();
        param.name = part.name;
        param.kind = *kind;
        param.default_value = *initial;
      }
      for (const footprint_node& part : parts_of(declared, "slot")) {
        material_layout::texture_slot& slot = layout.texture_slots.emplace_back();
        slot.name = part.name;
        slot.uniform = text_of(part, "uniform");
        slot.unit = static_cast<uint32_t>(integer_of(part, "unit"));
      }
      if (!state.errors.empty()) {
        return;
      }
      layout.finalize();
      def.materials = std::move(layout);
    }

    /// what a pass's shader is told by what runs it, into param. the engine's executors take
    ///  these three; false for any other, or for one given a value of the wrong type
    bool executor_param(const std::string_view name, const asset_value& given, value& param) {
      switch (FNV(name)) {
        case FNV("barrier"):
          if (given.kind != ASSET_VALUE_WORD && given.kind != ASSET_VALUE_TEXT) {
            return false;
          }
          param = given.text;
          return true;
        case FNV("voxel_dim"):
          if (given.kind != ASSET_VALUE_INTEGER) {
            return false;
          }
          param = static_cast<int>(given.integer);
          return true;

        case FNV("groups"): {
          if (given.kind != ASSET_VALUE_LIST || given.items.size() > 3 || !std::ranges::all_of(given.items, is_number)) {
            return false;
          }
          glm::vec3 groups{ 1.f, 1.f, 1.f };
          for (size_t i = 0; i < given.items.size(); ++i) {
            groups[static_cast<glm::length_t>(i)] = std::floor(number_of(given.items[i]));
          }
          param = groups;
          return true;
        }

        default: return false;
      }
    }

    void decode_pass(decoding& state, const footprint_node& pipeline, const footprint_node& declared, pipeline_definition& def) {
      const std::string owner = label(declared, "pass");
      const opt<render_pass::type> type = from_word(kPassTypes, word_of(declared, "type"));
      const std::string shader_name = word_of(declared, "shader");
      const std::string executor = word_of(declared, "run");
      if (!type.has_value() || shader_name.empty() || executor.empty() || declared.name.empty()) {
        state.fail("{} needs a name, a type the engine knows, a shader and what runs it; it has type '{}', shader '{}' and run '{}'", owner, word_of(declared, "type"), shader_name, executor);
        return;
      }

      pipeline_pass_definition& pass = def.passes.emplace_back();
      pass.name = declared.name;
      pass.pass_type = *type;
      pass.shader_name = shader_name;
      pass.executor = { .name = executor };
      /// a compute pass draws into no framebuffer, whatever it says
      pass.create_framebuffer = truth_of(declared, "framebuffer", true) && *type != render_pass::COMPUTE_PASS;

      if (const std::span<const asset_value> colour = list_of(declared, "clear-color"); !colour.empty()) {
        if (colour.size() < 3 || colour.size() > 4 || !std::ranges::all_of(colour, is_number)) {
          state.fail("{}: clear-color is three or four numbers", owner);
        } else {
          pass.clear_color = glm::vec4{ 0.f };
          for (size_t i = 0; i < colour.size(); ++i) {
            (*pass.clear_color)[static_cast<glm::length_t>(i)] = number_of(colour[i]);
          }
        }
      }
      if (const std::span<const asset_value> size = list_of(declared, "size"); !size.empty()) {
        if (size.size() != 2 || size[0].kind != ASSET_VALUE_INTEGER || size[1].kind != ASSET_VALUE_INTEGER) {
          state.fail("{}: size is two whole numbers, or nothing for the window's", owner);
        } else {
          pass.use_window_size = false;
          pass.fixed_size = { static_cast<int32_t>(size[0].integer), static_cast<int32_t>(size[1].integer) };
        }
      }

      /// [default] is what its framebuffer clears, which is what a pass that says nothing gets
      const std::span<const asset_value> clear = list_of(declared, "clear");
      const bool says_nothing = declared.field(asset_hash("clear")) == nullptr || (clear.size() == 1 && clear.front().text == "default");
      if (!says_nothing) {
        int32_t flags = framebuffer::NONE;
        for (const asset_value& bit : clear) {
          const opt<framebuffer::clear_mask_bit> known = from_word(kClearBits, bit.text);
          if (bit.kind != ASSET_VALUE_WORD || !known.has_value()) {
            state.fail("{}: '{}' is not something a pass clears", owner, bit.text);
            continue;
          }
          flags |= *known;
        }
        pass.clear_flags = static_cast<framebuffer::clear_mask_bit>(flags);
        pass.override_fb_clear = true;
      }

      for (const footprint_node& part : parts_of(declared, "bind")) {
        const auto binding = std::ranges::find_if(pipeline.parts, [&](const footprint_node& candidate) { return candidate.kind == asset_hash("binding") && candidate.name == part.name; });
        const opt<scope_words> scope = binding != pipeline.parts.end() ? from_word(kBindingScopes, word_of(*binding, "scope")) : std::nullopt;
        const opt<binding_type> kind = binding != pipeline.parts.end() ? from_word(kBindingTypes, word_of(*binding, "type")) : std::nullopt;
        if (!scope.has_value() || !kind.has_value()) {
          state.fail("{} binds '{}', which is not a binding of the pipeline with a scope and a type the engine knows", owner, part.name);
          continue;
        }

        const resource_tag tag = resource_tag_from_string(part.name);
        uint32_t element_size = static_cast<uint32_t>(integer_of(*binding, "size"));
        if (tag.value() == resource_tag::kMaterialTag && scope->scope == binding_scope::PER_DRAW_CALL && def.materials.has_value()) {
          /// the material block's size is the layout's; one that is written has to agree with it
          const uint32_t derived = def.materials->element_size * def.materials->instance_capacity;
          if (element_size != 0 && element_size != derived) {
            state.fail("binding '{}': its size {} is not the {} the material layout comes to; leave the size out, it is worked out", part.name, element_size, derived);
          }
          element_size = derived;
        }
        pass.bindings.push_back({
          .name = binding_label(scope->prefix, part.name),
          .tag = tag,
          .scope = scope->scope,
          .type = *kind,
          .binding = static_cast<uint32_t>(integer_of(part, "at")),
          .element_size = element_size,
        });
      }

      const auto reference = [&](const footprint_node& part, const std::string_view how, const access_flags access) -> opt<pipeline_resource_reference> {
        const bool is_texture = std::ranges::contains(def.textures, part.name, &pipeline_texture_definition::name);
        const bool is_buffer = std::ranges::contains(def.buffers, part.name, &pipeline_buffer_definition::name);
        if (!is_texture && !is_buffer) {
          state.fail("{} {}s '{}', which is not a texture or a buffer of the pipeline", owner, how, part.name);
          return std::nullopt;
        }
        return pipeline_resource_reference{
          .resource_name = part.name,
          .type = is_texture ? resource_type::TEXTURE : resource_type::BUFFER,
          .uniform_name = text_of(part, "as"),
          .binding = static_cast<uint32_t>(integer_of(part, "at")),
          .access = access,
        };
      };
      for (const footprint_node& part : parts_of(declared, "read")) {
        const opt<access_flags> access = from_word(kAccesses, word_of(part, "access", "read"));
        if (!access.has_value()) {
          state.fail("{} reads '{}' with '{}', which is not a way to read", owner, part.name, word_of(part, "access"));
          continue;
        }
        if (opt<pipeline_resource_reference> input = reference(part, "read", *access); input.has_value()) {
          pass.inputs.push_back(std::move(*input));
        }
      }
      for (const footprint_node& part : parts_of(declared, "write")) {
        const opt<framebuffer::attachment_type> attachment = from_word(kAttachments, word_of(part, "attach", "color"));
        if (!attachment.has_value()) {
          state.fail("{} writes '{}' as '{}', which is not something a framebuffer has", owner, part.name, word_of(part, "attach"));
          continue;
        }
        if (opt<pipeline_resource_reference> output = reference(part, "write", access_flags::WRITE); output.has_value()) {
          output->attachment = *attachment;
          pass.outputs.push_back(std::move(*output));
        }
      }

      for (const footprint_node& part : parts_of(declared, "with")) {
        const footprint_field* given = part.field(asset_hash("value"));
        value param;
        if (given == nullptr || !executor_param(part.name, given->value, param)) {
          state.fail("{}: '{}' is not something an executor is told, or its value is of the wrong type. there are barrier (a word), groups (up to three numbers) and voxel_dim (a whole number)", owner, part.name);
          continue;
        }
        pass.executor.params.insert({ part.name, std::move(param) });
      }

      for (const footprint_node& part : parts_of(declared, "set")) {
        const footprint_field* given = part.field(asset_hash("value"));
        pass_uniform_definition uniform = { .pass_name = pass.name, .name = part.name };
        if (given != nullptr && given->value.kind == ASSET_VALUE_DECIMAL) {
          uniform.val = static_cast<float>(given->value.decimal);
        } else if (given != nullptr && given->value.kind == ASSET_VALUE_INTEGER) {
          uniform.val = static_cast<int32_t>(given->value.integer);
        } else if (given != nullptr && given->value.kind == ASSET_VALUE_BOOL) {
          uniform.val = given->value.integer != 0;
        } else {
          state.fail("{} sets '{}' to something that is not a number or true or false", owner, part.name);
          continue;
        }
        /// keyed as the renderer looks a uniform up
        const natural_t key = FNV(std::format("{}.{}", pass.name, part.name));
        if (!pass.uniforms.insert({ key, std::move(uniform) }).second) {
          state.fail("{} sets '{}' twice", owner, part.name);
        }
      }
    }

    /// the units a file of declarations and what it uses come to, assembled; none when the
    ///  file has errors, which the toolchain has logged with their file, line and column
    ostd::vector<ocmd_unit> assemble_declarations(const filepath& path) {
      diagnostic_engine diagnostics;
      ocmd_toolchain toolchain;
      const ostd::vector<command_unit> units = toolchain.read_command_units(path, path.generic_string(), &diagnostics);
      if (units.empty()) {
        return {};
      }
      /// a file of declarations calls nothing, so there is no device it needs to find
      command_bus bus;
      const ostd::vector<oasm_source> sources = toolchain.lower_command_units(bus, units, &diagnostics);
      if (sources.empty()) {
        return {};
      }
      return toolchain.compile_units(sources, &diagnostics);
    }

    pipeline_definition read_for_renderer(const filepath& path) {
      opt<pipeline_definition> read = read_pipeline_declarations(path);
      if (read.has_value()) {
        return std::move(*read);
      }
      /// what the asset handler and the project compiler each take for "this is no pipeline"
      pipeline_definition none;
      none.name.clear();
      return none;
    }

  }  // namespace

  opt<pipeline_definition> pipeline_from_footprint(const footprint_node& footprint, const filepath& source) {
    PROFILE_SECTION("pipeline_from_footprint");
    if (footprint.kind != asset_hash("pipeline") || footprint.name.empty()) {
      CORE_LOG_ERROR("[RENDERING] : the asset '{}' of '{}' is not a pipeline with a name", footprint.name, source.string());
      return std::nullopt;
    }

    decoding state;
    pipeline_definition def;
    def.name = footprint.name;
    def.version = static_cast<uint32_t>(integer_of(footprint, "version", 1));
    def.source_path = std::filesystem::absolute(source);
    def.display_texture_name = word_of(footprint, "display");

    decode_resources(state, footprint, def);
    decode_material_layout(state, footprint, def);
    for (const footprint_node& part : parts_of(footprint, "pass")) {
      decode_pass(state, footprint, part, def);
    }
    /// 'a |> b': b comes after a
    for (const asset_edge& edge : footprint.edges) {
      const auto after = std::ranges::find(def.passes, edge.to, &pipeline_pass_definition::name);
      if (after == def.passes.end() || !std::ranges::contains(def.passes, edge.from, &pipeline_pass_definition::name)) {
        state.fail("'{} |> {}' joins what is not a pass of the pipeline", edge.from, edge.to);
        continue;
      }
      after->depends_on.push_back(edge.from);
    }
    if (!def.display_texture_name.empty() && !std::ranges::contains(def.textures, def.display_texture_name, &pipeline_texture_definition::name)) {
      state.fail("it displays '{}', which is not a texture of the pipeline", def.display_texture_name);
    }
    if (state.errors.empty()) {
      const detail::validation_result validated = validate_pipeline_definition(def);
      state.errors.append_range(validated.errors);
    }

    if (!state.errors.empty()) {
      CORE_LOG_ERROR("[RENDERING] : pipeline '{}' of '{}' cannot be given to the renderer:", footprint.name, source.string());
      for (const std::string& error : state.errors) {
        CORE_LOG_ERROR("[RENDERING] :  - {}", error);
      }
      return std::nullopt;
    }
    return def;
  }

  opt<pipeline_definition> read_pipeline_declarations(const filepath& path) {
    PROFILE_SECTION("read_pipeline_declarations");
    const ostd::vector<ocmd_unit> units = assemble_declarations(path);
    if (units.empty()) {
      CORE_LOG_ERROR("[RENDERING] : '{}' could not be read as a file of declarations; the errors are above", path.string());
      return std::nullopt;
    }
    /// units[0] is the file itself; what it uses may declare pipelines of its own
    const auto declared = std::ranges::find(units.front().program.assets, std::string{ "pipeline" }, &asset_node::kind);
    if (declared == units.front().program.assets.end()) {
      CORE_LOG_ERROR("[RENDERING] : '{}' declares no pipeline", path.string());
      return std::nullopt;
    }

    /// through the bytes a linked program carries, so a pipeline loaded from its source and
    ///  one loaded from a program are made by the same reading of the same thing
    if (const opt<std::string> refusal = asset_footprint_refusal(*declared); refusal.has_value()) {
      CORE_LOG_ERROR("[RENDERING] : '{}': {}", path.string(), *refusal);
      return std::nullopt;
    }
    const ostd::vector<uint8_t> bytes = asset_footprint(*declared);
    const opt<footprint_node> footprint = read_asset_footprint(bytes);
    OTHER_ASSERT(footprint.has_value(), "A footprint that was just written could not be read: {}", path.string());
    opt<pipeline_definition> pipeline = pipeline_from_footprint(*footprint, path);
    if (pipeline.has_value()) {
      CORE_LOG_INFO("[RENDERING] : pipeline '{}' read from its declaration in '{}': a footprint of {} bytes, hash {:016x}", pipeline->name, path.string(), bytes.size(),
                    asset_hash(std::span<const uint8_t>{ bytes }));
    }
    return pipeline;
  }

  opt<ostd::vector<asset_node>> read_declared_assets(const filepath& path) {
    PROFILE_SECTION("read_declared_assets");
    const ostd::vector<ocmd_unit> units = assemble_declarations(path);
    if (units.empty()) {
      return std::nullopt;
    }
    /// units[0] is the file itself
    return units.front().program.assets;
  }

  opt<asset::declared> describe_declarations(const filepath& path) {
    PROFILE_SECTION("describe_declarations");
    const ostd::vector<ocmd_unit> units = assemble_declarations(path);
    if (units.empty() || units.front().program.assets.empty()) {
      return std::nullopt;
    }
    const asset_node& first = units.front().program.assets.front();
    return asset::declared{ .kind = first.kind, .name = first.name };
  }

  void register_declaration_readers() {
    set_pipeline_declarations_reader(&read_for_renderer);
    set_material_declarations_reader(&read_material_declarations);
    asset::set_declaration_describer(&describe_declarations);
  }

}  // namespace other
