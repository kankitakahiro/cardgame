import unreal

LEVEL_PATH = "/Game/CardGame/Maps/L_Card_GamePrototype"

subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if subsystem is None:
    raise RuntimeError("LevelEditorSubsystem is unavailable")

created = False
if hasattr(subsystem, "new_level"):
    try:
        created = subsystem.new_level(LEVEL_PATH)
    except TypeError:
        created = subsystem.new_level(LEVEL_PATH, False)
else:
    raise RuntimeError("new_level is unavailable on LevelEditorSubsystem")

if not created:
    raise RuntimeError(f"Failed to create level: {LEVEL_PATH}")

world = unreal.EditorLevelLibrary.get_editor_world()
if world:
    unreal.EditorLevelLibrary.save_current_level()

unreal.log(f"Created level: {LEVEL_PATH}")
