using UnrealBuildTool;

public class CardGame : ModuleRules
{
	public CardGame(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "UMG", "Slate", "SlateCore" });

		// SteamSockets経由のインターネット越し接続(docs/online-play-design.md
		// 「SteamSocketsによるインターネット越し接続」)用。自分のSteamIDを
		// 取得するIOnlineIdentityインターフェースのために必要
		// (UCGLobbyHUD::GetLocalSteamIdString)。NetDriver自体の切り替えは
		// DefaultEngine.iniの設定のみで完結し、コード側の依存はこれだけで済む。
		PrivateDependencyModuleNames.AddRange(new string[] { "OnlineSubsystem", "OnlineSubsystemUtils" });
	}
}
