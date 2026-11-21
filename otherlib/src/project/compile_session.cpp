/**
 * \file project/compile_session.cpp
 **/
#include "project/compile_session.hpp"

#include <thread>

#include "core/job_system.hpp"
#include "core/logger.hpp"
#include "core/profiler.hpp"

namespace other {

  compile_session::~compile_session() {
    wait();
  }

  bool compile_session::start(job_system& jobs, const command_bus& bus, compile_settings settings) {
    PROFILE_SECTION("compile_session::start");
    if (in_flight) {
      return false;
    }

    report.reset();
    worker_report = {};
    worker_settings = std::move(settings);
    finished = false;
    in_flight = true;

    running_job = jobs.submit(
      {
        .name = std::format("Compile project '{}'", worker_settings.project_file.string()),
        .priority = job::priority::LOW,
        .thread_affinity = job::affinity::WORKER_THREAD,
      },
      [this, &bus]() {
        /// whatever happens in there, finished has to be set, or the owner waits for ever
        try {
          worker_report = compile_project(bus, worker_settings);
        } catch (const std::exception& e) {
          worker_report = {};
          worker_report.errors.push_back(std::format("the compile stopped on an error it did not expect: {}", e.what()));
        } catch (...) {
          worker_report = {};
          worker_report.errors.push_back("the compile stopped on an error it did not expect");
        }
        finished = true;
      });
    OTHER_ASSERT(running_job != nullptr, "Failed to create the job for a project compile.");
    return true;
  }

  bool compile_session::poll() {
    if (!in_flight || !finished) {
      return false;
    }
    report = std::move(worker_report);
    in_flight = false;
    running_job = nullptr;
    return true;
  }

  void compile_session::wait() {
    PROFILE_SECTION("compile_session::wait");
    while (in_flight && !finished) {
      std::this_thread::yield();
    }
    poll();
  }

}  // namespace other
