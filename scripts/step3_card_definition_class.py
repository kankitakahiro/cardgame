import unreal

STRUCT_ASSET_PATH = '/Game/CardGame/Data/ST_CG_CardRow'
CLASS_ASSET_NAME = 'BP_CG_CardDefinition'
DATA_PATH = '/Game/CardGame/Data'
CLASS_ASSET_PATH = f'{DATA_PATH}/{CLASS_ASSET_NAME}'


def make_pin_type(category_name):
    pin_type = unreal.EdGraphPinType()
    if not pin_type.import_text(f'(PinCategory={category_name})'):
        raise RuntimeError(f'Failed to import pin type: {category_name}')
    return pin_type


# Remove the abandoned UserDefinedStruct probe asset (Python cannot author struct
# fields in this engine build - StructureEditorUtils is not exposed) so it does not
# linger as dead/confusing content.
if unreal.EditorAssetLibrary.does_asset_exist(STRUCT_ASSET_PATH):
    deleted = unreal.EditorAssetLibrary.delete_asset(STRUCT_ASSET_PATH)
    unreal.log(f'Deleted abandoned struct asset: {deleted}')

if not unreal.EditorAssetLibrary.does_directory_exist(DATA_PATH):
    unreal.EditorAssetLibrary.make_directory(DATA_PATH)

if unreal.EditorAssetLibrary.does_asset_exist(CLASS_ASSET_PATH):
    unreal.log_warning(f'{CLASS_ASSET_NAME} already exists, will only ensure variables')
    blueprint = unreal.EditorAssetLibrary.load_asset(CLASS_ASSET_PATH)
else:
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    factory = unreal.BlueprintFactory()
    factory.set_editor_property('ParentClass', unreal.PrimaryDataAsset)
    blueprint = asset_tools.create_asset(CLASS_ASSET_NAME, DATA_PATH, unreal.Blueprint, factory)
    if not blueprint:
        raise RuntimeError(f'Failed to create {CLASS_ASSET_PATH}')
    unreal.log(f'Created {CLASS_ASSET_PATH}')

VARIABLES = [
    ('CardId', 'name'),
    ('CardName', 'string'),
    ('CardType', 'name'),
    ('Cost', 'int'),
    ('Atk', 'int'),
    ('Hp', 'int'),
    ('EffectId', 'name'),
    ('Ratio', 'float'),
    ('Tags', 'string'),
]

for variable_name, category in VARIABLES:
    pin_type = make_pin_type(category)
    result = unreal.BlueprintEditorLibrary.add_member_variable(blueprint, variable_name, pin_type)
    unreal.log(f'{CLASS_ASSET_PATH}:{variable_name} added={result}')

unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
unreal.EditorAssetLibrary.save_loaded_asset(blueprint)
unreal.log('STEP3_DONE')
unreal.SystemLibrary.quit_editor()
