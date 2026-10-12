
using UnrealBuildTool;

public class ModularTriggerEditor : ModuleRules
{
	public ModularTriggerEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"ApplicationCore",
				"Core",
				"CoreUObject",
				"DeveloperSettings",
				"Engine",
				"InputCore",
				"Settings",
				"Slate",
				"SlateCore",
				"ToolMenus",
				"UnrealEd",
				"PropertyEditor",
				"ModularTrigger"
			}
		);
	}
}
