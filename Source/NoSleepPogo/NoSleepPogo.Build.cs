using UnrealBuildTool;

public class NoSleepPogo : ModuleRules
{
	public NoSleepPogo(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput",
			"Slate", "SlateCore", "UMG"
		});
	}
}
