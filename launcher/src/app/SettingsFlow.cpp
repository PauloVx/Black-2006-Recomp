#include "app/SettingsFlow.h"

#include "app/KeyCapture.h"
#include "core/InputBindings.h"
#include "core/Process.h"
#include "core/Text.h"

#include "imgui.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <utility>

namespace black::app {
namespace {

namespace bindings = core::bindings;

template <typename T>
struct Choice {
    T value;
    const char* label;
};

template <typename T>
std::size_t indexOf(const std::vector<Choice<T>>& list, const T& value) {
    for (std::size_t i = 0; i < list.size(); ++i) {
        if (list[i].value == value) {
            return i;
        }
    }
    return 0;
}

template <typename T>
void cycle(const std::vector<Choice<T>>& list, T& field, int step) {
    const auto count = static_cast<long long>(list.size());
    const long long next = (static_cast<long long>(indexOf(list, field)) + (step == 0 ? 1 : step) + count) % count;
    field = list[static_cast<std::size_t>(next)].value;
}

std::string upper(std::string_view text) {
    std::string result(text);
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return result;
}

std::string fixed(double value, int decimals) {
    std::array<char, 32> text{};
    std::snprintf(text.data(), text.size(), "%.*f", decimals, value);  // NOLINT(cppcoreguidelines-pro-type-vararg)
    return text.data();
}

const std::vector<Choice<std::string>> kSteps = {{"off", "OFF"}, {"low", "LOW"}, {"medium", "MEDIUM"}, {"high", "HIGH"}};
const std::vector<Choice<std::string>> kQualities = {
    {"low", "LOW"}, {"medium", "MEDIUM"}, {"high", "HIGH"}, {"ultra", "ULTRA"}};

constexpr std::array<std::pair<int, int>, 29> kWindowSizes = {{
    {640, 480},   {800, 600},   {1024, 768},  {1152, 864},  {1280, 720},  {1280, 800},  {1280, 960},  {1280, 1024},
    {1366, 768},  {1400, 1050}, {1440, 900},  {1600, 900},  {1600, 1200}, {1680, 1050}, {1920, 1080}, {1920, 1200},
    {1920, 1440}, {2048, 1536}, {2560, 1080}, {2560, 1440}, {2560, 1600}, {3440, 1440}, {3840, 1080}, {3840, 1600},
    {3840, 2160}, {3840, 2400}, {5120, 1440}, {5120, 2160}, {7680, 2160},
}};

constexpr std::size_t kSlots = bindings::kMaxKeysPerAction;

}  // namespace

SettingsFlow::SettingsFlow(core::ProjectLayout project) : m_project(std::move(project)) { reload(); }

void SettingsFlow::reload() {
    m_settings = core::loadSettings(m_project);
    m_notice.clear();
}

void SettingsFlow::save() {
    if (auto saved = core::saveSettings(m_project, m_settings); !saved) {
        m_notice = saved.error().message;
        m_noticeIsProblem = true;
    } else if (m_noticeIsProblem) {
        m_notice.clear();
    }
}

void SettingsFlow::open(Section section) {
    if (section == Section::Hub) {
        m_lastSection = m_section;
    }
    m_section = section;
    m_slot = 0;
    m_notice.clear();
    if (section == Section::Report) {
        m_session = core::latestSession(m_project);
        m_summary.reset();
        if (m_session) {
            m_summary = core::summarizeSession(m_project, *m_session, core::userHome());
        }
    }
}

std::vector<SettingsFlow::Option> SettingsFlow::options(Section section) {
    switch (section) {
        case Section::Display: return displayOptions();
        case Section::Graphics: return graphicsOptions();
        case Section::Controls: return controlsOptions();
        case Section::Game: return gameOptions();
        case Section::Diagnostics: return diagnosticsOptions();
        case Section::Report:
        case Section::Hub:
        case Section::Bindings: break;
    }
    return {};
}

std::vector<SettingsFlow::Option> SettingsFlow::displayOptions() {
    core::Settings& s = m_settings;
    const auto pick = [](std::string label, std::string help, std::string& field,
                         std::vector<Choice<std::string>> list) {
        return Option{std::move(label), std::move(help),
                      [&field, list] { return std::string(list[indexOf(list, field)].label); },
                      [&field, list](int step) { cycle(list, field, step); }, {}};
    };
    std::vector<Option> rows;
    rows.push_back({"WINDOW SIZE", "The size of the game window when windowed.",
                    [&s] { return std::to_string(s.windowWidth) + " X " + std::to_string(s.windowHeight); },
                    [&s](int step) {
                        std::vector<std::pair<int, int>> sizes(kWindowSizes.begin(), kWindowSizes.end());
                        const std::pair<int, int> current{s.windowWidth, s.windowHeight};
                        if (std::find(sizes.begin(), sizes.end(), current) == sizes.end()) {
                            sizes.push_back(current);  // a size saved by hand stays on the list
                            std::sort(sizes.begin(), sizes.end());
                        }
                        const auto at = static_cast<long long>(std::find(sizes.begin(), sizes.end(), current) -
                                                               sizes.begin());
                        const auto count = static_cast<long long>(sizes.size());
                        const auto next = static_cast<std::size_t>((at + (step == 0 ? 1 : step) + count) % count);
                        s.windowWidth = sizes[next].first;
                        s.windowHeight = sizes[next].second;
                    },
                    {}});
    rows.push_back(pick("WINDOW", "Borderless fullscreen covers the screen the game window is on. F11 switches in game.",
                        s.windowMode, {{"windowed", "WINDOWED"}, {"borderless", "BORDERLESS FULLSCREEN"}}));
    rows.push_back(pick("SCALING", "How the picture fits the window. Keep aspect adds bars; stretch fills the window.",
                        s.scaleMode, {{"fit", "KEEP ASPECT"}, {"stretch", "STRETCH"}, {"integer", "INTEGER"}}));
    rows.push_back(pick("FILTER", "Smooth blends neighbouring pixels when scaling; sharp keeps pixel edges.",
                        s.presentFilter, {{"linear", "SMOOTH"}, {"nearest", "SHARP"}}));
    rows.push_back({"VSYNC", "Waits for the display between frames, so the picture never tears.",
                    [&s] { return std::string(s.vsync ? "ON" : "OFF"); }, [&s](int) { s.vsync = !s.vsync; }, {}});
    std::vector<Choice<int>> limits;
    for (const int limit : core::choices::kFrameLimits) {
        limits.push_back({limit, limit == 0 ? "UNLIMITED" : nullptr});
    }
    rows.push_back({"FRAME LIMIT", "Limits how often the picture is shown. The game's speed does not change.",
                    [&s] { return s.fpsLimit == 0 ? std::string("UNLIMITED") : std::to_string(s.fpsLimit) + " FPS"; },
                    [&s, limits](int step) { cycle(limits, s.fpsLimit, step); }, {}});
    rows.push_back(pick("CAMERA", "How wide the camera sees. Original keeps the game's 4:3 view.", s.cameraAspect,
                        {{"original", "ORIGINAL"},
                         {"4:3", "4:3"},
                         {"16:9", "16:9 WIDESCREEN"},
                         {"21:9", "21:9 ULTRAWIDE"},
                         {"32:9", "32:9 ULTRAWIDE"}}));
    rows.push_back({"FIELD OF VIEW",
                    "The vertical angle the camera sees, in degrees. Original keeps BLACK's own view and aim zoom.",
                    [&s] { return s.verticalFov == 0 ? std::string("ORIGINAL") : fixed(s.verticalFov, 0); },
                    [&s](int step) {
                        constexpr double kMin = core::choices::kFovMin;
                        constexpr double kMax = core::choices::kFovMax;
                        const int delta = step == 0 ? 1 : step;
                        if (s.verticalFov == 0) {
                            s.verticalFov = delta > 0 ? kMin : kMax;
                        } else {
                            const double next = std::round(s.verticalFov) + delta;
                            s.verticalFov = next < kMin || next > kMax ? 0 : next;
                        }
                    },
                    {}});
    rows.push_back({"INTERNAL RESOLUTION",
                    "How many lines the game draws; the width follows the camera. Higher is sharper and slower.",
                    [&s] {
                        const auto size = core::renderSize(s);
                        return std::to_string(s.internalHeight) + "P  (" + std::to_string(size[0]) + " X " +
                               std::to_string(size[1]) + ")";
                    },
                    [&s](int step) {
                        std::vector<Choice<int>> heights;
                        for (const int height : core::choices::kInternalHeights) {
                            heights.push_back({height, nullptr});
                        }
                        cycle(heights, s.internalHeight, step);
                    },
                    {}});
    rows.push_back(pick("MOTION BLUR", "Original keeps BLACK's motion blur; off removes only that effect.",
                        s.motionBlur, {{"original", "ORIGINAL"}, {"off", "OFF"}}));
    return rows;
}

std::vector<SettingsFlow::Option> SettingsFlow::graphicsOptions() {
    core::Settings& s = m_settings;
    const auto gpu = [&s] { return s.gpuRenderer; };
    const auto pick = [&gpu](std::string label, std::string help, std::string& field,
                             std::vector<Choice<std::string>> list) {
        return Option{std::move(label), std::move(help),
                      [&field, list] { return std::string(list[indexOf(list, field)].label); },
                      [&field, list](int step) { cycle(list, field, step); }, gpu};
    };
    std::vector<Option> rows;
    rows.push_back(pick("ANTI-ALIASING",
                        "FXAA smooths jagged edges at almost no cost. SSAA draws four times the pixels: the cleanest "
                        "and the heaviest.",
                        s.antiAliasing, {{"off", "OFF"}, {"fxaa", "FXAA"}, {"ssaa", "SSAA 4X"}}));
    rows.push_back({"ANISOTROPIC FILTERING", "Keeps floors and walls seen at a slant sharp. Costs almost nothing.",
                    [&s] { return s.anisotropy == 1 ? std::string("OFF") : std::to_string(s.anisotropy) + "X"; },
                    [&s](int step) {
                        std::vector<Choice<int>> levels;
                        for (const int level : core::choices::kAnisotropy) {
                            levels.push_back({level, nullptr});
                        }
                        cycle(levels, s.anisotropy, step);
                    },
                    gpu});
    rows.push_back(pick("DEPTH OF FIELD", "Blurs what lies behind the point you look at. The weapon and HUD stay sharp.",
                        s.depthOfField, kSteps));
    rows.push_back(pick("SHARPENING", "Brings back fine detail that scaling and filtering soften.", s.sharpening, kSteps));
    rows.push_back(pick("AMBIENT OCCLUSION", "Contact shadows in corners and under objects, from the scene's depth.",
                        s.aoMethod,
                        {{"off", "OFF"}, {"ssao", "SSAO"}, {"hbao", "HBAO"}, {"hbao_plus", "HBAO+"}, {"gtao", "GTAO"}}));
    Option quality = pick("AO QUALITY", "More samples and a wider radius for ambient occlusion, at more GPU cost.",
                          s.aoQuality, kQualities);
    quality.enabled = [&s] { return s.gpuRenderer && s.aoMethod != "off"; };
    rows.push_back(std::move(quality));
    rows.push_back(pick("REFLECTIONS",
                        "Reflects the scene on weapons and glass instead of BLACK's low-resolution maps. Original "
                        "keeps the maps.",
                        s.reflections,
                        {{"original", "ORIGINAL"}, {"low", "LOW"}, {"medium", "MEDIUM"}, {"high", "HIGH"}, {"ultra", "ULTRA"}}));
    return rows;
}

std::vector<SettingsFlow::Option> SettingsFlow::controlsOptions() {
    core::Settings& s = m_settings;
    const auto pick = [](std::string label, std::string help, std::string& field,
                         std::vector<Choice<std::string>> list) {
        return Option{std::move(label), std::move(help),
                      [&field, list] { return std::string(list[indexOf(list, field)].label); },
                      [&field, list](int step) { cycle(list, field, step); }, {}};
    };
    std::vector<Option> rows;
    rows.push_back({"KEY BINDINGS", "The keys and mouse buttons for each action.", [] { return std::string(); },
                    [this](int) { open(Section::Bindings); }, {}, false});
    rows.push_back(pick("DEVICES", "Which devices the game listens to. A controller works alongside by default.",
                        s.inputMode,
                        {{"auto", "KEYBOARD, MOUSE AND PAD"},
                         {"keyboard_mouse", "KEYBOARD AND MOUSE"},
                         {"controller", "CONTROLLER ONLY"}}));
    rows.push_back(pick("BUTTON PROMPTS", "Which buttons the game's prompts show.", s.promptMode,
                        {{"auto", "THE DEVICE IN USE"}, {"keyboard_mouse", "KEYBOARD AND MOUSE"}, {"controller", "CONTROLLER"}}));
    rows.push_back({"MOUSE SENSITIVITY", "Raw mouse input with no acceleration. 1.00 turns 0.06 degrees per count.",
                    [&s] { return fixed(s.mouseSensitivity, 2); },
                    [&s](int step) {
                        const double next = std::round(s.mouseSensitivity * 20.0 + (step == 0 ? 1 : step)) / 20.0;
                        s.mouseSensitivity = std::clamp(next, bindings::kSensitivityMin, bindings::kSensitivityMax);
                    },
                    {}});
    rows.push_back({"INVERT MOUSE Y", "Moving the mouse forward looks down.",
                    [&s] { return std::string(s.invertY ? "ON" : "OFF"); }, [&s](int) { s.invertY = !s.invertY; }, {}});
    return rows;
}

std::vector<SettingsFlow::Option> SettingsFlow::gameOptions() {
    core::Settings& s = m_settings;
    const auto toggle = [](std::string label, std::string help, bool& field) {
        return Option{std::move(label), std::move(help), [&field] { return std::string(field ? "ON" : "OFF"); },
                      [&field](int) { field = !field; }, {}};
    };
    std::vector<Option> rows;
    rows.push_back({"RENDERER", "The GPU draws the game on your graphics card. The CPU renderer is far slower.",
                    [&s] { return std::string(s.gpuRenderer ? "GPU (DIRECT3D 11)" : "CPU"); },
                    [&s](int) { s.gpuRenderer = !s.gpuRenderer; }, {}});
    rows.push_back({"GAME CLOCK",
                    "How often the game steps. The original ran at 30; every shown frame is a real one. 240 needs a "
                    "fast PC and display.",
                    [&s] { return !s.smooth60 ? std::string("ORIGINAL 30") : std::to_string(s.smoothHz == 0 ? 60 : s.smoothHz); },
                    [&s](int step) {
                        const std::vector<Choice<int>> rates = {{30, nullptr}, {60, nullptr}, {120, nullptr}, {240, nullptr}};
                        int rate = !s.smooth60 ? 30 : (s.smoothHz == 0 ? 60 : s.smoothHz);
                        cycle(rates, rate, step);
                        s.smooth60 = rate != 30;
                        s.smoothHz = rate == 30 || rate == 60 ? 0 : rate;
                    },
                    {}});
    rows.push_back({"RENDERER THREADS",
                    "Threads that draw for the game. The default is half of this PC's: " +
                        std::to_string(core::defaultThreads()) + ".",
                    [&s] { return std::to_string(s.threads); },
                    [&s](int step) { s.threads = std::clamp(s.threads + (step == 0 ? 1 : step), 1, 64); }, {}});
    rows.push_back({"MENU LANGUAGE",
                    "The language of the game's menus. Spanish, French, German and Italian are machine drafts.",
                    [&s] {
                        const std::vector<Choice<std::string>> names = {
                            {"en-US", "ENGLISH"},          {"pt-BR", "PORTUGUESE (BRAZIL)"}, {"es", "SPANISH (DRAFT)"},
                            {"fr", "FRENCH (DRAFT)"},      {"de", "GERMAN (DRAFT)"},          {"it", "ITALIAN (DRAFT)"}};
                        return std::string(names[indexOf(names, s.menuLanguage)].label);
                    },
                    [&s](int step) {
                        std::vector<Choice<std::string>> languages;
                        for (const std::string_view language : core::choices::kLanguages) {
                            languages.push_back({std::string(language), nullptr});
                        }
                        cycle(languages, s.menuLanguage, step);
                    },
                    {}});
    rows.push_back(toggle("SKIP MOVIES", "Skips the intro and story movies.", s.skipMovies));
    rows.push_back(toggle("SKIP STARTUP MENUS", "Presses A through the startup menus for the first minute.",
                          s.advanceMenus));
    rows.push_back(toggle("INFINITE AMMO", "Weapons never run out.", s.infiniteAmmo));
    rows.push_back({"SILVER WEAPONS", "The silver weapons the game unlocks on Hard. Game keeps its own rules.",
                    [&s] {
                        return std::string(s.silverWeaponMode == "force-on"    ? "ALWAYS"
                                           : s.silverWeaponMode == "force-off" ? "NEVER"
                                                                               : "AS THE GAME DECIDES");
                    },
                    [&s](int step) {
                        const std::vector<Choice<std::string>> modes = {
                            {"native", nullptr}, {"force-on", nullptr}, {"force-off", nullptr}};
                        cycle(modes, s.silverWeaponMode, step);
                    },
                    {}});
    return rows;
}

std::vector<SettingsFlow::Option> SettingsFlow::diagnosticsOptions() {
    core::Settings& s = m_settings;
    const auto toggle = [](std::string label, std::string help, bool& field) {
        return Option{std::move(label), std::move(help), [&field] { return std::string(field ? "ON" : "OFF"); },
                      [&field](int) { field = !field; }, {}};
    };
    std::vector<Option> rows;
    rows.push_back(toggle("VISUAL TRACE",
                          "Records what the CPU renderer draws to visual-trace.log (1 MB at most). Needs the CPU "
                          "renderer, and slows the frames it records.",
                          s.visualTraceEnabled));
    Option frames{"TRACE FRAMES", "Which frames to trace: one (120), a range (120-180) or every Nth of a range (120-240/30).",
                  [&s] { return s.visualTraceFrames; }, {}, [&s] { return s.visualTraceEnabled; }, false};
    frames.allowed = "0123456789-/";
    frames.commit = [&s](const std::string& text) -> std::optional<std::string> {
        const auto normalized = core::normalizeTraceFrames(text);
        if (!normalized) {
            return "Enter a frame (120), a range (120-180) or a stepped range (120-240/30).";
        }
        s.visualTraceFrames = *normalized;
        return std::nullopt;
    };
    rows.push_back(std::move(frames));
    rows.push_back(toggle("AUDIO DSP TRACE",
                          "Samples the audio processor's state about once a second to audio-dsp-trace.log (1 MB at "
                          "most). No sound is recorded.",
                          s.dspTraceEnabled));
    Option drawLog{"GPU DRAW LOG",
                   "Logs every draw of some frames to kgpu-draw-trace.log: a frame, or frame,count (up to 120). Empty "
                   "turns it off. Needs the GPU renderer.",
                   [&s] { return s.kgpuDrawLogFrames.empty() ? std::string("OFF") : s.kgpuDrawLogFrames; }, {}, {},
                   false};
    drawLog.allowed = "0123456789,";
    drawLog.commit = [&s](const std::string& text) -> std::optional<std::string> {
        if (text.empty()) {
            s.kgpuDrawLogFrames.clear();
            return std::nullopt;
        }
        const auto normalized = core::normalizeDrawLogFrames(text);
        if (!normalized) {
            return "Enter a frame (2400) or frame,count (2400,2), up to 120 frames.";
        }
        s.kgpuDrawLogFrames = *normalized;
        return std::nullopt;
    };
    rows.push_back(std::move(drawLog));
    rows.push_back({"OPEN LOGS FOLDER", "Each game run keeps its logs in reports/launcher-<date and time>.",
                    [] { return std::string(); },
                    [this](int) {
                        const std::filesystem::path reports = m_project.root() / "reports";
                        std::error_code error;
                        std::filesystem::create_directories(reports, error);
                        if (auto opened = core::openFolder(reports); !opened) {
                            m_notice = opened.error().message;
                            m_noticeIsProblem = true;
                        }
                    },
                    {}, false});
    rows.push_back({"REPORT A PROBLEM", "Prepares a GitHub issue about the last game run, for you to check and send.",
                    [] { return std::string(); }, [this](int) { open(Section::Report); }, {}, false});
    return rows;
}

void SettingsFlow::prepareReport() {
    if (!m_summary) {
        return;
    }
    const std::string body = core::reportBody(m_summary->text);
    const auto draft = core::saveReportDraft(m_project, body);
    ImGui::SetClipboardText(body.c_str());
    if (auto opened = core::openUrl(core::issueUrl("Automatic report", body)); !opened) {
        m_notice = opened.error().message + ". The report is copied: paste it into a new issue at " +
                   std::string(core::kIssuesUrl) + ".";
        m_noticeIsProblem = true;
        return;
    }
    m_notice = "The report is open on GitHub and copied" +
               (draft ? " (also saved in reports/issue-drafts)" : std::string()) +
               ". Attach the session's logs there, then submit it.";
    m_noticeIsProblem = false;
}

Page SettingsFlow::reportPage() const {
    Page page;
    page.id = "settings-report";
    page.title = "REPORT A PROBLEM";
    page.listWidth = 640.0f;
    page.notice = m_notice;
    page.noticeIsProblem = m_noticeIsProblem;
    if (!m_summary) {
        page.rows = {{"NO GAME RUN YET", "", "Play once, then come back here if something goes wrong.", false},
                     {"BACK", "", "", true}};
        return page;
    }
    const std::string& name = m_summary->folderName;  // launcher-YYYYMMDDTHHMMSSmmmZ-id
    const std::string when = name.size() >= 22 ? name.substr(9, 4) + "-" + name.substr(13, 2) + "-" +
                                                     name.substr(15, 2) + "  " + name.substr(18, 2) + ":" +
                                                     name.substr(20, 2) + " UTC"
                                               : name;
    const std::string ended = !m_summary->exitCode      ? std::string("STILL RUNNING OR UNKNOWN")
                              : *m_summary->exitCode == 0 ? std::string("NORMALLY")
                                                          : "WITH ERROR " + std::to_string(*m_summary->exitCode);
    page.rows = {
        {"LAST GAME RUN", when, "The game run this report is about.", false},
        {"ENDED", ended, "How the game ended.", false},
        {"ERROR LINES IN ITS LOGS", std::to_string(m_summary->errorLines.size()),
         "Lines with error, failed, exception or crash in session.log and kernel.log. The report lists the latest.",
         false},
        {"PREPARE THE REPORT", "",
         "Opens a new GitHub issue filled in with this run's details, without your folder names. Nothing is sent "
         "until you submit it there.",
         true},
        {"OPEN THIS RUN'S LOGS", "", "Attach session.log and kernel.log from here to the issue.", true},
        {"BACK", "", "", true},
    };
    page.initialRow = 3;
    return page;
}

std::vector<std::string> SettingsFlow::slotsOf(std::size_t action) const {
    return bindings::tokens(bindings::effective(m_settings.bindings, bindings::actions()[action]));
}

void SettingsFlow::setSlot(std::size_t action, std::size_t slot, const std::string& token) {
    std::vector<std::string> keys = slotsOf(action);
    if (token.empty()) {
        if (slot < keys.size()) {
            keys.erase(keys.begin() + static_cast<std::ptrdiff_t>(slot));
        }
    } else {
        keys.erase(std::remove(keys.begin(), keys.end(), token), keys.end());
        if (slot >= keys.size()) {
            keys.push_back(token);
        } else {
            keys[slot] = token;
        }
        keys.resize(std::min(keys.size(), kSlots));
    }
    m_settings.bindings[std::string(bindings::actions()[action].name)] = bindings::join(keys);
    m_settings.bindings = bindings::repair(m_settings.bindings);
    save();
}

void SettingsFlow::updateEdit() {
    const std::vector<Option> rows = options(m_section);
    if (m_edit.row >= rows.size() || !rows[m_edit.row].commit) {
        m_edit.active = false;
        return;
    }
    const Option& option = rows[m_edit.row];
    ImGuiIO& io = ImGui::GetIO();
    for (const ImWchar c : io.InputQueueCharacters) {
        if (c < 128 && option.allowed.find(static_cast<char>(c)) != std::string::npos && m_edit.text.size() < 24) {
            m_edit.text.push_back(static_cast<char>(c));
        }
    }
    io.InputQueueCharacters.resize(0);
    if (ImGui::IsKeyPressed(ImGuiKey_Backspace) && !m_edit.text.empty()) {
        m_edit.text.pop_back();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)) {
        if (const auto problem = option.commit(m_edit.text)) {
            m_notice = *problem;
            m_noticeIsProblem = true;
            return;
        }
        m_edit.active = false;
        m_notice.clear();
        save();
    }
}

void SettingsFlow::update(double time, std::size_t selectedRow) {
    m_selectedRow = selectedRow;
    m_time = time;
    const bool wasBusy = m_capture.active || m_edit.active;
    m_inputUsed = false;
    if (m_edit.active) {
        updateEdit();
    } else {
        updateCapture(time);
    }
    m_inputUsed = wasBusy && !m_capture.active && !m_edit.active;
}

void SettingsFlow::updateCapture(double time) {
    if (m_capture.active) {
        std::optional<std::string> token;
        if (const auto key = takeCapturedKey()) {
            switch (key->kind) {
                case CapturedKey::Kind::Key: token = key->token; break;
                case CapturedKey::Kind::Cancel:
                    m_capture.active = false;
                    endKeyCapture();
                    m_notice.clear();
                    return;
                case CapturedKey::Kind::Reserved:
                    m_notice = "F11 switches the game window and cannot be bound. Press another key.";
                    m_noticeIsProblem = true;
                    break;
                case CapturedKey::Kind::Unbindable:
                    m_notice = "That key cannot be bound. Press another key.";
                    m_noticeIsProblem = true;
                    break;
            }
        }
        if (!token && time - m_capture.since > 0.3) {  // not the click that started the capture
            constexpr std::array<const char*, 5> kButtons = {"Mouse1", "Mouse2", "Mouse3", "Mouse4", "Mouse5"};
            for (int button = 0; button < 5; ++button) {
                if (ImGui::IsMouseClicked(button)) {
                    token = kButtons.at(static_cast<std::size_t>(button));
                }
            }
            const float wheel = ImGui::GetIO().MouseWheel;
            if (wheel != 0.0f) {
                token = wheel > 0.0f ? "WheelUp" : "WheelDown";
            }
        }
        if (token) {
            m_capture.active = false;
            endKeyCapture();
            m_notice.clear();
            setSlot(m_capture.action, m_slot, *token);
        }
        return;
    }
    if (m_section != Section::Bindings || m_selectedRow >= bindings::actions().size()) {
        return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) {
        m_slot = (m_slot + kSlots - 1) % kSlots;
    } else if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) {
        m_slot = (m_slot + 1) % kSlots;
    } else if (ImGui::IsKeyPressed(ImGuiKey_Delete) || ImGui::IsKeyPressed(ImGuiKey_Backspace)) {
        setSlot(m_selectedRow, m_slot, {});
    }
}

Page SettingsFlow::bindingsPage() const {
    Page page;
    page.id = "settings-bindings";
    page.title = "KEY BINDINGS";
    page.listWidth = 640.0f;
    const auto& actions = bindings::actions();
    for (std::size_t i = 0; i < actions.size(); ++i) {
        const std::vector<std::string> keys = slotsOf(i);
        MenuRow row{upper(actions[i].label), "", "", true};
        for (std::size_t slot = 0; slot < kSlots; ++slot) {
            row.cells.push_back(slot < keys.size() ? upper(keys[slot]) : std::string("-"));
        }
        if (i == m_selectedRow) {
            row.markedCell = static_cast<int>(m_slot);
        }
        std::string help = "Enter sets the framed key; left and right choose another of the three; Delete clears "
                           "it.";
        if (m_slot < keys.size()) {
            help = std::string(bindings::findKey(keys[m_slot])->label) + " (key " + std::to_string(m_slot + 1) + " of 3). " + help;
        }
        row.help = std::move(help);
        page.rows.push_back(std::move(row));
    }
    page.rows.push_back({"RESET TO DEFAULTS", "", "Puts every action back on its original keys.", true});
    page.rows.push_back({"BACK", "", "", true});

    if (m_capture.active) {
        page.notice = m_notice.empty() ? "Press a key or mouse button, or turn the wheel, for " +
                                             upper(actions[m_capture.action].label) + ". Escape cancels."
                                       : m_notice;
        page.noticeIsProblem = !m_notice.empty() && m_noticeIsProblem;
    } else if (!m_notice.empty()) {
        page.notice = m_notice;
        page.noticeIsProblem = m_noticeIsProblem;
    } else if (const auto shared = bindings::sharedKeys(m_settings.bindings); !shared.empty()) {
        page.notice = "Shared keys (both actions happen): " + shared.front() +
                      (shared.size() > 1 ? " and " + std::to_string(shared.size() - 1) + " more." : ".");
    }
    return page;
}

Page SettingsFlow::page() {
    if (m_section == Section::Bindings) {
        return bindingsPage();
    }
    if (m_section == Section::Report) {
        return reportPage();
    }
    Page page;
    page.listWidth = 640.0f;
    page.notice = m_notice;
    page.noticeIsProblem = m_noticeIsProblem;
    if (m_section == Section::Hub) {
        page.id = "settings";
        page.title = "SETTINGS";
        page.rows = {
            {"DISPLAY", "", "Window, resolution, camera and field of view.", true},
            {"GRAPHICS", "", "Anti-aliasing, ambient occlusion, reflections and other effects.", true},
            {"CONTROLS", "", "Key bindings, mouse and controller.", true},
            {"GAME", "", "Renderer, game clock, menu language and cheats.", true},
            {"DIAGNOSTICS", "", "Logs for finding problems, and reporting one.", true},
            {"BACK", "", "", true},
        };
        page.initialRow = m_lastSection == Section::Graphics   ? 1
                          : m_lastSection == Section::Controls ? 2
                          : m_lastSection == Section::Bindings ? 2
                          : m_lastSection == Section::Game     ? 3
                          : m_lastSection == Section::Diagnostics || m_lastSection == Section::Report ? 4
                                                               : 0;
        return page;
    }
    const std::array<const char*, 7> titles = {"SETTINGS",     "DISPLAY", "GRAPHICS",   "CONTROLS",
                                               "KEY BINDINGS", "GAME",    "DIAGNOSTICS"};
    page.title = titles.at(static_cast<std::size_t>(m_section));
    page.id = "settings-" + page.title;
    const std::vector<Option> rows = options(m_section);
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const Option& option = rows[i];
        const bool enabled = !option.enabled || option.enabled();
        std::string help = option.help;
        if (m_section == Section::Graphics && !m_settings.gpuRenderer) {
            help += " Needs the GPU renderer (Settings > Game).";
        }
        std::string value = option.value();
        if (m_edit.active && m_edit.row == i) {
            value = m_edit.text + (std::fmod(m_time, 1.0) < 0.5 ? "_" : " ");
            help = "Type the value, then Enter. Escape keeps the old one.";
        }
        page.rows.push_back({option.label, value, help, enabled, option.adjustable});
    }
    page.rows.push_back({"BACK", "", "", true});
    if (m_section == Section::Diagnostics && page.notice.empty()) {
        if (const auto problem = core::settingsProblem(m_settings)) {
            page.notice = "Play will not start: " + *problem;
            page.noticeIsProblem = true;
        }
    }
    return page;
}

SettingsFlow::Outcome SettingsFlow::handle(MenuEvent event) {
    if (m_capture.active) {
        return Outcome::Stay;
    }
    if (m_edit.active) {
        return Outcome::Stay;
    }
    if (m_section == Section::Hub) {
        constexpr std::array kSections = {Section::Display, Section::Graphics, Section::Controls, Section::Game,
                                          Section::Diagnostics};
        if (event.row < kSections.size()) {
            open(kSections.at(event.row));
            return Outcome::Stay;
        }
        return Outcome::Back;
    }
    if (m_section == Section::Bindings) {
        const std::size_t count = bindings::actions().size();
        if (event.row < count && event.step == 0) {
            m_capture = Capture{true, event.row, ImGui::GetTime()};
            m_notice.clear();
            beginKeyCapture();
        } else if (event.row == count) {
            m_settings.bindings.clear();
            save();
        } else if (event.row == count + 1) {
            return back();
        }
        return Outcome::Stay;
    }
    if (m_section == Section::Report) {
        const std::size_t rowCount = m_summary ? 6 : 2;
        if (event.row + 1 == rowCount) {
            return back();
        }
        if (event.row == 3) {
            prepareReport();
        } else if (event.row == 4 && m_session) {
            if (auto opened = core::openFolder(*m_session); !opened) {
                m_notice = opened.error().message;
                m_noticeIsProblem = true;
            }
        }
        return Outcome::Stay;
    }
    std::vector<Option> rows = options(m_section);
    if (event.row >= rows.size()) {
        return back();
    }
    Option& option = rows[event.row];
    if (option.enabled && !option.enabled()) {
        return Outcome::Stay;
    }
    if (option.commit) {
        m_edit = Edit{true, event.row, option.value() == "OFF" ? std::string() : option.value()};
        m_notice.clear();
        return Outcome::Stay;
    }
    option.change(event.step);
    if (option.adjustable) {
        save();
    }
    return Outcome::Stay;
}

SettingsFlow::Outcome SettingsFlow::back() {
    if (m_edit.active) {
        m_edit.active = false;
        m_notice.clear();
        return Outcome::Stay;
    }
    if (m_capture.active) {
        m_capture.active = false;
        endKeyCapture();
        m_notice.clear();
        return Outcome::Stay;
    }
    switch (m_section) {
        case Section::Hub: return Outcome::Back;
        case Section::Bindings:
            m_section = Section::Controls;
            m_notice.clear();
            return Outcome::Stay;
        case Section::Report:
            m_section = Section::Diagnostics;
            m_notice.clear();
            return Outcome::Stay;
        default: open(Section::Hub); return Outcome::Stay;
    }
}

}  // namespace black::app
