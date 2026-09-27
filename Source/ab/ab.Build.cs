using UnrealBuildTool;

public class ab : ModuleRules
{
	public ab(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"NavigationSystem",
			"GameplayTasks",
			"PhysicsCore",
			"ProceduralMeshComponent",
			"Landscape",
			"Learning",
			"LearningTraining",
			"LearningAgents",
			"LearningAgentsTraining",
			"MeshDescription",
			"StaticMeshDescription"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"MovieSceneCapture",
			"ImageWrapper",
			"Slate",
			"SlateCore",
			"RenderCore",
			"RHI",
			"HTTP",
			"Json",
			"AssetRegistry"
		});

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[] {
				"UnrealEd",
				"AssetRegistry",
				"AssetTools"
			});
		}
	}
}
