# OpenGL Extension Hash Test

Throwaway tool to hash all the known extensions and test for collisions.

Extensions were taken from the [Khronos registry](//registry.khronos.org/OpenGL/extensions/) in July 2026, as well as the [WebGL registry](//registry.khronos.org/webgl/extensions/), [ANGLE source](//chromium.googlesource.com/angle/angle/+/HEAD/scripts/registry_xml.py), and observations seen in the wild over the years.

With the simple [`hash32()`](/main.c#L125) no collisions were found (testing with and without the various `GL_` prefixes, a total of 2301 unique strings as of August 2026).

The hash and [`unprefix()`](/main.c#L147) call (to remove known GL prefixes) are also verified on multiple CPU types and OSes, generating the same result on Intel, ARM, PowerPC and wasm, big- and little-endian.

On target this can also be used to find unknown extensions by generating a sorted unique list of hashes (call with the `--sorted` option), and performing a binary search on this to rule out all known entries (see [`hasUnique()`](/main.c#L211) for an example).
