# GLAD (vendored)

Generated OpenGL loader, committed so the build needs no Python.

- Generator: [glad 2.0.8](https://github.com/Dav1dde/glad) (`pip install glad2`)
- API: OpenGL 4.6 core profile (the app requests a 4.3 context; 4.4–4.6 entry points are loaded when present)
- Extensions: `GL_KHR_debug`
- License: generated code `(WTFPL OR CC0-1.0) AND Apache-2.0` (Khronos XML registry)

Regenerate (from the repository root):

```sh
pip install glad2
python -m glad --reproducible --api gl:core=4.6 --extensions GL_KHR_debug --out-path external/glad c
```
