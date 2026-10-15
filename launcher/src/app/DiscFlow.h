#pragma once

#include "app/BackgroundTask.h"
#include "app/Widgets.h"
#include "core/DiscImport.h"

#include <deque>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

struct GLFWwindow;

namespace black::app {

class DiscFlow {
public:
    enum class Outcome { Stay, Back, Imported };

    DiscFlow(std::filesystem::path gameDirectory, GLFWwindow* window);

    void setFonts(const MenuFont* rows, const MenuFont* titles) noexcept;
    void restart();
    void offer(std::filesystem::path location);

    [[nodiscard]] Page page(double time);
    Outcome activate(std::size_t row);
    Outcome back();
    [[nodiscard]] bool copying() const;

private:
    enum class Step { Choose, Inspecting, Review, Copying, Finished, Stopped };
    enum class Action { ChooseImage, ChooseFolder, Copy, ChooseAnother, TryAgain, Cancel, Continue, Back };

    struct Inspection {
        core::Result<core::DiscSource> source = core::Error{};
        std::uint64_t needed = 0;
        std::uint64_t available = 0;
    };

    void poll(double time);
    void startCopy();
    [[nodiscard]] bool canCopy() const;

    float drawChoose(ImDrawList& list, ImVec2 at, float width) const;
    float drawInspecting(ImDrawList& list, ImVec2 at, float width, double time) const;
    float drawReview(ImDrawList& list, ImVec2 at, float width) const;
    float drawCopying(ImDrawList& list, ImVec2 at, float width, double time) const;
    float drawFinished(ImDrawList& list, ImVec2 at, float width) const;

    std::filesystem::path m_gameDirectory;
    GLFWwindow* m_window = nullptr;
    const MenuFont* m_rowFont = nullptr;
    const MenuFont* m_titleFont = nullptr;

    Step m_step = Step::Choose;
    std::vector<Action> m_actions;
    std::filesystem::path m_location;
    std::optional<core::DiscSource> m_source;
    std::uint64_t m_needed = 0;
    std::uint64_t m_available = 0;
    std::string m_problem;
    bool m_problemIsCancel = false;

    std::deque<std::pair<double, std::uint64_t>> m_samples;  // (time, bytes) over the last seconds, for speed
    BackgroundTask<Inspection, int> m_inspect;
    BackgroundTask<core::Result<bool>, core::ImportProgress> m_copy;
};

}  // namespace black::app
