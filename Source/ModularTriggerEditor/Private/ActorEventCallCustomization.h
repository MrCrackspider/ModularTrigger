
#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"
#include "Input/Reply.h"
#include "Layout/Visibility.h"

class AActor;
class IPropertyHandle;
class IPropertyUtilities;
class ITableRow;
class SComboButton;
class SSearchBox;
class STableViewBase;
class SWidget;
struct FActorEventCall;
template <typename ItemType> class SListView;

/** One entry in the event picker list. */
struct FActorEventItem
{
	FName EventName;
	FText Label;
	FText Tooltip;
};

/**
 * Details customization for FActorEventCall.
 * Adds an event picker listing the target actor's callable events, and rebuilds the parameter bag from the
 * chosen event's signature so its parameters appear as editable rows.
 */
class FActorEventCallCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow,
		IPropertyTypeCustomizationUtils& CustomizationUtils) override;

	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder,
		IPropertyTypeCustomizationUtils& CustomizationUtils) override;

private:
	// --- Data access ---
	/** @return The single edited struct, or nullptr when nothing or several objects are selected. */
	FActorEventCall* GetEditedCall() const;
	FName GetSelectedEventName() const;

	/**
	 * @return The actor class that will supply the context actor at execution time, derived from whatever owns
	 *         the edited object: the placed actor for a level instance, the generated class for a Blueprint's
	 *         component template. Nullptr when the chain leads to neither.
	 */
	UClass* GetContextActorClass() const;

	/** @return The class whose events the picker lists: TargetActor's class, or the context actor's class. */
	UClass* GetEventSourceClass() const;

	/** Display name for the context actor, from the "ContextActorName" metadata on the edited property. */
	FText GetContextActorName() const;

	// --- Mutation ---
	/**
	 * Rebuilds the parameter bag from the current event signature inside a transaction, optionally
	 * reassigning EventName first. Values of parameters whose name and type survived are carried over.
	 */
	void RebuildParameters(const FText& TransactionText, TOptional<FName> NewEventName);
	/** Sets EventName and rebuilds the parameter bag from the new signature. */
	void SetSelectedEvent(FName NewEventName);
	/** Re-reads the current signature without changing the event; used by the refresh button. */
	FReply OnUpdateParametersClicked();
	bool IsUpdateParametersEnabled() const;
	/** Drops the chosen event when the class it came from no longer offers it. */
	void OnEventSourceChanged();

	// --- Picker ---
	TSharedRef<SWidget> OnGetEventMenuContent();
	void RefreshEventItems();
	TSharedRef<ITableRow> OnGenerateEventRow(TSharedPtr<FActorEventItem> Item, const TSharedRef<STableViewBase>& OwnerTable);
	void OnEventSelectionChanged(TSharedPtr<FActorEventItem> Item, ESelectInfo::Type SelectInfo);
	void OnSearchTextChanged(const FText& NewText);

	// --- Display ---
	FText GetSelectedEventLabel() const;
	EVisibility GetUnresolvedWarningVisibility() const;
	FText GetUnresolvedWarningText() const;
	
	/**
	 * True when the stored parameter layout no longer matches the resolved event's signature.
	 */
	bool AreParametersOutOfSync() const;
	bool ComputeParametersOutOfSync() const;
	EVisibility GetOutOfSyncVisibility() const;

	TSharedPtr<IPropertyHandle> StructHandle;
	TSharedPtr<IPropertyHandle> UseContextActorHandle;
	TSharedPtr<IPropertyHandle> TargetActorHandle;
	TSharedPtr<IPropertyHandle> EventNameHandle;
	TSharedPtr<IPropertyHandle> ParametersHandle;
	TSharedPtr<IPropertyUtilities> PropUtils;

	/** Every callable event on the current actor, and the search-filtered subset the list view renders. */
	TArray<TSharedPtr<FActorEventItem>> AllEventItems;
	TArray<TSharedPtr<FActorEventItem>> FilteredEventItems;
	FString SearchFilter;

	/** Throttle state for AreParametersOutOfSync. */
	mutable double LastOutOfSyncCheckTime = 0.0;
	mutable bool bCachedParametersOutOfSync = false;

	TSharedPtr<SListView<TSharedPtr<FActorEventItem>>> EventListView;
	TSharedPtr<SComboButton> EventComboButton;
	TSharedPtr<SSearchBox> SearchBox;
};
