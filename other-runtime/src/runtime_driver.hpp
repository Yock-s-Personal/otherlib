/**
 * \file runtime_driver.hpp
 **/
#ifndef OTHER_RUNTIME_RUNTIME_DRIVER_HPP
#define OTHER_RUNTIME_RUNTIME_DRIVER_HPP

#include "driver/driver.hpp"

namespace other {

  class OTHER_CLASS runtime_driver : public driver {
   public:
    runtime_driver(const command_line& cmd, const config_table& config);
    virtual ~runtime_driver() override {}

    void on_initialize() override;
    void on_rendering_pipeline_loaded(natural_t asset_id, render_pipeline* pipeline) override;
    void on_rendering_pipeline_unloaded(natural_t asset_id, render_pipeline* pipeline) override;
    void on_scene_activated(natural_t scene_id) override;
    void update_running() override;
    void on_begin_frame(render_data* frame_data) override;
    void on_render() override;
    void on_shutdown() override;

   private:
    std::string main_pipeline_name;
    /// set while the main pipeline is loaded; the renderer owns it
    render_pipeline* main_pipeline = nullptr;
    /// configured pipelines that have not reported loaded yet
    size_t pipelines_outstanding = 0;

    /// set from hooks and listeners, acted on in update_running
    opt<std::string> failure;

    bool project_seen = false;
    bool frame_presentable = false;
    bool warned_no_camera = false;

    void fail(std::string reason);
    void check_boot();
  };

}  // namespace other

#endif  // OTHER_RUNTIME_RUNTIME_DRIVER_HPP