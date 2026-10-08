
#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"

class AActor;
class IPropertyHandle;
class ITableRow;
class SComboButton;
class STableViewBase;
class SWidget;
class UHierarchicalTriggerComponent;
class UHierarchicalTriggerComponentModule;
template <typename ItemType> class STreeView;

/** One node in the module-picker tree. Mirrors a UHierarchicalTriggerComponentModule and its children. */
struct FTriggerModuleNode
{
	TWeakObjectPtr<UHierarchicalTriggerComponentModule> Module;
	FGuid ModuleId;
	FText Label;
	TArray<TSharedPtr<FTriggerModuleNode>> Children;
};

/**
 * Details customization for FTriggerModuleReference. 
 * Adds a tree picker that lists the resolved component's module hierarchy and stores the picked module's GUID.
 */
class FTriggerModuleReferenceCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow,
		IPropertyTypeCustomizationUtils& CustomizationUtils) override;

	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder,
		IPropertyTypeCustomizationUtils& CustomizationUtils) override;

private:
	// --- Resolution helpers ---
	AActor* GetContextActor() const;
	UHierarchicalTriggerComponent* ResolveComponent() const;
	FGuid GetSelectedModuleId() const;
	void SetSelectedModuleId(const FGuid& NewId);

	// --- Tree building / rendering ---
	TSharedRef<SWidget> OnGetModuleMenuContent();
	void BuildNodes(const TArray<TObjectPtr<UHierarchicalTriggerComponentModule>>& InModules,
		TArray<TSharedPtr<FTriggerModuleNode>>& OutNodes) const;
	TSharedRef<ITableRow> OnGenerateModuleRow(TSharedPtr<FTriggerModuleNode> Node, const TSharedRef<STableViewBase>& OwnerTable);
	void OnGetModuleChildren(TSharedPtr<FTriggerModuleNode> Node, TArray<TSharedPtr<FTriggerModuleNode>>& OutChildren);
	void OnModuleSelectionChanged(TSharedPtr<FTriggerModuleNode> Node, ESelectInfo::Type SelectInfo);

	// --- Display / events ---
	FText GetSelectedModuleLabel() const;
	void OnComponentChanged();

	TSharedPtr<IPropertyHandle> StructHandle;
	TSharedPtr<IPropertyHandle> TriggerComponentHandle;
	TSharedPtr<IPropertyHandle> ModuleIdHandle;

	TArray<TSharedPtr<FTriggerModuleNode>> RootNodes;
	TSharedPtr<STreeView<TSharedPtr<FTriggerModuleNode>>> TreeView;
	TSharedPtr<SComboButton> ModuleComboButton;
};
