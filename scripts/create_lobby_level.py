"""L_Lobby レベルを新規作成し、GameModeOverrideをACGLobbyGameModeに設定する。
docs/lobby-deckbuilder-plan.md Step 3。
"""
import unreal

LEVEL_PATH = "/Game/CardGame/Maps/L_Lobby"
GAME_MODE_CLASS_PATH = "/Script/CardGame.CGLobbyGameMode"


def log(msg):
    unreal.log(f"[LobbySetup] {msg}")


def get_level_editor_subsystem():
    return unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)


def get_editor_world():
    if hasattr(unreal, "UnrealEditorSubsystem"):
        ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        world = ues.get_editor_world()
        if world:
            return world
    # フォールバック(非推奨だが5.8でも動く可能性があるため試す)。
    if hasattr(unreal, "EditorLevelLibrary"):
        return unreal.EditorLevelLibrary.get_editor_world()
    return None


def main():
    les = get_level_editor_subsystem()
    log(f"LevelEditorSubsystem: {les}")

    if unreal.EditorAssetLibrary.does_asset_exist(LEVEL_PATH):
        log(f"Level already exists at {LEVEL_PATH}. Loading it instead of creating.")
        loaded = les.load_level(LEVEL_PATH)
        log(f"load_level result: {loaded}")
    else:
        result = les.new_level(LEVEL_PATH)
        log(f"new_level result: {result}")

    world = get_editor_world()
    log(f"editor world: {world}")
    if not world:
        log("ERROR: could not resolve editor world, aborting.")
        return

    world_settings = world.get_world_settings()
    log(f"world_settings: {world_settings}")
    if not world_settings:
        log("ERROR: could not resolve world settings, aborting.")
        return

    game_mode_class = unreal.load_class(None, GAME_MODE_CLASS_PATH)
    log(f"game_mode_class: {game_mode_class}")
    if not game_mode_class:
        log(f"ERROR: could not load class {GAME_MODE_CLASS_PATH}, aborting.")
        return

    world_settings.set_editor_property("default_game_mode", game_mode_class)
    log("Set default_game_mode on world settings.")

    saved = les.save_current_level()
    log(f"save_current_level result: {saved}")

    log("Done.")


main()
