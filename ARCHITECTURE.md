# ModularTrigger — architecture notes

Working notes for picking this plugin back up quickly. Every claim here was read out of the source;
line numbers drift, so the symbol names are the reliable part.

Build commands, engine path and code conventions are in the project `CLAUDE.md`, not here.

## Modules

| Module | Type | Loading phase | Deps |
|---|---|---|---|
| `ModularTrigger` | Runtime | Default | public: `Core CoreUObject Engine WorldConditions`, private: `LevelSequence MovieScene Slate SlateCore` |
| `ModularTriggerEditor` | UncookedOnly | **PreDefault** | private: `ApplicationCore Core CoreUObject DeveloperSettings Engine InputCore Settings Slate SlateCore ToolMenus UnrealEd PropertyEditor ModularTrigger` |

The runtime module has no gameplay-framework deps beyond `Engine` — no GAS, no tags, no StateTree. Keep
it that way; everything project-specific lives in project-side module subclasses. `LevelSequence` /
`MovieScene` are the one exception, taken deliberately for `PlayLevelSequence`: both are engine runtime
modules rather than plugins, so they cost nothing to enable, but they are the precedent to point at if the
dependency list starts growing — the bar is "engine-generic", not "useful".

`WorldConditions` is a second deliberate exception, for `WorldConditionFilter`. It **is** a plugin, so
`ModularTrigger.uplugin` now carries a `Plugins` dependency on it (the project already enabled it for
SmartObjects). It clears the bar: it is an engine condition-expression framework with no gameplay opinions,
and it is a `Public` dep because `TriggerComponentModule_WorldConditionFilter.h` stores an
`FWorldConditionQueryDefinition` and the condition structs are public extension points.

`PreDefault` on the editor module is load-bearing: it can run **before `GUnrealEd` exists**, which is why
`FModularTriggerEditorModule::StartupModule` registers the component visualizer through
`FCoreDelegates::OnPostEngineInit` when `GUnrealEd` is null, and immediately otherwise (a hot reload never
sees that delegate fire). Don't "simplify" that to a bare `if (GUnrealEd)`.

## The object graph

```
AActor
└── UHierarchicalTriggerComponent : UActorComponent        (no tick; tick explicitly disabled in ctor)
    └── Modules : TArray<TObjectPtr<UHierarchicalTriggerComponentModule>>   <- depth 0
         └── each module owns its own Modules array                        <- depth 1, 2, ...
```

- `Modules` is `EditAnywhere, Instanced`. Every module is outered to whatever holds its array — the
  component for roots, the parent module for children. `GetOwningComponent()` is
  `GetTypedOuter<UHierarchicalTriggerComponent>()`, `GetOuterModule()` is
  `GetTypedOuter<UHierarchicalTriggerComponentModule>()`.
- `UHierarchicalTriggerComponentModule` is `UCLASS(DefaultToInstanced, EditInlineNew, CollapseCategories,
  Abstract, HideDropdown)`. **`HideDropdown` is why the details customization exists** — the stock
  instanced-object row would offer no class picker at all.
- **Null array entries are legal and expected.** The details panel adds an empty slot first and the row's
  class combo fills it. Every traversal skips with `IsValid(Module)`.
- `UWorld` comes from the outer chain: `UHierarchicalTriggerComponentModule::GetWorld()` is
  `GetOuter()->GetWorld()`.

### Key files

| Thing | File |
|---|---|
| Component: module array, ids, save hook | `ModularTrigger/Public\|Private/HierarchicalTriggerComponent.{h,cpp}` |
| Module base + `FTriggerExecutionContext` | `ModularTrigger/Public/Modules/HierarchicalTriggerComponentModule.h`, `Private/Modules/…cpp` |
| Shared tree walks (find / begin / end / save) | `ModularTrigger/Public\|Private/HierarchicalTriggerModuleUtils.{h,cpp}` |
| Stable reference to one module | `ModularTrigger/Public\|Private/TriggerModuleReference.{h,cpp}` |
| Authored call to a BP event on an actor | `ModularTrigger/Public\|Private/ActorEventCall.{h,cpp}` |
| Editor-only declarative draw API | `ModularTrigger/Public\|Private/TriggerModuleVisualization.{h,cpp}` |
| Module tree UI (the big one, ~1080 lines) | `ModularTriggerEditor/Private/HierarchicalTriggerComponentDetails.{h,cpp}` |
| Viewport visualizer + hit proxy | `ModularTriggerEditor/Private/HierarchicalTriggerComponentVisualizer.{h,cpp}` |
| Per-user visualization settings | `ModularTriggerEditor/Private/ModularTriggerEditorSettings.{h,cpp}` |
| Module picker for `FTriggerModuleReference` | `ModularTriggerEditor/Private/TriggerModuleReferenceCustomization.{h,cpp}` |
| Event picker + parameter bag sync | `ModularTriggerEditor/Private/ActorEventCallCustomization.{h,cpp}` |

### Built-in modules

| Class | Display name | Role |
|---|---|---|
| `UTriggerComponentModule_OverlapEvent` | Generate Overlap Event | **Event source.** One picked `UPrimitiveComponent`. |
| `UTriggerComponentModule_ActorOverlapEvent` | Generate Actor Overlap Event | **Event source.** Every primitive component on the owner, aggregated per actor. |
| `UTriggerComponentModule_Filter` | Filter | Gates its own children on whether the context passes its entries. **Superseded**, see below. |
| `UTriggerComponentModule_WorldConditionFilter` | World Condition Filter | Same gate, expressed as a `FWorldConditionQueryDefinition`. |
| `UTriggerComponentModule_CallActorEvent` | Call Actor Event | Invokes an `FActorEventCall`, context actor = trigger owner. |
| `UTriggerComponentModule_PlayLevelSequence` | Play Level Sequence | Drives a placed `ALevelSequenceActor`'s player. Also a line-drawing visualization example. |
| `UTriggerComponentModule_DebugMessage` | Debug Message | On-screen message + log; also the text-only visualization example. |
| `UTriggerComponentModule_Blueprint` | Trigger Component Module Blueprintable | `Blueprintable, Abstract` base for BP modules. |

### The World Conditions filter

`UTriggerComponentModule_WorldConditionFilter` is the `Filter` replacement, built on the engine's
`WorldConditions` plugin. Three pieces:

| Piece | File | What it is |
|---|---|---|
| Schema | `Public\|Private/WorldConditions/TriggerWorldConditionSchema.{h,cpp}` | What conditions may read, and which conditions may be authored. |
| Conditions | `Public\|Private/WorldConditions/TriggerWorldConditions.{h,cpp}` | The five ports of the old `FFilterModuleEntry` types. |
| Module | `Public\|Private/Modules/TriggerComponentModule_WorldConditionFilter.{h,cpp}` | Owns the query, drives its lifecycle, answers `ShouldPropagateEvent`. |

Context data declared by `UTriggerWorldConditionSchema` — the **names are saved data**, a rename unresolves
every reference pointing at it:

| Name | Type | Availability |
|---|---|---|
| `TriggerComponent` | `UHierarchicalTriggerComponent` | Persistent |
| `TriggerActor` | `AActor` | Persistent |
| `OtherActor` | `AActor` | Dynamic |
| `OtherComponent` | `UPrimitiveComponent` | Dynamic |
| `Event` | `FTriggerWorldConditionEvent` (just `bEndOverlap`) | Dynamic |

Persistent vs. Dynamic is not decoration: `FWorldConditionBase::bCanCacheResult` **defaults to true**, so a
condition reading per-event data has to clear it in `Initialize` or it will answer with the first event's
result forever. Every condition here derives that from the type of the data it reads.

Lifecycle, in the module:

- ctor and `PostLoad` — `Condition.SetSchemaClass()` + `Condition.Initialize(this)`. The ctor matters because
  **`PostLoad` does not run on a newly created module**, and the condition picker reads the schema off the
  definition, so a module added in the editor would otherwise offer an empty list. In the editor
  `FWorldConditionQueryDefinitionDetails` re-initializes on every edit; only the schema change is ours.
- `OnBeginPlay` — `ConditionState.Initialize(*this, Condition)` then `Activate()` with the persistent context
  data only (dynamic data is documented as unavailable there). This is safe only because `ReceiveBeginPlay`
  is post-order (see Lifecycle): a generator's initial sweep dispatches from *its* `OnBeginPlay`, by which
  time every filter under it is already activated.
- `ShouldPropagateEvent` — build a throwaway `FWorldConditionContextData`, fill it, `IsTrue()`.
- `OnEndPlay` — `Deactivate()`, which also `Free()`s the state, so conditions unregister before the memory
  their invalidation handles point at goes away.

**The failure modes are the reason the lifecycle is written that way.** An *initialized but empty* state
answers true (empty query passes); an *uninitialized* state answers false. A query that fails to link —
which is what a condition returning false from `Initialize` causes, e.g. a context data reference left
unset — never initializes, so the filter rejects everything and the engine logs the error. Fail-closed is
deliberate: a filter that cannot evaluate must not let events through.

`FWorldConditionContextData` and `FWorldConditionContext` are documented as not storable, and the container
heap-allocates its view array, so there is one small allocation per event. Caching them would mean holding
non-copyable types across frames against the engine's own guidance; if it ever shows up in a profile, the
fix is upstream.

### Project-side modules (`Source/TheEmperorsOwnRuntime`)

`TriggerModules/` holds `BroadcastDelegate`, `ForcePlayerTurn`, `ModifyGameplayTags`,
`NiagaraActorAction`, `NotifyFlowAsset`, `StartFlowAsset`, `WorldStateMutation`, `WorldStateQuery`.
`AI/Components/TriggerModules/` holds `GetTargets`.

Project-side **Filter entries** all live in one pair, `TriggerModules/FilterModuleEntries.{h,cpp}` —
`FFilterModuleEntry_PlayersVehicle` and `FFilterModuleEntry_WorldStateQuery`. Add new ones there rather
than one file per entry; they are a few lines each and `ExcludeBaseStruct` picks them up from the reflection
data with no registration step.

`FFilterModuleEntry_WorldStateQuery` deliberately duplicates the condition in
`UTriggerComponentModule_WorldStateQuery`: the module for when the query is the whole reason a subtree
exists, the entry for when it is one condition among several on one Filter. Both are tied to the old
`Filter`; the eventual third form is a `FTriggerWorldConditionBase` subclass, project-side, which would
serve both roles at once.

Also project-side: `AHierarchicalTriggerActor` (`Actors/Triggers/`),
`UTEOHierarchicalTriggerComponent` (`Components/TriggerComponent/`, an empty subclass that only adds
`IEMSObjectSaveInterface` for EasyMultiSave), and `UFlowNode_ListenForTrigger` (`FlowNodes/Gameplay/`).

## Event dispatch — exact order

This is the part to get right; everything surprising about the plugin lives here.

```cpp
void UHierarchicalTriggerComponentModule::ReceiveTriggerEvent(FTriggerExecutionContext& Ctx)
{
    if (bEnabled)                       { OnTriggerEventReceived(Ctx); }
    const bool bGateOpen = !bEnabled || ShouldPropagateEvent(Ctx);
    if (bChildrenEnabled && bGateOpen)  { PropagateEvent(Ctx); }
    if (bTriggerOnce)                   { bEnabled = false; bChildrenEnabled = false; }
}

void UHierarchicalTriggerComponentModule::PropagateEvent(FTriggerExecutionContext& Ctx)
{
    const FTriggerExecutionDataScope DataScope(Ctx);
    for (Module : Modules) if (IsValid(Module)) Module->ReceiveTriggerEvent(Ctx);
}
```

**Nothing dispatches to root modules.** The component never calls `ReceiveTriggerEvent`. An event source
module calls `PropagateEvent` on **its own children** directly from its overlap handler. Consequences:

1. A generator must be an **ancestor** of everything it drives.
2. A root-level non-generator module **never receives a trigger event at all** — only `OnBeginPlay` /
   `OnEndPlay`.
3. A generator's own `OnTriggerEventReceived` is never called, so its `bEnabled` is **dead** — only
   `bChildrenEnabled` gates it. Unticking "Enabled" on a Generate Overlap Event module does nothing.
   (The generators re-implement the `bTriggerOnce` clear themselves inside `HandleOverlap`, and both call
   `ShouldPropagateEvent` on themselves before `PropagateEvent` so the gate is honoured on every path.)

### The propagation gate

A module decides about its **own children and nothing else**, by overriding

```cpp
virtual bool ShouldPropagateEvent(const FTriggerExecutionContext& Ctx) const;
```

Asked once per event, **after** `OnTriggerEventReceived` and only when `bEnabled` — a disabled module is a
pass-through, so a disabled Filter does not block anything. A module that needs to do work before it can
decide does that work in the handler and answers from what it recorded there; that is what
`UTriggerComponentModule_Blueprint` does with `bPropagationStopped`, which its BP graph sets through
`StopPropagationToChildren()` and which is cleared at the **start** of the next handler, not the end of the
last one.

The gate is a return value, not a flag on the context, so the blast radius is structural: there is nothing
for a rejecting module to leave set, and the modules beside it cannot be affected. **A Filter still has to
be the parent of what it guards** — as a sibling it now simply does nothing to its siblings.

There was a `bPendingInterruption` sticky bool on the context until 2026-10-12. It leaked: a Filter's
rejection skipped the children of every *later* sibling too, while those siblings' own work still ran. If
you find `SetPendingInterruption` / `IsPendingInterruption` referenced anywhere, it is stale — the only
survivor is a deprecated BP forwarder on `UTriggerComponentModule_Blueprint`.

## The three module flags

All three are `UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)` on the base, surfaced by the details
customization as row toggles ("✓", "Children", "Once") rather than as details rows.

| Flag | Meaning |
|---|---|
| `bEnabled` | Run this module's own `OnTriggerEventReceived`. Dead on generator modules (see above). |
| `bChildrenEnabled` | Descend into children. On a generator this is the real on/off switch. |
| `bTriggerOnce` | After dispatch, clear **both** of the above. Persisted by save game. |

`bTriggerOnce` mutates authored state at runtime. Because the three flags are the only things the save
system writes (below), a `bTriggerOnce` module that has fired stays fired across a save/load.

## `FTriggerExecutionContext`

A plain struct (not a `USTRUCT`), stack-allocated by the generator and passed by mutable reference down
the whole walk. Holds `TNotNull<const …*>` for the owning component, the overlapped component and the
other component, plus an optional `const FHitResult*` **borrowed from the engine's delegate frame** —
valid only for the duration of the dispatch, never store it.

### Execution data

`ExecutionData` is an `FInstancedStructContainer` and the only channel between modules — e.g. `GetTargets`
writes an `FActorsArray` that a later module reads. It is **scoped like a block scope**, not flat:

- `PropagateEvent` opens an `FTriggerExecutionDataScope` around a module's child list, so **one sibling
  list shares one scope**. What a child writes is readable by the children after it and by all of their
  descendants, and is destroyed when that list has been walked.
- **Reads** (`GetExecutionDataPtr<T>()`) search innermost-first: own scope, then outwards through
  ancestors. Nearest value wins.
- **Writes** (`AddExecutionData`, `GetOrAddExecutionData<T>()`) only ever touch the current scope. A value
  of the same type in an outer scope is **shadowed, never written through** — a nested module cannot change
  what its ancestors are still reading.
- `GetOrAddExecutionData<T>()` creates its scope's value as a **copy of the nearest visible outer value**
  (default constructed if there is none), so it reads like "give me a writable copy of what I can see".
  That is what makes `GetTargets`'s `bAppend` a one-liner. The returned reference is **invalidated by any
  later add on the same context**, including one made further down the tree — the values share one
  contiguous block that moves as it grows.
- Unique by type **per scope**, so the same type can legitimately appear more than once in the container.
  Anything iterating it by hand (the BP module's `InExecutionData` copy) must go back to front.

Before 2026-10-12 the container was flat and lived for the whole dispatch, which meant a `GetTargets` deep
in one branch was still visible to an unrelated branch that ran later. Scoping is what fixed that.

The context is a stack struct, so the `UObject` references inside execution data are **not** reachable by
GC. That is fine for a synchronous dispatch; do not stash a context or its data anywhere.

Cross-event state does **not** belong here — the context dies with the event. It would belong on the
component (and would then need `SaveGame` treatment); nothing needs it yet.

Accessors are inline in the header except `GetOtherActor()`, which is out of line because
`UPrimitiveComponent` is only forward declared there. Keep it that way.

## Lifecycle

`UHierarchicalTriggerComponent::BeginPlay` calls `FHierarchicalTriggerModuleUtils::ReceiveBeginPlay(Modules)`
**before** `Super::BeginPlay()`. `EndPlay` likewise runs modules before `Super`.

`ReceiveBeginPlay` / `ReceiveEndPlay` on a module are **depth-first, post-order** — children first, then
self:

```cpp
void ReceiveBeginPlay() { FHierarchicalTriggerModuleUtils::ReceiveBeginPlay(Modules); OnBeginPlay(); }
```

That ordering is deliberate and load-bearing: a generator's `OnBeginPlay` performs an initial overlap
sweep that immediately propagates events, so every descendant must already have had its `OnBeginPlay`.
Do not flip it to pre-order.

## The two overlap generators

| | `OverlapEvent` | `ActorOverlapEvent` |
|---|---|---|
| Source components | one, via `FComponentReference TargetOverlapComponent` (`UseComponentPicker`) | every `UPrimitiveComponent` on the owner, cached in `BoundComponents` |
| Per-actor dedupe | opt-in `bOverlapOncePerActor` (default true) | always |
| First-in / last-out only | `bOverlapOncePerAnyActor` | `bOverlapOncePerAnyActor` |
| Skips self-overlap | **no** | yes (`OtherActor == OwnerActor`) |
| `bUpdateOverlapsOnBeginPlay == false` | actors already inside produce an **End with no Begin** | `CacheOrHandleExistingOverlaps(false)` seeds `OverlappedActors` silently, so no spurious End |
| Runtime retarget | `SetOverlapComponent()` (`BlueprintCallable`) rewrites the `FComponentReference` and rebinds | — |

Both support `ActivationDelay` via a `FTimerHandle` on the module. **If `GetWorld()` is null when
`ActivationDelay > 0`, the delay is silently skipped** and activation happens immediately.

`ActorOverlapEvent::SetOverlapEnabled(true)` unbinds and resets `BoundComponents` before rebinding, so it
is idempotent and safe to call twice.

`AHierarchicalTriggerActor` auto-assigns the overlap component for `OverlapEvent` modules (recursively,
from `OnConstruction` / `PostActorCreated` / `PreInitializeComponents`) when the reference is entirely
unset — it picks the first non-root, non-editor-only, non-billboard primitive component. It does **not**
know about `ActorOverlapEvent`, which needs no assignment.

## Module identity and references

`FGuid ModuleId` on every module — `VisibleAnywhere, AdvancedDisplay`, never user-editable.

`UHierarchicalTriggerComponent::EnsureModuleIds()` (editor only) walks the tree with an explicit stack and
reassigns any id that is invalid **or a duplicate of one already seen**. Called from:

- `PostLoad` — backfills assets saved before ids existed.
- `PostEditChangeChainProperty` — nested instanced edits route to the owning component, so this covers
  add/paste at any depth.
- `TriggerModuleReferenceCustomization::OnGetModuleMenuContent` — before showing the picker.
- the details customization, after every structural mutation.

**Risks worth remembering.** `PostLoad` repairs ids in memory without dirtying the package, so a repair is
only persisted if the asset is saved for some other reason. `EnsureModuleIds` is `WITH_EDITOR`-only, so a
cooked build takes whatever ids were cooked, as-is. And because a duplicate is resolved by *reassigning
the later one in stack order*, a duplicate can move an existing reference's target — which is exactly why
`HierarchicalTriggerComponentDetails::InvalidateModuleIds` clears ids on copy/duplicate/paste before
letting `EnsureModuleIds` mint fresh ones. That function has a comment explaining it; read it before
touching copy/paste.

`FTriggerModuleReference` = `FComponentReference TriggerComponent` + `FGuid ModuleId`, with a `mutable
TWeakObjectPtr` resolve cache. Two traps:

- The cache is **never invalidated** when `ModuleId` or `TriggerComponent` changes. It is only written on
  a successful resolve, so a reference re-pointed after resolving keeps returning the old module.
- The no-arg `Resolve()` passes `TriggerComponent.OtherActor.Get()` as the context actor, so it works
  only for **cross-actor** references. A same-actor reference (`OtherActor` unset) resolves against null
  and fails. The Blueprint helper `UHierarchicalTriggerBlueprintUtils::Resolve` uses that overload, so
  same-actor references do not work from Blueprint.

**`FTriggerModuleReference` currently has zero consumers** outside the plugin and its own customization.
The de facto pattern instead is `UFlowNode_ListenForTrigger`: resolve actors through a `UActorProvider`,
then walk each trigger component's tree and bind **every** `UTriggerComponentModule_BroadcastDelegate`
found (`FTriggerModuleBroadcastDelegateSignature` = `(Module*, const AActor*, bool bEndOverlap)`, a plain
non-dynamic multicast). Decide which of the two is the intended direction before building on either.

## Save game

`UTEOHierarchicalTriggerComponent` implements `IEMSObjectSaveInterface`, which is how EasyMultiSave routes
a save archive to it. Both `Serialize` overrides do the same thing:

```cpp
Super::Serialize(Ar);
if (Ar.IsSaveGame()) { FHierarchicalTriggerModuleUtils::SerializeSaveGame(Modules, Ar); }
```

Neither `Modules` array is marked `SaveGame`, so the arrays themselves are not written. What actually
persists is the three flags on each module, **positionally, in current in-memory tree order**. So:

- Adding, removing or reordering modules invalidates existing saves — flags land on the wrong modules.
- `SerializeSaveGame` tests `if (Module)` rather than `IsValid(Module)`, unlike every other walk in the
  plugin.

## Editor layer

**`FHierarchicalTriggerComponentDetails`** replaces the stock `Modules` array UI entirely with a flat list
of indented rows (`IndentPerDepth = 14.0f`), one row per array slot including empty ones. Clicking a row
expands that module's properties inline through a single shared `IDetailsView` — an accordion, one open at
a time, so the view is bound exactly once per refresh. It owns add / set-class / delete / duplicate /
copy / paste / promote ("Make Parent") / reorder, plus source navigation. Several non-obvious decisions
are already commented in place and should be read before editing:

- `SetModuleClass` keeps the subtree, the three flags and the `ModuleId` when swapping class, so existing
  references survive a retype.
- `OnGetClassPickerContent` deliberately passes **no** `InitiallySelectedClass`: `SClassViewer::Populate`
  answers it with `SetSelection`, which reports `ESelectInfo::Direct` and fires `OnClassPicked` with the
  class already in the slot, dismissing the menu the instant it opens.
- Copy uses `FObjectPropertyBase::GetExportPath(..., PPF_Copy)` — the same clipboard format as a stock
  object-property row, so modules paste between this UI and any other object reference.
- Expansion state lives in a file-scope `TMap<FObjectKey, FGuid> GExpandedModules`, not in the
  customization, because `RequestForceRefresh` destroys and rebuilds the instance.
  `GetExpandedModuleId(const UHierarchicalTriggerComponent*)` is the public static the visualizer reads;
  `SetExpandedModuleId` also calls `RedrawLevelEditingViewports`.

**`FActorEventCallCustomization`** adds a searchable event picker over the target class's callable events
and rebuilds the parameter bag from the chosen signature. `FActorEventCall::IsCallableEvent` is the
filter; `SyncParametersToSignature` is the migration. The out-of-sync warning is throttled behind
`LastOutOfSyncCheckTime`. The `ContextActorName` metadata on the owning property labels the context-actor
toggle — `CallActorEvent` sets it to `"Trigger Owner"`.

**`FTriggerModuleReferenceCustomization`** renders the engine's `FComponentReference` picker plus an
`STreeView` of modules in a combo button; changing the component clears `ModuleId`.

## Viewport visualization

Declarative by design: modules describe, the visualizer draws.

```cpp
#if WITH_EDITOR
virtual void DescribeVisualization(FTriggerVisualizationBuilder& Builder) const {}
#endif
```

`FTriggerVisualizationBuilder` (runtime module, whole header behind `#if WITH_EDITOR`) accepts
`AddLine/AddArrow/AddSphere/AddBox/AddCircle/AddPoint/AddText` and returns a fluent
`FTriggerVisualizationHandle` with `SetColor/SetThickness/SetLabel/SetClickTarget`. The handle stores an
**index, not a reference**, so it survives the array reallocating on the next `Add`. `SetClickTarget`
makes the shape a hit proxy; clicking it selects that actor.

This keeps modules free of UnrealEd, `FPrimitiveDrawInterface`, `FCanvas` and hit proxies, and lets the
visualizer apply the user's settings once instead of per module. The builder is **per-frame scratch** —
it holds the component and modules as raw pointers deliberately; never keep one across frames.

`FHierarchicalTriggerComponentVisualizer` walks the tree, collects, draws at `SDPG_Foreground`, projects
labels in `DrawVisualizationHUD`, and selects the click target in `VisProxyHandleClick` (with
`ShouldAutoSelectElementOnHandleClick() == false` so it does not also select the component). It rebuilds
the description in both the PDI and HUD passes rather than caching — the passes have no ordering
guarantee. It early-outs on `World->IsGameWorld()`.

`UModularTriggerEditorSettings` (`config = EditorPerProjectUserSettings`, so per-developer, never in the
project) holds `bEnableVisualization`, `bDrawLabels`, `bOnlySelectedModule`, `bHideDisabledModules`,
`bDimUnselectedModules`, `LineThickness`, `DimOpacity`, and a `ToggleFlag(bool UModularTriggerEditorSettings::*)`
that saves config and redraws. Surfaced as a **Trigger Debug** toggle + options dropdown in the
`LevelEditor.LevelEditorToolBar.User` section, and in Editor Preferences → Plugins → Modular Trigger.
`bOnlySelectedModule` falls back to drawing everything when nothing is expanded, so it cannot blank the
viewport on its own.

Reference implementations: `GetTargets` (arrows + spheres + labels + click targets, and an explicit
"element N is empty" text for null slots), `DebugMessage` (text only, staggered by
`Builder.GetModuleDepth()`).

## Engine facts worth not re-deriving

Paths under `F:\UE\UE_5.7\Engine\Source`.

- **Sequence playback direction and loop count are orthogonal.** On `UMovieSceneSequencePlayer`
  (`Runtime/MovieScene`), `Play()` / `PlayReverse()` set only `bReversePlayback` and `PlayLooping(N)` sets
  only `PlaybackSettings.LoopCount.Value`; all three then call the same `PlayInternal()`. The loop-wrap
  code branches on direction throughout (`LoopEndTime = bReversePlayback ? StartTime : GetLastValidTime()`),
  so **a reversed loop is supported** — hence `PlayLevelSequence` exposes two flags rather than a play mode
  enum. `LoopCount.Value`: `-1` infinite, `0` none, `N` that many loops.
  `PlayInternal` early-outs when `IsPlaying()`, so of several play calls in one frame only the first starts
  anything — set direction with the starting call, and the loop count after it, since the count is not read
  until the cursor reaches the far end. `PlaybackSettings` itself is `protected` with no getter, so
  `PlayLooping` is the only public way to write the count without clobbering the rest of the settings.
  Per-tick updates recompute the rate from `bReversePlayback`, so direction can also be flipped mid-play.
- **`WorldConditions` ships the framework and no conditions.** Under
  `F:\UE\UE_5.7\Engine\Plugins\Runtime\WorldConditions`, the only structs deriving from
  `FWorldConditionBase` are the test suite's two. `FWorldConditionCommonBase` and
  `FWorldConditionCommonActorBase` are **empty** extension points — the names suggest a shared condition
  library that does not exist. SmartObjects brings its own set, tied to its own schema. So adopting
  WorldConditions anywhere means authoring the conditions too; the schema alone gives you an empty picker.
- **`FWorldConditionBase::GetDescription()` must be overridden on every condition.** The base returns
  `StaticStruct()->GetDisplayNameText()`, and `StaticStruct()` is not virtual, so it always resolves to
  `FWorldConditionBase` — every authored row reads "World Condition Base" otherwise. Note the collapsed row
  uses `GetDescription()` while the *picker* uses the struct's `DisplayName` meta: both are needed.
- **Condition picker filter** (`WorldConditionEditableDetails.cpp:597`): base struct `FWorldConditionBase`,
  then the schema's `IsStructAllowed`, then anything with the `Hidden` meta is dropped. The schema is found
  by walking parent property handles up to the enclosing `FWorldConditionQueryDefinition` and taking its
  `GetSchemaClass()` CDO — which is why the schema class has to be set before the picker is opened.
- **`FWorldConditionContextDataRef` picker** reads the property's `BaseStruct` / `BaseClass` meta (full
  paths, comma-separated) and offers every context data whose declared type `IsChildOf` one of them. Name
  lookup is `Desc.Struct->IsChildOf(Queried)`, so a descriptor declared as `AActor` resolves for `AActor`
  but not for a subclass query.
- **Query result semantics** (`WorldConditionContext.cpp`, `WorldConditionQuery.cpp`): an initialized state
  with no shared definition is an *empty* query and `IsTrue()` returns **true**; an uninitialized state
  returns **false** without ensuring. `Initialize()` leaves the state uninitialized when the definition
  failed to link, and `Deactivate()` calls `Free()`, so deactivating is not reversible without a fresh
  `Initialize`. A condition returning false from its own `Initialize` fails the whole query to link.
- `FWorldConditionQueryDefinition::EditableConditions` carries
  `meta=(BaseStruct="/Script/SmartObjectsModule.WorldCondition")` — a stale engine-side leftover pointing at
  a type that is not there. Harmless (the picker sets its own base struct), but don't copy it as a pattern.
- **The PDI draw helpers moved in 5.7.** `DrawWireSphere`, `DrawWireSphereAutoSides`, `DrawWireBox`,
  `DrawCircle`, `DrawDirectionalArrow`, `DrawWireDiamond` are in
  `Runtime/Engine/Public/PrimitiveDrawingUtils.h` (`:227 :240 :180 :256 :366 :415`), **not**
  `SceneManagement.h`. `DrawDirectionalArrow` takes an `FMatrix` whose +X axis is the arrow direction.
- `FComponentVisualizer` — `Editor/UnrealEd/Public/ComponentVisualizer.h:158`. `DrawVisualization :169`,
  `DrawVisualizationHUD :171`, `VisProxyHandleClick :173`, `ShouldShowForSelectedSubcomponents :167`
  (default **true**), `ShouldAutoSelectElementOnHandleClick :178` (defaults to the
  `Editor.ComponentVisualizer.AutoSelectComponent` cvar). Visualizers run for every component of a
  **selected actor**.
- `UUnrealEdEngine::RegisterComponentVisualizer` / `UnregisterComponentVisualizer` —
  `Editor/UnrealEd/Classes/Editor/UnrealEdEngine.h:233,235`, keyed by `FName` class name.
- `DECLARE_HIT_PROXY( ... /* APIDecl */ )` takes an optional API macro and can be called empty —
  `Runtime/Engine/Public/HitProxies.h:61`. Pair with `IMPLEMENT_HIT_PROXY(Type, ParentType)` (`:79`).
- `FSceneView::WorldToScreen` (`SceneView.h:1868`) → `FVector4`; `ScreenToPixel` (`:1874`) returns
  **false when `W <= 0`**, i.e. behind the camera — that is the cheap offscreen test.
  `FCanvas::DrawShadowedString(X, Y, Str, Font, Color)` + `StringSize(...)` is the simple label idiom
  (see `SplineComponentVisualizer.cpp:1016-1026`, the only engine visualizer that draws HUD text).
- `FViewportClick` is declared in `Editor/UnrealEd/Public/EditorViewportClient.h:189`.
- **`TWeakObjectPtr<const T>` cannot be assigned from a `const T*` in 5.7** — `FWeakObjectPtr` only
  converts from `const UObject*`, so a const-qualified template argument fails to match
  (`WeakObjectPtrTemplates.h:108`). Use a raw pointer for frame-scoped data, which is what the
  visualization structs do.
- `UDeveloperSettings::GetContainerName()` returns `"Editor"` when the class config is
  `EditorPerProjectUserSettings` or `EditorSettings` (`DeveloperSettings.cpp:18`); `GetSectionName()`
  defaults to the **class name** (`:68`); the category defaults to `"General"` (`:36`), so set
  `CategoryName` in the constructor. Hence `ShowViewer("Editor", "Plugins", "ModularTriggerEditorSettings")`.
- Level editor toolbar extension point `"LevelEditor.LevelEditorToolBar.User"` is registered at
  `Editor/LevelEditor/Private/LevelEditorToolBar.cpp:1525`. `FToolMenuEntry::InitToolBarButton` /
  `InitComboButton` — `Developer/ToolMenus/Public/ToolMenuEntry.h:189,191`. Custom engine *show flags*
  are not extensible from a plugin; a toolbar toggle is the available mechanism.
- `FEditorScriptExecutionGuard` is required to `ProcessEvent` from a non-PIE editor world
  (`ActorEventCall::Execute` wraps the call in one).
- **UHT sets `CPF_OutParm` on const reference parameters too**, so a genuine output is
  `CPF_OutParm && !CPF_ConstParm && !CPF_ReturnParm`. Testing `CPF_OutParm` alone rejects nearly every
  C++ `BlueprintCallable` function. `ActorEventCall.cpp:17` mirrors the engine's own test.
- `FPropertyBagPropertyDesc(FName, const FProperty*)` derives value type, container types and (in editor)
  the source property's metadata, so clamps and tooltips carry into the details row for free.
  `ValueType == EPropertyBagPropertyType::None` means the bag **cannot represent** it — maps, interfaces,
  delegates, weak/lazy object pointers, field paths. The bag drops those descriptors silently, which is
  why `IsCallableEvent` rejects the whole function instead.
- Migrating a property bag built from a `UFunction` must use `CopyMatchingValuesByName`, not
  `MigrateToNewBagStruct`: descriptors built from a `UFunction` get **fresh GUIDs every time**, so
  ID-based migration drops every value. Name matching also converts between numeric types, so widening a
  parameter keeps its authored value.
- A `TEnumAsByte<>` parameter is an `FByteProperty` while the bag always builds an `FEnumProperty` for
  enums, so `CopyValueToParam` falls back to exported text rather than reinterpreting memory.
- `ProcessEvent` on a `FUNC_Net` function follows the normal RPC path — the call may be sent to the
  authority rather than executed locally.

### Include leaks already fixed — do not regress

These only ever compiled because of adaptive unity; editing a file knocks it out of the unity blob and
exposes them. All already applied:

| File | Needed |
|---|---|
| `Modules/HierarchicalTriggerComponentModule.h` | `Engine/HitResult.h`; `GetOtherActor()` moved out of line (`UPrimitiveComponent` only forward declared) |
| `Private/Modules/HierarchicalTriggerComponentModule.cpp` | `Components/PrimitiveComponent.h` |
| `ActorEventCallCustomization.h` | `Input/Reply.h`, `Layout/Visibility.h` |
| `HierarchicalTriggerComponentDetails.h` | `Input/Reply.h`, `Styling/SlateTypes.h`, `UObject/ObjectMacros.h` |
| `TriggerModuleVisualization.h` | `UObject/WeakObjectPtr.h` (`CoreMinimal` does not define `FWeakObjectPtr`) |
| `Private/Modules/TriggerComponentModule_Filter.cpp` | `GameFramework/Actor.h`, `GameFramework/Pawn.h` |
| `Private/Modules/TriggerComponentModule_OverlapEvent.cpp` | `Components/PrimitiveComponent.h` |
| project `Core/TEOTypes.h` | forward declarations for `UAnimInstance`, `UAnimMontage` |

Still leaking, not yet fixed because the file is otherwise untouched: project
`AI/Components/TriggerModules/AIHierarchicalTriggerModules.cpp` wants `GameFramework/Actor.h`,
`GameFramework/Pawn.h` and `GameFramework/Controller.h`. A `-SingleFile` on it fails loudly for that
reason alone — do not read those errors as fallout from whatever you just changed.

## Known defects and traps

1. **`bEnabled` is dead on generator modules** (they call `PropagateEvent` directly and gate only on
   `bChildrenEnabled`), but the details row shows the toggle anyway.
2. `UTriggerComponentModule_OverlapEvent` does not skip self-overlap, so it can fire on other components
   of its own actor; and with `bUpdateOverlapsOnBeginPlay == false` it emits an End with no matching
   Begin for actors already inside.
3. `ActivationDelay` is silently ignored when `GetWorld()` is null at `OnBeginPlay`.
4. Save-game persistence is positional over the live tree (see "Save game"), and
   `SerializeSaveGame` uses `if (Module)` instead of `IsValid(Module)`.
5. `FTriggerModuleReference`'s resolve cache is never invalidated, and its no-arg `Resolve()` — the one
   Blueprint uses — cannot resolve a same-actor reference.
6. `UTriggerComponentModule_Blueprint` mirrors the context into `UPROPERTY(Transient)` members around
   `BPOnTriggerEventReceived()` and clears them afterwards. `InExecutionData` / `OutExecutionData` are
   `FInstancedStructContainer` with their `UPROPERTY` **commented out**, so anything held there is
   invisible to GC for the duration of the BP event. Also `OwningTriggerComponent`,
   `OverlappedComponent`, `OtherComponent` are raw `const U…*` rather than `TObjectPtr`.
7. `UTriggerComponentModule_DebugMessage` logs to `LogTemp`.
8. `FModularTriggerModule::StartupModule` / `ShutdownModule` are still the empty wizard stubs.

### Fixed 2026-10-12, recorded so the old shape is recognisable

- Interruption was a sticky context bool that leaked across siblings → replaced by `ShouldPropagateEvent`.
- Execution data was flat and dispatch-wide → now block-scoped per sibling list.
- `GetOrAddExecutionData<T>()` did not compile (assigned `const T*` to `T*`, marked `// TODO: Testing`).
- An unset Filter row made the Filter reject everything: the loop skipped invalid entries but fell through
  to `return false`, so "I added a row and haven't picked a type yet" silently blocked the trigger. Unset
  rows are now ignored, and the pass/fail logic is written out instead of `bAll ^ bPassed`.
- `UTriggerComponentModule_GetTargets` ignored `bAppend` — it built a local `FActorsArray` with the append
  logic and then published `FActorsArray{Targets}` anyway.
- Copyright headers were a three-way mix (`© Volga Studios`, `Copyright Volga Studios`,
  `Copyright Epic Games`). **The plugin now carries none at all**, deliberately — don't re-add one to a new
  file, and don't let a file wizard leave its Epic header behind.

## Open threads

- **Nothing migrates authored Filters to `WorldConditionFilter`.** The two modules exist side by side and
  store unrelated data (`TArray<TInstancedStruct<FFilterModuleEntry>>` vs. an `FWorldConditionQueryDefinition`
  whose authored rows are `TArray<FWorldConditionEditable>`). Converting means re-authoring, or a one-shot
  commandlet that walks trigger components and rewrites the entries it recognises. Until the content is
  moved, both module sets have to stay compiled, and the project's two Filter entries
  (`PlayersVehicle`, `WorldStateQuery`) have no condition equivalent yet.
- **Two competing ways to point at a module.** `FTriggerModuleReference` (stable GUID, full editor picker,
  no consumers) vs. `UFlowNode_ListenForTrigger`'s approach (resolve actors via `UActorProvider`, bind
  every `BroadcastDelegate` module in the tree). Pick one. The reference type is the better-engineered
  half but is dead code today; the Flow node's approach cannot target a specific module.
- **The gate and the data scoping changed authored behaviour; levels have not been re-checked.** Two
  patterns that used to work now do something different, and both are silent:
  a Filter placed *beside* the modules it was meant to gate no longer suppresses their subtrees (it is now
  a no-op there), and a `GetTargets` nested inside one branch is no longer visible to a later branch. Both
  were accidents of the old flat design, but content may lean on either. A pass over the trigger actors in
  `Content/Developers/NYA/Maps/TriggerTesting.umap` and the shipping maps is the missing half of that
  change.
- The generators share a lot: `ActivationDelay` + timer, `OverlappedActors` dedupe,
  `bOverlapOncePerAnyActor`, the `bTriggerOnce` clear. `ActorOverlapEvent` is the newer and more correct
  of the two (self-overlap skip, no spurious End). A shared base, with `OverlapEvent` keeping only the
  component picker, would delete most of the duplication and fix defect 6 by construction.
