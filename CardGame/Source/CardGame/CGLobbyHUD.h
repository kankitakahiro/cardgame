#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CGLobbyHUD.generated.h"

// ロビー画面。「デッキ構築」「バトル開始」の2ボタンのみを持つ最小構成。
// UIはCGGameHUDと同じくC++側でWidgetTreeを組み立てる
// (docs/architecture.md「レベルと画面遷移」、UMG編集の制約はdocs/automation-notes.md参照)。
UCLASS()
class UCGLobbyHUD : public UUserWidget
{
	GENERATED_BODY()

protected:
	// WidgetTree->RootWidgetは NativeConstruct() ではなく RebuildWidget() の中で
	// 設定しないと、Slate側が先に空のプレースホルダー(SSpacer)を取得してキャッシュ
	// してしまい、後からRootWidgetを設定しても画面に反映されない
	// (docs/automation-notes.mdの黒画面バグ参照)。
	virtual TSharedRef<SWidget> RebuildWidget() override;

	void EnsureWidgetTreeBuilt();
	bool bWidgetTreeBuilt = false;

	UFUNCTION()
	void HandleDeckBuilderClicked();

	UFUNCTION()
	void HandleStartBattleClicked();
};
