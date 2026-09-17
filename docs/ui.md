# UI Architecture

The UI uses FreeInk immediate-mode drawing. A requested frame redraws the complete composition; there is no retained widget tree, virtual DOM, or automatic property observation. The display refresh gate still decides when a frame can be submitted. The physical panel supports four grayscale levels; the framebuffer format is a separate implementation choice.

## Rendering belongs to Views

`View<Props>` is the stateless rendering base, and `Page<Props> : View<Props>` identifies a complete page. The generic base declares `render(Canvas&, const Rect&, const Props&) const`; concrete Views override it with their own strongly typed Props. The template establishes a common rendering contract without erasing property types. Props are plain named structs, also available through each View's inherited `Props` alias. Views remain value members; this interface does not require heap allocation.

All drawing, layout, text measurement, and generation of hit regions belong to Views. Controllers must not contain FreeInk drawing or layout code. Extract child Views only when they are reusable; a page should otherwise draw its content directly with FreeInk primitives. Full-frame drawing does not require a persistent object for every visual element.

Views must not include operating-system or device-specific code, Arduino, HAL, BLE implementations, clocks, application services, routing, or controller headers. Device access belongs in controllers through the appropriate service/HAL adapter. Low-level device implementations remain in adapters, not duplicated inside controllers. The reason is practical: the same View must compile and render on the device and on a host without initializing hardware or linking native services.

Rendering reads explicit Props and bounds, writes to the supplied Canvas, and may produce a layout result. It must not mutate business state, read the current time, initiate navigation, or retain borrowed Props. Given the same Props, bounds, fonts, and drawing target configuration, output must be deterministic. This is rendering determinism, not mathematical purity: writing the Canvas and layout output is intentional.

## Controllers own state and behavior

`ViewController` provides runtime `render`, `update`, and `onInput` dispatch. `PageController : ViewController` adds route validation, enter/leave lifecycle, application association, and fullscreen policy. Its `render` method only prepares Props, calls the owned View, and records layout results; it does not perform drawing.

A concrete controller owns its View and state. State can use simple typed fields; a separate State struct is useful only when it improves clarity. Props are constructed for each render from state and external inputs, rather than stored as a second synchronized copy through property setters. Borrow large data for the duration of rendering and keep the source alive through the call. Do not add universal state setters or dynamic property dictionaries for previews.

Interactive Views provide an additional render overload accepting a concrete `RenderResult&`. The controller owns this output, including fixed-capacity FreeInk interaction buffers. The three-argument overload uses a temporary result for screenshot-only rendering. Output references avoid copying FreeInk's non-copyable interaction buffers. Interactive Views draw using a fresh local buffer and copy its bounded hit entries to the output; prior focus, flash, or press state in an output buffer cannot become a hidden drawing input. Views generate hit regions; controllers route input through them and perform the resulting actions. Clear rendered interaction state on entry/leave and when it no longer describes the visible layout.

Preserve last-rendered-layout guards, absolute logical coordinates, the status-bar content reservation, and home gesture policy. Input consumption and visual invalidation are separate concepts: request another render when visible content changes. Hardware refresh readiness remains independent of input and state updates.

## Ownership and lifecycle

```text
ApplicationManager
  owns Application instances (unique_ptr)
    owns PageControllers (value members or unique_ptr)
      owns state and Page
      optionally owns reusable child controllers and their Views
    owns Router and Navigation (borrow PageController pointers)

Shell
  owns ApplicationContainer
    owns StatusBarController
      owns sampled state and StatusBar
```

Router registers a `PageController&`; it does not own it. Navigation history stores borrowed controller pointers, and repeated entries do not clone controllers or state. Owner back-pointers are borrowed. Avoid shared ownership and cycles. Prefer value members; use `unique_ptr` when dynamic lifetime is needed. A controller must remain alive for every use by its application's router or history. Their destructors must not dereference registered controllers because derived Application members are destroyed before base members.

Leaving the foreground suspends an application; it does not promise destruction. Resident applications persist, transient applications use the existing recency cache, and lock presentation preserves the interrupted instance for restoration. Eviction or manager shutdown destroys the Application and recursively releases all its owned controllers, state, Pages, and Views. The global status bar is owned by the shell container and therefore outlives ordinary application eviction.

Stop app-owned background work on suspension. Before destruction, cancel subscriptions and timers and quiesce in-flight callbacks while their state is still valid. RAII handles owned memory but cannot make a callback into a deleted object safe. No generic asynchronous task framework is implied by this architecture.

### Mandatory destruction contract

Application destruction is the lifetime boundary for every application-owned resource. When destruction completes, all owned Pages, Views, controllers, state, navigation storage, subscriptions, timers, task resources, and native handles must have been destroyed or released exactly once. No application-owned resource may survive through an orphan allocation, owning global registry, reference cycle, or outstanding callback. Borrowed platform services and the shell-owned status bar are outside this ownership boundary.

Every new resource must have an explicit owner and a deterministic cleanup path. Use value members or RAII owners, including suitable deleters for native handles. Cancel and drain asynchronous work before destroying anything it can access. Do not rely on a later application launch, process termination, or an `onLeave()` call alone to reclaim resources. Construction failures must also release resources already acquired.

Foreground departure is suspension, not this destruction boundary: cached and resident instances intentionally keep their owned UI alive. This retention is not a leak. Transient eviction and manager shutdown invoke the destruction contract; returning Home or switching applications does not necessarily do so.

Lifecycle tests must verify retention during suspension, exactly-once cleanup on eviction, and zero remaining owned instances after manager shutdown. `tests/ApplicationManagerTest.cpp` checks live controller, View, state, and resource counts and matches controller construction/destruction counts. Extend these checks for new resource types and asynchronous integrations. These tests establish the covered ownership behavior; they are not a blanket proof that future native or asynchronous code cannot leak.

## Preview and performance

Route previews exercise the real Application, Router, controller lifecycle, and full composition through host hardware adapters. The CLI supplies current local time by default, supports a fixed `--time HH:MM`, and accepts battery percentages such as `--battery 85` and charging state via `--battery-charging`. Time is sampled outside Views and injected through the host adapter; View rendering remains deterministic for fixed inputs. Device-dependent controllers need host-capable adapters for this path.

Isolated View previews compile only rendering sources, FreeInk, fonts, typed C++ examples, and framebuffer export utilities. They do not compile controllers, applications, Router, HAL, or Arduino stubs. A controller can therefore acquire a new device-only dependency without breaking isolated screenshots. Examples provide Props and bounds directly; they never implement a second version of the UI. Page examples do not implicitly draw a status bar.

Keep the two build targets independently cached. Warm screenshots should complete in under one second on the development host; measure end-to-end CLI latency separately from cold compilation. Reuse unchanged translation units, font objects, and SDK objects within each target. Avoid per-frame heap allocation, large input copies, and unnecessary rendering; splitting responsibilities does not require a reactive framework.

See the [UI code map](platform/ui.md) for implementation entry points, [runtime map](platform/runtime.md) for ownership and navigation, and [Preview CLI](../tools/preview/README.md) for commands.
