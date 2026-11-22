/**
 * \file scripting/bindings.cpp
 **/
#include "scripting/bindings.hpp"

#include "core/logger.hpp"

#include "script/scripting_environment.hpp"

#include "object/animation_component.hpp"
#include "object/audio_listener_component.hpp"
#include "object/audio_source_component.hpp"
#include "object/grid_component.hpp"
#include "object/light_component.hpp"
#include "object/physics_component.hpp"
#include "object/script_component.hpp"
#include "object/transform.hpp"

#include "driver/driver.hpp"
#include "scripting/binding_descriptor.hpp"
#include "scripting/dotnet_bindings/component_bindings.hpp"
#include "scripting/dotnet_bindings/core_bindings.hpp"
#include "scripting/dotnet_bindings/draw_bindings.hpp"
#include "scripting/dotnet_bindings/driver_bindings.hpp"
#include "scripting/dotnet_bindings/environment_api_bindings.hpp"
#include "scripting/dotnet_bindings/event_bindings.hpp"
#include "scripting/dotnet_bindings/input_bindings.hpp"
#include "scripting/dotnet_bindings/network_bindings.hpp"
#include "scripting/dotnet_bindings/project_bindings.hpp"
#include "scripting/dotnet_bindings/scene_bindings.hpp"
#include "scripting/dotnet_bindings/scene_object_bindings.hpp"
#include "scripting/dotnet_bindings/steam_bindings.hpp"
#include "scripting/dotnet_bindings/ui_bindings.hpp"
#include "scripting/dotnet_bindings/window_bindings.hpp"
#include "scripting/lua_bindings/bind_math_types_lua.hpp"
#include "scripting/lua_bindings/bind_rendering_types_lua.hpp"
#include "scripting/main_thread_only.hpp"
#include "scripting/other_abi.hpp"
#include "scripting/scene_interface.hpp"
#include "tools/environment_console.hpp"

namespace other {
  namespace detail {

    void bind_native_types_lua(sol::state& lua_state);
    void bind_native_types_dotnet(dotnet_host& dn_host);
    void bind_scene_interface(driver* drv);
    void bind_abi_functions(dotnet_host& dn_host);

  }  // namespace detail

  namespace bindings {

    namespace {

      /// the engine's own functions the managed side would not take, and the first of them
      uint32_t refused_binds = 0;
      std::string first_refused_bind;

    }  // namespace

    void validate_binding_points(dotnet_host& dn_host) {
      PROFILE_SECTION("other::bindings::validate-binding-points");
      CORE_LOG_DEBUG("Validating native function binding points...");

      /// these are the engine's two halves of one declaration. a script would call through a
      ///  pointer that is null or that takes other arguments than it passes
      if (refused_binds > 0) {
        OTHER_CRITICAL_FAILURE("{} native function(s) could not be bound to C#, the first being '{}'. The error above each says how its native and C# declarations differ.", refused_binds, first_refused_bind);
      }

      nbool32 res = dn_host.interop().validate_binding_points();
      if (!res) {
        CORE_LOG_ERROR("One or more native functions failed to bind to managed counterparts.");
      }
    }

    struct binding_context {
      dotnet_host& host;

      binding_context(dotnet_host& h)
          : host(h) {}

      /// what C# gets is not Fn but the call that checks its thread first. name is a literal:
      ///  it is kept for the error
      template <auto Fn>
      binding_context& bind(const std::string_view name) {
        PROFILE_SECTION("other::bindings::binding_context::bind");
        if (bind_to_script<Fn>(host, name) != binding_result::BOUND) {
          if (refused_binds++ == 0) {
            first_refused_bind = name;
          }
        }
        return *this;
      }
    };

  }  // namespace bindings

  void bind_otherlib_dotnet_functions(dotnet_host& dn_host) {
    PROFILE_SECTION("other::bind-otherlib-dotnet-functions");
    bindings::refused_binds = 0;
    bindings::first_refused_bind.clear();
    dn_host.rediscover_binding_points();

    detail::bind_native_types_dotnet(dn_host);
    detail::bind_abi_functions(dn_host);

    bindings::binding_context{ dn_host }
      /// Filesystem.
      .bind<bindings::native_get_program_files_folder>("GetProgramFilesFolder")
      .bind<bindings::native_get_app_data_folder>("GetAppDataFolder")
      .bind<bindings::native_get_install_folder>("GetInstallFolder");

    bindings::binding_context{ dn_host }
      /// UI.
      .bind<bindings::native_begin_window>("BeginWindow")
      .bind<bindings::native_end_window>("EndWindow")
      .bind<bindings::native_begin_child>("BeginChild")
      .bind<bindings::native_end_child>("EndChild");

    bindings::binding_context{ dn_host }
      /// UI - Widgets.
      .bind<bindings::native_ui_text>("UIText")
      .bind<bindings::native_ui_text_colored>("UITextColored")
      .bind<bindings::native_ui_text_wrapped>("UITextWrapped")
      .bind<bindings::native_ui_label_text>("UILabelText")
      .bind<bindings::native_ui_button>("UIButton")
      .bind<bindings::native_ui_small_button>("UISmallButton")
      .bind<bindings::native_ui_invisible_button>("UIInvisibleButton")
      .bind<bindings::native_ui_checkbox>("UICheckbox")
      .bind<bindings::native_ui_radio_button>("UIRadioButton")
      .bind<bindings::native_ui_progress_bar>("UIProgressBar")
      .bind<bindings::native_ui_separator>("UISeparator")
      .bind<bindings::native_ui_same_line>("UISameLine")
      .bind<bindings::native_ui_spacing>("UISpacing")
      .bind<bindings::native_ui_indent>("UIIndent")
      .bind<bindings::native_ui_unindent>("UIUnindent")
      .bind<bindings::native_ui_new_line>("UINewLine")
      .bind<bindings::native_ui_dummy>("UIDummy");

    bindings::binding_context{ dn_host }
      /// UI - Input Widgets.
      .bind<bindings::native_ui_input_text>("UIInputText")
      .bind<bindings::native_ui_input_text_multiline>("UIInputTextMultiline")
      .bind<bindings::native_ui_input_float>("UIInputFloat")
      .bind<bindings::native_ui_input_float2>("UIInputFloat2")
      .bind<bindings::native_ui_input_float3>("UIInputFloat3")
      .bind<bindings::native_ui_input_float4>("UIInputFloat4")
      .bind<bindings::native_ui_input_int>("UIInputInt")
      .bind<bindings::native_ui_drag_float>("UIDragFloat")
      .bind<bindings::native_ui_drag_float3>("UIDragFloat3")
      .bind<bindings::native_ui_slider_float>("UISliderFloat")
      .bind<bindings::native_ui_slider_int>("UISliderInt")
      .bind<bindings::native_ui_color_edit3>("UIColorEdit3")
      .bind<bindings::native_ui_color_edit4>("UIColorEdit4");

    bindings::binding_context{ dn_host }
      /// UI - Tree / Collapsing.
      .bind<bindings::native_ui_tree_node>("UITreeNode")
      .bind<bindings::native_ui_tree_node_ex>("UITreeNodeEx")
      .bind<bindings::native_ui_tree_pop>("UITreePop")
      .bind<bindings::native_ui_collapsing_header>("UICollapsingHeader");

    bindings::binding_context{ dn_host }
      /// UI - Selectables / Lists.
      .bind<bindings::native_ui_selectable>("UISelectable")
      .bind<bindings::native_ui_begin_combo>("UIBeginCombo")
      .bind<bindings::native_ui_end_combo>("UIEndCombo")
      .bind<bindings::native_ui_begin_listbox>("UIBeginListBox")
      .bind<bindings::native_ui_end_listbox>("UIEndListBox");

    bindings::binding_context{ dn_host }
      /// UI - Tabs.
      .bind<bindings::native_ui_begin_tab_bar>("UIBeginTabBar")
      .bind<bindings::native_ui_end_tab_bar>("UIEndTabBar")
      .bind<bindings::native_ui_begin_tab_item>("UIBeginTabItem")
      .bind<bindings::native_ui_end_tab_item>("UIEndTabItem");

    bindings::binding_context{ dn_host }
      /// UI - Menus.
      .bind<bindings::native_ui_begin_menu_bar>("UIBeginMenuBar")
      .bind<bindings::native_ui_end_menu_bar>("UIEndMenuBar")
      .bind<bindings::native_ui_begin_main_menu_bar>("UIBeginMainMenuBar")
      .bind<bindings::native_ui_end_main_menu_bar>("UIEndMainMenuBar")
      .bind<bindings::native_ui_begin_menu>("UIBeginMenu")
      .bind<bindings::native_ui_end_menu>("UIEndMenu")
      .bind<bindings::native_ui_menu_item>("UIMenuItem");

    bindings::binding_context{ dn_host }
      /// UI - Popups / Modals.
      .bind<bindings::native_ui_open_popup>("UIOpenPopup")
      .bind<bindings::native_ui_begin_popup>("UIBeginPopup")
      .bind<bindings::native_ui_begin_popup_modal>("UIBeginPopupModal")
      .bind<bindings::native_ui_end_popup>("UIEndPopup")
      .bind<bindings::native_ui_close_current_popup>("UICloseCurrentPopup");

    bindings::binding_context{ dn_host }
      /// UI - Tables.
      .bind<bindings::native_ui_begin_table>("UIBeginTable")
      .bind<bindings::native_ui_end_table>("UIEndTable")
      .bind<bindings::native_ui_table_next_row>("UITableNextRow")
      .bind<bindings::native_ui_table_next_column>("UITableNextColumn")
      .bind<bindings::native_ui_table_set_column_index>("UITableSetColumnIndex")
      .bind<bindings::native_ui_table_setup_column>("UITableSetupColumn")
      .bind<bindings::native_ui_table_headers_row>("UITableHeadersRow");

    bindings::binding_context{ dn_host }
      /// UI - Layout Queries.
      .bind<bindings::native_ui_get_content_region_avail>("UIGetContentRegionAvail")
      .bind<bindings::native_ui_get_window_size>("UIGetWindowSize")
      .bind<bindings::native_ui_get_window_pos>("UIGetWindowPos")
      .bind<bindings::native_ui_set_next_window_size>("UISetNextWindowSize")
      .bind<bindings::native_ui_set_next_window_pos>("UISetNextWindowPos")
      .bind<bindings::native_ui_is_item_hovered>("UIIsItemHovered")
      .bind<bindings::native_ui_is_item_clicked>("UIIsItemClicked")
      .bind<bindings::native_ui_is_item_active>("UIIsItemActive")
      .bind<bindings::native_ui_is_window_focused>("UIIsWindowFocused")
      .bind<bindings::native_ui_is_window_hovered>("UIIsWindowHovered");

    bindings::binding_context{ dn_host }
      /// UI - Style.
      .bind<bindings::native_ui_push_style_color>("UIPushStyleColor")
      .bind<bindings::native_ui_pop_style_color>("UIPopStyleColor")
      .bind<bindings::native_ui_push_style_var_float>("UIPushStyleVarFloat")
      .bind<bindings::native_ui_push_style_var_vec2>("UIPushStyleVarVec2")
      .bind<bindings::native_ui_pop_style_var>("UIPopStyleVar")
      .bind<bindings::native_ui_push_item_width>("UIPushItemWidth")
      .bind<bindings::native_ui_pop_item_width>("UIPopItemWidth");

    bindings::binding_context{ dn_host }
      /// UI - ID Stack.
      .bind<bindings::native_ui_push_id_str>("UIPushIDStr")
      .bind<bindings::native_ui_push_id_int>("UIPushIDInt")
      .bind<bindings::native_ui_pop_id>("UIPopID");

    bindings::binding_context{ dn_host }
      /// UI - Tooltips.
      .bind<bindings::native_ui_set_tooltip>("UISetTooltip")
      .bind<bindings::native_ui_begin_tooltip>("UIBeginTooltip")
      .bind<bindings::native_ui_end_tooltip>("UIEndTooltip");

    bindings::binding_context{ dn_host }
      /// UI - Drag and Drop.
      .bind<bindings::native_ui_begin_drag_drop_source>("UIBeginDragDropSource")
      .bind<bindings::native_ui_set_drag_drop_payload>("UISetDragDropPayload")
      .bind<bindings::native_ui_end_drag_drop_source>("UIEndDragDropSource")
      .bind<bindings::native_ui_begin_drag_drop_target>("UIBeginDragDropTarget")
      .bind<bindings::native_ui_end_drag_drop_target>("UIEndDragDropTarget");

    bindings::binding_context{ dn_host }
      /// UI - Text, state and the clipboard.
      .bind<bindings::native_ui_separator_text>("UISeparatorText")
      .bind<bindings::native_ui_text_disabled>("UITextDisabled")
      .bind<bindings::native_ui_bullet>("UIBullet")
      .bind<bindings::native_ui_begin_disabled>("UIBeginDisabled")
      .bind<bindings::native_ui_end_disabled>("UIEndDisabled")
      .bind<bindings::native_ui_set_clipboard_text>("UISetClipboardText")
      .bind<bindings::native_ui_get_clipboard_text>("UIGetClipboardText")
      /// UI - the inspector's rows.
      .bind<bindings::native_ui_begin_property_row>("UIBeginPropertyRow")
      .bind<bindings::native_ui_end_property_row>("UIEndPropertyRow")
      .bind<bindings::native_ui_property_display>("UIPropertyDisplay");

    bindings::binding_context{ dn_host }
      /// UI - Windows.
      .bind<bindings::native_ui_register_window>("UIRegisterWindow")
      .bind<bindings::native_ui_unregister_window>("UIUnregisterWindow")
      .bind<bindings::native_ui_set_window_draw_function>("UISetWindowDrawFunction")
      .bind<bindings::native_ui_open_window>("UIOpenWindow")
      .bind<bindings::native_ui_close_window>("UICloseWindow")
      .bind<bindings::native_ui_is_window_open>("UIIsWindowOpen")
      .bind<bindings::native_ui_window_draw_count>("UIWindowDrawCount")
      .bind<bindings::native_ui_window_draw_microseconds>("UIWindowDrawMicroseconds")
      .bind<bindings::native_ui_add_menu_item>("UIAddMenuItem")
      .bind<bindings::native_ui_remove_menu_item>("UIRemoveMenuItem")
      .bind<bindings::native_ui_set_menu_item_click_function>("UISetMenuItemClickFunction");

    bindings::binding_context{ dn_host }
      /// Scene
      .bind<bindings::native_scene_create_object>("SceneCreateObject")
      .bind<bindings::native_scene_destroy_object>("SceneDestroyObject")
      .bind<bindings::native_scene_has_object>("SceneHasObject")
      .bind<bindings::native_scene_get_object_name>("SceneGetObjectName")
      .bind<bindings::native_scene_set_object_name>("SceneSetObjectName")
      .bind<bindings::native_scene_get_object_ids>("SceneGetObjectIds")
      .bind<bindings::native_scene_get_object_count>("SceneGetObjectCount")
      .bind<bindings::native_scene_find_object_by_name>("SceneFindObjectByName")
      /// Object Hierarchy.
      .bind<bindings::native_scene_get_parent_id>("SceneGetParentId")
      .bind<bindings::native_scene_get_children_ids>("SceneGetChildrenIds")
      /// Object Tags.
      .bind<bindings::native_scene_object_has_tag>("SceneObjectHasTag")
      .bind<bindings::native_scene_add_object_tag>("SceneAddObjectTag")
      .bind<bindings::native_scene_remove_object_tag>("SceneRemoveObjectTag")
      /// Object Visibility.
      .bind<bindings::native_scene_get_object_visible>("SceneGetObjectVisible")
      .bind<bindings::native_scene_set_object_visible>("SceneSetObjectVisible");

    bindings::binding_context{ dn_host }
      /// Events.
      .bind<bindings::native_event_register>("EventRegister")
      .bind<bindings::native_event_trigger>("EventTrigger")
      .bind<bindings::native_event_trigger_with_string>("EventTriggerWithString")
      .bind<bindings::native_event_trigger_with_value>("EventTriggerWithValue")
      .bind<bindings::native_event_subscribe>("EventSubscribe")
      .bind<bindings::native_event_unsubscribe>("EventUnsubscribe");

    bindings::binding_context{ dn_host }
      /// Input.
      .bind<bindings::native_input_is_key_down>("InputIsKeyDown")
      .bind<bindings::native_input_is_key_pressed>("InputIsKeyPressed")
      .bind<bindings::native_input_is_mouse_button_down>("InputIsMouseButtonDown")
      .bind<bindings::native_input_is_mouse_button_clicked>("InputIsMouseButtonClicked")
      .bind<bindings::native_input_get_mouse_position>("InputGetMousePosition")
      .bind<bindings::native_input_get_mouse_delta>("InputGetMouseDelta")
      .bind<bindings::native_input_get_mouse_wheel>("InputGetMouseWheel");

    bindings::binding_context{ dn_host }
      /// Dynamic drawing (in-scene overlay or debug view).
      .bind<bindings::native_draw_line>("DrawLine")
      .bind<bindings::native_draw_triangle>("DrawTriangle")
      .bind<bindings::native_draw_point>("DrawPoint")
      .bind<bindings::native_draw_ray>("DrawRay")
      .bind<bindings::native_draw_arrow>("DrawArrow")
      .bind<bindings::native_draw_aabb>("DrawAABB")
      .bind<bindings::native_draw_obb>("DrawOBB")
      .bind<bindings::native_draw_sphere>("DrawSphere")
      .bind<bindings::native_draw_frustum>("DrawFrustum")
      .bind<bindings::native_draw_transform>("DrawTransform")
      .bind<bindings::native_draw_mesh>("DrawMesh")
      .bind<bindings::native_draw_grid>("DrawGrid");

    bindings::binding_context{ dn_host }
      /// Time.
      .bind<bindings::native_time_get_delta_time>("TimeGetDeltaTime")
      .bind<bindings::native_time_get_elapsed_time>("TimeGetElapsedTime")
      .bind<bindings::native_time_get_frame_count>("TimeGetFrameCount");

    bindings::binding_context{ dn_host }
      /// Driver / Application State.
      .bind<bindings::native_driver_get_state>("DriverGetState")
      .bind<bindings::native_driver_request_shutdown>("DriverRequestShutdown")
      .bind<bindings::native_driver_get_project_name>("DriverGetProjectName");

    bindings::binding_context{ dn_host }
      /// Project.
      .bind<bindings::native_project_change_stamp>("ProjectChangeStamp")
      .bind<bindings::native_project_get_state>("ProjectGetState")
      .bind<bindings::native_project_get_name>("ProjectGetName")
      .bind<bindings::native_project_get_version>("ProjectGetVersion")
      .bind<bindings::native_project_get_author>("ProjectGetAuthor")
      .bind<bindings::native_project_get_description>("ProjectGetDescription")
      .bind<bindings::native_project_get_file>("ProjectGetFile")
      .bind<bindings::native_project_get_scene_count>("ProjectGetSceneCount")
      .bind<bindings::native_project_get_starting_scene>("ProjectGetStartingScene")
      .bind<bindings::native_project_has_file_notice>("ProjectHasFileNotice")
      .bind<bindings::native_project_get_file_notice_error>("ProjectGetFileNoticeError")
      .bind<bindings::native_project_get_file_notice_part_count>("ProjectGetFileNoticePartCount")
      .bind<bindings::native_project_get_file_notice_part>("ProjectGetFileNoticePart")
      .bind<bindings::native_project_get_load_error_count>("ProjectGetLoadErrorCount")
      .bind<bindings::native_project_get_load_error_subject>("ProjectGetLoadErrorSubject")
      .bind<bindings::native_project_get_load_error_message>("ProjectGetLoadErrorMessage")
      .bind<bindings::native_project_get_script_project_path>("ProjectGetScriptProjectPath")
      .bind<bindings::native_project_scripts_building>("ProjectScriptsBuilding")
      .bind<bindings::native_project_scripts_loaded>("ProjectScriptsLoaded")
      .bind<bindings::native_project_has_script_failure>("ProjectHasScriptFailure")
      .bind<bindings::native_project_get_script_failure_project_path>("ProjectGetScriptFailureProjectPath")
      .bind<bindings::native_project_get_script_failure_reason>("ProjectGetScriptFailureReason")
      .bind<bindings::native_project_get_script_failure_details>("ProjectGetScriptFailureDetails")
      .bind<bindings::native_project_script_failure_can_retry>("ProjectScriptFailureCanRetry")
      .bind<bindings::native_project_request_script_rebuild>("ProjectRequestScriptRebuild");

    bindings::binding_context{ dn_host }
      /// Configuration.
      .bind<bindings::native_config_get_string>("ConfigGetString")
      .bind<bindings::native_config_get_int>("ConfigGetInt")
      .bind<bindings::native_config_get_float>("ConfigGetFloat")
      .bind<bindings::native_config_get_bool>("ConfigGetBool");

    // bindings::binding_context{ dn_host }
    /// Networking.
    // .bind<bindings::native_network_is_connected>("NetworkIsConnected")
    // .bind<bindings::native_network_get_role>("NetworkGetRole")
    // .bind<bindings::native_network_local_peer_id>("NetworkLocalPeerId")
    // .bind<bindings::native_network_host>("NetworkHost")
    // .bind<bindings::native_network_join>("NetworkJoin")
    // .bind<bindings::native_network_leave>("NetworkLeave")
    // .bind<bindings::native_network_send_event>("NetworkSendEvent")
    // .bind<bindings::native_network_broadcast_event>("NetworkBroadcastEvent")
    // .bind<bindings::native_network_copy_event_payload>("NetworkCopyEventPayload")
    // .bind<bindings::native_network_spawn>("NetworkSpawn")
    // .bind<bindings::native_network_sync_component>("NetworkSyncComponent")
    // .bind<bindings::native_network_is_mine>("NetworkIsMine")
    // .bind<bindings::native_network_request_op>("NetworkRequestOp")
    // .bind<bindings::native_network_stage_replicated>("NetworkStageReplicated")
    // .bind<bindings::native_network_peer_count>("NetworkPeerCount")
    // .bind<bindings::native_network_peer_id>("NetworkPeerId")
    // .bind<bindings::native_network_peer_node>("NetworkPeerNode")
    // .bind<bindings::native_network_peer_name>("NetworkPeerName")
    // .bind<bindings::native_network_actor_stage_payload>("NetworkActorStagePayload")
    // .bind<bindings::native_network_actor_send>("NetworkActorSend")
    // .bind<bindings::native_network_actor_open_link>("NetworkActorOpenLink")
    // .bind<bindings::native_network_actor_open_listener>("NetworkActorOpenListener")
    // .bind<bindings::native_network_actor_close_link>("NetworkActorCloseLink")
    // .bind<bindings::native_network_host_steam>("NetworkHostSteam")
    // .bind<bindings::native_network_join_lobby>("NetworkJoinLobby")
    // .bind<bindings::native_steam_is_available>("SteamIsAvailable")
    // .bind<bindings::native_steam_player_id>("SteamPlayerId")
    // .bind<bindings::native_steam_player_name>("SteamPlayerName")
    // .bind<bindings::native_steam_open_invite_dialog>("SteamOpenInviteDialog");

    bindings::binding_context{ dn_host }
      /// OtherObject
      .bind<bindings::native_validate_object_handle>("ValidateObjectHandle")
      .bind<bindings::native_get_object_id>("GetObjectID");

    bindings::binding_context{ dn_host }
      /// Scene Object
      .bind<bindings::native_transform_get_world_matrix>("GetWorldMatrix");

    bindings::binding_context{ dn_host }
      /// Component Helpers
      .bind<bindings::native_render_component_get_num_vertices>("GetNumVertices")
      .bind<bindings::native_render_component_get_num_indices>("GetNumIndices")
      .bind<bindings::native_render_component_get_mesh_name>("GetMeshName")
      .bind<bindings::native_render_component_fetch_mesh>("FetchMesh")
      .bind<bindings::native_render_component_upload_mesh>("UploadMesh")
      .bind<bindings::native_render_component_get_material_path>("GetMaterialPath")
      .bind<bindings::native_render_component_set_material_path>("SetMaterialPath")
      .bind<bindings::native_animation_component_get_clip_path>("GetAnimationClipPath")
      .bind<bindings::native_animation_component_set_clip_path>("SetAnimationClipPath")
      .bind<bindings::native_audio_source_get_clip_path>("GetAudioClipPath")
      .bind<bindings::native_audio_source_set_clip_path>("SetAudioClipPath")
      .bind<bindings::native_physics_component_get_body_type>("GetPhysicsBodyType")
      .bind<bindings::native_physics_component_set_body_type>("SetPhysicsBodyType")
      .bind<bindings::native_physics_component_get_mass>("GetPhysicsMass")
      .bind<bindings::native_physics_component_set_mass>("SetPhysicsMass")
      .bind<bindings::native_physics_component_get_is_trigger>("GetPhysicsIsTrigger")
      .bind<bindings::native_physics_component_set_is_trigger>("SetPhysicsIsTrigger")
      .bind<bindings::native_physics_component_get_linear_velocity>("GetPhysicsLinearVelocity")
      .bind<bindings::native_physics_component_set_linear_velocity>("SetPhysicsLinearVelocity")
      .bind<bindings::native_physics_component_get_angular_velocity>("GetPhysicsAngularVelocity")
      .bind<bindings::native_physics_component_set_angular_velocity>("SetPhysicsAngularVelocity")
      .bind<bindings::native_physics_component_add_force>("PhysicsAddForce")
      .bind<bindings::native_physics_component_add_impulse>("PhysicsAddImpulse")
      .bind<bindings::native_physics_component_add_torque>("PhysicsAddTorque")
      .bind<bindings::native_physics_raycast>("PhysicsRaycast")
      .bind<bindings::native_camera_component_get_position>("CameraGetPosition")
      .bind<bindings::native_camera_component_look_from>("CameraLookFrom")
      .bind<bindings::native_camera_component_get_direction>("CameraGetDirection")
      .bind<bindings::native_camera_component_look_at>("CameraLookAt")
      .bind<bindings::native_camera_component_look>("CameraLook")
      .bind<bindings::native_camera_component_get_fov>("CameraGetFov")
      .bind<bindings::native_camera_component_set_fov>("CameraSetFov")
      .bind<bindings::native_camera_component_get_clip_planes>("CameraGetClipPlanes")
      .bind<bindings::native_camera_component_set_clip_planes>("CameraSetClipPlanes")
      .bind<bindings::native_point_light_get_position>("PointLightGetPosition")
      .bind<bindings::native_point_light_set_position>("PointLightSetPosition")
      .bind<bindings::native_point_light_get_color>("PointLightGetColor")
      .bind<bindings::native_point_light_set_color>("PointLightSetColor")
      .bind<bindings::native_direction_light_get_direction>("DirectionLightGetDirection")
      .bind<bindings::native_direction_light_set_direction>("DirectionLightSetDirection")
      .bind<bindings::native_direction_light_get_color>("DirectionLightGetColor")
      .bind<bindings::native_direction_light_set_color>("DirectionLightSetColor");

    bindings::binding_context{ dn_host }
      /// Audio
      .bind<bindings::native_audio_play_one_shot>("AudioPlayOneShot")
      .bind<bindings::native_audio_set_bus_volume>("AudioSetBusVolume")
      .bind<bindings::native_audio_get_bus_volume>("AudioGetBusVolume");

    bindings::validate_binding_points(dn_host);
  }

  void bind_otherlib_lua_functions(lua_host& lua_host) {
    PROFILE_SECTION("bind_otherlib_lua_functions");
    filepath lua_defs_path = lua_host.retrieve_script_path("global_definitions.lua");
    filepath lua_bridge_path = lua_host.retrieve_script_path("other_bridge.lua");
    if (!std::filesystem::exists(lua_defs_path)) {
      CORE_LOG_ERROR("Lua global definitions script '{}' does not exist.", lua_defs_path.string());
      return;
    }

    if (!std::filesystem::exists(lua_bridge_path)) {
      CORE_LOG_ERROR("Lua bridge script '{}' does not exist.", lua_bridge_path.string());
      return;
    }

    sol::state& lua_state = lua_host.get_lua_state();
    detail::bind_native_types_lua(lua_state);

    lua_state.create_named_table(
      "__other_native",
      "__log", lua_state.create_table_with(),
      "__driver", lua_state.create_table_with(),
      "__environment_console", lua_state.create_table_with(),
      "__native_scene", lua_state.create_table_with(),
      "__scene_interface", lua_state.create_table_with(),
      "__component_names", lua_state.create_table_with(),
      "__dotnet_types", lua_state.create_table_with());

    lua_state.create_named_table(
      "__lua_bridge_metadata",
      "__paths", lua_state.create_table_with());

    sol::table paths_table = lua_state["__lua_bridge_metadata"]["__paths"];
    paths_table["script_directory"] = lua_host.get_environment_script_directory().string();
    paths_table["global_definitions"] = lua_defs_path.string();
    paths_table["other_bridge"] = lua_bridge_path.string();

    {
      PROFILE_SECTION("bind_otherlib_lua_functions--run-global-definitions");
      CORE_LOG_DEBUG("Loading lua global definitions script '{}'.", lua_defs_path.string());
      lua_state.script_file(lua_defs_path.string());
    }

    sol::table log_table = lua_state["__other_native"]["__log"];
    log_table.set_function("send_log_message", [](spdlog::level::level_enum level, const std::string& message, const std::string& source, int line) {
      other::logger::send_log_guarded(level, 0, std::format(" [Lua] {} @ ({}:{})", message, source, line));
    });

    {
      PROFILE_SECTION("bind_otherlib_lua_functions--run-bridge-script");
      CORE_LOG_DEBUG("Loading lua bridge script '{}'.", lua_bridge_path.string());
      lua_state.script_file(lua_bridge_path.string());
    }
  }

  void do_script_interface_bindings(driver* drv) {
    OTHER_ASSERT(drv != nullptr, "Driver pointer is null in do_script_interface_bindings.");

    detail::set_dotnet_native_driver(drv);
    abi::oe_init_abi(drv);
  }

  void do_script_interface_unbinding() {
    /// the event system outlives the scripts that listen on it
    bindings::release_script_subscriptions();
    abi::oe_cleanup_abi();
  }

  namespace detail {
    template <typename T>
      requires reflected_type<T>
    std::string get_native_type_name() {
      std::string full_name = std::string{ refl::reflect<T>().name };
      size_t last_scope = full_name.rfind("::");
      if (last_scope != std::string::npos) {
        return full_name.substr(last_scope + 2);
      }
      return full_name;
    }

    /// \todo this @p lua_name feels fragile although changing it in the lua code on accident would be immediately obvious so maybe it's fine?
    template <typename T>
      requires reflected_type<T>
    void bind_lua_component(sol::state& lua_state, const std::string_view lua_name) {
      CORE_LOG_DEBUG(" - binding to Lua: '{}'", get_native_type_name<T>());
      sol::usertype<T> lua_usertype = lua_state.new_usertype<T>(lua_name);
      refl::util::for_each(refl::reflect<T>().members, [&](auto member) {
        if constexpr (!refl::descriptor::is_function(member) && refl::descriptor::has_attribute<attr::serializable>(member)) {
          using field_t = std::remove_cvref_t<decltype(member(std::declval<T&>()))>;
          field_t T::* field_ptr = member.pointer;
          std::string name = std::string{ member.name };
          lua_usertype.set(name, field_ptr);
          CORE_LOG_TRACE("   - Bound field '{}' of type [{}]", name, typeid(field_t).name());
        }
      });

      /// ensures bound types are registered in the type database
      reflection_data& _ = type_data_handler<T>::get_reflection_data(T{});
    }

    template <typename T>
      requires reflected_type<T>
    void bind_dotnet_component(dotnet_host& dn_host, dotnet_object* managed_binder) {
      // CORE_LOG_DEBUG(" - binding to .NET: '{}'", get_native_type_name<T>());

      auto desc = make_component_descriptor<T>(get_native_type_name<T>());
      set_ecs_lifecycle<T>(desc);
      abi::oe_get_registry().register_component(std::move(desc));
    }

    void bind_native_types_lua(sol::state& lua_state) {
      PROFILE_SECTION("bind_native_types_lua");
      lua_state.new_enum(
        "log_level",
        "TRACE", spdlog::level::trace,
        "DEBUG", spdlog::level::debug,
        "INFO", spdlog::level::info,
        "WARN", spdlog::level::warn,
        "ERROR", spdlog::level::err,
        "CRITICAL", spdlog::level::critical);
      lua_state.new_enum(
        "console_message",
        "CONSOLE_NONE", CONSOLE_MESSAGE_NONE,
        "CONSOLE_MESSAGE", CONSOLE_MESSAGE_MESSAGE,
        "CONSOLE_TRACE", CONSOLE_MESSAGE_TRACE,
        "CONSOLE_DEBUG", CONSOLE_MESSAGE_DEBUG,
        "CONSOLE_INFO", CONSOLE_MESSAGE_INFO,
        "CONSOLE_WARN", CONSOLE_MESSAGE_WARN,
        "CONSOLE_ERROR", CONSOLE_MESSAGE_ERROR,
        "CONSOLE_COMMAND", CONSOLE_MESSAGE_COMMAND);
      lua_state.new_enum(
        "driver_state",
        "STOPPED", driver_state::DRIVER_STATE_STOPPED,
        "INITIALIZING", driver_state::DRIVER_STATE_INITIALIZING,
        "RUNNING", driver_state::DRIVER_STATE_RUNNING,
        "PAUSED", driver_state::DRIVER_STATE_PAUSED,
        "SHUTTING_DOWN", driver_state::DRIVER_STATE_SHUTTING_DOWN,
        "NUM_DRIVER_STATES", driver_state::NUM_STATES);
      lua_state.new_enum(
        "driver_event",
        "START", driver_event::DRIVER_EVENT_START,
        "READY", driver_event::DRIVER_EVENT_READY,
        "PAUSE", driver_event::DRIVER_EVENT_PAUSE,
        "RESUME", driver_event::DRIVER_EVENT_RESUME,
        "STOP", driver_event::DRIVER_EVENT_STOP,
        "NUM_DRIVER_EVENTS", driver_event::NUM_EVENTS);

      bind_linear_algebra_types(lua_state);
      bind_rendering_types(lua_state);

      /// bind scene components (and other native types)
      CORE_LOG_DEBUG("Binding native scene components to Lua");
      bind_lua_component<scene_object>(lua_state, "__native_scene_object");
      bind_lua_component<transform>(lua_state, "__native_transform_component");
      bind_lua_component<script_component>(lua_state, "__native_script_component");
      bind_lua_component<render_component>(lua_state, "__native_render_component");
      bind_lua_component<camera_component>(lua_state, "__native_camera_component");
      bind_lua_component<grid_component>(lua_state, "__native_grid_component");
      bind_lua_component<point_light_component>(lua_state, "__native_point_light_component");
      bind_lua_component<direction_light_component>(lua_state, "__native_direction_light_component");
      bind_lua_component<animation_component>(lua_state, "__native_animation_component");
      bind_lua_component<audio_source_component>(lua_state, "__native_audio_source_component");
      bind_lua_component<audio_listener_component>(lua_state, "__native_audio_listener_component");
    }

    void bind_native_types_dotnet(dotnet_host& dn_host) {
      PROFILE_SECTION("bind_native_types_dotnet");
      CORE_LOG_DEBUG("Binding native types to .NET");

      dotnet_object* obj = nullptr;
      // dn_host.instantiate_managed_object("Other.TypeBinder", "Binder");

      bind_dotnet_component<transform>(dn_host, obj);
      bind_dotnet_component<script_component>(dn_host, obj);
      bind_dotnet_component<render_component>(dn_host, obj);
      bind_dotnet_component<camera_component>(dn_host, obj);
      bind_dotnet_component<grid_component>(dn_host, obj);
      bind_dotnet_component<point_light_component>(dn_host, obj);
      bind_dotnet_component<direction_light_component>(dn_host, obj);
      bind_dotnet_component<animation_component>(dn_host, obj);
      bind_dotnet_component<audio_source_component>(dn_host, obj);
      bind_dotnet_component<audio_listener_component>(dn_host, obj);
      bind_dotnet_component<physics_component>(dn_host, obj);

      // dn_host.destroy_managed_object(obj);
    }

    void bind_scene_interface(driver* drv) {
      OTHER_ASSERT(drv != nullptr, "Driver pointer is null in bind_scene_interface.");
    }

    void bind_abi_functions(dotnet_host& dn_host) {
      PROFILE_SECTION("other::bind-abi-functions");

      bindings::binding_context{ dn_host }
        /// Core.
        .bind<bindings::native_oe_fnv_hash>("OeFnvHash");

      bindings::binding_context{ dn_host }
        /// Validation.
        .bind<abi::oe_validate_handle>("OeValidateHandle")
        /// Component management.
        .bind<abi::oe_has_component>("OeHasComponent")
        .bind<abi::oe_add_component>("OeAddComponent")
        .bind<abi::oe_remove_component>("OeRemoveComponent");

      bindings::binding_context{ dn_host }
        /// Field accessors - Primitives.
        .bind<abi::oe_get_field_bool>("OeGetFieldBool")
        .bind<abi::oe_set_field_bool>("OeSetFieldBool")
        .bind<abi::oe_get_field_i32>("OeGetFieldI32")
        .bind<abi::oe_set_field_i32>("OeSetFieldI32")
        .bind<abi::oe_get_field_u32>("OeGetFieldU32")
        .bind<abi::oe_set_field_u32>("OeSetFieldU32")
        .bind<abi::oe_get_field_i64>("OeGetFieldI64")
        .bind<abi::oe_set_field_i64>("OeSetFieldI64")
        .bind<abi::oe_get_field_u64>("OeGetFieldU64")
        .bind<abi::oe_set_field_u64>("OeSetFieldU64")
        .bind<abi::oe_get_field_f32>("OeGetFieldF32")
        .bind<abi::oe_set_field_f32>("OeSetFieldF32")
        .bind<abi::oe_get_field_f64>("OeGetFieldF64")
        .bind<abi::oe_set_field_f64>("OeSetFieldF64");

      bindings::binding_context{ dn_host }
        /// Field accessors - Math types.
        .bind<abi::oe_get_field_vec2>("OeGetFieldVec2")
        .bind<abi::oe_set_field_vec2>("OeSetFieldVec2")
        .bind<abi::oe_get_field_vec3>("OeGetFieldVec3")
        .bind<abi::oe_set_field_vec3>("OeSetFieldVec3")
        .bind<abi::oe_get_field_vec4>("OeGetFieldVec4")
        .bind<abi::oe_set_field_vec4>("OeSetFieldVec4")
        .bind<abi::oe_get_field_quat>("OeGetFieldQuat")
        .bind<abi::oe_set_field_quat>("OeSetFieldQuat")
        .bind<abi::oe_get_field_mat4>("OeGetFieldMat4")
        .bind<abi::oe_set_field_mat4>("OeSetFieldMat4");

      bindings::binding_context{ dn_host }
        /// Field accessors - String.
        .bind<abi::oe_get_field_string>("OeGetFieldString")
        .bind<abi::oe_set_field_string>("OeSetFieldString");
    }

  }  // namespace detail
}  // namespace other