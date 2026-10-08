
#include "TriggerModuleReferenceCustomization.h"

#include "DetailWidgetRow.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "HierarchicalTriggerComponent.h"
#include "IDetailChildrenBuilder.h"
#include "Modules/HierarchicalTriggerComponentModule.h"
#include "PropertyHandle.h"
#include "TriggerModuleReference.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STreeView.h"

#define LOCTEXT_NAMESPACE "TriggerModuleReferenceCustomization"

TSharedRef<IPropertyTypeCustomization> FTriggerModuleReferenceCustomization::MakeInstance()
{
	return MakeShared<FTriggerModuleReferenceCustomization>();
}

void FTriggerModuleReferenceCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow,
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
		.Text(this, &FTriggerModuleReferenceCustomization::GetSelectedModuleLabel)
		.Font(CustomizationUtils.GetRegularFont())
	];
}

void FTriggerModuleReferenceCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	StructHandle = PropertyHandle;
	TriggerComponentHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FTriggerModuleReference, TriggerComponent));
	ModuleIdHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FTriggerModuleReference, ModuleId));

	check(TriggerComponentHandle.IsValid());
	check(ModuleIdHandle.IsValid());

	// Selecting a different actor/component invalidates the previously chosen module.
	const FSimpleDelegate OnComponentChangedDelegate =
		FSimpleDelegate::CreateSP(this, &FTriggerModuleReferenceCustomization::OnComponentChanged);
	TriggerComponentHandle->SetOnPropertyValueChanged(OnComponentChangedDelegate);
	TriggerComponentHandle->SetOnChildPropertyValueChanged(OnComponentChangedDelegate);

	// Engine's FComponentReference customization renders the actor + component picker.
	ChildBuilder.AddProperty(TriggerComponentHandle.ToSharedRef());

	ChildBuilder.AddCustomRow(LOCTEXT("ModuleRowFilter", "Module"))
	.NameContent()
	[
		SNew(STextBlock)
		.Text(LOCTEXT("ModuleLabel", "Module"))
		.Font(CustomizationUtils.GetRegularFont())
	]
	.ValueContent()
	.MinDesiredWidth(250.0f)
	[
		SAssignNew(ModuleComboButton, SComboButton)
		.OnGetMenuContent(this, &FTriggerModuleReferenceCustomization::OnGetModuleMenuContent)
		.ContentPadding(FMargin(4.0f, 2.0f))
		.ButtonContent()
		[
			SNew(STextBlock)
			.Text(this, &FTriggerModuleReferenceCustomization::GetSelectedModuleLabel)
			.Font(CustomizationUtils.GetRegularFont())
		]
	];
}

AActor* FTriggerModuleReferenceCustomization::GetContextActor() const
{
	if (!StructHandle.IsValid())
	{
		return nullptr;
	}

	TArray<UObject*> OuterObjects;
	StructHandle->GetOuterObjects(OuterObjects);
	for (UObject* Object : OuterObjects)
	{
		if (!Object)
		{
			continue;
		}
		if (AActor* AsActor = Cast<AActor>(Object))
		{
			return AsActor;
		}
		if (AActor* Owner = Object->GetTypedOuter<AActor>())
		{
			return Owner;
		}
	}
	return nullptr;
}

UHierarchicalTriggerComponent* FTriggerModuleReferenceCustomization::ResolveComponent() const
{
	if (!TriggerComponentHandle.IsValid())
	{
		return nullptr;
	}

	TArray<void*> RawData;
	TriggerComponentHandle->AccessRawData(RawData);
	if (RawData.Num() != 1 || RawData[0] == nullptr)
	{
		return nullptr;
	}

	const FComponentReference* Reference = static_cast<const FComponentReference*>(RawData[0]);
	return Cast<UHierarchicalTriggerComponent>(Reference->GetComponent(GetContextActor()));
}

FGuid FTriggerModuleReferenceCustomization::GetSelectedModuleId() const
{
	if (ModuleIdHandle.IsValid())
	{
		TArray<void*> RawData;
		ModuleIdHandle->AccessRawData(RawData);
		if (RawData.Num() == 1 && RawData[0] != nullptr)
		{
			return *static_cast<const FGuid*>(RawData[0]);
		}
	}
	return FGuid();
}

void FTriggerModuleReferenceCustomization::SetSelectedModuleId(const FGuid& NewId)
{
	if (!ModuleIdHandle.IsValid())
	{
		return;
	}

	ModuleIdHandle->NotifyPreChange();

	TArray<void*> RawData;
	ModuleIdHandle->AccessRawData(RawData);
	for (void* Data : RawData)
	{
		if (Data)
		{
			*static_cast<FGuid*>(Data) = NewId;
		}
	}

	ModuleIdHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
	ModuleIdHandle->NotifyFinishedChangingProperties();
}

TSharedRef<SWidget> FTriggerModuleReferenceCustomization::OnGetModuleMenuContent()
{
	RootNodes.Reset();

	UHierarchicalTriggerComponent* Component = ResolveComponent();
	if (Component)
	{
		// Guarantee every module has a stable id before we let the user pick one (covers freshly added modules).
		Component->EnsureModuleIds();
		BuildNodes(Component->Modules, RootNodes);
	}

	TSharedRef<SVerticalBox> Content = SNew(SVerticalBox);

	Content->AddSlot()
	.AutoHeight()
	.Padding(2.0f)
	[
		SNew(SButton)
		.Text(LOCTEXT("ClearModule", "Clear"))
		.OnClicked_Lambda([this]()
		{
			SetSelectedModuleId(FGuid());
			if (ModuleComboButton.IsValid())
			{
				ModuleComboButton->SetIsOpen(false);
			}
			return FReply::Handled();
		})
	];

	if (!Component)
	{
		Content->AddSlot()
		.AutoHeight()
		.Padding(4.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("NoComponent", "Select a Trigger Component first."))
		];
	}
	else if (RootNodes.Num() == 0)
	{
		Content->AddSlot()
		.AutoHeight()
		.Padding(4.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("NoModules", "Component has no modules."))
		];
	}
	else
	{
		Content->AddSlot()
		.FillHeight(1.0f)
		[
			SAssignNew(TreeView, STreeView<TSharedPtr<FTriggerModuleNode>>)
			.TreeItemsSource(&RootNodes)
			.SelectionMode(ESelectionMode::Single)
			.OnGenerateRow(this, &FTriggerModuleReferenceCustomization::OnGenerateModuleRow)
			.OnGetChildren(this, &FTriggerModuleReferenceCustomization::OnGetModuleChildren)
			.OnSelectionChanged(this, &FTriggerModuleReferenceCustomization::OnModuleSelectionChanged)
		];
	}

	return SNew(SBox)
		.MinDesiredWidth(280.0f)
		.MaxDesiredHeight(400.0f)
		[
			Content
		];
}

void FTriggerModuleReferenceCustomization::BuildNodes(const TArray<TObjectPtr<UHierarchicalTriggerComponentModule>>& InModules,
	TArray<TSharedPtr<FTriggerModuleNode>>& OutNodes) const
{
	for (const TObjectPtr<UHierarchicalTriggerComponentModule>& Module : InModules)
	{
		if (!IsValid(Module))
		{
			continue;
		}

		TSharedRef<FTriggerModuleNode> Node = MakeShared<FTriggerModuleNode>();
		Node->Module = Module;
		Node->ModuleId = Module->GetModuleId();
		Node->Label = Module->GetClass()->GetDisplayNameText();
		BuildNodes(Module->Modules, Node->Children);
		OutNodes.Add(Node);
	}
}

TSharedRef<ITableRow> FTriggerModuleReferenceCustomization::OnGenerateModuleRow(TSharedPtr<FTriggerModuleNode> Node, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(STableRow<TSharedPtr<FTriggerModuleNode>>, OwnerTable)
	[
		SNew(STextBlock)
		.Text(Node.IsValid() ? Node->Label : FText::GetEmpty())
	];
}

void FTriggerModuleReferenceCustomization::OnGetModuleChildren(TSharedPtr<FTriggerModuleNode> Node, TArray<TSharedPtr<FTriggerModuleNode>>& OutChildren)
{
	if (Node.IsValid())
	{
		OutChildren = Node->Children;
	}
}

void FTriggerModuleReferenceCustomization::OnModuleSelectionChanged(TSharedPtr<FTriggerModuleNode> Node, ESelectInfo::Type SelectInfo)
{
	// Ignore programmatic/keyboard-navigation churn; only act on an explicit click.
	if (SelectInfo == ESelectInfo::Direct)
	{
		return;
	}

	if (Node.IsValid())
	{
		SetSelectedModuleId(Node->ModuleId);
		if (ModuleComboButton.IsValid())
		{
			ModuleComboButton->SetIsOpen(false);
		}
	}
}

FText FTriggerModuleReferenceCustomization::GetSelectedModuleLabel() const
{
	const FGuid CurrentId = GetSelectedModuleId();
	if (!CurrentId.IsValid())
	{
		return LOCTEXT("NoModuleSelected", "None");
	}

	if (UHierarchicalTriggerComponent* Component = ResolveComponent())
	{
		if (UHierarchicalTriggerComponentModule* Module = Component->FindModuleByGuid(CurrentId))
		{
			TArray<FString> Parts;
			for (UHierarchicalTriggerComponentModule* Current = Module; Current; Current = Current->GetOuterModule())
			{
				Parts.Insert(Current->GetClass()->GetDisplayNameText().ToString(), 0);
			}
			return FText::FromString(FString::Join(Parts, TEXT("\n")));
		}
	}

	return LOCTEXT("MissingModule", "(missing module)");
}

void FTriggerModuleReferenceCustomization::OnComponentChanged()
{
	SetSelectedModuleId(FGuid());
}

#undef LOCTEXT_NAMESPACE
