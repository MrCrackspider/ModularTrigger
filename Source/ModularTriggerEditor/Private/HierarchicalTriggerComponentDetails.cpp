
#include "HierarchicalTriggerComponentDetails.h"

#include "ClassViewerFilter.h"
#include "ClassViewerModule.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Editor.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HierarchicalTriggerComponent.h"
#include "IPropertyUtilities.h"
#include "Modules/HierarchicalTriggerComponentModule.h"
#include "Modules/ModuleManager.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyEditorModule.h"
#include "ScopedTransaction.h"
#include "SourceCodeNavigation.h"
#include "Styling/AppStyle.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/UnrealType.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "HierarchicalTriggerComponentDetails"

namespace
{
	/** Property names hidden from the inline per-module details view: recursion, the read-only id, and the
	 *  three flags (already surfaced as the row's own toggles). */
	const FName ModulesPropertyName = GET_MEMBER_NAME_CHECKED(UHierarchicalTriggerComponentModule, Modules);
	const FName ModuleIdPropertyName = GET_MEMBER_NAME_CHECKED(UHierarchicalTriggerComponentModule, ModuleId);
	const FName EnabledPropertyName = GET_MEMBER_NAME_CHECKED(UHierarchicalTriggerComponentModule, bEnabled);
	const FName ChildrenEnabledPropertyName = GET_MEMBER_NAME_CHECKED(UHierarchicalTriggerComponentModule, bChildrenEnabled);
	const FName TriggerOncePropertyName = GET_MEMBER_NAME_CHECKED(UHierarchicalTriggerComponentModule, bTriggerOnce);

	/** Bitfields have no addressable member pointer, so the three flags are reached through a small switch. */
	bool GetModuleFlag(const UHierarchicalTriggerComponentModule& Module, EModuleFlag Flag)
	{
		switch (Flag)
		{
		case EModuleFlag::ChildrenEnabled:	return Module.bChildrenEnabled;
		case EModuleFlag::TriggerOnce:		return Module.bTriggerOnce;
		default:							return Module.bEnabled;
		}
	}

	void SetModuleFlag(UHierarchicalTriggerComponentModule& Module, EModuleFlag Flag, bool bValue)
	{
		switch (Flag)
		{
		case EModuleFlag::ChildrenEnabled:	Module.bChildrenEnabled = bValue; break;
		case EModuleFlag::TriggerOnce:		Module.bTriggerOnce = bValue; break;
		default:							Module.bEnabled = bValue; break;
		}
	}

	FName GetModuleFlagPropertyName(EModuleFlag Flag)
	{
		switch (Flag)
		{
		case EModuleFlag::ChildrenEnabled:	return ChildrenEnabledPropertyName;
		case EModuleFlag::TriggerOnce:		return TriggerOncePropertyName;
		default:							return EnabledPropertyName;
		}
	}

	FText GetModuleFlagTooltip(EModuleFlag Flag)
	{
		switch (Flag)
		{
		case EModuleFlag::ChildrenEnabled:
			return NSLOCTEXT("HierarchicalTriggerComponentDetails", "ChildrenEnabledTip",
				"Should children of this module receive the incoming event");
		case EModuleFlag::TriggerOnce:
			return NSLOCTEXT("HierarchicalTriggerComponentDetails", "TriggerOnceTip",
				"Should this module and its children be deactivated after the first overlap");
		default:
			return NSLOCTEXT("HierarchicalTriggerComponentDetails", "EnabledTip",
				"Should this module react to the incoming event");
		}
	}

	/** Horizontal indent (in Slate units) applied per tree depth. */
	constexpr float IndentPerDepth = 14.0f;

	/** Class picker filter: concrete, instantiable subclasses of UHierarchicalTriggerComponentModule. */
	class FModuleClassFilter : public IClassViewerFilter
	{
	public:
		EClassFlags DisallowedClassFlags = CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists | CLASS_HideDropDown;
		TSet<const UClass*> AllowedChildrenOfClasses;

		virtual bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass,
			TSharedRef<FClassViewerFilterFuncs> InFilterFuncs) override
		{
			return InClass
				&& !InClass->HasAnyClassFlags(DisallowedClassFlags)
				&& InFilterFuncs->IfInChildOfClassesSet(AllowedChildrenOfClasses, InClass) != EFilterReturn::Failed;
		}

		virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions,
			const TSharedRef<const IUnloadedBlueprintData> InUnloadedClassData, TSharedRef<FClassViewerFilterFuncs> InFilterFuncs) override
		{
			return !InUnloadedClassData->HasAnyClassFlags(DisallowedClassFlags)
				&& InFilterFuncs->IfInChildOfClassesSet(AllowedChildrenOfClasses, InUnloadedClassData) != EFilterReturn::Failed;
		}
	};

	/**
	 * Clears the stable ids of a freshly copied subtree so UHierarchicalTriggerComponent::EnsureModuleIds
	 * mints new ones. Without this the copy carries the source's ids, and EnsureModuleIds is free to hand the
	 * contested id to the copy and reassign the original — silently repointing every FTriggerModuleReference
	 * that named it.
	 */
	void InvalidateModuleIds(UHierarchicalTriggerComponentModule* Module)
	{
		if (!IsValid(Module))
		{
			return;
		}

		Module->ModuleId.Invalidate();
		for (const TObjectPtr<UHierarchicalTriggerComponentModule>& Child : Module->Modules)
		{
			InvalidateModuleIds(Child);
		}
	}
}

TSharedRef<IDetailCustomization> FHierarchicalTriggerComponentDetails::MakeInstance()
{
	return MakeShared<FHierarchicalTriggerComponentDetails>();
}

void FHierarchicalTriggerComponentDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	PropertyUtilities = DetailBuilder.GetPropertyUtilities();

	// Resolve a single component to edit.
	TArray<TWeakObjectPtr<UObject>> Customized;
	DetailBuilder.GetObjectsBeingCustomized(Customized);
	UHierarchicalTriggerComponent* Comp = nullptr;
	int32 ComponentCount = 0;
	for (const TWeakObjectPtr<UObject>& Object : Customized)
	{
		if (UHierarchicalTriggerComponent* AsComp = Cast<UHierarchicalTriggerComponent>(Object.Get()))
		{
			Comp = AsComp;
			++ComponentCount;
		}
	}

	IDetailCategoryBuilder& Category = DetailBuilder.EditCategory("Trigger Component");

	// Take over rendering of the Modules array entirely.
	TSharedRef<IPropertyHandle> ModulesHandle =
		DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UHierarchicalTriggerComponent, Modules));
	DetailBuilder.HideProperty(ModulesHandle);

	if (ComponentCount != 1 || !Comp)
	{
		Category.AddCustomRow(LOCTEXT("ModulesFilter", "Modules")).WholeRowContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("MultiSelectMsg", "Select a single Trigger Component to edit its modules."))
		];
		return;
	}

	Component = Comp;
	ComponentKey = FObjectKey(Comp);
	Comp->EnsureModuleIds();

	// Shared details view embedded under the expanded module's row. No search/options/scrollbar/name area:
	// it should read as an inline block and let the outer details panel do the scrolling.
	FPropertyEditorModule& PropertyEditor = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs Args;
	Args.bAllowSearch = false;
	Args.bShowOptions = false;
	Args.bHideSelectionTip = true;
	Args.bShowScrollBar = false;
	Args.bAllowMultipleTopLevelObjects = false;
	Args.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	InlineDetailsView = PropertyEditor.CreateDetailView(Args);
	InlineDetailsView->SetIsPropertyVisibleDelegate(
		FIsPropertyVisible::CreateSP(this, &FHierarchicalTriggerComponentDetails::IsModuleDetailPropertyVisible));

	// Header toolbar: add a root module.
	Category.AddCustomRow(LOCTEXT("ModulesFilter", "Modules")).WholeRowContent()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			PropertyCustomizationHelpers::MakeAddButton(
				FSimpleDelegate::CreateSP(this, &FHierarchicalTriggerComponentDetails::AddModuleSlot, TSharedPtr<FModuleTreeNode>()),
				LOCTEXT("AddRootTip", "Add a root module slot"))
		]
		+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(6.0f, 0.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ModulesHeader", "Modules"))
			.Font(IDetailLayoutBuilder::GetDetailFontBold())
		]
	];

	// Build the node tree from the raw arrays and emit one row per slot (plus an inline details row when expanded).
	RootNodes.Reset();
	BuildNodes(Comp, Comp->Modules, nullptr, 0, RootNodes);

	if (RootNodes.Num() == 0)
	{
		Category.AddCustomRow(LOCTEXT("ModulesFilter", "Modules")).WholeRowContent()
		[
			SNew(SBox).Padding(FMargin(IndentPerDepth, 2.0f))
			[
				SNew(STextBlock).Text(LOCTEXT("NoModules", "No modules. Use + to add one."))
			]
		];
		return;
	}

	AddModuleRows(Category, RootNodes);
}

void FHierarchicalTriggerComponentDetails::BuildNodes(UObject* Owner,
	TArray<TObjectPtr<UHierarchicalTriggerComponentModule>>& Array, TSharedPtr<FModuleTreeNode> ParentNode, int32 Depth,
	TArray<TSharedPtr<FModuleTreeNode>>& OutNodes)
{
	for (int32 Index = 0; Index < Array.Num(); ++Index)
	{
		// Empty slots get a node too: that is where the class combo lives until a module is chosen.
		UHierarchicalTriggerComponentModule* Module = Array[Index];
		if (!IsValid(Module))
		{
			Module = nullptr;
		}

		TSharedRef<FModuleTreeNode> Node = MakeShared<FModuleTreeNode>();
		Node->Module = Module;
		Node->Owner = Owner;
		Node->SlotIndex = Index;
		Node->ModuleId = Module ? Module->GetModuleId() : FGuid();
		Node->Label = Module ? Module->GetClass()->GetDisplayNameText() : LOCTEXT("EmptySlotLabel", "Select Module");
		Node->Depth = Depth;
		Node->Parent = ParentNode;
		if (Module)
		{
			BuildNodes(Module, Module->Modules, Node, Depth + 1, Node->Children);
		}
		OutNodes.Add(Node);
	}
}

void FHierarchicalTriggerComponentDetails::AddModuleRows(IDetailCategoryBuilder& Category,
	const TArray<TSharedPtr<FModuleTreeNode>>& Nodes)
{
	for (const TSharedPtr<FModuleTreeNode>& Node : Nodes)
	{
		if (!Node.IsValid())
		{
			continue;
		}

		Category.AddCustomRow(Node->Label).WholeRowContent()
		[
			MakeHeaderWidget(Node)
		];

		if (IsExpanded(Node) && InlineDetailsView.IsValid() && Node->Module.IsValid())
		{
			// Accordion: only the selected node is expanded, so the shared view is bound exactly once.
			InlineDetailsView->SetObject(Node->Module.Get(), /*bForceRefresh*/ true);

			Category.AddCustomRow(Node->Label).WholeRowContent()
			[
				SNew(SBox)
				.Padding(FMargin((Node->Depth + 1) * IndentPerDepth, 0.0f, 0.0f, 6.0f))
				[
					InlineDetailsView.ToSharedRef()
				]
			];
		}

		AddModuleRows(Category, Node->Children);
	}
}

TSharedRef<SWidget> FHierarchicalTriggerComponentDetails::MakeHeaderWidget(TSharedPtr<FModuleTreeNode> Node)
{
	const bool bHasModule = Node->Module.IsValid();
	const bool bExpanded = IsExpanded(Node);

	// Controls that only make sense on a filled slot stay laid out but hidden, so every row's class combo
	// starts at the same x and the trailing buttons stay column-aligned.
	const EVisibility ModuleOnly = bHasModule ? EVisibility::Visible : EVisibility::Hidden;

	const TSharedPtr<FModuleTreeNode> ParentNode = Node->Parent.Pin();
	const int32 SiblingCount = ParentNode.IsValid() ? ParentNode->Children.Num() : RootNodes.Num();
	const bool bCanMoveUp = Node->SlotIndex > 0;
	const bool bCanMoveDown = Node->SlotIndex >= 0 && Node->SlotIndex < SiblingCount - 1;

	const FSlateBrush* ArrowBrush = FAppStyle::Get().GetBrush(bExpanded ? "TreeArrow_Expanded" : "TreeArrow_Collapsed");

	return SNew(SHorizontalBox)
	// Depth indent.
	+ SHorizontalBox::Slot().AutoWidth()
	[
		SNew(SBox).WidthOverride(Node->Depth * IndentPerDepth)
	]
	// Expand/collapse arrow.
	+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
	[
		SNew(SButton)
		.ButtonStyle(FAppStyle::Get(), "NoBorder")
		.ContentPadding(FMargin(2.0f, 0.0f))
		.Visibility(ModuleOnly)
		.OnClicked(this, &FHierarchicalTriggerComponentDetails::OnToggleExpand, Node)
		.ToolTipText(LOCTEXT("ExpandTip", "Show / hide this module's properties"))
		[
			SNew(SImage).Image(ArrowBrush)
		]
	]
	// Enable toggle: the primary switch keeps the familiar checkbox.
		
	+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(2.0f, 0.0f)
	[
		SNew(SCheckBox)
		.Visibility(ModuleOnly)
		.IsChecked(this, &FHierarchicalTriggerComponentDetails::GetFlagState, Node, EModuleFlag::Enabled)
		.OnCheckStateChanged(this, &FHierarchicalTriggerComponentDetails::OnFlagChanged, Node, EModuleFlag::Enabled)
		.ToolTipText(GetModuleFlagTooltip(EModuleFlag::Enabled))
	]
	// The two secondary flags ride along as compact labelled toggles rather than as details rows.
	+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(2.0f, 0.0f)
	[
		SNew(SBox)
		.Visibility(ModuleOnly)
		[
			MakeFlagToggleWidget(Node, EModuleFlag::ChildrenEnabled, LOCTEXT("ChildrenEnabledLabel", "Children"))
		]
	]
	+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 2.0f, 0.0f)
	[
		SNew(SBox)
		.Visibility(ModuleOnly)
		[
			MakeFlagToggleWidget(Node, EModuleFlag::TriggerOnce, LOCTEXT("TriggerOnceLabel", "Once"))
		]
	]
	// Class combo: names the module by its class display name, and swaps it for another class in place.
	+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
	[
		MakeClassComboWidget(Node, bExpanded)
	]
	// Add child.
	+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
	[
		SNew(SBox)
		.Visibility(ModuleOnly)
		[
			PropertyCustomizationHelpers::MakeAddButton(
				FSimpleDelegate::CreateSP(this, &FHierarchicalTriggerComponentDetails::AddModuleSlot, Node),
				LOCTEXT("AddChildTip", "Add a child module slot"))
		]
	]
	// Move up.
	+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
	[
		SNew(SButton)
		.ButtonStyle(FAppStyle::Get(), "SimpleButton")
		.IsEnabled(bCanMoveUp)
		.ToolTipText(LOCTEXT("MoveUpTip", "Move up among siblings"))
		.OnClicked_Lambda([this, Node]() { MoveModule(Node, -1); return FReply::Handled(); })
		[
			SNew(SImage).Image(FAppStyle::Get().GetBrush("Icons.ChevronUp"))
		]
	]
	// Move down.
	+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
	[
		SNew(SButton)
		.ButtonStyle(FAppStyle::Get(), "SimpleButton")
		.IsEnabled(bCanMoveDown)
		.ToolTipText(LOCTEXT("MoveDownTip", "Move down among siblings"))
		.OnClicked_Lambda([this, Node]() { MoveModule(Node, +1); return FReply::Handled(); })
		[
			SNew(SImage).Image(FAppStyle::Get().GetBrush("Icons.ChevronDown"))
		]
	]
	// Everything else.
	+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(2.0f, 0.0f)
	[
		SNew(SComboButton)
		.ComboButtonStyle(FAppStyle::Get(), "SimpleComboButton")
		.ContentPadding(2.0f)
		.ForegroundColor(FSlateColor::UseForeground())
		.HasDownArrow(false)
		.ToolTipText(LOCTEXT("ActionsTip", "Module actions"))
		.OnGetMenuContent(this, &FHierarchicalTriggerComponentDetails::MakeActionsMenu, Node)
		.ButtonContent()
		[
			SNew(SImage)
			.Image(FAppStyle::Get().GetBrush("Icons.EllipsisVerticalNarrow"))
			.ColorAndOpacity(FSlateColor::UseForeground())
		]
	];
}

TSharedRef<SWidget> FHierarchicalTriggerComponentDetails::MakeFlagToggleWidget(TSharedPtr<FModuleTreeNode> Node,
	EModuleFlag Flag, const FText& Label)
{
	return SNew(SCheckBox)
		.Style(FAppStyle::Get(), "DetailsView.SectionButton")
		.ToolTipText(GetModuleFlagTooltip(Flag))
		.IsChecked(this, &FHierarchicalTriggerComponentDetails::GetFlagState, Node, Flag)
		.OnCheckStateChanged(this, &FHierarchicalTriggerComponentDetails::OnFlagChanged, Node, Flag)
		[
			SNew(STextBlock)
			.Text(Label)
			.Font(IDetailLayoutBuilder::GetDetailFont())
		];
}

TSharedRef<SWidget> FHierarchicalTriggerComponentDetails::MakeClassComboWidget(TSharedPtr<FModuleTreeNode> Node, bool bExpanded)
{
	return SNew(SComboButton)
		.ComboButtonStyle(FAppStyle::Get(), "SimpleComboButton")
		.ContentPadding(FMargin(4.0f, 2.0f))
		.HAlign(HAlign_Left)
		.ToolTipText(this, &FHierarchicalTriggerComponentDetails::GetModuleTooltip, Node)
		.OnGetMenuContent(this, &FHierarchicalTriggerComponentDetails::OnGetClassPickerContent, Node)
		.ButtonContent()
		[
			SNew(STextBlock)
			.Text(this, &FHierarchicalTriggerComponentDetails::GetModuleLabel, Node)
			.Font(bExpanded ? IDetailLayoutBuilder::GetDetailFontBold() : IDetailLayoutBuilder::GetDetailFont())
		];
}

TSharedRef<SWidget> FHierarchicalTriggerComponentDetails::OnGetClassPickerContent(TSharedPtr<FModuleTreeNode> Node)
{
	FClassViewerModule& ClassViewer = FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer");

	FClassViewerInitializationOptions Options;
	Options.Mode = EClassViewerMode::ClassPicker;
	// A flat, searchable list of display names: the inheritance tree of full class names is noise here.
	Options.DisplayMode = EClassViewerDisplayMode::ListView;
	Options.NameTypeToDisplay = EClassViewerNameTypeToDisplay::DisplayName;
	Options.bShowNoneOption = true;
	Options.bAllowViewOptions = false;
	Options.bShowBackgroundBorder = false;
	// Deliberately no InitiallySelectedClass: SClassViewer::Populate answers it with ClassList->SetSelection,
	// which reports ESelectInfo::Direct and so fires OnClassPicked with the class the slot already holds
	// (SClassViewer.cpp:2620). The menu would dismiss itself the instant it opened, making a filled slot look
	// unclickable. The combo's own label already says which class is in the slot.

	const TSharedRef<FModuleClassFilter> Filter = MakeShared<FModuleClassFilter>();
	Filter->AllowedChildrenOfClasses.Add(UHierarchicalTriggerComponentModule::StaticClass());
	Options.ClassFilters.Add(Filter);

	return SNew(SBox)
		.WidthOverride(280.0f)
		.HeightOverride(320.0f)
		[
			ClassViewer.CreateClassViewer(Options,
				FOnClassPicked::CreateSP(this, &FHierarchicalTriggerComponentDetails::OnModuleClassPicked, Node))
		];
}

TSharedRef<SWidget> FHierarchicalTriggerComponentDetails::MakeActionsMenu(TSharedPtr<FModuleTreeNode> Node)
{
	const bool bHasModule = Node.IsValid() && Node->Module.IsValid();
	const FName StyleName = FAppStyle::GetAppStyleSetName();

	FMenuBuilder MenuBuilder(/*bShouldCloseWindowAfterMenuSelection*/ true, nullptr, nullptr, /*bCloseSelfOnly*/ true);

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("HierarchySection", "Hierarchy"));
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("MakeParentLabel", "Make Parent"),
			LOCTEXT("MakeParentTip", "Move this module into its parent's slot. The former parent, with the rest of its subtree, becomes this module's first child."),
			FSlateIcon(StyleName, "Icons.ArrowUp"),
			FUIAction(
				FExecuteAction::CreateSP(this, &FHierarchicalTriggerComponentDetails::MakeParent, Node),
				FCanExecuteAction::CreateSP(this, &FHierarchicalTriggerComponentDetails::CanMakeParent, Node)));
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("EditSection", "Edit"));
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("CopyLabel", "Copy"),
			LOCTEXT("CopyTip", "Copy this module, so it can be pasted into another slot"),
			FSlateIcon(StyleName, "GenericCommands.Copy"),
			FUIAction(
				FExecuteAction::CreateSP(this, &FHierarchicalTriggerComponentDetails::CopyModule, Node),
				FCanExecuteAction::CreateLambda([bHasModule]() { return bHasModule; })));

		MenuBuilder.AddMenuEntry(
			LOCTEXT("PasteLabel", "Paste"),
			LOCTEXT("PasteTip", "Replace the contents of this slot with a copy of the module on the clipboard"),
			FSlateIcon(StyleName, "GenericCommands.Paste"),
			FUIAction(
				FExecuteAction::CreateSP(this, &FHierarchicalTriggerComponentDetails::PasteModule, Node),
				FCanExecuteAction::CreateSP(this, &FHierarchicalTriggerComponentDetails::CanPasteModule, Node)));

		MenuBuilder.AddMenuEntry(
			LOCTEXT("DuplicateLabel", "Duplicate"),
			LOCTEXT("DuplicateTip", "Insert a copy of this module, and its children, after it"),
			FSlateIcon(StyleName, "GenericCommands.Duplicate"),
			FUIAction(
				FExecuteAction::CreateSP(this, &FHierarchicalTriggerComponentDetails::DuplicateModule, Node),
				FCanExecuteAction::CreateLambda([bHasModule]() { return bHasModule; })));

		MenuBuilder.AddMenuEntry(
			LOCTEXT("DeleteLabel", "Delete"),
			LOCTEXT("DeleteTip", "Remove this slot, and everything under it"),
			FSlateIcon(StyleName, "GenericCommands.Delete"),
			FUIAction(FExecuteAction::CreateSP(this, &FHierarchicalTriggerComponentDetails::DeleteModule, Node)));
	}
	MenuBuilder.EndSection();
	
	UClass* ModuleClass = bHasModule ? Node->Module->GetClass() : nullptr;
	const UBlueprintGeneratedClass* BlueprintClass = Cast<UBlueprintGeneratedClass>(ModuleClass);
	const bool bCanOpenSource = ModuleClass && !BlueprintClass && FSourceCodeNavigation::CanNavigateToClass(ModuleClass);
	// A cooked Blueprint has no UBlueprint left to open, but can still be found in the content browser.
	const bool bCanEditBlueprint = BlueprintClass && BlueprintClass->ClassGeneratedBy;

	if (bCanOpenSource || BlueprintClass)
	{
		MenuBuilder.BeginSection(NAME_None, LOCTEXT("SourceSection", "Source"));
		{
			if (bCanOpenSource)
			{
				MenuBuilder.AddMenuEntry(
					LOCTEXT("OpenSourceLabel", "Open Source Code"),
					FText::Format(LOCTEXT("OpenSourceTip", "Open this module's source file in {0}"),
						FSourceCodeNavigation::GetSelectedSourceCodeIDE()),
					FSlateIcon(StyleName, "Icons.OpenSourceLocation"),
					FUIAction(FExecuteAction::CreateSP(this, &FHierarchicalTriggerComponentDetails::OpenModuleSource, Node)));
			}

			if (BlueprintClass)
			{
				MenuBuilder.AddMenuEntry(
					LOCTEXT("BrowseToBlueprintLabel", "Browse to Blueprint"),
					LOCTEXT("BrowseToBlueprintTip", "Show this module's Blueprint in the Content Browser"),
					FSlateIcon(StyleName, "Icons.BrowseContent"),
					FUIAction(FExecuteAction::CreateSP(this, &FHierarchicalTriggerComponentDetails::BrowseToModuleBlueprint, Node)));
			}

			if (bCanEditBlueprint)
			{
				MenuBuilder.AddMenuEntry(
					LOCTEXT("EditBlueprintLabel", "Edit Blueprint"),
					LOCTEXT("EditBlueprintTip", "Open this module's Blueprint in the Blueprint editor"),
					FSlateIcon(StyleName, "Icons.Edit"),
					FUIAction(FExecuteAction::CreateSP(this, &FHierarchicalTriggerComponentDetails::EditModuleBlueprint, Node)));
			}
		}
		MenuBuilder.EndSection();
	}

	return MenuBuilder.MakeWidget();
}

FReply FHierarchicalTriggerComponentDetails::OnToggleExpand(TSharedPtr<FModuleTreeNode> Node)
{
	if (Node.IsValid())
	{
		FViewState& State = GetViewState();
		State.Selected = (State.Selected == Node->ModuleId) ? FGuid() : Node->ModuleId;
		RequestRefresh();
	}
	return FReply::Handled();
}

bool FHierarchicalTriggerComponentDetails::IsExpanded(const TSharedPtr<FModuleTreeNode>& Node) const
{
	return Node.IsValid() && Node->ModuleId.IsValid() && Node->ModuleId == GetViewState().Selected;
}

ECheckBoxState FHierarchicalTriggerComponentDetails::GetFlagState(TSharedPtr<FModuleTreeNode> Node, EModuleFlag Flag) const
{
	if (Node.IsValid() && Node->Module.IsValid())
	{
		return GetModuleFlag(*Node->Module.Get(), Flag) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}
	return ECheckBoxState::Undetermined;
}

void FHierarchicalTriggerComponentDetails::OnFlagChanged(ECheckBoxState NewState, TSharedPtr<FModuleTreeNode> Node,
	EModuleFlag Flag)
{
	if (!Node.IsValid() || !Node->Module.IsValid())
	{
		return;
	}

	UHierarchicalTriggerComponentModule* Module = Node->Module.Get();
	FScopedTransaction Transaction(LOCTEXT("ToggleFlagTx", "Toggle Module Flag"));
	Module->Modify();
	SetModuleFlag(*Module, Flag, NewState == ECheckBoxState::Checked);

	if (FProperty* Property = FindFProperty<FProperty>(Module->GetClass(), GetModuleFlagPropertyName(Flag)))
	{
		FPropertyChangedEvent ChangeEvent(Property);
		Module->PostEditChangeProperty(ChangeEvent);
	}
	// No refresh: the toggles read the flags live, and we don't want to collapse the row.
}

void FHierarchicalTriggerComponentDetails::OnModuleClassPicked(UClass* PickedClass, TSharedPtr<FModuleTreeNode> Node)
{
	// Close the picker before the refresh below tears these widgets down.
	FSlateApplication::Get().DismissAllMenus();
	SetModuleClass(Node, PickedClass);
}

void FHierarchicalTriggerComponentDetails::AddModuleSlot(TSharedPtr<FModuleTreeNode> ParentNode)
{
	UHierarchicalTriggerComponent* Comp = GetComponent();
	if (!Comp)
	{
		return;
	}

	UObject* Owner = ParentNode.IsValid() ? Cast<UObject>(ParentNode->Module.Get()) : Cast<UObject>(Comp);
	TArray<TObjectPtr<UHierarchicalTriggerComponentModule>>* Array = GetChildArray(Owner);
	if (!Owner || !Array)
	{
		return;
	}

	// Empty slot only: the class combo on the new row decides what fills it. Null entries are skipped by
	// every runtime traversal, so an unfilled slot is inert rather than broken.
	FScopedTransaction Transaction(LOCTEXT("AddModuleTx", "Add Trigger Module Slot"));
	Comp->Modify();
	Owner->Modify();
	Array->Add(nullptr);

	NotifyModulesChanged(Owner, EPropertyChangeType::ArrayAdd);
	RequestRefresh();
}

void FHierarchicalTriggerComponentDetails::SetModuleClass(TSharedPtr<FModuleTreeNode> Node, UClass* NewClass)
{
	UHierarchicalTriggerComponent* Comp = GetComponent();
	const FModuleSlot Slot = ResolveSlot(Node);
	if (!Comp || !Slot.IsValid())
	{
		return;
	}

	UHierarchicalTriggerComponentModule* Old = (*Slot.Array)[Slot.Index];
	if (!IsValid(Old))
	{
		Old = nullptr;
	}

	if ((Old ? Old->GetClass() : nullptr) == NewClass)
	{
		return;
	}

	FScopedTransaction Transaction(LOCTEXT("SetModuleClassTx", "Set Trigger Module Class"));
	Comp->Modify();
	Slot.Owner->Modify();

	UHierarchicalTriggerComponentModule* NewModule = nullptr;
	if (NewClass)
	{
		NewModule = NewObject<UHierarchicalTriggerComponentModule>(Slot.Owner, NewClass, NAME_None, RF_Transactional);

		if (Old)
		{
			// Swapping the type keeps everything that is not specific to the old class: the subtree, the
			// shared flags, and the stable id, so existing FTriggerModuleReferences still resolve. Only the
			// old module's own properties are lost, which is the point of the swap.
			Old->Modify();
			NewModule->bEnabled = Old->bEnabled;
			NewModule->bChildrenEnabled = Old->bChildrenEnabled;
			NewModule->bTriggerOnce = Old->bTriggerOnce;
			NewModule->ModuleId = Old->ModuleId;
			NewModule->Modules = MoveTemp(Old->Modules);
			Old->Modules.Reset();

			for (const TObjectPtr<UHierarchicalTriggerComponentModule>& Child : NewModule->Modules)
			{
				if (IsValid(Child))
				{
					Child->Modify();
					Child->Rename(nullptr, NewModule, REN_DontCreateRedirectors);
				}
			}
		}
	}

	(*Slot.Array)[Slot.Index] = NewModule;

	NotifyModulesChanged(Slot.Owner, EPropertyChangeType::ValueSet);
	Comp->EnsureModuleIds();

	GetViewState().Selected = NewModule ? NewModule->GetModuleId() : FGuid();
	RequestRefresh();
}

void FHierarchicalTriggerComponentDetails::DeleteModule(TSharedPtr<FModuleTreeNode> Node)
{
	UHierarchicalTriggerComponent* Comp = GetComponent();
	const FModuleSlot Slot = ResolveSlot(Node);
	if (!Comp || !Slot.IsValid())
	{
		return;
	}

	FScopedTransaction Transaction(LOCTEXT("DeleteModuleTx", "Delete Trigger Module"));
	Comp->Modify();
	Slot.Owner->Modify();
	Slot.Array->RemoveAt(Slot.Index);

	NotifyModulesChanged(Slot.Owner, EPropertyChangeType::ArrayRemove);
	Comp->EnsureModuleIds();

	FViewState& State = GetViewState();
	if (State.Selected == Node->ModuleId)
	{
		State.Selected = FGuid();
	}
	RequestRefresh();
}

void FHierarchicalTriggerComponentDetails::DuplicateModule(TSharedPtr<FModuleTreeNode> Node)
{
	UHierarchicalTriggerComponent* Comp = GetComponent();
	const FModuleSlot Slot = ResolveSlot(Node);
	if (!Comp || !Slot.IsValid() || !Node->Module.IsValid())
	{
		return;
	}

	FScopedTransaction Transaction(LOCTEXT("DuplicateModuleTx", "Duplicate Trigger Module"));
	Comp->Modify();
	Slot.Owner->Modify();

	UHierarchicalTriggerComponentModule* Duplicate =
		DuplicateObject<UHierarchicalTriggerComponentModule>(Node->Module.Get(), Slot.Owner);
	if (!Duplicate)
	{
		return;
	}
	Duplicate->SetFlags(RF_Transactional);
	InvalidateModuleIds(Duplicate);
	Slot.Array->Insert(Duplicate, Slot.Index + 1);

	NotifyModulesChanged(Slot.Owner, EPropertyChangeType::Duplicate);
	Comp->EnsureModuleIds();

	GetViewState().Selected = Duplicate->GetModuleId();
	RequestRefresh();
}

void FHierarchicalTriggerComponentDetails::MoveModule(TSharedPtr<FModuleTreeNode> Node, int32 Delta)
{
	UHierarchicalTriggerComponent* Comp = GetComponent();
	const FModuleSlot Slot = ResolveSlot(Node);
	if (!Comp || !Slot.IsValid() || Delta == 0)
	{
		return;
	}

	const int32 NewIndex = Slot.Index + Delta;
	if (!Slot.Array->IsValidIndex(NewIndex))
	{
		return;
	}

	FScopedTransaction Transaction(LOCTEXT("MoveModuleTx", "Reorder Trigger Module"));
	Comp->Modify();
	Slot.Owner->Modify();
	Slot.Array->Swap(Slot.Index, NewIndex);

	NotifyModulesChanged(Slot.Owner, EPropertyChangeType::Unspecified);
	RequestRefresh();
}

bool FHierarchicalTriggerComponentDetails::CanMakeParent(TSharedPtr<FModuleTreeNode> Node) const
{
	if (!Node.IsValid() || !Node->Module.IsValid())
	{
		return false;
	}

	const TSharedPtr<FModuleTreeNode> ParentNode = Node->Parent.Pin();
	return ParentNode.IsValid() && ParentNode->Module.IsValid();
}

void FHierarchicalTriggerComponentDetails::MakeParent(TSharedPtr<FModuleTreeNode> Node)
{
	UHierarchicalTriggerComponent* Comp = GetComponent();
	if (!Comp || !CanMakeParent(Node))
	{
		return;
	}

	const TSharedPtr<FModuleTreeNode> ParentNode = Node->Parent.Pin();
	const FModuleSlot ParentSlot = ResolveSlot(ParentNode);
	if (!ParentSlot.IsValid())
	{
		return;
	}

	UHierarchicalTriggerComponentModule* Module = Node->Module.Get();
	UHierarchicalTriggerComponentModule* Parent = ParentNode->Module.Get();
	const int32 ChildIndex = Parent->Modules.IndexOfByKey(Module);
	if (ChildIndex == INDEX_NONE)
	{
		return;
	}

	FScopedTransaction Transaction(LOCTEXT("MakeParentTx", "Promote Trigger Module"));
	Comp->Modify();
	ParentSlot.Owner->Modify();
	Parent->Modify();
	Module->Modify();

	// Swap the two levels rather than discarding anything: the module takes the parent's slot, and the parent
	// keeps the rest of its own subtree while becoming this module's first child. Applying this to the
	// demoted parent puts the hierarchy back.
	Parent->Modules.RemoveAt(ChildIndex);
	Module->Rename(nullptr, ParentSlot.Owner, REN_DontCreateRedirectors);
	(*ParentSlot.Array)[ParentSlot.Index] = Module;
	Parent->Rename(nullptr, Module, REN_DontCreateRedirectors);
	Module->Modules.Insert(Parent, 0);

	NotifyModulesChanged(ParentSlot.Owner, EPropertyChangeType::ValueSet);
	Comp->EnsureModuleIds();

	GetViewState().Selected = Module->GetModuleId();
	RequestRefresh();
}

void FHierarchicalTriggerComponentDetails::CopyModule(TSharedPtr<FModuleTreeNode> Node)
{
	if (!Node.IsValid() || !Node->Module.IsValid())
	{
		return;
	}

	// Same clipboard format the details panel uses for an object property, so a module can be copied here and
	// pasted into any other object reference that accepts it (and the other way round).
	const FString ExportPath = FObjectPropertyBase::GetExportPath(Node->Module.Get(), nullptr, nullptr, PPF_Copy);
	FPlatformApplicationMisc::ClipboardCopy(*ExportPath);
}

void FHierarchicalTriggerComponentDetails::OpenModuleSource(TSharedPtr<FModuleTreeNode> Node)
{
	if (Node.IsValid() && Node->Module.IsValid())
	{
		FSourceCodeNavigation::NavigateToClass(Node->Module->GetClass());
	}
}

void FHierarchicalTriggerComponentDetails::BrowseToModuleBlueprint(TSharedPtr<FModuleTreeNode> Node)
{
	if (!Node.IsValid() || !Node->Module.IsValid() || !GEditor)
	{
		return;
	}

	if (UBlueprintGeneratedClass* BlueprintClass = Cast<UBlueprintGeneratedClass>(Node->Module->GetClass()))
	{
		// A cooked Blueprint has had its UBlueprint stripped, so fall back to the generated class itself.
		GEditor->SyncBrowserToObject(BlueprintClass->ClassGeneratedBy
			? BlueprintClass->ClassGeneratedBy.Get()
			: static_cast<UObject*>(BlueprintClass));
	}
}

void FHierarchicalTriggerComponentDetails::EditModuleBlueprint(TSharedPtr<FModuleTreeNode> Node)
{
	if (!Node.IsValid() || !Node->Module.IsValid() || !GEditor)
	{
		return;
	}

	const UBlueprintGeneratedClass* BlueprintClass = Cast<UBlueprintGeneratedClass>(Node->Module->GetClass());
	if (BlueprintClass && BlueprintClass->ClassGeneratedBy)
	{
		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(BlueprintClass->ClassGeneratedBy);
	}
}

UHierarchicalTriggerComponentModule* FHierarchicalTriggerComponentDetails::ResolveClipboardModule()
{
	FString ClipboardText;
	FPlatformApplicationMisc::ClipboardPaste(ClipboardText);
	ClipboardText.TrimStartAndEndInline();
	if (ClipboardText.IsEmpty())
	{
		return nullptr;
	}

	// The clipboard holds a path, so the paste is a duplicate of a live module: any module property will do
	// as the parse context, and the array's inner is the one that carries the right metadata.
	const FArrayProperty* ModulesProperty = FindFProperty<FArrayProperty>(
		UHierarchicalTriggerComponent::StaticClass(), ModulesPropertyName);
	const FProperty* InnerProperty = ModulesProperty ? ModulesProperty->Inner : nullptr;
	if (!InnerProperty)
	{
		return nullptr;
	}

	const TCHAR* Buffer = *ClipboardText;
	TObjectPtr<UObject> Resolved = nullptr;
	FObjectPropertyBase::ParseObjectPropertyValue(InnerProperty, nullptr,
		UHierarchicalTriggerComponentModule::StaticClass(), PPF_None, Buffer, Resolved, nullptr,
		/*bAllowAnyPackage*/ false);

	return Cast<UHierarchicalTriggerComponentModule>(Resolved.Get());
}

bool FHierarchicalTriggerComponentDetails::CanPasteModule(TSharedPtr<FModuleTreeNode> Node) const
{
	return Node.IsValid() && ResolveClipboardModule() != nullptr;
}

void FHierarchicalTriggerComponentDetails::PasteModule(TSharedPtr<FModuleTreeNode> Node)
{
	UHierarchicalTriggerComponent* Comp = GetComponent();
	UHierarchicalTriggerComponentModule* Source = ResolveClipboardModule();
	const FModuleSlot Slot = ResolveSlot(Node);
	if (!Comp || !Source || !Slot.IsValid())
	{
		return;
	}

	FScopedTransaction Transaction(LOCTEXT("PasteModuleTx", "Paste Trigger Module"));
	Comp->Modify();
	Slot.Owner->Modify();

	UHierarchicalTriggerComponentModule* Pasted =
		DuplicateObject<UHierarchicalTriggerComponentModule>(Source, Slot.Owner);
	if (!Pasted)
	{
		return;
	}
	Pasted->SetFlags(RF_Transactional);
	InvalidateModuleIds(Pasted);
	(*Slot.Array)[Slot.Index] = Pasted;

	NotifyModulesChanged(Slot.Owner, EPropertyChangeType::ValueSet);
	Comp->EnsureModuleIds();

	GetViewState().Selected = Pasted->GetModuleId();
	RequestRefresh();
}

FText FHierarchicalTriggerComponentDetails::GetModuleLabel(TSharedPtr<FModuleTreeNode> Node) const
{
	if (Node.IsValid() && Node->Module.IsValid())
	{
		return Node->Module->GetClass()->GetDisplayNameText();
	}
	return LOCTEXT("EmptySlotLabel", "Select Module");
}

FText FHierarchicalTriggerComponentDetails::GetModuleTooltip(TSharedPtr<FModuleTreeNode> Node) const
{
	if (Node.IsValid() && Node->Module.IsValid())
	{
		const FText ClassTooltip = Node->Module->GetClass()->GetToolTipText();
		if (!ClassTooltip.IsEmpty())
		{
			return ClassTooltip;
		}
		return LOCTEXT("ReplaceModuleTip", "Replace this module with another type. Children and shared settings are kept.");
	}
	return LOCTEXT("PickModuleTip", "Choose the module type for this slot");
}

FHierarchicalTriggerComponentDetails::FModuleSlot FHierarchicalTriggerComponentDetails::ResolveSlot(
	const TSharedPtr<FModuleTreeNode>& Node) const
{
	FModuleSlot Slot;
	if (!Node.IsValid())
	{
		return Slot;
	}

	UObject* Owner = Node->Owner.Get();
	TArray<TObjectPtr<UHierarchicalTriggerComponentModule>>* Array = GetChildArray(Owner);
	if (!Owner || !Array)
	{
		return Slot;
	}

	// Every mutation rebuilds the tree, so the cached index is normally exact; fall back to a search if
	// something moved the entry from under us.
	int32 Index = Node->SlotIndex;
	if (!Array->IsValidIndex(Index) || (*Array)[Index] != Node->Module.Get())
	{
		Index = Node->Module.IsValid() ? Array->IndexOfByKey(Node->Module.Get()) : INDEX_NONE;
	}
	if (Index == INDEX_NONE)
	{
		return Slot;
	}

	Slot.Owner = Owner;
	Slot.Array = Array;
	Slot.Index = Index;
	return Slot;
}

bool FHierarchicalTriggerComponentDetails::IsModuleDetailPropertyVisible(const FPropertyAndParent& PropertyAndParent) const
{
	const FName PropertyName = PropertyAndParent.Property.GetFName();
	return PropertyName != ModulesPropertyName
		&& PropertyName != ModuleIdPropertyName
		&& PropertyName != EnabledPropertyName
		&& PropertyName != ChildrenEnabledPropertyName
		&& PropertyName != TriggerOncePropertyName;
}

UHierarchicalTriggerComponent* FHierarchicalTriggerComponentDetails::GetComponent() const
{
	return Component.Get();
}

void FHierarchicalTriggerComponentDetails::NotifyModulesChanged(UObject* Owner, EPropertyChangeType::Type ChangeType) const
{
	if (!Owner)
	{
		return;
	}
	if (FProperty* Property = FindFProperty<FProperty>(Owner->GetClass(), ModulesPropertyName))
	{
		FPropertyChangedEvent ChangeEvent(Property, ChangeType);
		Owner->PostEditChangeProperty(ChangeEvent);
	}
}

void FHierarchicalTriggerComponentDetails::RequestRefresh()
{
	if (PropertyUtilities.IsValid())
	{
		PropertyUtilities->RequestForceRefresh();
	}
}

TArray<TObjectPtr<UHierarchicalTriggerComponentModule>>* FHierarchicalTriggerComponentDetails::GetChildArray(UObject* Owner)
{
	if (UHierarchicalTriggerComponent* AsComponent = Cast<UHierarchicalTriggerComponent>(Owner))
	{
		return &AsComponent->Modules;
	}
	if (UHierarchicalTriggerComponentModule* AsModule = Cast<UHierarchicalTriggerComponentModule>(Owner))
	{
		return &AsModule->Modules;
	}
	return nullptr;
}

FHierarchicalTriggerComponentDetails::FViewState& FHierarchicalTriggerComponentDetails::GetViewState() const
{
	// Static so expansion survives the ForceRefresh that recreates this customization instance.
	static TMap<FObjectKey, FViewState> ViewStates;
	return ViewStates.FindOrAdd(ComponentKey);
}

#undef LOCTEXT_NAMESPACE
