import unreal

CLASS_ASSET_PATH = '/Game/CardGame/Data/BP_CG_CardDefinition'
DATA_PATH = '/Game/CardGame/Data'


def make_pin_type(category_name):
    pin_type = unreal.EdGraphPinType()
    if not pin_type.import_text(f'(PinCategory={category_name})'):
        raise RuntimeError(f'Failed to import pin type: {category_name}')
    return pin_type


blueprint = unreal.EditorAssetLibrary.load_asset(CLASS_ASSET_PATH)
if not blueprint:
    raise RuntimeError(f'Blueprint not found: {CLASS_ASSET_PATH}')

result = unreal.BlueprintEditorLibrary.add_member_variable(blueprint, 'EffectValue', make_pin_type('int'))
unreal.log(f'{CLASS_ASSET_PATH}:EffectValue added={result}')
unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
unreal.EditorAssetLibrary.save_loaded_asset(blueprint)

card_class = blueprint.generated_class()
if not card_class:
    raise RuntimeError('BP_CG_CardDefinition has no generated_class')

# CardId, CardName, CardType, Cost, Atk, Hp, EffectId, EffectValue, Ratio, Tags
CARDS = [
    ('C001', '先駆けの斥候', 'Unit', 1, 1, 1, 'TODO_ScoutTop1', 0, 1.00, ''),
    ('C002', '炎走りの小鬼', 'Unit', 1, 2, 1, 'None', 0, 1.50, ''),
    ('C003', '盾持ち見習い', 'Unit', 1, 1, 2, 'None', 0, 2.00, 'Guard'),
    ('C004', '小さな研究者', 'Unit', 1, 1, 1, 'OnDeathDraw', 1, 1.75, ''),
    ('C005', '切り込み隊長', 'Unit', 2, 2, 2, 'None', 0, 1.25, 'Haste'),
    ('C006', '墓場あさり', 'Unit', 2, 2, 2, 'TODO_GraveyardToDeckBottomDraw1', 1, 1.50, ''),
    ('C007', '市場の仲買人', 'Unit', 2, 1, 3, 'TODO_OnBuyEndTurnDiscardDraw', 1, 1.25, ''),
    ('C008', '錆びた巨兵', 'Unit', 2, 3, 2, 'TODO_OnPlayDiscard1', 1, 1.13, ''),
    ('C009', '街道の突撃兵', 'Unit', 3, 3, 3, 'TODO_SecondPlayBuff', 1, 1.08, ''),
    ('C010', '追撃の射手', 'Unit', 3, 2, 3, 'TODO_OnAllySpellPing1', 1, 1.08, ''),
    ('C011', '再誕の司祭', 'Unit', 3, 2, 4, 'TODO_OnPlayReturnGraveyardCheapCard', 1, 1.17, ''),
    ('C012', '市場監督官', 'Unit', 3, 3, 4, 'TODO_BuyCostReductionThisTurn', 1, 1.33, ''),
    ('C013', '戦場の旗手', 'Unit', 4, 4, 4, 'TODO_AllyBuffAtkThisTurn', 1, 1.13, ''),
    ('C014', '霊廟の守り手', 'Unit', 4, 3, 5, 'TODO_OnDeathReturnRandomGraveyardUnit', 0, 1.13, 'Guard'),
    ('C015', '隕鉄の突進獣', 'Unit', 4, 5, 4, 'None', 0, 1.13, ''),
    ('C016', '連鎖術の教授', 'Unit', 4, 3, 4, 'TODO_FirstSpellBonusDamage', 1, 1.06, ''),
    ('C017', '火花の一撃', 'Spell', 1, 0, 0, 'OnPlayDamageTarget', 2, 1.00, ''),
    ('C018', '応急手当', 'Spell', 1, 0, 0, 'OnPlayHealSelf', 3, 1.20, ''),
    ('C019', '手札の選別', 'Spell', 1, 0, 0, 'TODO_Discard1Draw2', 0, 1.25, ''),
    ('C020', '見習い召集', 'Spell', 2, 0, 0, 'TODO_Summon2x1_1Unit', 2, 1.00, ''),
    ('C021', '墓地再点火', 'Spell', 2, 0, 0, 'TODO_ReturnGraveyardSpellSelfDamage1', 1, 0.88, ''),
    ('C022', '市場調達', 'Spell', 2, 0, 0, 'TODO_BuyFromMarketCostUnder3ToHand', 3, 1.13, ''),
    ('C023', '連弾の雨', 'Spell', 3, 0, 0, 'TODO_RandomEnemyDamage1x4', 1, 0.67, ''),
    ('C024', '逆転の号令', 'Spell', 3, 0, 0, 'TODO_ConditionalDamage3or2', 0, 0.45, ''),
]

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
created_count = 0
updated_count = 0

for card_id, name, card_type, cost, atk, hp, effect_id, effect_value, ratio, tags in CARDS:
    asset_name = f'DA_CG_{card_id}'
    asset_path = f'{DATA_PATH}/{asset_name}'

    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        instance = unreal.EditorAssetLibrary.load_asset(asset_path)
        updated_count += 1
    else:
        instance = asset_tools.create_asset(asset_name, DATA_PATH, card_class, None)
        if not instance:
            raise RuntimeError(f'Failed to create {asset_path}')
        created_count += 1

    instance.set_editor_property('CardId', unreal.Name(card_id))
    instance.set_editor_property('CardName', name)
    instance.set_editor_property('CardType', unreal.Name(card_type))
    instance.set_editor_property('Cost', cost)
    instance.set_editor_property('Atk', atk)
    instance.set_editor_property('Hp', hp)
    instance.set_editor_property('EffectId', unreal.Name(effect_id))
    instance.set_editor_property('EffectValue', effect_value)
    instance.set_editor_property('Ratio', ratio)
    instance.set_editor_property('Tags', tags)

    unreal.EditorAssetLibrary.save_loaded_asset(instance)
    unreal.log(f'CARD_OK:{card_id}:{asset_path}')

unreal.log(f'STEP4_DONE created={created_count} updated={updated_count} total={len(CARDS)}')
unreal.SystemLibrary.quit_editor()
