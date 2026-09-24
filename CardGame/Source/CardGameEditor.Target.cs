using UnrealBuildTool;
using System.Collections.Generic;

public class CardGameEditorTarget : TargetRules
{
	public CardGameEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("CardGame");
	}
}
