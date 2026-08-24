using UnrealBuildTool;
using System.Collections.Generic;

public class CardGameTarget : TargetRules
{
	public CardGameTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("CardGame");
	}
}
