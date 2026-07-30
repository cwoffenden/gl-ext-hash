# OpenGL Extension Hash Test

Throwaway tool to hash all the known extensions and test for a collision.

The extensions were taken from the [Khronos registry](//registry.khronos.org/OpenGL/extensions/) in July 2026.

With the simple [`hash32()`](//github.com/cwoffenden/gl-ext-hash/blob/main/main.c#L66) no collisions were found (testing with and without the `GL_` prefix).
