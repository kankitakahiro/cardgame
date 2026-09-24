import unreal

# ユーザーから提供された戦場モチーフの背景画像を取り込む
# (「フィールドをもっとリアルな戦場をモチーフにしたものに変更してください」への対応)。
SOURCE_PATH = r'C:\Users\kanki\Documents\app-develop\game\cardgame\CardGame\SourceArt\BattlefieldBackground_Source.png'
DEST_PACKAGE_PATH = '/Game/CardGame/Textures'
ASSET_NAME = 'T_BattlefieldBackground'

task = unreal.AssetImportTask()
task.filename = SOURCE_PATH
task.destination_path = DEST_PACKAGE_PATH
task.destination_name = ASSET_NAME
task.automated = True
task.save = True
task.replace_existing = True

unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

imported = task.get_editor_property('imported_object_paths')
unreal.log(f'BattlefieldTextureImport: imported={list(imported)}')

# UIで使うテクスチャはミップマップ不要(スケーリングの品質より、常に等倍〜縮小
# 表示前提のUI用途のため)。UI向けの標準設定に寄せておく。
texture_path = f'{DEST_PACKAGE_PATH}/{ASSET_NAME}'
texture = unreal.EditorAssetLibrary.load_asset(texture_path)
if texture:
    texture.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
    unreal.EditorAssetLibrary.save_loaded_asset(texture)
    unreal.log(f'BattlefieldTextureImport: configured UI texture settings for {texture_path}')
else:
    unreal.log_error(f'BattlefieldTextureImport: failed to load imported texture at {texture_path}')

unreal.SystemLibrary.quit_editor()
