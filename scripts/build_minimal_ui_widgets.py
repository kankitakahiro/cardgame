import unreal


def add_text(tree, parent, name, label):
    text_widget = tree.construct_widget(unreal.TextBlock, name)
    text_widget.set_editor_property('text', label)
    parent.add_child_to_vertical_box(text_widget)
    return text_widget


def build_hand_widget():
    asset_path = '/Game/CardGame/UI/WBP_CG_HandWidget'
    blueprint = unreal.EditorAssetLibrary.load_asset(asset_path)
    if not blueprint:
        raise RuntimeError(f'Widget blueprint not found: {asset_path}')

    tree = blueprint.get_editor_property('widget_tree')
    root = tree.construct_widget(unreal.VerticalBox, 'HandRootVBox')
    tree.set_editor_property('root_widget', root)

    add_text(tree, root, 'TurnLabelText', 'Turn')
    add_text(tree, root, 'TurnValueText', 'Your Turn')
    add_text(tree, root, 'HandTitleText', 'Hand')
    add_text(tree, root, 'HandCardsPlaceholderText', 'Card list placeholder')

    unreal.EditorAssetLibrary.set_metadata_tag(blueprint, 'CG_UI_LAYOUT', 'hand_minimal_v1')
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    unreal.EditorAssetLibrary.save_loaded_asset(blueprint)
    unreal.log(f'Built minimal hand UI: {asset_path}')


def build_board_widget():
    asset_path = '/Game/CardGame/UI/WBP_CG_BoardWidget'
    blueprint = unreal.EditorAssetLibrary.load_asset(asset_path)
    if not blueprint:
        raise RuntimeError(f'Widget blueprint not found: {asset_path}')

    tree = blueprint.get_editor_property('widget_tree')
    root = tree.construct_widget(unreal.VerticalBox, 'BoardRootVBox')
    tree.set_editor_property('root_widget', root)

    add_text(tree, root, 'BoardTitleText', 'Board')
    add_text(tree, root, 'PhaseText', 'Phase: Main')
    add_text(tree, root, 'PlayerStatusText', 'You HP:20 Mana:0')
    add_text(tree, root, 'EnemyStatusText', 'Enemy HP:20 Mana:0')
    add_text(tree, root, 'BoardAreaPlaceholderText', 'Board area placeholder')

    unreal.EditorAssetLibrary.set_metadata_tag(blueprint, 'CG_UI_LAYOUT', 'board_minimal_v1')
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    unreal.EditorAssetLibrary.save_loaded_asset(blueprint)
    unreal.log(f'Built minimal board UI: {asset_path}')


build_hand_widget()
build_board_widget()
unreal.SystemLibrary.quit_editor()
