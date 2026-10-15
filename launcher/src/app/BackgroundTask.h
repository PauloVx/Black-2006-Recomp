#pragma once

#include <functional>
#include <mutex>
#include <optional>
#include <stop_token>
#include <thread>
#include <utility>

namespace black::app {

template <typename Outcome, typename Progress>
class BackgroundTask {
public:
    using Report = std::function<void(const Progress&)>;
    using Work = std::function<Outcome(std::stop_token, const Report&)>;

    BackgroundTask() = default;
    BackgroundTask(const BackgroundTask&) = delete;
    BackgroundTask& operator=(const BackgroundTask&) = delete;
    BackgroundTask(BackgroundTask&&) = delete;
    BackgroundTask& operator=(BackgroundTask&&) = delete;
    ~BackgroundTask() = default;  // std::jthread requests stop and joins

    void start(Work work, Progress initial = {}) {
        {
            const std::scoped_lock lock(m_mutex);
            m_progress = std::move(initial);
            m_outcome.reset();
            m_running = true;
        }
        m_thread = std::jthread([this, work = std::move(work)](std::stop_token stop) {
            const Report report = [this](const Progress& progress) {
                const std::scoped_lock lock(m_mutex);
                m_progress = progress;
            };
            Outcome outcome = work(stop, report);
            const std::scoped_lock lock(m_mutex);
            m_outcome.emplace(std::move(outcome));
            m_running = false;
        });
    }

    [[nodiscard]] bool running() const {
        const std::scoped_lock lock(m_mutex);
        return m_running;
    }

    [[nodiscard]] Progress progress() const {
        const std::scoped_lock lock(m_mutex);
        return m_progress;
    }

    [[nodiscard]] std::optional<Outcome> takeOutcome() {
        std::optional<Outcome> outcome;
        {
            const std::scoped_lock lock(m_mutex);
            outcome.swap(m_outcome);
        }
        if (outcome && m_thread.joinable()) {
            m_thread.join();
        }
        return outcome;
    }

    void cancel() { m_thread.request_stop(); }
    [[nodiscard]] bool cancelling() const { return m_thread.get_stop_token().stop_requested(); }

private:
    mutable std::mutex m_mutex;
    Progress m_progress{};
    std::optional<Outcome> m_outcome;
    bool m_running = false;
    std::jthread m_thread;  // last: joined before the members it uses go away
};

}  // namespace black::app
