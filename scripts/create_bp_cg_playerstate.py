import unreal

ASSET_NAME = 'BP_CG_PlayerState'
PACKAGE_PATH = '/Game/CardGame/Core'
ASSET_PATH = f'{PACKAGE_PATH}/{ASSET_NAME}'

if unreal.EditorAssetLibrary.does_asset_exist(ASSET_PATH):
    unreal.log_warning(f'{ASSET_NAME} already exists')
    unreal.SystemLibrary.quit_editor()

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
factory = unreal.BlueprintFactory()
factory.set_editor_property('ParentClass', unreal.PlayerState)

blueprint = asset_tools.create_asset(ASSET_NAME, PACKAGE_PATH, unreal.Blueprint, factory)
if not blueprint:
    raise RuntimeError(f'Failed to create {ASSET_NAME}')

variables = [
    ('PlayerIndex', '(PinCategory=int)'),
    ('CurrentHP', '(PinCategory=int)'),
    ('MaxHP', '(PinCategory=int)'),
    ('HandSize', '(PinCategory=int)'),
    ('DeckSize', '(PinCategory=int)'),
    ('CurrentMana', '(PinCategory=int)'),
    ('MaxMana', '(PinCategory=int)'),
    ('IsDefeated', '(PinCategory=bool)'),
]

for variable_name, pin_text in variables:
    pin_type = unreal.EdGraphPinType()
    if not pin_type.import_text(pin_text):
        raise RuntimeError(f'Failed to import pin type for {variable_name}: {pin_text}')
    result = unreal.BlueprintEditorLibrary.add_member_variable(blueprint, variable_name, pin_type)
    unreal.log(f'{ASSET_PATH}:{variable_name} result={result}')

unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
unreal.EditorAssetLibrary.save_loaded_asset(blueprint)
unreal.log(f'Created {ASSET_PATH}')
unreal.SystemLibrary.quit_editor()
