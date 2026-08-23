import unreal

ASSET_NAME = "BP_CG_GameState"
PACKAGE_PATH = "/Game/CardGame/Core"

if unreal.EditorAssetLibrary.does_asset_exist(f"{PACKAGE_PATH}/{ASSET_NAME}"):
    unreal.log_warning(f"{ASSET_NAME} already exists")
    unreal.SystemLibrary.quit_editor()

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
factory = unreal.BlueprintFactory()
factory.set_editor_property("ParentClass", unreal.GameStateBase)

blueprint = asset_tools.create_asset(ASSET_NAME, PACKAGE_PATH, unreal.Blueprint, factory)
if not blueprint:
    raise RuntimeError(f"Failed to create {ASSET_NAME}")

unreal.EditorAssetLibrary.save_loaded_asset(blueprint)
unreal.log(f"Created {PACKAGE_PATH}/{ASSET_NAME}")
try:
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
except Exception:
    pass

unreal.SystemLibrary.quit_editor()
