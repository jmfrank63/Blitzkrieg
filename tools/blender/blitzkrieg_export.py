bl_info = {
    "name": "Blitzkrieg MOD Exporter",
    "author": "Blitzkrieg Open Tools",
    "version": (0, 1, 0),
    "blender": (3, 6, 0),
    "location": "File > Export > Blitzkrieg MOD",
    "description": "Exports Blender meshes through the open BZMConvertor OBJ to MOD path",
    "category": "Import-Export",
}

import math
import pathlib
import subprocess

try:
    import bpy
    from bpy.props import BoolProperty, StringProperty
    from bpy_extras.io_utils import ExportHelper
except ImportError:
    bpy = None
    BoolProperty = None
    StringProperty = None
    ExportHelper = object


def format_float(value):
    return ("%0.9g" % float(value)).rstrip()


def sanitize_name(name):
    cleaned = []
    for ch in str(name):
        if ch.isalnum() or ch in "_-.":
            cleaned.append(ch)
        else:
            cleaned.append("_")
    value = "".join(cleaned).strip("_")
    return value or "Node"


def format_skeleton_node(index, parent, name, bone, quat, locator):
    values = [
        "node",
        str(int(index)),
        str(int(parent)),
        sanitize_name(name),
        format_float(bone[0]),
        format_float(bone[1]),
        format_float(bone[2]),
        format_float(quat[0]),
        format_float(quat[1]),
        format_float(quat[2]),
        format_float(quat[3]),
        "1" if locator else "0",
    ]
    return " ".join(values)


def format_animation_header(name, key_count, action_key, aabb_a_index, aabb_d_index):
    values = [
        "anim",
        sanitize_name(name),
        str(int(key_count)),
        str(int(action_key)),
        str(int(aabb_a_index)),
        str(int(aabb_d_index)),
    ]
    return " ".join(values)


def format_animation_key(node_index, key_index, pos, quat):
    values = [
        "key",
        str(int(node_index)),
        str(int(key_index)),
        format_float(pos[0]),
        format_float(pos[1]),
        format_float(pos[2]),
        format_float(quat[0]),
        format_float(quat[1]),
        format_float(quat[2]),
        format_float(quat[3]),
    ]
    return " ".join(values)


def build_converter_command(converter_path, obj_path, mod_path, skeleton_path=None, animation_path=None):
    command = [
        str(converter_path),
        "-obj2mod",
        str(obj_path),
        str(mod_path),
    ]
    if skeleton_path is not None:
        command.append(str(skeleton_path))
    if animation_path is not None:
        command.append(str(animation_path))
    return command


def sibling_path(path, suffix):
    path = pathlib.Path(path)
    return path.with_suffix(suffix)


def write_default_skeleton(path):
    path = pathlib.Path(path)
    path.write_text(
        format_skeleton_node(0, -1, "Root", (0.0, 0.0, 0.0), (0.0, 0.0, 0.0, 1.0), False) + "\n",
        encoding="utf-8",
    )


def collect_armature_bones(armature_obj):
    bones = list(armature_obj.data.bones)
    ordered = []

    def visit(bone):
        if bone in ordered:
            return
        if bone.parent is not None:
            visit(bone.parent)
        ordered.append(bone)

    for bone in bones:
        visit(bone)

    index_by_name = {bone.name: i for i, bone in enumerate(ordered)}
    lines = []
    for index, bone in enumerate(ordered):
        parent = index_by_name[bone.parent.name] if bone.parent is not None else -1
        if bone.parent is None:
            local_head = bone.head_local
        else:
            local_head = bone.head_local - bone.parent.head_local
        quat = bone.matrix_local.to_quaternion()
        lines.append(
            format_skeleton_node(
                index,
                parent,
                bone.name,
                (local_head.x, local_head.y, local_head.z),
                (quat.x, quat.y, quat.z, quat.w),
                bone.name.lower().startswith("loc_") or bone.name.lower().startswith("locator"),
            )
        )
    return lines


def write_skeleton(path, context):
    armatures = [
        obj for obj in context.selected_objects
        if getattr(obj, "type", None) == "ARMATURE"
    ]
    if not armatures:
        write_default_skeleton(path)
        return
    lines = collect_armature_bones(armatures[0])
    pathlib.Path(path).write_text("\n".join(lines) + "\n", encoding="utf-8")


def selected_armature_objects(context):
    return [
        obj for obj in context.selected_objects
        if getattr(obj, "type", None) == "ARMATURE"
    ]


def collect_ordered_bones(armature_obj):
    bones = list(armature_obj.data.bones)
    ordered = []

    def visit(bone):
        if bone in ordered:
            return
        if bone.parent is not None:
            visit(bone.parent)
        ordered.append(bone)

    for bone in bones:
        visit(bone)
    return ordered


def collect_action_lines(context, armature_obj, action):
    bones = collect_ordered_bones(armature_obj)
    if not bones:
        return []

    frame_start, frame_end = action.frame_range
    frame_start = int(math.floor(frame_start))
    frame_end = int(math.ceil(frame_end))
    if frame_end < frame_start:
        frame_end = frame_start

    frames = list(range(frame_start, frame_end + 1))
    action_key = min(max(0, int(round(-frame_start))), len(frames) - 1)

    lines = [format_animation_header(action.name, len(frames), action_key, -1, -1)]
    previous_action = armature_obj.animation_data.action if armature_obj.animation_data else None
    if armature_obj.animation_data is None:
        armature_obj.animation_data_create()

    armature_obj.animation_data.action = action
    try:
        for key_index, frame in enumerate(frames):
            context.scene.frame_set(frame)
            context.view_layer.update()
            for node_index, bone in enumerate(bones):
                pose_bone = armature_obj.pose.bones.get(bone.name)
                if pose_bone is None:
                    pos = (0.0, 0.0, 0.0)
                    quat = (0.0, 0.0, 0.0, 1.0)
                else:
                    matrix = pose_bone.matrix_basis
                    loc, rot, _scale = matrix.decompose()
                    pos = (loc.x, loc.y, loc.z)
                    quat = (rot.x, rot.y, rot.z, rot.w)
                lines.append(format_animation_key(node_index, key_index, pos, quat))
    finally:
        armature_obj.animation_data.action = previous_action
    return lines


def write_animations(path, context):
    if bpy is None:
        raise RuntimeError("Blender Python module is not available")

    armatures = selected_armature_objects(context)
    if not armatures:
        return False

    armature = armatures[0]
    actions = list(bpy.data.actions)
    if not actions:
        return False

    original_frame = context.scene.frame_current
    all_lines = []
    try:
        for action in actions:
            lines = collect_action_lines(context, armature, action)
            if lines:
                all_lines.extend(lines)
    finally:
        context.scene.frame_set(original_frame)
        context.view_layer.update()

    if not all_lines:
        return False

    pathlib.Path(path).write_text("\n".join(all_lines) + "\n", encoding="utf-8")
    return True


def selected_mesh_objects(context):
    return [
        obj for obj in context.selected_objects
        if getattr(obj, "type", None) == "MESH"
    ]


def write_obj(path, context):
    if bpy is None:
        raise RuntimeError("Blender Python module is not available")

    depsgraph = context.evaluated_depsgraph_get()
    objects = selected_mesh_objects(context)
    if not objects:
        raise RuntimeError("Select at least one mesh object to export")

    vertex_offset = 1
    normal_offset = 1
    uv_offset = 1
    with open(path, "w", encoding="utf-8", newline="\n") as out:
        out.write("# Blitzkrieg Blender exporter intermediate OBJ\n")
        for obj in objects:
            obj_eval = obj.evaluated_get(depsgraph)
            mesh = obj_eval.to_mesh()
            try:
                mesh.calc_loop_triangles()
                out.write("o %s\n" % sanitize_name(obj.name))

                world = obj.matrix_world
                normal_matrix = world.to_3x3().inverted().transposed()
                uv_layer = mesh.uv_layers.active.data if mesh.uv_layers.active else None

                for vertex in mesh.vertices:
                    co = world @ vertex.co
                    out.write("v %s %s %s\n" % (format_float(co.x), format_float(co.y), format_float(co.z)))

                loop_normal_indices = {}
                for loop_index, loop in enumerate(mesh.loops):
                    normal = (normal_matrix @ loop.normal).normalized()
                    loop_normal_indices[loop_index] = normal_offset
                    out.write("vn %s %s %s\n" % (format_float(normal.x), format_float(normal.y), format_float(normal.z)))
                    normal_offset += 1

                loop_uv_indices = {}
                for loop_index, _loop in enumerate(mesh.loops):
                    loop_uv_indices[loop_index] = uv_offset
                    if uv_layer is not None:
                        uv = uv_layer[loop_index].uv
                        out.write("vt %s %s\n" % (format_float(uv.x), format_float(uv.y)))
                    else:
                        out.write("vt 0 0\n")
                    uv_offset += 1

                for tri in mesh.loop_triangles:
                    refs = []
                    for loop_index in tri.loops:
                        loop = mesh.loops[loop_index]
                        refs.append(
                            "%d/%d/%d" % (
                                vertex_offset + loop.vertex_index,
                                loop_uv_indices[loop_index],
                                loop_normal_indices[loop_index],
                            )
                        )
                    out.write("f %s\n" % " ".join(refs))

                vertex_offset += len(mesh.vertices)
            finally:
                obj_eval.to_mesh_clear()


def run_converter(converter_path, obj_path, mod_path, skeleton_path, animation_path=None):
    command = build_converter_command(converter_path, obj_path, mod_path, skeleton_path, animation_path)
    return subprocess.run(command, check=False)


if bpy is not None:
    class EXPORT_SCENE_OT_blitzkrieg_mod(bpy.types.Operator, ExportHelper):
        bl_idname = "export_scene.blitzkrieg_mod"
        bl_label = "Export Blitzkrieg MOD"
        bl_options = {"PRESET"}

        filename_ext = ".mod"

        filter_glob: StringProperty(default="*.mod", options={"HIDDEN"})
        converter_path: StringProperty(
            name="BZMConvertor.exe",
            subtype="FILE_PATH",
            default="",
        )
        run_bzmconvertor: BoolProperty(
            name="Run BZMConvertor",
            default=True,
        )
        keep_intermediate: BoolProperty(
            name="Keep OBJ and skeleton files",
            default=True,
        )
        export_animations: BoolProperty(
            name="Export animations",
            default=True,
        )

        def execute(self, context):
            mod_path = pathlib.Path(self.filepath)
            obj_path = sibling_path(mod_path, ".obj")
            skeleton_path = sibling_path(mod_path, ".skeleton.txt")
            animation_path = sibling_path(mod_path, ".anim.txt")

            write_obj(obj_path, context)
            write_skeleton(skeleton_path, context)
            has_animation = self.export_animations and write_animations(animation_path, context)

            if self.run_bzmconvertor:
                converter = pathlib.Path(self.converter_path) if self.converter_path else pathlib.Path("BZMConvertor.exe")
                result = run_converter(converter, obj_path, mod_path, skeleton_path, animation_path if has_animation else None)
                if result.returncode != 0:
                    self.report({"ERROR"}, "BZMConvertor failed with exit code %d" % result.returncode)
                    return {"CANCELLED"}

            if not self.keep_intermediate:
                obj_path.unlink(missing_ok=True)
                skeleton_path.unlink(missing_ok=True)
                if has_animation:
                    animation_path.unlink(missing_ok=True)

            self.report({"INFO"}, "Exported %s" % mod_path)
            return {"FINISHED"}


    def menu_func_export(self, _context):
        self.layout.operator(EXPORT_SCENE_OT_blitzkrieg_mod.bl_idname, text="Blitzkrieg MOD (.mod)")


    def register():
        bpy.utils.register_class(EXPORT_SCENE_OT_blitzkrieg_mod)
        bpy.types.TOPBAR_MT_file_export.append(menu_func_export)


    def unregister():
        bpy.types.TOPBAR_MT_file_export.remove(menu_func_export)
        bpy.utils.unregister_class(EXPORT_SCENE_OT_blitzkrieg_mod)
else:
    def register():
        raise RuntimeError("This add-on must be registered from Blender")


    def unregister():
        pass


if __name__ == "__main__" and bpy is not None:
    register()
