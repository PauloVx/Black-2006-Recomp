#pragma once

#include "app/Widgets.h"

#include <cstddef>

namespace black::app {

/// A set of pages reached from the main menu (settings, saves). Each frame the launcher calls update(), draws
/// page() and passes back what the player chose.
class MenuFlow {
public:
    enum class Outcome { Stay, Back };

    MenuFlow() = default;
    MenuFlow(const MenuFlow&) = delete;
    MenuFlow& operator=(const MenuFlow&) = delete;
    MenuFlow(MenuFlow&&) = delete;
    MenuFlow& operator=(MenuFlow&&) = delete;
    virtual ~MenuFlow() = default;

    /// `selectedRow` is the menu's current selection.
    virtual void update(double time, std::size_t selectedRow) = 0;
    [[nodiscard]] virtual Page page() = 0;
    virtual Outcome handle(MenuEvent event) = 0;
    /// Escape: leaves what is in progress, or the page.
    virtual Outcome back() = 0;
    /// Reading a key or typed text: the menu keys are not the menu's meanwhile.
    [[nodiscard]] virtual bool busy() const = 0;
    /// Whether Escape can leave now (not while a build runs, say).
    [[nodiscard]] virtual bool canLeave() const { return true; }
};

}  // namespace black::app
