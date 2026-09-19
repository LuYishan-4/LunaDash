"""Prove that invalid architecture changes fail the same checker CI uses."""
import importlib.util
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location(
    "layout", Path(__file__).resolve().parents[2] / "scripts/check-source-layout.py"
)
layout = importlib.util.module_from_spec(spec)
spec.loader.exec_module(layout)


class LayoutTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        for domain in layout.DOMAINS:
            (self.root / "src" / domain).mkdir(parents=True)
        (self.root / "src/service/portal").mkdir()
        for name in layout.MAINS:
            (self.root / "src" / name).write_text("int main() { return 0; }\n")
        (self.root / "CMakeLists.txt").write_text(
            "add_executable(example\n" + "\n".join("src/" + p for p in layout.MAINS) + ")\n"
        )

    def add(self, path, contents="", listed=False):
        p = self.root / path
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(contents)
        if listed:
            with (self.root / "CMakeLists.txt").open("a") as cmake:
                cmake.write(f"target_sources(example PRIVATE {path})\n")

    def test_valid_tree(self):
        self.assertEqual(layout.violations(self.root), [])

    def test_bad_names(self):
        self.add("src/compositor/Render/renderer_backend.cpp", "class RenderBackend {};", listed=True)
        errors = "\n".join(layout.violations(self.root))
        for expected in ("lowercase", "PascalCase", "legacy renderer"):
            self.assertIn(expected, errors)

    def test_dependency_and_missing_header(self):
        self.add("src/core/Feature.hpp", '#include "desktop/Absent.hpp"\n')
        errors = "\n".join(layout.violations(self.root))
        self.assertIn("core cannot depend", errors)
        self.assertIn("missing project header", errors)

    def test_gl_and_unlisted_source(self):
        self.add("src/desktop/Graphics.cpp", '#include <QOpenGLContext>\n')
        errors = "\n".join(layout.violations(self.root))
        self.assertIn("OpenGL implementation outside", errors)
        self.assertIn("not explicitly listed", errors)

    def test_unlisted_shader(self):
        self.add("src/compositor/renderer/opengl/shaders/Future.frag")
        self.assertIn("shader is not explicitly embedded", "\n".join(layout.violations(self.root)))

    def test_desktop_cannot_include_renderer(self):
        self.add("src/compositor/renderer/opengl/Texture.hpp")
        self.add("src/desktop/Widget.hpp", '#include "compositor/renderer/opengl/Texture.hpp"\n')
        self.assertIn("desktop cannot depend", "\n".join(layout.violations(self.root)))


if __name__ == "__main__":
    unittest.main()
