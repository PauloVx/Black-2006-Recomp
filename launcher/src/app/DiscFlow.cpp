#include "app/DiscFlow.h"

#include "app/NativeDialogs.h"
#include "app/Theme.h"
#include "app/Translations.h"
#include "core/Format.h"
#include "core/Text.h"

#include <algorithm>
#include <cmath>
#include <system_error>

namespace black::app {
namespace {

constexpr float kText = 12.0f;
constexpr float kLine = 22.0f;
constexpr double kSpeedWindow = 4.0;

std::string percentText(double fraction) {
    return std::to_string(static_cast<int>(std::floor(std::clamp(fraction, 0.0, 1.0) * 100.0))) + "%";
}

std::string withThousands(std::size_t value) {
    std::string digits = std::to_string(value);
    for (auto i = static_cast<std::ptrdiff_t>(digits.size()) - 3; i > 0; i -= 3) {
        digits.insert(static_cast<std::size_t>(i), ",");
    }
    return digits;
}

void statusLine(ImDrawList& list, const MenuFont& font, ImVec2 at, std::string_view text, const ImVec4& mark) {
    list.AddCircleFilled(ImVec2(at.x + 4.0f, at.y + 6.0f), 3.5f, toU32(mark));
    font.draw(list, ImVec2(at.x + 18.0f, at.y), text, kText, toU32(colors::kText));
}

}  // namespace

DiscFlow::DiscFlow(std::filesystem::path gameDirectory, GLFWwindow* window)
    : m_gameDirectory(std::move(gameDirectory)), m_window(window) {}

void DiscFlow::setFonts(const MenuFont* rows, const MenuFont* titles) noexcept {
    m_rowFont = rows;
    m_titleFont = titles;
}

void DiscFlow::restart() {
    if (copying()) {
        return;
    }
    m_step = Step::Choose;
    m_problem.clear();
}

bool DiscFlow::copying() const {
    return m_step == Step::Copying;
}

void DiscFlow::offer(std::filesystem::path location) {
    if (copying() || m_inspect.running()) {
        return;
    }
    m_location = std::move(location);
    m_source.reset();
    m_problem.clear();
    m_step = Step::Inspecting;
    const std::filesystem::path gameDirectory = m_gameDirectory;
    m_inspect.start([location = m_location, gameDirectory](std::stop_token, const auto&) {
        Inspection inspection;
        inspection.source = core::inspectDiscSource(location);
        if (inspection.source) {
            inspection.needed = core::bytesStillNeeded(inspection.source.value(), gameDirectory);
            std::error_code error;
            std::filesystem::path drive = gameDirectory;
            while (!drive.empty() && !std::filesystem::exists(drive, error) && drive.has_parent_path() &&
                   drive.parent_path() != drive) {
                drive = drive.parent_path();
            }
            const std::filesystem::space_info space = std::filesystem::space(drive, error);
            inspection.available = error ? 0 : space.available;
        }
        return inspection;
    });
}

void DiscFlow::poll(double time) {
    if (auto inspection = m_inspect.takeOutcome()) {
        if (inspection->source) {
            m_source = std::move(inspection->source).value();
            m_needed = inspection->needed;
            m_available = inspection->available;
            m_step = Step::Review;
        } else {
            m_problem = inspection->source.error().message;
            m_problemIsCancel = false;
            m_step = Step::Stopped;
        }
    }
    if (m_step == Step::Copying) {
        const core::ImportProgress progress = m_copy.progress();
        m_samples.emplace_back(time, progress.doneBytes);
        while (m_samples.size() > 2 && time - m_samples.front().first > kSpeedWindow) {
            m_samples.pop_front();
        }
    }
    if (auto result = m_copy.takeOutcome()) {
        if (!*result) {
            m_problem = result->error().message;
            m_problemIsCancel = false;
            m_step = Step::Stopped;
        } else if (!result->value()) {
            m_problem = tr("Copying stopped. What was copied is kept: copying again carries on from there.");
            m_problemIsCancel = true;
            m_step = Step::Stopped;
        } else {
            m_step = Step::Finished;
        }
    }
}

bool DiscFlow::canCopy() const {
    return m_source && m_source->release.state == core::GameFilesState::Supported && m_available >= m_needed;
}

void DiscFlow::startCopy() {
    if (!canCopy()) {
        return;
    }
    m_step = Step::Copying;
    m_samples.clear();
    core::ImportProgress initial;
    initial.totalBytes = m_source->totalBytes;
    initial.totalFiles = m_source->fileCount;
    m_copy.start(
        [source = *m_source, gameDirectory = m_gameDirectory](std::stop_token stop, const auto& report) {
            return core::importDisc(source, gameDirectory, report, std::move(stop));
        },
        initial);
}

Page DiscFlow::page(double time) {
    poll(time);
    Page page;
    m_actions.clear();
    const auto row = [&](Action action, MenuRow menuRow) {
        page.rows.push_back(std::move(menuRow));
        m_actions.push_back(action);
    };

    switch (m_step) {
        case Step::Choose:
            page.id = "disc-choose";
            page.title = tr("YOUR DISC");
            page.body = [this](ImDrawList& list, ImVec2 at, float width) { return drawChoose(list, at, width); };
            row(Action::ChooseImage, {tr("CHOOSE A DISC IMAGE"), tr("ISO"),
                                      tr("An ISO made from your own original Xbox disc: a full "
                                         "Redump dump or an XISO.")});
            row(Action::ChooseFolder,
                {tr("CHOOSE A FOLDER"), "", tr("A folder with the files of your disc, already extracted.")});
            row(Action::Back, {tr("BACK"), "", tr("Back to the main menu.")});
            break;
        case Step::Inspecting:
            page.id = "disc-reading";
            page.title = tr("YOUR DISC");
            page.body = [this, time](ImDrawList& list, ImVec2 at, float width) {
                return drawInspecting(list, at, width, time);
            };
            break;
        case Step::Review: {
            page.id = "disc-review";
            page.title = tr("YOUR DISC");
            page.body = [this](ImDrawList& list, ImVec2 at, float width) { return drawReview(list, at, width); };
            const bool supported = m_source->release.state == core::GameFilesState::Supported;
            MenuRow copy{tr("COPY INTO THE GAME FOLDER"), core::formatBytes(m_needed), "", canCopy()};
            copy.help = !supported ? tr("This is not the supported release, so it cannot be used.")
                        : !canCopy()
                            ? tr("Free up space on this drive first.")
                            : tr("Copies the game's files into the game folder. Saves already there are kept.");
            row(Action::Copy, copy);
            row(Action::ChooseAnother, {tr("CHOOSE ANOTHER DISC"), "", tr("Pick a different ISO or folder.")});
            row(Action::Back, {tr("BACK"), "", tr("Back to the main menu.")});
            page.initialRow = canCopy() ? 0 : 1;
            break;
        }
        case Step::Copying:
            page.id = "disc-copying";
            page.title = tr("COPYING YOUR DISC");
            page.body = [this, time](ImDrawList& list, ImVec2 at, float width) {
                return drawCopying(list, at, width, time);
            };
            row(Action::Cancel, {m_copy.cancelling() ? tr("STOPPING...") : tr("STOP"), "",
                                 tr("Stops copying. What was copied is kept, and copying again carries on from there."),
                                 !m_copy.cancelling()});
            break;
        case Step::Finished:
            page.id = "disc-done";
            page.title = tr("YOUR DISC IS IN PLACE");
            page.body = [this](ImDrawList& list, ImVec2 at, float width) { return drawFinished(list, at, width); };
            row(Action::Continue,
                {tr("CONTINUE"), "", tr("Back to the main menu, where the next step is building the game.")});
            break;
        case Step::Stopped:
            page.id = m_problemIsCancel ? "disc-stopped" : "disc-problem";
            page.title = tr("YOUR DISC");
            page.notice = m_problem;
            page.noticeIsProblem = !m_problemIsCancel;
            if (!m_location.empty()) {
                row(Action::TryAgain, {m_problemIsCancel ? tr("CARRY ON COPYING") : tr("TRY AGAIN"), "", ""});
            }
            row(Action::ChooseAnother, {tr("CHOOSE ANOTHER DISC"), "", ""});
            row(Action::Back, {tr("BACK"), "", ""});
            break;
    }
    return page;
}

DiscFlow::Outcome DiscFlow::activate(std::size_t row) {
    if (row >= m_actions.size()) {
        return Outcome::Stay;
    }
    switch (m_actions[row]) {
        case Action::ChooseImage:
            if (auto chosen = chooseDiscImage(m_window)) {
                offer(std::move(*chosen));
            }
            break;
        case Action::ChooseFolder:
            if (auto chosen = chooseFolder(m_window)) {
                offer(std::move(*chosen));
            }
            break;
        case Action::Copy: startCopy(); break;
        case Action::TryAgain:
            if (m_problemIsCancel && m_source) {
                startCopy();
            } else {
                offer(m_location);
            }
            break;
        case Action::ChooseAnother: m_step = Step::Choose; break;
        case Action::Cancel: m_copy.cancel(); break;
        case Action::Continue: m_step = Step::Choose; return Outcome::Imported;
        case Action::Back: return back();
    }
    return Outcome::Stay;
}

DiscFlow::Outcome DiscFlow::back() {
    if (copying() || m_step == Step::Inspecting) {
        return Outcome::Stay;
    }
    m_step = Step::Choose;
    return Outcome::Back;
}

float DiscFlow::drawChoose(ImDrawList& list, ImVec2 at, float width) const {
    float y = at.y;
    y +=
        drawWrapped(list, *m_rowFont, ImVec2(at.x, y),
                    tr("Use your own original Xbox copy of BLACK: an ISO made from the disc, or its files in a folder. "
                       "The launcher checks it is the supported release before copying anything."),
                    kText, width, toU32(colors::kRow));
    y += 10.0f;
    const ImVec2 boxMin(at.x, y);
    const ImVec2 boxMax(at.x + width, y + 40.0f);
    for (float x = boxMin.x; x < boxMax.x; x += 12.0f) {
        list.AddLine(ImVec2(x, boxMin.y), ImVec2(std::min(x + 6.0f, boxMax.x), boxMin.y), toU32(colors::kRail, 0.4f));
        list.AddLine(ImVec2(x, boxMax.y), ImVec2(std::min(x + 6.0f, boxMax.x), boxMax.y), toU32(colors::kRail, 0.4f));
    }
    const std::string hint = tr("OR DROP AN ISO OR FOLDER ONTO THIS WINDOW");
    m_rowFont->draw(list, ImVec2(at.x + (width - m_rowFont->width(hint, kText)) * 0.5f, y + 14.0f), hint, kText,
                    toU32(colors::kMuted));
    return boxMax.y - at.y + 18.0f;
}

float DiscFlow::drawInspecting(ImDrawList& list, ImVec2 at, float width, double time) const {
    m_rowFont->draw(list, at, tr("READING YOUR DISC AND CHECKING ITS RELEASE..."), kText, toU32(colors::kText));
    drawProgressBar(list, ImVec2(at.x, at.y + 26.0f), width, std::nullopt, time);
    m_rowFont->draw(list, ImVec2(at.x, at.y + 62.0f), core::toUtf8(m_location.filename()), kText,
                    toU32(colors::kMuted));
    return 90.0f;
}

float DiscFlow::drawReview(ImDrawList& list, ImVec2 at, float width) const {
    const core::DiscSource& source = *m_source;
    const bool supported = source.release.state == core::GameFilesState::Supported;
    const bool fits = m_available >= m_needed;
    float y = at.y;
    const std::string kind = source.kind == core::DiscSource::Kind::Image ? tr("DISC IMAGE") : tr("FOLDER");
    const std::string name = core::toUtf8(source.location.filename());
    drawWrapped(list, *m_rowFont, ImVec2(at.x, y), kind + ": " + name, kText, width, toU32(colors::kMuted));
    y += kLine + 6.0f;
    statusLine(list, *m_rowFont, ImVec2(at.x, y),
               supported ? tr("THE SUPPORTED XBOX RETAIL RELEASE OF BLACK") : tr("A DIFFERENT RELEASE: NOT SUPPORTED"),
               supported ? colors::kGood : colors::kBad);
    y += kLine;
    statusLine(list, *m_rowFont, ImVec2(at.x, y),
               trf("{0} IN {1} FILES", {core::formatBytes(source.totalBytes), withThousands(source.fileCount)}),
               colors::kRail);
    y += kLine;
    const std::string space =
        m_needed == 0 ? tr("ALREADY COPIED: NOTHING LEFT TO COPY")
                      : trf("FREE SPACE {0}, NEEDS {1}", {core::formatBytes(m_available), core::formatBytes(m_needed)});
    statusLine(list, *m_rowFont, ImVec2(at.x, y), space, fits ? colors::kGood : colors::kBad);
    return y + kLine + 8.0f - at.y;
}

float DiscFlow::drawCopying(ImDrawList& list, ImVec2 at, float width, double time) const {
    const core::ImportProgress progress = m_copy.progress();
    const double fraction = progress.totalBytes == 0
                                ? 0.0
                                : static_cast<double>(progress.doneBytes) / static_cast<double>(progress.totalBytes);
    std::string stage;
    switch (progress.stage) {
        case core::ImportProgress::Stage::Reading:
        case core::ImportProgress::Stage::Checking: stage = tr("CHECKING FREE SPACE..."); break;
        case core::ImportProgress::Stage::Copying: stage = tr("COPYING THE GAME'S FILES"); break;
        case core::ImportProgress::Stage::Finishing: stage = tr("CHECKING THE COPY..."); break;
        case core::ImportProgress::Stage::Done: stage = tr("DONE"); break;
    }
    if (m_copy.cancelling()) {
        stage = tr("STOPPING...");
    }

    const std::string percent = percentText(fraction);
    m_titleFont->draw(list, ImVec2(at.x + width - m_titleFont->width(percent, 26.0f), at.y), percent, 26.0f,
                      toU32(colors::kText));
    m_rowFont->draw(list, ImVec2(at.x, at.y + 12.0f), stage, kText, toU32(colors::kText));
    const bool exact =
        progress.stage == core::ImportProgress::Stage::Copying || progress.stage == core::ImportProgress::Stage::Done;
    drawProgressBar(list, ImVec2(at.x, at.y + 40.0f), width,
                    exact ? std::optional<float>(static_cast<float>(fraction)) : std::nullopt, time);

    std::string line =
        trf("{0} OF {1}", {core::formatBytes(progress.doneBytes), core::formatBytes(progress.totalBytes)});
    if (m_samples.size() >= 2) {
        const auto& [firstTime, firstBytes] = m_samples.front();
        const auto& [lastTime, lastBytes] = m_samples.back();
        const double seconds = lastTime - firstTime;
        if (seconds > 0.5 && lastBytes > firstBytes) {
            const double rate = static_cast<double>(lastBytes - firstBytes) / seconds;
            line += "    " + core::formatRate(rate);
            line += "    " + core::formatTimeLeft(static_cast<double>(progress.totalBytes - progress.doneBytes) / rate);
        }
    }
    m_rowFont->draw(list, ImVec2(at.x, at.y + 76.0f), line, kText, toU32(colors::kRow));
    const std::string file =
        trf("FILE {0} OF {1}", {withThousands(std::min(progress.doneFiles + 1, progress.totalFiles)),
                                withThousands(progress.totalFiles)}) +
        (progress.item.empty() ? "" : ":  " + progress.item);
    const std::vector<std::string> fileLines = wrapText(*m_rowFont, file, kText, width);
    m_rowFont->draw(list, ImVec2(at.x, at.y + 76.0f + kLine), fileLines.empty() ? "" : fileLines.front(), kText,
                    toU32(colors::kMuted));
    return 76.0f + kLine * 2.0f + 8.0f;
}

float DiscFlow::drawFinished(ImDrawList& list, ImVec2 at, float width) const {
    statusLine(list, *m_rowFont, at, tr("THE GAME'S FILES ARE IN THE GAME FOLDER"), colors::kGood);
    if (m_source) {
        statusLine(
            list, *m_rowFont, ImVec2(at.x, at.y + kLine),
            trf("{0} IN {1} FILES", {core::formatBytes(m_source->totalBytes), withThousands(m_source->fileCount)}),
            colors::kGood);
    }
    drawWrapped(list, *m_rowFont, ImVec2(at.x, at.y + kLine * 2.0f + 8.0f),
                tr("Next, build the game: the launcher turns your disc into the PC game."), kText, width,
                toU32(colors::kRow));
    return kLine * 3.0f + 16.0f;
}

}  // namespace black::app
