import unreal

for asset_path in ['/Game/CardGame/UI/WBP_CG_HandWidget', '/Game/CardGame/UI/WBP_CG_BoardWidget']:
    asset = unreal.EditorAssetLibrary.load_asset(asset_path)
    if not asset:
        unreal.log_error(f'NOT_FOUND:{asset_path}')
        continue

    tag = unreal.EditorAssetLibrary.get_metadata_tag(asset, 'CG_UI_LAYOUT')
    unreal.log(f'UI_TAG:{asset_path}:{tag}')

    try:
        tree = asset.get_editor_property('widget_tree')
        root = tree.get_editor_property('root_widget')
        root_name = root.get_name() if root else 'None'
        unreal.log(f'ROOT:{asset_path}:{root_name}')
    except Exception as exc:
        unreal.log_warning(f'ROOTERR:{asset_path}:{exc}')

unreal.SystemLibrary.quit_editor()
