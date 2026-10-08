
#include "ActorEventCallCustomization.h"

#include "ActorEventCall.h"
#include "Components/ActorComponent.h"
#include "DetailWidgetRow.h"
#include "GameFramework/Actor.h"
#include "IDetailChildrenBuilder.h"
#include "IPropertyUtilities.h"
#include "PropertyHandle.h"
#include "ScopedTransaction.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "ActorEventCallCustomization"

namespace UE::ModularTriggerEditor::ActorEventCall
{
	/**
	 * The actor class that will own the edited call at execution time.
	 *
	 * The two cases that matter are a call authored on an actor placed in a level, where the owner is a live
	 * actor, and one authored on a Blueprint's component template, where there is no actor at all and the
	 * template is outered to the generated class instead. Both end at a class, which is all the event picker
	 * needs, so the same call can be authored once in the Blueprint and left alone on every instance.
	 */
	static UClass* FindContextActorClass(UObject* Object)
	{
		for (UObject* Current = Object; Current != nullptr; Current = Current->GetOuter())
		{
			if (const AActor* Actor = Cast<AActor>(Current))
			{
				return Actor->GetClass();
			}

			if (const UActorComponent* Component = Cast<UActorComponent>(Current))
			{
				if (const AActor* Owner = Component->GetOwner())
				{
					return Owner->GetClass();
				}
			}

			if (UClass* Class = Cast<UClass>(Current))
			{
				return Class->IsChildOf(AActor::StaticClass()) ? Class : nullptr;
			}
		}

		return nullptr;
	}
}

TSharedRef<IPropertyTypeCustomization> FActorEventCallCustomization::MakeInstance()
{
	return MakeShared<FActorEventCallCustomization>();
}

void FActorEventCallCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	StructHandle = PropertyHandle;
	
	HeaderRow
	.NameContent()
	[
		PropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	[
		SNew(STextBlock)
		.Text(this, &FActorEventCallCustomization::GetSelectedEventLabel)
		.Font(CustomizationUtils.GetRegularFont())
	];
}

void FActorEventCallCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	StructHandle = PropertyHandle;
	PropUtils = CustomizationUtils.GetPropertyUtilities();

	UseContextActorHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FActorEventCall, bUseContextActor));
	TargetActorHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FActorEventCall, TargetActor));
	EventNameHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FActorEventCall, EventName));
	ParametersHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FActorEventCall, Parameters));

	check(UseContextActorHandle.IsValid());
	check(TargetActorHandle.IsValid());
	check(EventNameHandle.IsValid());
	check(ParametersHandle.IsValid());

	// Either switch changes the class the event is read from, which can invalidate the chosen event and its
	// parameter layout.
	const FSimpleDelegate OnSourceChanged =
		FSimpleDelegate::CreateSP(this, &FActorEventCallCustomization::OnEventSourceChanged);
	UseContextActorHandle->SetOnPropertyValueChanged(OnSourceChanged);
	TargetActorHandle->SetOnPropertyValueChanged(OnSourceChanged);

	const FText ContextActorName = GetContextActorName();

	ChildBuilder.AddProperty(UseContextActorHandle.ToSharedRef())
		.DisplayName(FText::Format(LOCTEXT("UseContextActorLabel", "Use {0}"), ContextActorName))
		.ToolTip(FText::Format(
			LOCTEXT("UseContextActorTooltip",
				"Call the event on the {0} rather than on an actor picked here.\n\nNo actor reference is stored, so this can be set once on a Blueprint's defaults and every placed instance calls the event on itself."),
			ContextActorName));

	// Hidden by EditConditionHides while the call targets its context actor.
	ChildBuilder.AddProperty(TargetActorHandle.ToSharedRef());

	ChildBuilder.AddCustomRow(LOCTEXT("EventRowFilter", "Event"))
	.NameContent()
	[
		SNew(STextBlock)
		.Text(LOCTEXT("EventLabel", "Event"))
		.Font(CustomizationUtils.GetRegularFont())
	]
	.ValueContent()
	.MinDesiredWidth(250.0f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		[
			SAssignNew(EventComboButton, SComboButton)
			.OnGetMenuContent(this, &FActorEventCallCustomization::OnGetEventMenuContent)
			.ContentPadding(FMargin(4.0f, 2.0f))
			.ButtonContent()
			[
				SNew(STextBlock)
				.Text(this, &FActorEventCallCustomization::GetSelectedEventLabel)
				.Font(CustomizationUtils.GetRegularFont())
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(2.0f, 0.0f, 0.0f, 0.0f)
		[
			// Blueprint signature changes do not notify us, so re-reading the signature stays a manual step.
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SimpleButton")
			.ContentPadding(FMargin(2.0f))
			.OnClicked(this, &FActorEventCallCustomization::OnUpdateParametersClicked)
			.IsEnabled(this, &FActorEventCallCustomization::IsUpdateParametersEnabled)
			.ToolTipText(LOCTEXT("UpdateParametersTooltip",
				"Re-read the event's signature and rebuild the parameters from it, keeping the values of parameters whose name and type still match."))
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush("Icons.Refresh"))
				.ColorAndOpacity(FSlateColor::UseForeground())
			]
		]
	];

	// Shown when a previously picked event no longer exists on the actor, so the stale values are visible
	// rather than silently ignored at runtime.
	ChildBuilder.AddCustomRow(LOCTEXT("UnresolvedRowFilter", "Warning"))
	.Visibility(TAttribute<EVisibility>(this, &FActorEventCallCustomization::GetUnresolvedWarningVisibility))
	.WholeRowContent()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0.0f, 0.0f, 4.0f, 0.0f)
		[
			SNew(SImage)
			.Image(FAppStyle::GetBrush("Icons.Warning"))
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(this, &FActorEventCallCustomization::GetUnresolvedWarningText)
			.AutoWrapText(true)
		]
	];

	// Shown when the event still resolves but its signature has drifted from the stored parameters, which is
	// the common case after editing the event in Blueprint.
	ChildBuilder.AddCustomRow(LOCTEXT("OutOfSyncRowFilter", "Update Parameters"))
	.Visibility(TAttribute<EVisibility>(this, &FActorEventCallCustomization::GetOutOfSyncVisibility))
	.WholeRowContent()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0.0f, 0.0f, 4.0f, 0.0f)
		[
			SNew(SImage)
			.Image(FAppStyle::GetBrush("Icons.Warning"))
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("OutOfSyncText", "The event's parameters have changed. Use the refresh button to update them."))
			.AutoWrapText(true)
		]
	];

	// FPropertyBagDetails renders one typed row per parameter; "ChildRowFeatures = Fixed" on the UPROPERTY
	// suppresses the add/rename/delete affordances, since the layout is owned by the event signature.
	ChildBuilder.AddProperty(ParametersHandle.ToSharedRef());
}

FActorEventCall* FActorEventCallCustomization::GetEditedCall() const
{
	if (!StructHandle.IsValid())
	{
		return nullptr;
	}

	TArray<void*> RawData;
	StructHandle->AccessRawData(RawData);
	if (RawData.Num() != 1 || RawData[0] == nullptr)
	{
		return nullptr;
	}

	return static_cast<FActorEventCall*>(RawData[0]);
}

UClass* FActorEventCallCustomization::GetContextActorClass() const
{
	if (!StructHandle.IsValid())
	{
		return nullptr;
	}

	TArray<UObject*> OuterObjects;
	StructHandle->GetOuterObjects(OuterObjects);
	if (OuterObjects.Num() != 1)
	{
		return nullptr;
	}

	return UE::ModularTriggerEditor::ActorEventCall::FindContextActorClass(OuterObjects[0]);
}

UClass* FActorEventCallCustomization::GetEventSourceClass() const
{
	const FActorEventCall* Call = GetEditedCall();
	return Call ? Call->ResolveTargetClass(GetContextActorClass()) : nullptr;
}

FText FActorEventCallCustomization::GetContextActorName() const
{
	static const FName ContextActorNameKey(TEXT("ContextActorName"));

	if (StructHandle.IsValid())
	{
		const FString& Name = StructHandle->GetMetaData(ContextActorNameKey);
		if (!Name.IsEmpty())
		{
			return FText::FromString(Name);
		}
	}

	return LOCTEXT("DefaultContextActorName", "Context Actor");
}

FName FActorEventCallCustomization::GetSelectedEventName() const
{
	const FActorEventCall* Call = GetEditedCall();
	return Call ? Call->EventName : NAME_None;
}

void FActorEventCallCustomization::RebuildParameters(const FText& TransactionText, const TOptional<FName> NewEventName)
{
	if (!StructHandle.IsValid())
	{
		return;
	}

	FScopedTransaction Transaction(TransactionText);

	StructHandle->NotifyPreChange();

	UClass* ContextActorClass = GetContextActorClass();

	StructHandle->EnumerateRawData([&NewEventName, ContextActorClass](void* RawData, const int32 /*DataIndex*/, const int32 /*NumData*/)
	{
		if (FActorEventCall* Call = static_cast<FActorEventCall*>(RawData))
		{
			if (NewEventName.IsSet())
			{
				Call->EventName = *NewEventName;
			}
			Call->SyncParametersToSignature(ContextActorClass);
		}
		return true;
	});

	StructHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
	StructHandle->NotifyFinishedChangingProperties();

	// Force the next poll to recompute rather than showing the stale notice until the throttle expires.
	LastOutOfSyncCheckTime = 0.0;

	// The parameter rows are built from the bag's layout, so the details view has to be rebuilt.
	if (PropUtils.IsValid())
	{
		PropUtils->ForceRefresh();
	}
}

void FActorEventCallCustomization::SetSelectedEvent(const FName NewEventName)
{
	RebuildParameters(LOCTEXT("SetActorEvent", "Set Actor Event"), NewEventName);
}

FReply FActorEventCallCustomization::OnUpdateParametersClicked()
{
	RebuildParameters(LOCTEXT("UpdateActorEventParameters", "Update Actor Event Parameters"), TOptional<FName>());
	return FReply::Handled();
}

bool FActorEventCallCustomization::IsUpdateParametersEnabled() const
{
	const FActorEventCall* Call = GetEditedCall();
	return Call != nullptr && !Call->EventName.IsNone();
}

bool FActorEventCallCustomization::AreParametersOutOfSync() const
{
	// Polled from a Slate attribute, so recompute at a few Hz rather than every frame; rebuilding the
	// descriptors copies each parameter's editor metadata and is not worth doing per-frame.
	constexpr double RecheckInterval = 0.25;

	const double Now = FPlatformTime::Seconds();
	if (Now - LastOutOfSyncCheckTime >= RecheckInterval)
	{
		LastOutOfSyncCheckTime = Now;
		bCachedParametersOutOfSync = ComputeParametersOutOfSync();
	}

	return bCachedParametersOutOfSync;
}

bool FActorEventCallCustomization::ComputeParametersOutOfSync() const
{
	const FActorEventCall* Call = GetEditedCall();
	if (Call == nullptr || Call->EventName.IsNone())
	{
		return false;
	}

	// An event that cannot be resolved at all is reported by the unresolved warning row instead.
	const UFunction* Function = FActorEventCall::ResolveFunction(GetEventSourceClass(), Call->EventName);
	if (Function == nullptr)
	{
		return false;
	}

	TArray<FPropertyBagPropertyDesc> SignatureDescs;
	FActorEventCall::BuildParameterDescs(Function, SignatureDescs);

	const UPropertyBag* BagStruct = Call->Parameters.GetPropertyBagStruct();
	const TConstArrayView<FPropertyBagPropertyDesc> StoredDescs =
		BagStruct ? BagStruct->GetPropertyDescs() : TConstArrayView<FPropertyBagPropertyDesc>();

	if (StoredDescs.Num() != SignatureDescs.Num())
	{
		return true;
	}

	// Order matters: parameters are displayed in declaration order, so a reorder is worth reporting too.
	for (int32 Index = 0; Index < SignatureDescs.Num(); ++Index)
	{
		if (StoredDescs[Index].Name != SignatureDescs[Index].Name
			|| !StoredDescs[Index].CompatibleType(SignatureDescs[Index]))
		{
			return true;
		}
	}

	return false;
}

EVisibility FActorEventCallCustomization::GetOutOfSyncVisibility() const
{
	return AreParametersOutOfSync() ? EVisibility::Visible : EVisibility::Collapsed;
}

void FActorEventCallCustomization::OnEventSourceChanged()
{
	// Keep the event only if the new class still has it; otherwise clear it along with the stale parameters.
	const FActorEventCall* Call = GetEditedCall();
	if (Call == nullptr)
	{
		return;
	}

	const UFunction* Function = FActorEventCall::ResolveFunction(GetEventSourceClass(), Call->EventName);
	SetSelectedEvent(FActorEventCall::IsCallableEvent(Function) ? Call->EventName : NAME_None);
}

void FActorEventCallCustomization::RefreshEventItems()
{
	AllEventItems.Reset();

	if (const UClass* SourceClass = GetEventSourceClass())
	{
		for (TFieldIterator<UFunction> It(SourceClass, EFieldIterationFlags::IncludeSuper); It; ++It)
		{
			const UFunction* Function = *It;
			if (!FActorEventCall::IsCallableEvent(Function))
			{
				continue;
			}

			TSharedRef<FActorEventItem> Item = MakeShared<FActorEventItem>();
			Item->EventName = Function->GetFName();
			Item->Label = FText::FromName(Function->GetFName());
			Item->Tooltip = FText::Format(
				LOCTEXT("EventTooltipFormat", "{0}\nDeclared in: {1}"),
				Function->GetToolTipText(),
				FText::FromString(GetNameSafe(Function->GetOwnerClass())));
			AllEventItems.Add(Item);
		}
	}

	AllEventItems.Sort([](const TSharedPtr<FActorEventItem>& A, const TSharedPtr<FActorEventItem>& B)
	{
		return A->EventName.LexicalLess(B->EventName);
	});

	FilteredEventItems = AllEventItems;
	if (!SearchFilter.IsEmpty())
	{
		FilteredEventItems.RemoveAll([this](const TSharedPtr<FActorEventItem>& Item)
		{
			return !Item->EventName.ToString().Contains(SearchFilter);
		});
	}
}

TSharedRef<SWidget> FActorEventCallCustomization::OnGetEventMenuContent()
{
	SearchFilter.Reset();
	RefreshEventItems();

	TSharedRef<SVerticalBox> Content = SNew(SVerticalBox);

	Content->AddSlot()
	.AutoHeight()
	.Padding(2.0f)
	[
		SAssignNew(SearchBox, SSearchBox)
		.HintText(LOCTEXT("SearchEventsHint", "Search events..."))
		.OnTextChanged(this, &FActorEventCallCustomization::OnSearchTextChanged)
	];

	if (GetEventSourceClass() == nullptr)
	{
		const FActorEventCall* Call = GetEditedCall();
		Content->AddSlot()
		.AutoHeight()
		.Padding(4.0f)
		[
			SNew(STextBlock)
			.Text(Call && Call->bUseContextActor
				? FText::Format(LOCTEXT("NoContextClass", "The {0} could not be determined from here, so its events cannot be listed."), GetContextActorName())
				: LOCTEXT("NoActor", "Select a Target Actor first."))
			.AutoWrapText(true)
		];
	}
	else if (AllEventItems.Num() == 0)
	{
		Content->AddSlot()
		.AutoHeight()
		.Padding(4.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("NoEvents", "Actor has no callable events.\nEvents with a return value, output parameters, or parameters of an unsupported type (maps, delegates, interfaces) are not listed."))
			.AutoWrapText(true)
		];
	}
	else
	{
		Content->AddSlot()
		.FillHeight(1.0f)
		[
			SAssignNew(EventListView, SListView<TSharedPtr<FActorEventItem>>)
			.ListItemsSource(&FilteredEventItems)
			.SelectionMode(ESelectionMode::Single)
			.OnGenerateRow(this, &FActorEventCallCustomization::OnGenerateEventRow)
			.OnSelectionChanged(this, &FActorEventCallCustomization::OnEventSelectionChanged)
		];
	}

	Content->AddSlot()
	.AutoHeight()
	.Padding(2.0f)
	[
		SNew(SButton)
		.Text(LOCTEXT("ClearEvent", "Clear"))
		.OnClicked_Lambda([this]()
		{
			SetSelectedEvent(NAME_None);
			if (EventComboButton.IsValid())
			{
				EventComboButton->SetIsOpen(false);
			}
			return FReply::Handled();
		})
	];

	return SNew(SBox)
		.MinDesiredWidth(320.0f)
		.MaxDesiredHeight(400.0f)
		[
			Content
		];
}

void FActorEventCallCustomization::OnSearchTextChanged(const FText& NewText)
{
	SearchFilter = NewText.ToString();
	RefreshEventItems();

	if (EventListView.IsValid())
	{
		EventListView->RequestListRefresh();
	}
}

TSharedRef<ITableRow> FActorEventCallCustomization::OnGenerateEventRow(TSharedPtr<FActorEventItem> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(STableRow<TSharedPtr<FActorEventItem>>, OwnerTable)
	[
		SNew(STextBlock)
		.Text(Item.IsValid() ? Item->Label : FText::GetEmpty())
		.ToolTipText(Item.IsValid() ? Item->Tooltip : FText::GetEmpty())
	];
}

void FActorEventCallCustomization::OnEventSelectionChanged(TSharedPtr<FActorEventItem> Item, ESelectInfo::Type SelectInfo)
{
	// Ignore the selection the list view sets on itself while being built.
	if (SelectInfo == ESelectInfo::Direct || !Item.IsValid())
	{
		return;
	}

	if (EventComboButton.IsValid())
	{
		EventComboButton->SetIsOpen(false);
	}

	SetSelectedEvent(Item->EventName);
}

FText FActorEventCallCustomization::GetSelectedEventLabel() const
{
	const FName EventName = GetSelectedEventName();
	return EventName.IsNone() ? LOCTEXT("NoEventSelected", "None") : FText::FromName(EventName);
}

EVisibility FActorEventCallCustomization::GetUnresolvedWarningVisibility() const
{
	const FActorEventCall* Call = GetEditedCall();
	if (Call == nullptr || Call->EventName.IsNone())
	{
		return EVisibility::Collapsed;
	}

	const UFunction* Function = FActorEventCall::ResolveFunction(GetEventSourceClass(), Call->EventName);
	return Function != nullptr ? EVisibility::Collapsed : EVisibility::Visible;
}

FText FActorEventCallCustomization::GetUnresolvedWarningText() const
{
	const FActorEventCall* Call = GetEditedCall();
	const FName EventName = Call ? Call->EventName : NAME_None;
	const UClass* SourceClass = GetEventSourceClass();

	if (SourceClass == nullptr)
	{
		if (Call != nullptr && Call->bUseContextActor)
		{
			return FText::Format(
				LOCTEXT("NoContextActorForEvent", "'{0}' cannot be checked here: the {1} could not be determined from this editor. The call still runs against whichever actor owns this at runtime."),
				FText::FromName(EventName),
				GetContextActorName());
		}

		return FText::Format(
			LOCTEXT("NoActorForEvent", "'{0}' cannot be resolved: no Target Actor is set."),
			FText::FromName(EventName));
	}

	return FText::Format(
		LOCTEXT("EventNotFound", "'{0}' no longer exists on {1}. Pick another event; the stored parameter values will be dropped."),
		FText::FromName(EventName),
		FText::FromString(GetNameSafe(SourceClass)));
}

#undef LOCTEXT_NAMESPACE
