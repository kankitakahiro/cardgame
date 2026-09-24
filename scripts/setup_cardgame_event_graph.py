import unreal

GAME_MODE_PATH = "/Game/CardGame/Core/BP_CG_GameMode"
HAND_WIDGET_PATH = "/Game/CardGame/UI/WBP_CG_HandWidget"


def ensure_event_graph(blueprint):
    graph = unreal.BlueprintEditorLibrary.find_event_graph(blueprint)
    if graph is not None:
        return graph

    graph = unreal.BlueprintEditorLibrary.add_function_graph(blueprint, "EventGraph")
    if graph is None:
        raise RuntimeError(f"Failed to create EventGraph for {blueprint.get_name()}")
    return graph


def ensure_custom_event(graph, event_name):
    for node in graph.get_all_nodes():
        if node.get_class().get_name() == "K2Node_CustomEvent":
            try:
                current_name = node.get_editor_property("custom_function_name")
            except Exception:
                current_name = ""
            if current_name == event_name:
                return node

    node = unreal.K2Node_CustomEvent()
    node.set_editor_property("custom_function_name", event_name)
    graph.add_node(node)
    node.node_pos_x = 0
    node.node_pos_y = 0
    unreal.log(f"Created custom event: {event_name}")
    return node


def ensure_event(graph, event_name):
    for node in graph.get_all_nodes():
        if node.get_class().get_name() == "K2Node_Event":
            try:
                current_name = node.get_editor_property("event_reference").to_string()
            except Exception:
                current_name = ""
            if current_name == event_name:
                return node

    node = unreal.K2Node_Event()
    node.set_editor_property("event_reference", unreal.FName(event_name))
    graph.add_node(node)
    node.node_pos_x = 0
    node.node_pos_y = 0
    unreal.log(f"Created event: {event_name}")
    return node


def get_pin(node, pin_name, direction):
    for pin in node.get_all_pins():
        if pin.pin_name == pin_name and pin.direction == direction:
            return pin
    return None


def connect_exec(source_node, source_pin_name, target_node, target_pin_name):
    source_pin = get_pin(source_node, source_pin_name, unreal.EdGraphPinDirection.EGPD_OUTPUT)
    target_pin = get_pin(target_node, target_pin_name, unreal.EdGraphPinDirection.EGPD_INPUT)
    if source_pin is None or target_pin is None:
        return False

    schema = source_pin.get_schema() if hasattr(source_pin, "get_schema") else None
    if schema is not None and hasattr(schema, "try_create_connection"):
        return schema.try_create_connection(source_pin, target_pin)
    return False


def add_print_string_node(graph, label):
    node = unreal.K2Node_CallFunction()
    func = unreal.KismetSystemLibrary.static_class().find_function("PrintString")
    if func is not None:
        node.set_editor_property("function_reference", func)
    else:
        node.set_editor_property("function_name", "PrintString")

    graph.add_node(node)
    node.node_pos_x = 500
    node.node_pos_y = 0

    text_pin = get_pin(node, "InString", unreal.EdGraphPinDirection.EGPD_INPUT)
    if text_pin is not None:
        text_pin.default_value = label

    unreal.log(f"Added PrintString node: {label}")
    return node


def ensure_onclicked_event(graph):
    for node in graph.get_all_nodes():
        if node.get_class().get_name() == "K2Node_ComponentBoundEvent":
            try:
                current_name = node.get_editor_property("handler_name")
            except Exception:
                current_name = ""
            if current_name == "OnClicked":
                return node

    node = unreal.K2Node_ComponentBoundEvent()
    node.set_editor_property("handler_name", "OnClicked")
    graph.add_node(node)
    node.node_pos_x = 0
    node.node_pos_y = 0
    unreal.log("Created widget OnClicked event")
    return node


def build_game_mode_graph():
    blueprint = unreal.EditorAssetLibrary.load_asset(GAME_MODE_PATH)
    if blueprint is None:
        raise RuntimeError(f"Blueprint not found: {GAME_MODE_PATH}")

    graph = ensure_event_graph(blueprint)
    begin_play = ensure_event(graph, "ReceiveBeginPlay")
    refresh_ui = ensure_custom_event(graph, "RefreshUI")
    end_turn = ensure_custom_event(graph, "EndTurn")

    refresh_print = add_print_string_node(graph, "RefreshUI")
    end_turn_print = add_print_string_node(graph, "EndTurn")

    connect_exec(begin_play, "then", refresh_ui, "then")
    connect_exec(refresh_ui, "then", refresh_print, "then")
    connect_exec(end_turn, "then", end_turn_print, "then")

    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    unreal.EditorAssetLibrary.save_loaded_asset(blueprint)
    unreal.log(f"Completed GameMode event graph for {GAME_MODE_PATH}")
    return blueprint


def build_widget_graph():
    blueprint = unreal.EditorAssetLibrary.load_asset(HAND_WIDGET_PATH)
    if blueprint is None:
        raise RuntimeError(f"Blueprint not found: {HAND_WIDGET_PATH}")

    graph = ensure_event_graph(blueprint)
    on_clicked = ensure_onclicked_event(graph)
    print_node = add_print_string_node(graph, "End Turn Button Clicked")
    connect_exec(on_clicked, "then", print_node, "then")

    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    unreal.EditorAssetLibrary.save_loaded_asset(blueprint)
    unreal.log(f"Completed widget event graph for {HAND_WIDGET_PATH}")
    return blueprint


def run():
    build_game_mode_graph()
    build_widget_graph()

    try:
        unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    except Exception as exc:
        unreal.log_warning(f"Save dirty packages failed: {exc}")

    unreal.log("CardGame EventGraph setup completed. Editor remains open for review.")


if __name__ == "__main__":
    run()
