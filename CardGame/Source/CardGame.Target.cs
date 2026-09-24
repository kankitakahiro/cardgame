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

		// bUseLoggingInShippingを試したが、Epic Games Launcher配布のインストール版
		// エンジンではUniqueビルド環境が使えず("Targets with a unique build
		// environment cannot be built with an installed engine")、代わりの
		// bOverrideBuildEnvironment=trueは共有PCH経由でログカテゴリの型
		// (FLogCategoryLogType/FNoLoggingCategory)がオブジェクトファイル間で
		// 食い違いLNK2001になったため断念。Shipping配布時の調査が必要な場合は
		// 一時的にDevelopment構成でパッケージすること。
	}
}
