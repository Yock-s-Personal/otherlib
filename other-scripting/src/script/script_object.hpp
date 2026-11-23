/**
 * \file script/script_object.hpp
 **/
#ifndef OTHER_SCRIPTING_SCRIPT_SCRIPT_OBJECT_HPP
#define OTHER_SCRIPTING_SCRIPT_SCRIPT_OBJECT_HPP

#include <refl/refl.hpp>

#include "core/defines.hpp"

namespace other {

  class dotnet_object;

  struct script_object {
    script_object() = default;
    ~script_object() = default;

    std::string name;
    integer_t id = -1;

    dotnet_object* dotnet_object = nullptr;

    /// set when this object was registered with the C# NativeObjectManager (scene objects
    ///  are, behaviors are not) so detach only unregisters what was actually attached
    bool dotnet_native_registered = false;

    /// one behaviour on this object. the engine keeps its life here; what the script itself
    ///  does is reached through the behaviour runtime
    struct behavior_handle {
      /// what a scene file calls it
      std::string type_name;
      /// the engine's hold on its managed object. null while it could not be made: the name
      ///  is kept, and a rebuild of the scripts tries again
      other::dotnet_object* managed = nullptr;
      /// the runtime's handle to it; null as managed is
      void* instance = nullptr;
      /// one bit for each callback its type has written: the others are never called
      uint32_t callbacks = 0;

      /// the user's switch
      bool enabled = true;
      /// it has been told it is awake, which happens once
      bool awake = false;
      /// its scene is playing and it is enabled: it has been told so and not told otherwise
      bool running = false;
      /// it has been told to start since it last began running
      bool started = false;

      /// its saved fields while there is no instance to ask: the type is missing, or the
      ///  scripts are being rebuilt. given to the instance when there is one
      ostd::vector<uint8_t> fields;
    };
    ostd::vector<behavior_handle> behavior_handles;
    /// the scene this object is in is playing
    bool playing = false;
  };

}  // namespace other

#endif  // OTHER_SCRIPTING_SCRIPT_SCRIPT_OBJECT_HPP