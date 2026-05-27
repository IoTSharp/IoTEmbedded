# Projects

Project-specific build/debug entry and wiring only.

Reusable platform and application code should live under `src`.

## VisualGDB Build

Use the repository C# wrapper when validating STM32 firmware builds from the
command line. It invokes `VisualGDB.exe /build` for the selected `.vgdbcmake`
project, so it follows the same toolchain resolution as VisualGDB instead of
depending on global `PATH`.

```console
dotnet run --project tools/VisualGDBBuild -- l4
dotnet run --project tools/VisualGDBBuild -- f1
dotnet run --project tools/VisualGDBBuild -- l4 --diagnose
```

`--diagnose` prints the VisualGDB CMake, Ninja, and ARM GCC paths discovered
from the local VisualGDB installation and project metadata.
