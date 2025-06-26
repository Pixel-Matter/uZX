# TE + JUCE Design Pattern

## What can we call this TE + JUCE design pattern?

It’s not a textbook MVC or MVVM. It blends ideas:

- Observable **model tree** (`ValueTree`) that everything revolves around.
- UI **components** that bind to the model via listeners.
- Optional ViewState for pure UI needs.
- Logic is either in model-manipulating functions (like `track->setMute()`) or distributed via component callbacks.

So here’s a candidate name for the pattern:

> **OVM** – *Observable-View-Model*

> - **Observable**: Everything reacts to changes in a shared observable model (ValueTree).
> - **View**: JUCE components render things based on current model + UI state.
> - **Model**: Tracktion’s `Edit`, `Track`, `Clip` — the business logic and data layer.

Or optional extension: **OVVM** — Observable-View-ViewModel (if you distinguish ViewState/UI-state from the actual business model, like `EditViewState`).

---

## Comparison Table: JUCE vs MVC / MVP / MVVM

| Aspect                        | **JUCE (OVM/OVVM)**           | **MVC**                          | **MVP**                             | **MVVM**                            |
|-------------------------------|------------------------------------|----------------------------------|-------------------------------------|-------------------------------------|
| **Model**                     | `ValueTree` + Engine model classes | Data model, passive              | Data model                          | Data model                          |
| **View**                      | JUCE Components                    | View + event handlers            | View + event handlers               | UI bound to ViewModel               |
| **Controller / Presenter**    | Mostly implicit — logic lives in components or model helpers | Explicit controller acts on model | Presenter intermediates between View and Model | ViewModel exposes observables for View |
| **UI ↔ Model binding**        | Via `ValueTree::Listener`          | Via controller                   | View calls Presenter methods        | Data binding (two-way)              |
| **UI state (zoom, selection)**| ViewState / `CachedValue`          | Often in controller or local     | In Presenter                        | In ViewModel                        |
| **Event flow**                | Edit changes → listeners update UI | UI → Controller → Model          | UI → Presenter → Model              | Model ↔ ViewModel ↔ View            |
| **Encapsulation**             | Semi-leaky (UI knows VT directly)  | Strict (controller isolates)     | Strict (presenter isolates)         | Strict (view talks to ViewModel)    |
| **Undo/Redo**                 | Handled by `UndoManager` in model  | Often custom in controller       | Presenter controls undo             | Usually in Model layer              |
| **Good for complex UIs?**     | Yes — if structured well           | Gets messy at scale              | OK, but verbose                     | Excellent when bindings are available |
| **Coupling**                  | Medium — via ValueTree + ptrs      | Medium to high                   | Lower                               | Lowest                              |

---

## Summary

The Tracktion Engine + JUCE combo implements something close to **MVVM**, but with:
- the ViewModel being a `ValueTree` structure (possibly shared);
- logic not centralized in a Presenter, but spread between model and View;
- optional “ViewState” as a semi-ViewModel.

Because of this hybrid, it doesn't fit neatly into one box — but calling it **OVM (Observable-View-Model)** captures its essence better than trying to shoehorn it into MVC or MVVM.

If you're building a serious DAW on top of this, it’s worth embracing this architecture instead of fighting it — and wrapping your own domain logic around ValueTrees to enforce clean separation when needed.

Want me to sketch an ideal project skeleton or class graph for a TE-based DAW to go with this pattern?