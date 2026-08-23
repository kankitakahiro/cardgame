import unreal

WIDGETS = [
    ('WBP_CG_HandWidget', '/Game/CardGame/UI'),
    ('WBP_CG_BoardWidget', '/Game/CardGame/UI'),
]

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
factory = unreal.WidgetBlueprintFactory()
factory.set_editor_property('ParentClass', unreal.UserWidget)

for asset_name, package_path in WIDGETS:
    asset_path = f'{package_path}/{asset_name}'
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        unreal.log_warning(f'{asset_name} already exists')
        continue

    blueprint = asset_tools.create_asset(asset_name, package_path, unreal.Blueprint, factory)
    if not blueprint:
        raise RuntimeError(f'Failed to create {asset_name}')

    unreal.EditorAssetLibrary.save_loaded_asset(blueprint)
    unreal.log(f'Created {asset_path}')

try:
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
except Exception:
    pass

unreal.SystemLibrary.quit_editor()
