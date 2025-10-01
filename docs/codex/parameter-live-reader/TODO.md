# Parameter Live Reader Refactor TODO

- [x] Create git branch `feature/parameter-live-reader` once current build finishes.
- [x] Introduce lightweight `LiveAccessor` inside `MoTool::ParameterValue` with `getStoredValue()`, `getLiveValue()`, and `hasLiveReader()`.
- [x] Update existing `attachSource`/`detachSource` logic to set and clear the live accessor while keeping current Tracktion wiring.
- [x] Provide temporary compatibility alias (`getCurrentValue()` → `getLiveValue()`) to keep callers compiling.
- [x] Audit existing call sites and plan the split between stored vs live reads (UI, automation, processing).
  - [x] NotesToPsg: use stored value for base channel updates.
  - [x] UI initialisation (LabeledSlider/ChoiceButton) now uses stored values.
  - [x] AY plugin reset uses stored values; live updates use `getLiveValue()`.
  - [x] ChipInstrumentVoice pulls envelopes via `getLiveValue()` for RT safety.
  - [x] Review remaining runtime uses (automation handlers).
  - [x] Documentation references audited (no code changes required).
- [x] Design follow-up binding class that owns the Tracktion parameter pointer and feeds the live accessor.
- [x] Prepare GUI attachment refactor to switch from direct `AutomatableParameter` awareness to the new binding.

### Work in Progress
- [x] Move `tracktion::AutomatableParameter::Ptr` ownership entirely into the binding layer so `ParameterValue` keeps only metadata, cached storage, and the live accessor hook.
- [x] Introduce `ParameterStorageTraits` (float/bool/int/EnumChoice) with `toSliderValue`/`fromSliderValue` helpers to unify UI scaling without leaking type knowledge.
- [x] Rework slider/choice attachments to use the traits and drop direct `AutomatableParameter` assertions/warnings.

---

## Context

### Problem
- `ParameterValue::getCurrentValue()` избежит задержки обновления `juce::CachedValue`, читая `tracktion::AutomatableParameter::getCurrentValue()` напрямую.
- При удалении Tracktion-зависимости из `ParameterValue` теряем доступ к live-значению, хотя аудио-процессинг ожидает актуальные значения в реальном времени.
- Существующий дизайн смешивает хранение состояния (ValueTree/Undo) и live-доступ, усложняя рефакторинг и повторное использование параметров в UI/модели без Tracktion.

### Current Architecture
- `ParameterValue<T>` хранит `ParameterDef`, `CachedValue<ValueType>` и `tracktion::AutomatableParameter::Ptr` для live-привязки.
- При привязке к автоматизируемому параметру `attachSource` вызывает `AutomatableParameter::attachToCurrentValue(value)` для синхронизации с `CachedValue` и предоставляет live-доступ через `getCurrentValue()`.
- GUI-виджеты и MIDI-мэппинг напрямую зависят от `AutomatableParameter`, вызывая `getCurrentValue()` и `parameterChangeGestureBegin/End()`.

### Proposed Solution
- Оставить `ParameterValue` чистым: хранит только ValueTree-backed `CachedValue` и метаданные.
- Добавить лёгкий live-доступ через делегат `LiveAccessor` (указатель на функцию + контекст), устанавливаемый при связывании с `AutomatableParameter`.
- `getStoredValue()` возвращает значение из `CachedValue`, `getLiveValue()` — если делегат установлен, иначе fallback к stored.
- Новый binding-слой (следующий этап) отвечает за связь с Tracktion: держит `AutomatableParameter::Ptr`, настраивает `valueToString`/`stringToValue`, устанавливает live-доступ и чистит его при `detach`/деструкции.
- UI и RT-код получают чистый API: `hasLiveReader()` для realtime, `getStoredValue()` для Undo/сериализации.
