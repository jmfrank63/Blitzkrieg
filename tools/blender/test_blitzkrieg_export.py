import importlib.util
import pathlib
import unittest


MODULE_PATH = pathlib.Path(__file__).with_name("blitzkrieg_export.py")


def load_module():
    spec = importlib.util.spec_from_file_location("blitzkrieg_export", MODULE_PATH)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class BlitzkriegExporterTests(unittest.TestCase):
    def test_module_imports_without_blender(self):
        module = load_module()
        self.assertTrue(hasattr(module, "build_converter_command"))

    def test_converter_command_uses_obj2mod_contract(self):
        module = load_module()
        command = module.build_converter_command(
            pathlib.Path("BZMConvertor.exe"),
            pathlib.Path("tank.obj"),
            pathlib.Path("tank.mod"),
            pathlib.Path("tank.skeleton.txt"),
            None,
        )
        self.assertEqual(
            command,
            [
                "BZMConvertor.exe",
                "-obj2mod",
                "tank.obj",
                "tank.mod",
                "tank.skeleton.txt",
            ],
        )

    def test_skeleton_line_format_matches_bzmconvertor(self):
        module = load_module()
        line = module.format_skeleton_node(
            2,
            0,
            "Turret",
            (1.0, 2.0, 3.0),
            (0.0, 0.0, 0.0, 1.0),
            False,
        )
        self.assertEqual(line, "node 2 0 Turret 1 2 3 0 0 0 1 0")

    def test_animation_header_format_matches_bzmconvertor(self):
        module = load_module()
        line = module.format_animation_header("Move Forward", 4, 1, -1, -1)
        self.assertEqual(line, "anim Move_Forward 4 1 -1 -1")

    def test_animation_key_format_matches_bzmconvertor(self):
        module = load_module()
        line = module.format_animation_key(
            3,
            2,
            (1.0, 2.0, 3.0),
            (0.0, 0.0, 0.0, 1.0),
        )
        self.assertEqual(line, "key 3 2 1 2 3 0 0 0 1")

    def test_converter_command_includes_animation_after_skeleton(self):
        module = load_module()
        command = module.build_converter_command(
            pathlib.Path("BZMConvertor.exe"),
            pathlib.Path("tank.obj"),
            pathlib.Path("tank.mod"),
            pathlib.Path("tank.skeleton.txt"),
            pathlib.Path("tank.anim.txt"),
        )
        self.assertEqual(
            command,
            [
                "BZMConvertor.exe",
                "-obj2mod",
                "tank.obj",
                "tank.mod",
                "tank.skeleton.txt",
                "tank.anim.txt",
            ],
        )


if __name__ == "__main__":
    unittest.main()
