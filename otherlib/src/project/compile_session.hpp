/**
 * \file project/compile_session.hpp
 * a project compile run on a worker thread, for a caller that has a frame to keep drawing.
 * a compile builds a C# project and copies a package, which takes seconds
 **/
#ifndef OTHERLIB_PROJECT_COMPILE_SESSION_HPP
#define OTHERLIB_PROJECT_COMPILE_SESSION_HPP

#include <atomic>

#include "core/job.hpp"
#include "core/ref.hpp"

#include "project/project_compiler.hpp"

namespace other {

  class command_bus;
  class job_system;

  /// one compile at a time. the owner polls it once a frame and reads the report when the
  ///  compile has finished
  class compile_session {
   public:
    compile_session() = default;
    /// waits for a compile that is still running: the worker writes into this object
    ~compile_session();

    compile_session(const compile_session&) = delete;
    compile_session& operator=(const compile_session&) = delete;

    /// false when a compile is already running. bus has to stay alive until the compile has
    ///  finished; the worker only reads from it
    bool start(job_system& jobs, const command_bus& bus, compile_settings settings);

    /// true on the one call that finds the running compile finished; its report is then in
    ///  last_report
    bool poll();

    bool running() const { return in_flight; }

    /// asks the running compile to stop. it stops at the next file or stage, ends a script
    ///  build that is running, and its report says it was cancelled
    void cancel() { progress.cancel_requested = true; }
    bool cancelling() const { return in_flight && progress.cancel_requested.load(); }
    /// how far the running compile has got, from 0 to 1, and what it is doing
    float fraction() const { return static_cast<float>(progress.stage.load()) / static_cast<float>(compile_stage::NUM_STAGES); }
    std::string_view stage_label() const { return compile_stage_label(progress.stage.load()); }

    /// empty until a compile has finished, and again while the next one runs
    const opt<compile_report>& last_report() const { return report; }

    /// blocks until the running compile has finished, then takes its report as poll does
    void wait();

   private:
    bool in_flight = false;
    /// set by the worker as the last thing it does
    std::atomic<bool> finished = false;
    /// written by the worker while in_flight, read by the owner only after finished is set
    compile_report worker_report;
    compile_settings worker_settings;
    /// the worker says where it is through this, and is told to stop through it
    compile_progress progress;
    opt<compile_report> report;
    /// kept so the job outlives its work
    ref<job> running_job;
  };

}  // namespace other

#endif  // OTHERLIB_PROJECT_COMPILE_SESSION_HPP
