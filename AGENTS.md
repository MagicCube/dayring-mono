# Development Guidelines

These guidelines apply to development and code reviews throughout this repository.

## Commit Messages

- Use semantic commit prefixes such as `feature:`, `fix:`, and `chore:`.
- Group changes into focused commits by responsibility.

## Architecture Context

Read [Font principles and usage](docs/platform/fonts.md) before choosing fonts or changing typography, font assets, or slots.

Read [UI architecture and design principles](docs/ui.md) before UI work. Views own all rendering and must not depend on operating-system or device code; controllers own state, behavior, and service access.

Read [PaperMono hardware constraints](docs/platform/hardware.md) before display, touch, or power work. The panel supports **2-bit grayscale (four levels)**; do not infer panel capability from the current framebuffer format.

## Display Hardware Contract

- **The device has a 480 × 800 portrait e-ink display with 2-bit grayscale (four levels: black, dark gray, light gray, and white).** Use this geometry and color capability when designing pages, boot screens, images, and previews.
- The driver's native 800 × 480 buffer layout is an implementation detail; it does not change the product's 480 × 800 portrait UI orientation.
- A 1-bit framebuffer or B/W upload path is a software implementation choice, not a hardware limitation. Never describe this panel as supporting only black and white.
- When a design calls for gray, inspect and use the SDK grayscale upload contract. Do not silently replace gray with white or B/W dithering because an existing rendering path uses a 1-bit buffer.

## Code Maps

Use these code maps to locate implementation; load only the relevant map, then read the listed symbols. Paths are repository-relative. Update maps when responsibilities or entry points move; `plans/` may be outdated.

| Search topic | Code map |
| --- | --- |
| Ownership, lifecycle, URLs, navigation, retention | [Runtime](docs/platform/runtime.md) |
| Facade, home, lock/restore, input and power policy | [Shell](docs/platform/shell.md) |
| Pages, composition, invalidation, rendering and hit testing | [UI](docs/platform/ui.md) |
| Font slots, character coverage, generation and registration | [Fonts](docs/platform/fonts.md) |
| Hardware ownership, initialization and refresh completion | [HAL](docs/platform/hal.md) |

## UI Design Principles

- The visual direction is Nothing + Teenage Engineering: restrained, functional, instrument-like interfaces with deliberate typography, precise alignment, strong hierarchy, and generous negative space.
- Translate this direction through the existing font system: Roboto for readable labels and text, Ndot for clocks, large numbers, and short display accents. Follow `docs/platform/fonts.md`; do not introduce decorative fonts merely to imitate a brand.
- Prefer uppercase for short utility labels and compact date metadata when it improves the composition. Do not apply uppercase indiscriminately to body text or long titles.
- Make spacing and alignment systematic, and give each screen one clear visual focus. Preserve stable positions when optional status information appears or disappears.
- Keep decoration purposeful. Avoid ornamental technical labels, fake model numbers, gratuitous borders, and icons or indicators that do not convey useful state.
- Treat these brands as a design direction, not a requirement to reproduce their products. Evaluate the actual page on the monochrome display and prioritize legibility and function.

## Documentation and Comment Language

- All Markdown documents (`*.md`) must be written in English.
- Use English when creating or updating documentation, including headings, explanations, tables, and comments in examples.
- Code comments, when present, must be written in English. Comments are optional; add them only when they provide useful context or explain non-obvious intent.

## C++ Standard and Style

- Use C++20 and prefer clear, modern idioms. Avoid adding complexity solely to use a newer language feature.
- Manage resources with RAII and make ownership explicit. Prefer value semantics and standard library containers; use smart pointers when dynamic ownership is necessary.
- Use `const`, `constexpr`, `enum class`, `[[nodiscard]]`, range-based `for` loops, and standard library algorithms where appropriate. Adopt C++20 features such as concepts, ranges, and `std::span` when they improve clarity.
- Treat the root `.clang-format` as the single source of truth for formatting. Do not introduce conflicting formatting rules.
- The current configuration uses Google style, four-space indentation, no tabs, a 120-column limit, left-aligned pointer symbols, attached opening braces, and no single-line short functions. If the configuration changes, follow `.clang-format`.
- Format modified C++ files with a C++20-compatible clang-format using the repository configuration. Avoid broad formatting changes to unrelated code.

## Recommended Size Limits

These are maintainability guidelines, not rigid limits. Count blank lines and comments. Do not compress code, remove useful comments, or introduce meaningless abstractions merely to meet a line count.

| Scope | Recommended target | Suggested upper limit and action |
| --- | --- | --- |
| Header file (`.h`) | At most 200 lines | Above 300 lines, consider splitting declarations by responsibility |
| Implementation file (`.cpp`, etc.) | At most 400 lines | Above 600 lines, consider extracting independent modules |
| Class or struct definition | At most 150 lines | Above 250 lines, consider separating responsibilities; count the definition itself, including inline implementations |
| Function, member method, or lambda | At most 40 lines | Above 60 lines, prefer extracting helpers with clear semantics; count the complete definition |

- Consider refactoring even below these limits when code has multiple responsibilities, deep nesting, or duplicated logic.
- Assess a class's overall responsibilities even when its member implementations span multiple files. A short class definition alone does not establish that the class has a manageable scope.
- Exceptions are reasonable for generated code, third-party code, declarative data tables, and code whose readability would clearly suffer from splitting. Briefly explain handwritten code exceeding the suggested upper limits in the change description.

## Header File Extension

- Use `.h` for C++ headers, not `.hpp`.
- Follow this convention for new headers and their corresponding `#include` paths.
- Do not perform unsolicited bulk renames of existing files or modify third-party files solely to satisfy this convention. Handle migrations as explicitly scoped tasks.

## Application and Page Naming

- All application class names must end with `Application`.
- All page View class names must end with `Page`, and page controller names must end with `PageController`.
- Reusable Views may use descriptive names such as `StatusBar`; their controllers end with `Controller`.
- Keep corresponding header and implementation filenames consistent with their class names.

## Private Member Naming

- Prefix all private data members and ordinary member functions with a single underscore followed by a lowercase letter, such as `_name` and `_refreshState()`. This includes static members and private constants.
- Use the prefix instead of a trailing underscore: write `_name`, not `name_` or `_name_`.
- Apply the same convention to private members in test classes. Access is determined by the member's declaration, including implicit private access in classes.
- Constructors, destructors, and operators retain their required C++ names. Overrides retain the inherited method name so they continue to implement the base interface.
- Nested types and type aliases retain their type naming conventions. Public and protected members do not receive this prefix.
- Never introduce a name beginning with an underscore followed by an uppercase letter, or containing a double underscore; these identifiers are reserved. Do not use underscore-prefixed names in the global namespace.
- Apply these rules to project-owned code; do not rename members in external SDKs or third-party dependencies.

## Property Accessor Naming

- Name non-boolean property readers after the property itself, such as `name()`. Do not use `get_name()` or `getName()`.
- Name boolean state readers with `is` followed by the capitalized state, such as `isLocked()` rather than `locked()`. Use natural predicate names such as `hasChildren()` or `canPop()` when they express existence or capability.
- Name property writers with `set` followed by the capitalized property name, such as `setName()`. Do not use `set_name()`.
- Apply the same rule to multiword properties, such as `displayName()` / `setDisplayName()`.
- Declare read-only accessors `const` when semantically appropriate. Choose return and parameter types based on ownership, lifetime, and copying cost.
- This convention applies to property accessors; ordinary behavior methods do not need to follow the property naming pattern.
- For private accessors, apply these rules after the leading underscore, such as `_name()`, `_isLocked()`, and `_setName()`.

```cpp
class Item {
public:
    [[nodiscard]] const std::string& name() const;
    void setName(std::string name);
    [[nodiscard]] bool isLocked() const;
    void setLocked(bool locked);

   private:
    void _refreshState();
    std::string _name;
    bool _isLocked = false;
};
```

## UI Verification

For UI changes, capture the affected real page and visually inspect the PNG before finishing:

```sh
make preview "app://typography/?article=display"
```

`make preview` without a URL captures `app://shell/`. For options, use `./tools/preview/preview capture --help`; discover shared firmware routes with `routes`, and page parameters with `help <application>`.

Show the resulting screenshot to the user in the response using a Markdown image with its absolute filesystem path, for example `![Typography preview](/absolute/project/path/.preview/typography--article-display.png)`. A tool-only image inspection or a plain file link does not satisfy this requirement. Re-capture after the final UI change so the displayed image matches the current code.

The launcher uses the project's PlatformIO Python. `.preview/` contains PNGs only; filenames identify the application, omit the root page, and include a readable query without random/hash suffixes. Repeated captures overwrite the same screenshot, including device-state changes. Build artifacts, dependency/object caches, Python bytecode, and temporary work live in `.cache/preview/`. Both directories are Git-ignored. See [Preview CLI](tools/preview/README.md) for options and `make test-preview`. Keep route definitions and parameter behavior shared with firmware.
