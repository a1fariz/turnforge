# Third-party notices

Turnforge uses raylib 5.5 by Ramon Santamaria and contributors, under the zlib/libpng license. The build downloads the unmodified official 5.5 source archive and verifies its SHA-256 hash. The release package retains its license under `licenses/raylib.txt`.

raylib's desktop backend uses GLFW (zlib/libpng license) and GLAD (MIT / generated OpenGL bindings). Their notices are included under `licenses/` in the release package. raylib's enabled image/font utilities also use the public-domain option of the dual-licensed stb libraries. UI drawing uses raylib's built-in font and geometric primitives. The bundled character atlas is project-specific AI-generated artwork; its provenance and prompt are documented in [art-assets.md](docs/art-assets.md). No music, sound effects, externally downloaded character pack, or external font files are bundled.

Upstream sources:

- raylib: https://github.com/raysan5/raylib/tree/5.5
- GLFW: bundled in raylib's `src/external/glfw`
- GLAD: bundled in raylib's `src/external/glad.h`
- stb: license statements in raylib's `src/external/stb_*.h`

CMake is used only as a development tool. The project-local portable CMake distribution retains its own notices and is not included in the game ZIP. The MinGW compiler and Ninja are not included in the game ZIP.
