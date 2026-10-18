#include "app/NativeDialogs.h"

#include "core/Process.h"
#include "core/Text.h"

#include <GLFW/glfw3.h>

#include <mutex>

#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <shobjidl.h>
#include <windows.h>
#else
#include <array>
#include <cstdio>
#endif

#include <utility>

namespace black::app {
namespace {

std::mutex g_dropMutex;
std::vector<std::filesystem::path> g_dropped;

void onDrop(GLFWwindow* /*window*/, int count, const char** paths) {
    const std::scoped_lock lock(g_dropMutex);
    for (int i = 0; i < count; ++i) {
        g_dropped.push_back(core::pathFromUtf8(paths[i]));
    }
}

#ifdef _WIN32
template <typename T>
struct ComPtr {
    T* p = nullptr;
    ComPtr() = default;
    ComPtr(const ComPtr&) = delete;
    ComPtr& operator=(const ComPtr&) = delete;
    ~ComPtr() {
        if (p != nullptr) {
            p->Release();
        }
    }
};

std::optional<std::filesystem::path> showOpenDialog(GLFWwindow* owner, bool folders) {
    const HRESULT init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    const bool uninitialize = SUCCEEDED(init);
    std::optional<std::filesystem::path> chosen;
    {
        ComPtr<IFileOpenDialog> dialog;
        if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog.p)))) {
            FILEOPENDIALOGOPTIONS options = 0;
            dialog.p->GetOptions(&options);
            dialog.p->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST |
                                 (folders ? FOS_PICKFOLDERS : FOS_FILEMUSTEXIST));
            if (folders) {
                dialog.p->SetTitle(L"Choose the folder with your BLACK disc's files");
            } else {
                const COMDLG_FILTERSPEC filters[] = {{L"Xbox disc images (*.iso, *.xiso)", L"*.iso;*.xiso"},
                                                     {L"All files", L"*.*"}};
                dialog.p->SetFileTypes(2, filters);
                dialog.p->SetTitle(L"Choose your BLACK disc image");
            }
            HWND window = owner != nullptr ? glfwGetWin32Window(owner) : nullptr;
            ComPtr<IShellItem> item;
            if (SUCCEEDED(dialog.p->Show(window)) && SUCCEEDED(dialog.p->GetResult(&item.p))) {
                PWSTR path = nullptr;
                if (SUCCEEDED(item.p->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
                    chosen = std::filesystem::path(path);
                    CoTaskMemFree(path);
                }
            }
        }
    }
    if (uninitialize) {
        CoUninitialize();
    }
    return chosen;
}
#else
std::optional<std::filesystem::path> runPicker(const char* command) {
    FILE* pipe = popen(command, "r");
    if (pipe == nullptr) {
        return std::nullopt;
    }
    std::string output;
    std::array<char, 512> buffer{};
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        output += buffer.data();
    }
    const int status = pclose(pipe);
    while (!output.empty() && (output.back() == '\n' || output.back() == '\r')) {
        output.pop_back();
    }
    if (status != 0 || output.empty()) {
        return std::nullopt;
    }
    return core::pathFromUtf8(output);
}

std::optional<std::filesystem::path> showOpenDialog(bool folders) {
    const char* zenity =
        folders ? "zenity --file-selection --directory --title='Choose the folder with your BLACK disc' 2>/dev/null"
                : "zenity --file-selection --title='Choose your BLACK disc image' "
                  "--file-filter='Xbox disc images | *.iso *.xiso *.ISO *.XISO' 2>/dev/null";
    const char* kdialog = folders ? "kdialog --getexistingdirectory . 2>/dev/null"
                                  : "kdialog --getopenfilename . 'Xbox disc images (*.iso *.xiso)' 2>/dev/null";
    if (auto chosen = runPicker(zenity)) {
        return chosen;
    }
    return runPicker(kdialog);
}
#endif

}  // namespace

std::optional<std::filesystem::path> chooseDiscImage(GLFWwindow* owner) {
#ifdef _WIN32
    return showOpenDialog(owner, false);
#else
    (void)owner;
    return showOpenDialog(false);
#endif
}

std::optional<std::filesystem::path> chooseFolder(GLFWwindow* owner) {
#ifdef _WIN32
    return showOpenDialog(owner, true);
#else
    (void)owner;
    return showOpenDialog(true);
#endif
}

void showError(const std::string& title, const std::string& message) {
#ifdef _WIN32
    MessageBoxW(nullptr, core::pathFromUtf8(message).c_str(), core::pathFromUtf8(title).c_str(), MB_OK | MB_ICONERROR);
#else
    std::fprintf(stderr, "%s: %s\n", title.c_str(), message.c_str());  // NOLINT(cppcoreguidelines-pro-type-vararg)
    const core::Result<bool> shown =
        core::startDetached("zenity", {"--error", "--title=" + title, "--text=" + message}, {});
    (void)shown;
#endif
}

void acceptDroppedFiles(GLFWwindow* window) {
    glfwSetDropCallback(window, onDrop);
}

std::vector<std::filesystem::path> takeDroppedFiles() {
    const std::scoped_lock lock(g_dropMutex);
    return std::exchange(g_dropped, {});
}

}  // namespace black::app
