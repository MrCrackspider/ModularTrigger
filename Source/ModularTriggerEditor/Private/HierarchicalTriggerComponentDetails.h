
#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "Input/Reply.h"
#include "Styling/SlateTypes.h"
#include "UObject/ObjectKey.h"
#include "UObject/ObjectMacros.h"

class IDetailCategoryBuilder;
class IDetailLayoutBuilder;
class IDetailsView;
class IPropertyUtilities;
class SWidget;
class UHierarchicalTriggerComponent;
class UHierarchicalTriggerComponentModule;
struct FPropertyAndParent;

/** The per-module flags surfaced as toggles on the row instead of as rows in the inline details view. */
enum class EModuleFlag : uint8
{
	Enabled,
	ChildrenEnabled,
	TriggerOnce,
};

/**
 * One entry of a Modules array in the editor tree.
 *
 * There is exactly one node per array element, including empty (null) elements: adding a module creates an
 * empty slot first, and the class combo on its row fills it in. Keeps enough context to mutate that slot.
 */
struct FModuleTreeNode
{
	/** The module occupying this slot, or null while the slot is still empty. */
	TWeakObjectPtr<UHierarchicalTriggerComponentModule> Module;
	/** Object owning the array this slot lives in: the component (roots) or the parent module (children). */
	TWeakObjectPtr<UObject> Owner;
	/** Index of this slot inside the owner's Modules array. */
	int32 SlotIndex = INDEX_NONE;
	FGuid ModuleId;
	FText Label;
	int32 Depth = 0;
	TWeakPtr<FModuleTreeNode> Parent;
	TArray<TSharedPtr<FModuleTreeNode>> Children;
};

/**
 * Class-level details customization for UHierarchicalTriggerComponent.
 *
 * Replaces the default inline rendering of the deeply-nested Modules array with a compact, always-visible
 * tree of module rows. Each row reads like an instanced-object row — a class combo showing the module's
 * display name — which the module base class cannot offer on its own because it is marked HideDropdown.
 * Clicking a row expands that module's properties inline, in place, via an embedded details view (accordion:
 * one open at a time). Also owns add / delete / duplicate / copy / paste / promote / within-level reorder,
 * since it takes over from the stock array UI.
 */
class FHierarchicalTriggerComponentDetails : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

	/**
	 * Stable id of the module currently expanded in this component's details panel, or an invalid guid when
	 * none is. Read by the component visualizer to highlight — or to draw only — the module being edited.
	 */
	static FGuid GetExpandedModuleId(const UHierarchicalTriggerComponent* Component);

private:
	/** An addressable array element: the owning object, its Modules array and the index within it. */
	struct FModuleSlot
	{
		UObject* Owner = nullptr;
		TArray<TObjectPtr<UHierarchicalTriggerComponentModule>>* Array = nullptr;
		int32 Index = INDEX_NONE;

		bool IsValid() const { return Owner != nullptr && Array != nullptr && Array->IsValidIndex(Index); }
	};

	// --- Tree building / rendering ---
	void BuildNodes(UObject* Owner, TArray<TObjectPtr<UHierarchicalTriggerComponentModule>>& Array,
		TSharedPtr<FModuleTreeNode> ParentNode, int32 Depth, TArray<TSharedPtr<FModuleTreeNode>>& OutNodes);
	void AddModuleRows(IDetailCategoryBuilder& Category, const TArray<TSharedPtr<FModuleTreeNode>>& Nodes);
	TSharedRef<SWidget> MakeHeaderWidget(TSharedPtr<FModuleTreeNode> Node);
	/** The class combo standing in for the instanced-object dropdown: shows the module's class display name. */
	TSharedRef<SWidget> MakeClassComboWidget(TSharedPtr<FModuleTreeNode> Node, bool bExpanded);
	/** Kebab menu holding the actions that do not earn a permanent slot on the row. */
	TSharedRef<SWidget> MakeActionsMenu(TSharedPtr<FModuleTreeNode> Node);
	/** A compact toggle for one of the module's flags, styled as a labelled button. */
	TSharedRef<SWidget> MakeFlagToggleWidget(TSharedPtr<FModuleTreeNode> Node, EModuleFlag Flag, const FText& Label);
	TSharedRef<SWidget> OnGetClassPickerContent(TSharedPtr<FModuleTreeNode> Node);

	// --- Interaction ---
	FReply OnToggleExpand(TSharedPtr<FModuleTreeNode> Node);
	bool IsExpanded(const TSharedPtr<FModuleTreeNode>& Node) const;
	ECheckBoxState GetFlagState(TSharedPtr<FModuleTreeNode> Node, EModuleFlag Flag) const;
	void OnFlagChanged(ECheckBoxState NewState, TSharedPtr<FModuleTreeNode> Node, EModuleFlag Flag);
	void OnModuleClassPicked(UClass* PickedClass, TSharedPtr<FModuleTreeNode> Node);

	// --- Authoring ---
	/** Appends an empty slot to the owner's array; the row's class combo then decides what goes in it. */
	void AddModuleSlot(TSharedPtr<FModuleTreeNode> ParentNode);
	/** Fills (or replaces, or clears) a slot. A replacement keeps the children, shared flags and stable id. */
	void SetModuleClass(TSharedPtr<FModuleTreeNode> Node, UClass* NewClass);
	void DeleteModule(TSharedPtr<FModuleTreeNode> Node);
	void DuplicateModule(TSharedPtr<FModuleTreeNode> Node);
	void MoveModule(TSharedPtr<FModuleTreeNode> Node, int32 Delta);
	/** Moves this module into its parent's slot; the former parent becomes this module's first child. */
	void MakeParent(TSharedPtr<FModuleTreeNode> Node);
	bool CanMakeParent(TSharedPtr<FModuleTreeNode> Node) const;
	void CopyModule(TSharedPtr<FModuleTreeNode> Node);

	// --- Source navigation ---
	void OpenModuleSource(TSharedPtr<FModuleTreeNode> Node);
	void BrowseToModuleBlueprint(TSharedPtr<FModuleTreeNode> Node);
	void EditModuleBlueprint(TSharedPtr<FModuleTreeNode> Node);

	void PasteModule(TSharedPtr<FModuleTreeNode> Node);
	bool CanPasteModule(TSharedPtr<FModuleTreeNode> Node) const;

	// --- Display ---
	FText GetModuleLabel(TSharedPtr<FModuleTreeNode> Node) const;
	FText GetModuleTooltip(TSharedPtr<FModuleTreeNode> Node) const;

	// --- Helpers ---
	FModuleSlot ResolveSlot(const TSharedPtr<FModuleTreeNode>& Node) const;
	bool IsModuleDetailPropertyVisible(const FPropertyAndParent& PropertyAndParent) const;
	UHierarchicalTriggerComponent* GetComponent() const;
	void NotifyModulesChanged(UObject* Owner, EPropertyChangeType::Type ChangeType) const;
	void RequestRefresh();
	static TArray<TObjectPtr<UHierarchicalTriggerComponentModule>>* GetChildArray(UObject* Owner);
	/** @return The module the clipboard currently points at, or nullptr when it holds something else. */
	static UHierarchicalTriggerComponentModule* ResolveClipboardModule();

	/**
	 * ModuleId of the single expanded (inline-details) module for the component being customized; invalid when
	 * nothing is expanded. Kept in a static map keyed by component so it survives the ForceRefresh that
	 * rebuilds this customization, and so the viewport visualizer can read it. @see GetExpandedModuleId
	 */
	FGuid GetExpandedModuleId() const;
	void SetExpandedModuleId(const FGuid& ModuleId);

	TWeakObjectPtr<UHierarchicalTriggerComponent> Component;
	FObjectKey ComponentKey;
	TSharedPtr<IPropertyUtilities> PropertyUtilities;
	TArray<TSharedPtr<FModuleTreeNode>> RootNodes;
	TSharedPtr<IDetailsView> InlineDetailsView;
};
