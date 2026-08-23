import unreal

GAME_MODE_PATH = '/Game/CardGame/Core/BP_CG_GameMode'
GAME_STATE_PATH = '/Game/CardGame/Core/BP_CG_GameState'

variables_by_asset = {
    GAME_MODE_PATH: [
        ('CurrentTurnPlayerIndex', '(PinCategory=int)'),
        ('TurnCount', '(PinCategory=int)'),
        ('CurrentPhase', '(PinCategory=int)'),
    ],
    GAME_STATE_PATH: [
        ('CurrentTurnPlayerIndex', '(PinCategory=int)'),
        ('TurnCount', '(PinCategory=int)'),
        ('CurrentPhase', '(PinCategory=int)'),
        ('WinnerPlayerIndex', '(PinCategory=int)'),
    ],
}

for asset_path, variables in variables_by_asset.items():
    blueprint = unreal.EditorAssetLibrary.load_asset(asset_path)
    if not blueprint:
        raise RuntimeError(f'Blueprint not found: {asset_path}')

    changed = False
    for variable_name, pin_text in variables:
        pin_type = unreal.EdGraphPinType()
        if not pin_type.import_text(pin_text):
            raise RuntimeError(f'Failed to import pin type for {variable_name}: {pin_text}')
        result = unreal.BlueprintEditorLibrary.add_member_variable(blueprint, variable_name, pin_type)
        unreal.log(f'{asset_path}:{variable_name} result={result}')
        changed = changed or bool(result)

    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    unreal.EditorAssetLibrary.save_loaded_asset(blueprint)
    unreal.log(f'saved {asset_path} changed={changed}')

unreal.SystemLibrary.quit_editor()
