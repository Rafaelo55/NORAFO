using UnrealBuildTool;
using System.Collections.Generic;

public class NoSleepPogoEditorTarget : TargetRules
{
	public NoSleepPogoEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("NoSleepPogo");
	}
}
