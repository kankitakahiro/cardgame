import unreal

ASSET_NAME = 'WBP_CG_MarketWidget'
PACKAGE_PATH = '/Game/CardGame/UI'
ASSET_PATH = f'{PACKAGE_PATH}/{ASSET_NAME}'


def add_text(tree, parent, name, label):
    text_widget = tree.construct_widget(unreal.TextBlock, name)
    text_widget.set_editor_property('text', label)
    parent.add_child_to_vertical_box(text_widget)
    return text_widget


if unreal.EditorAssetLibrary.does_asset_exist(ASSET_PATH):
    blueprint = unreal.EditorAssetLibrary.load_asset(ASSET_PATH)
    unreal.log_warning(f'{ASSET_NAME} already exists, reusing')
else:
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    factory = unreal.WidgetBlueprintFactory()
    factory.set_editor_property('ParentClass', unreal.UserWidget)
    blueprint = asset_tools.create_asset(ASSET_NAME, PACKAGE_PATH, unreal.WidgetBlueprint, factory)
    if not blueprint:
        raise RuntimeError(f'Failed to create {ASSET_PATH}')
    unreal.log(f'Created {ASSET_PATH}')

props = [p for p in dir(blueprint) if not p.startswith('_')]
unreal.log(f'WIDGETBP_PROPS:{props}')

tree = None
for candidate in ('widget_tree', 'WidgetTree', 'tree'):
    try:
        tree = blueprint.get_editor_property(candidate)
        unreal.log(f'FOUND_TREE_PROP:{candidate}')
        break
    except Exception as exc:
        unreal.log_warning(f'candidate {candidate} failed: {exc}')

if tree is None:
    unreal.log_error('Could not find widget tree property, skipping tree population')
else:
    root = tree.construct_widget(unreal.VerticalBox, 'MarketRootVBox')
    tree.set_editor_property('root_widget', root)
    add_text(tree, root, 'MarketTitleText', 'Market')
    for i in range(1, 6):
        add_text(tree, root, f'MarketSlot{i}Text', f'Slot {i}: (empty)')
    unreal.EditorAssetLibrary.set_metadata_tag(blueprint, 'CG_UI_LAYOUT', 'market_minimal_v1')

unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
unreal.EditorAssetLibrary.save_loaded_asset(blueprint)
unreal.log(f'STEP5_DONE:{ASSET_PATH}')
unreal.SystemLibrary.quit_editor()
