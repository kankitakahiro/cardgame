import unreal

CORE_PATH = '/Game/CardGame/Core'

TARGETS = [
    ('BP_CG_PlayerState', 'CGPlayerState'),
    ('BP_CG_GameState', 'CGGameState'),
    ('BP_CG_GameMode', 'CGGameMode'),
]

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
created = {}

for asset_name, cpp_class_name in TARGETS:
    asset_path = f'{CORE_PATH}/{asset_name}'

    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        deleted = unreal.EditorAssetLibrary.delete_asset(asset_path)
        unreal.log(f'Deleted old {asset_path}: {deleted}')

    cpp_class = getattr(unreal, cpp_class_name, None)
    if cpp_class is None:
        raise RuntimeError(f'C++ class not found in unreal module: {cpp_class_name}. Build may not have loaded.')

    factory = unreal.BlueprintFactory()
    factory.set_editor_property('ParentClass', cpp_class)
    blueprint = asset_tools.create_asset(asset_name, CORE_PATH, unreal.Blueprint, factory)
    if not blueprint:
        raise RuntimeError(f'Failed to create {asset_path}')

    created[asset_name] = blueprint
    unreal.log(f'Created {asset_path} (parent={cpp_class_name})')

# BP_CG_GameMode の GameStateClass / PlayerStateClass を、C++既定(ACGGameState/ACGPlayerState)
# ではなく BP_CG_GameState / BP_CG_PlayerState に向ける(将来Blueprint側で拡張できるように)。
game_mode_bp = created['BP_CG_GameMode']
game_state_class = created['BP_CG_GameState'].generated_class()
player_state_class = created['BP_CG_PlayerState'].generated_class()

game_mode_cdo = unreal.get_default_object(game_mode_bp.generated_class())
game_mode_cdo.set_editor_property('GameStateClass', game_state_class)
game_mode_cdo.set_editor_property('PlayerStateClass', player_state_class)
unreal.log('Configured BP_CG_GameMode GameStateClass/PlayerStateClass')

for asset_name, blueprint in created.items():
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    unreal.EditorAssetLibrary.save_loaded_asset(blueprint)
    unreal.log(f'Compiled+saved {asset_name}')

unreal.log('STEP6_DONE')
unreal.SystemLibrary.quit_editor()
