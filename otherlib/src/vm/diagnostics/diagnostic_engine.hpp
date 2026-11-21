/**
 * \file vm/diagnostics/diagnostic_engine.hpp
 **/
#ifndef OTHERLIB_VM_DIAGNOSTICS_DIAGNOSTIC_ENGINE_HPP
#define OTHERLIB_VM_DIAGNOSTICS_DIAGNOSTIC_ENGINE_HPP

#include "vm/diagnostics/source_map.hpp"
#include "vm/diagnostics/vm_diagnostic.hpp"

namespace other {

  class diagnostic_sink;

  class diagnostic_engine {
   public:
    diagnostic_engine() = default;
    ~diagnostic_engine() = default;

    natural_t register_sink(const std::string_view name, diagnostic_sink* sink);
    void remove_sink(const natural_t sink_id);

    void emit(const diagnostic& diag);
    void emit(const natural_t sink_id, const diagnostic& diag);

   private:
    source_map sources;
    /// std: vm::diagnostics is a static-storage instance (no arena at static init/deinit)
    std::map<natural_t, diagnostic_sink*> sinks;
  };

  /// keeps a sink registered for one scope, so an early return cannot leave the engine
  ///  pointing at a sink that has gone
  class scoped_diagnostic_sink {
   public:
    scoped_diagnostic_sink(diagnostic_engine* engine, const std::string_view name, diagnostic_sink* sink)
        : engine(engine), id(engine->register_sink(name, sink)) {}
    ~scoped_diagnostic_sink() {
      /// 0 is what register_sink answers when the name was already taken
      if (id != 0) {
        engine->remove_sink(id);
      }
    }

    scoped_diagnostic_sink(const scoped_diagnostic_sink&) = delete;
    scoped_diagnostic_sink& operator=(const scoped_diagnostic_sink&) = delete;

   private:
    diagnostic_engine* engine;
    natural_t id;
  };

}  // namespace other

#endif  // OTHERLIB_VM_DIAGNOSTICS_DIAGNOSTIC_ENGINE_HPP