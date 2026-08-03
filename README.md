# OpenGL Extension Hash Test

Throwaway tool to hash all the known extensions and test for a collision.

Extensions were taken from the [Khronos registry](//registry.khronos.org/OpenGL/extensions/) in July 2026, as well as the [WebGL registry](//registry.khronos.org/webgl/extensions/), [ANGLE source](//chromium.googlesource.com/angle/angle/+/HEAD/scripts/registry_xml.py), and observations seen in the wild over the years.

With the simple [`hash32()`](//github.com/cwoffenden/gl-ext-hash/blob/main/main.c#L66) no collisions were found (testing with and without the `GL_` prefix, a total of 2238 strings as of August 2026).
