
using UnrealBuildTool;

public class ModularTrigger : ModuleRules
{
	public ModularTrigger(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				// ActorEventCall.h is a public header including StructUtils/PropertyBag.h and GameFramework/Actor.h.
				"CoreUObject",
				"Engine",
				// Public: TriggerComponentModule_WorldConditionFilter.h holds an FWorldConditionQueryDefinition and
				// the conditions in WorldConditions/ are public extension points.
				"WorldConditions"
				// ... add other public dependencies that you statically link with here ...
			}
			);


		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				// TriggerComponentModule_PlayLevelSequence drives an ALevelSequenceActor's player.
				// Both are engine runtime modules, not plugins, so nothing has to be enabled for this.
				"LevelSequence",
				"MovieScene",
				"Slate",
				"SlateCore",
				// ... add private dependencies that you statically link with here ...
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
