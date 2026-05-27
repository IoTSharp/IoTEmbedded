using System.Diagnostics;
using System.Xml.Linq;

namespace VisualGDBBuild;

internal static class Program {
  private const string DefaultConfiguration = "Debug";
  private const string DefaultPlatform = "VisualGDB";
  private const string DefaultTarget = "l4";

  private static readonly Dictionary<string, string> KnownTargetHints = new(StringComparer.OrdinalIgnoreCase) {
    ["l4"] = "stm32l475vetx",
    ["pandora"] = "stm32l475vetx",
    ["stm32l475"] = "stm32l475vetx",
    ["stm32l475vetx"] = "stm32l475vetx",
    ["f1"] = "stm32f103vetx",
    ["powerenv"] = "stm32f103vetx",
    ["powerenvdaq"] = "stm32f103vetx",
    ["stm32f103"] = "stm32f103vetx",
    ["stm32f103vetx"] = "stm32f103vetx",
  };

  public static int Main(string[] args) {
    try {
      Options options = Options.Parse(args);
      string repoRoot = FindRepositoryRoot(Directory.GetCurrentDirectory());

      if (options.ShowHelp) {
        PrintUsage();
        return 0;
      }

      string solutionPath = FindSolution(repoRoot, options.SolutionPath);
      IReadOnlyList<string> projects = FindVisualGdbProjects(repoRoot);

      if (options.ListProjects) {
        PrintProjects(repoRoot, projects);
        return 0;
      }

      string projectPath = ResolveProject(repoRoot, projects, options);
      VisualGdbProject project = ReadVisualGdbProject(projectPath);
      string visualGdbExe = FindVisualGdbExe(options.VisualGdbPath);

      if (options.Diagnose) {
        PrintDiagnostics(repoRoot, solutionPath, projectPath, project, visualGdbExe, options);
        return 0;
      }

      string actionSwitch = options.Action.ToLowerInvariant() switch {
        "build" => "/build",
        "clean" => "/clean",
        "rebuild" => "/rebuild",
        _ => throw new InvalidOperationException($"Unsupported action '{options.Action}'. Use build, clean, or rebuild."),
      };

      string configuration = options.Configuration ?? DefaultConfiguration;
      string platform = options.Platform ?? DefaultPlatform;
      string buildDir = ExpandVisualGdbBinaryDirectory(project.BinaryDirectory, platform, configuration);

      Console.WriteLine($"VisualGDB: {visualGdbExe}");
      Console.WriteLine($"Project:   {Path.GetRelativePath(repoRoot, projectPath)}");
      Console.WriteLine($"Solution:  {Path.GetRelativePath(repoRoot, solutionPath)}");
      Console.WriteLine($"Config:    {configuration}|{platform}");
      Console.WriteLine($"BuildDir:  {Path.GetRelativePath(Path.GetDirectoryName(projectPath)!, Path.Combine(Path.GetDirectoryName(projectPath)!, buildDir))}");
      Console.WriteLine();

      string[] visualGdbArgs = [
        actionSwitch,
        projectPath,
        $"/solution:{solutionPath}",
        $"/config:{configuration}",
        $"/platform:{platform}",
      ];

      Console.WriteLine(QuoteCommand(visualGdbExe, visualGdbArgs));
      if (options.DryRun) {
        return 0;
      }

      return RunProcess(visualGdbExe, visualGdbArgs, repoRoot);
    } catch (Exception ex) {
      Console.Error.WriteLine("VisualGDBBuild: " + ex.Message);
      return 1;
    }
  }

  private static string FindRepositoryRoot(string startDirectory) {
    DirectoryInfo? current = new(startDirectory);
    while (current != null) {
      if (File.Exists(Path.Combine(current.FullName, "IoTEmBASIC.sln")) ||
          Directory.Exists(Path.Combine(current.FullName, ".git"))) {
        return current.FullName;
      }

      current = current.Parent;
    }

    throw new InvalidOperationException("Cannot find repository root from " + startDirectory);
  }

  private static string FindSolution(string repoRoot, string? requestedPath) {
    if (!string.IsNullOrWhiteSpace(requestedPath)) {
      string fullPath = MakeFullPath(repoRoot, requestedPath);
      EnsureFileExists(fullPath, "solution");
      return fullPath;
    }

    string defaultSolution = Path.Combine(repoRoot, "IoTEmBASIC.sln");
    if (File.Exists(defaultSolution)) {
      return defaultSolution;
    }

    string? discovered = Directory.EnumerateFiles(repoRoot, "*.sln", SearchOption.TopDirectoryOnly).FirstOrDefault();
    if (discovered != null) {
      return discovered;
    }

    throw new InvalidOperationException("No .sln file found at repository root.");
  }

  private static IReadOnlyList<string> FindVisualGdbProjects(string repoRoot) {
    string projectsRoot = Path.Combine(repoRoot, "projects");
    if (!Directory.Exists(projectsRoot)) {
      return [];
    }

    return Directory.EnumerateFiles(projectsRoot, "*.vgdbcmake", SearchOption.AllDirectories)
      .OrderBy(p => p, StringComparer.OrdinalIgnoreCase)
      .ToArray();
  }

  private static string ResolveProject(string repoRoot, IReadOnlyList<string> projects, Options options) {
    if (!string.IsNullOrWhiteSpace(options.ProjectPath)) {
      string fullPath = MakeFullPath(repoRoot, options.ProjectPath);
      EnsureFileExists(fullPath, "VisualGDB project");
      return fullPath;
    }

    if (projects.Count == 0) {
      throw new InvalidOperationException("No .vgdbcmake project found under projects/.");
    }

    string target = options.Target ?? DefaultTarget;
    string normalizedTarget = NormalizeAlias(target);
    string hint = KnownTargetHints.TryGetValue(normalizedTarget, out string? knownHint) ? knownHint : normalizedTarget;
    string normalizedHint = NormalizeAlias(hint);

    List<string> matches = projects
      .Where(project => ProjectMatches(project, normalizedTarget, normalizedHint))
      .ToList();

    if (matches.Count == 1) {
      return matches[0];
    }

    if (matches.Count > 1) {
      throw new InvalidOperationException($"Target '{target}' is ambiguous. Use --project with one of: {string.Join(", ", matches.Select(p => Path.GetRelativePath(repoRoot, p)))}");
    }

    throw new InvalidOperationException($"Target '{target}' was not found. Use --list to show available VisualGDB projects.");
  }

  private static bool ProjectMatches(string projectPath, string normalizedTarget, string normalizedHint) {
    string fileName = NormalizeAlias(Path.GetFileNameWithoutExtension(projectPath));
    if (fileName == normalizedTarget || fileName == normalizedHint) {
      return true;
    }

    foreach (string part in projectPath.Split(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar)) {
      string normalizedPart = NormalizeAlias(part);
      if (normalizedPart == normalizedTarget || normalizedPart == normalizedHint) {
        return true;
      }
    }

    return false;
  }

  private static VisualGdbProject ReadVisualGdbProject(string projectPath) {
    XDocument document = XDocument.Load(projectPath);

    XElement? toolchain = document.Descendants().FirstOrDefault(e => e.Name.LocalName == "ToolchainID");
    string toolchainId = toolchain?.Elements().FirstOrDefault(e => e.Name.LocalName == "ID")?.Value.Trim() ?? "";
    XElement? version = toolchain?.Elements().FirstOrDefault(e => e.Name.LocalName == "Version");
    string gccVersion = version?.Elements().FirstOrDefault(e => e.Name.LocalName == "GCC")?.Value.Trim() ?? "";
    string gdbVersion = version?.Elements().FirstOrDefault(e => e.Name.LocalName == "GDB")?.Value.Trim() ?? "";
    string revision = version?.Elements().FirstOrDefault(e => e.Name.LocalName == "Revision")?.Value.Trim() ?? "";
    string toolchainVersion = string.IsNullOrWhiteSpace(gccVersion) ? "" : $"{gccVersion}/{gdbVersion}/r{revision}";

    string binaryDirectory = document.Descendants()
      .FirstOrDefault(e => e.Name.LocalName == "BinaryDirectory")?.Value.Trim() ?? "build/$(PlatformName)/$(ConfigurationName)";

    string makeCommand = document.Descendants()
      .FirstOrDefault(e => e.Name.LocalName == "MakeCommandTemplate")?
      .Descendants().FirstOrDefault(e => e.Name.LocalName == "Command")?.Value.Trim() ?? "$(VISUALGDB_DIR)/ninja.exe";

    string cmakeCommand = document.Descendants()
      .FirstOrDefault(e => e.Name.LocalName == "CMakeCommand")?
      .Descendants().FirstOrDefault(e => e.Name.LocalName == "Command")?.Value.Trim() ?? "$(SYSPROGS_CMAKE_PATH)";

    return new VisualGdbProject(toolchainId, toolchainVersion, binaryDirectory, makeCommand, cmakeCommand);
  }

  private static string FindVisualGdbExe(string? requestedPath) {
    if (!string.IsNullOrWhiteSpace(requestedPath)) {
      string fullPath = Path.GetFullPath(requestedPath);
      EnsureFileExists(fullPath, "VisualGDB executable");
      return fullPath;
    }

    string? envExe = Environment.GetEnvironmentVariable("VISUALGDB_EXE");
    if (!string.IsNullOrWhiteSpace(envExe) && File.Exists(envExe)) {
      return Path.GetFullPath(envExe);
    }

    string? envDir = Environment.GetEnvironmentVariable("VISUALGDB_DIR");
    if (!string.IsNullOrWhiteSpace(envDir)) {
      string fromEnvDir = Path.Combine(envDir, "VisualGDB.exe");
      if (File.Exists(fromEnvDir)) {
        return Path.GetFullPath(fromEnvDir);
      }
    }

    string[] candidates = [
      Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86), "Sysprogs", "VisualGDB", "VisualGDB.exe"),
      Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles), "Sysprogs", "VisualGDB", "VisualGDB.exe"),
    ];

    foreach (string candidate in candidates) {
      if (File.Exists(candidate)) {
        return candidate;
      }
    }

    throw new InvalidOperationException("VisualGDB.exe was not found. Set VISUALGDB_EXE, VISUALGDB_DIR, or pass --visualgdb <path>.");
  }

  private static void PrintDiagnostics(string repoRoot, string solutionPath, string projectPath, VisualGdbProject project,
                                       string visualGdbExe, Options options) {
    string visualGdbDir = Path.GetDirectoryName(visualGdbExe)!;
    string localAppData = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
    string visualGdbLocal = Path.Combine(localAppData, "VisualGDB");
    string cmakePath = Path.Combine(visualGdbLocal, "CMake", "bin", "cmake.exe");
    string ninjaPath = Path.Combine(visualGdbDir, "ninja.exe");
    string toolchainRoot = FindToolchainRoot(visualGdbLocal, project);
    string gccPath = string.IsNullOrWhiteSpace(toolchainRoot) ? "" : Path.Combine(toolchainRoot, "bin", "arm-none-eabi-gcc.exe");
    string configuration = options.Configuration ?? DefaultConfiguration;
    string platform = options.Platform ?? DefaultPlatform;
    string buildDir = Path.Combine(Path.GetDirectoryName(projectPath)!, ExpandVisualGdbBinaryDirectory(project.BinaryDirectory, platform, configuration));

    Console.WriteLine("Repository:        " + repoRoot);
    Console.WriteLine("Solution:          " + solutionPath);
    Console.WriteLine("Project:           " + projectPath);
    Console.WriteLine("Configuration:     " + configuration);
    Console.WriteLine("Platform:          " + platform);
    Console.WriteLine("Build directory:   " + buildDir);
    Console.WriteLine("VisualGDB.exe:     " + ExistingPath(visualGdbExe));
    Console.WriteLine("VisualGDB CMake:   " + ExistingPath(cmakePath));
    Console.WriteLine("VisualGDB Ninja:   " + ExistingPath(ninjaPath));
    Console.WriteLine("Toolchain ID:      " + EmptyAsUnknown(project.ToolchainId));
    Console.WriteLine("Toolchain version: " + EmptyAsUnknown(project.ToolchainVersion));
    Console.WriteLine("Toolchain root:    " + ExistingPath(toolchainRoot));
    Console.WriteLine("ARM GCC:           " + ExistingPath(gccPath));
    Console.WriteLine("Project CMake:     " + project.CMakeCommand);
    Console.WriteLine("Project Make:      " + project.MakeCommand);
  }

  private static string FindToolchainRoot(string visualGdbLocal, VisualGdbProject project) {
    string propsPath = Path.Combine(visualGdbLocal, "FindToolchain.props");
    if (File.Exists(propsPath)) {
      XDocument document = XDocument.Load(propsPath);
      foreach (XElement element in document.Descendants().Where(e => e.Name.LocalName == "ToolchainDir")) {
        string condition = element.Attribute("Condition")?.Value ?? "";
        if (!condition.Contains(project.ToolchainId, StringComparison.OrdinalIgnoreCase)) {
          continue;
        }

        if (!string.IsNullOrWhiteSpace(project.ToolchainVersion) &&
            !condition.Contains(project.ToolchainVersion, StringComparison.OrdinalIgnoreCase) &&
            !condition.Contains("$(ToolchainVersion)' == ''", StringComparison.OrdinalIgnoreCase)) {
          continue;
        }

        string expanded = ExpandMsBuildProperties(element.Value.Trim(), new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase) {
          ["LOCALAPPDATA"] = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
        });
        if (!string.IsNullOrWhiteSpace(expanded)) {
          return expanded;
        }
      }
    }

    string sysGcc = Path.Combine(Path.GetPathRoot(Environment.SystemDirectory) ?? "C:\\", "SysGCC", "arm-eabi");
    return Directory.Exists(sysGcc) ? sysGcc : "";
  }

  private static string ExpandVisualGdbBinaryDirectory(string binaryDirectory, string platform, string configuration) {
    return binaryDirectory
      .Replace("$(PlatformName)", platform, StringComparison.OrdinalIgnoreCase)
      .Replace("$(ConfigurationName)", configuration, StringComparison.OrdinalIgnoreCase)
      .Replace('\\', Path.DirectorySeparatorChar)
      .Replace('/', Path.DirectorySeparatorChar);
  }

  private static string ExpandMsBuildProperties(string value, IReadOnlyDictionary<string, string> properties) {
    string expanded = value;
    foreach ((string key, string propertyValue) in properties) {
      expanded = expanded.Replace("$(" + key + ")", propertyValue, StringComparison.OrdinalIgnoreCase);
    }

    return Environment.ExpandEnvironmentVariables(expanded);
  }

  private static void PrintProjects(string repoRoot, IReadOnlyList<string> projects) {
    if (projects.Count == 0) {
      Console.WriteLine("No .vgdbcmake projects found.");
      return;
    }

    foreach (string project in projects) {
      Console.WriteLine(Path.GetRelativePath(repoRoot, project));
    }
  }

  private static int RunProcess(string executable, IReadOnlyList<string> arguments, string workingDirectory) {
    ProcessStartInfo startInfo = new() {
      FileName = executable,
      WorkingDirectory = workingDirectory,
      UseShellExecute = false,
    };

    foreach (string argument in arguments) {
      startInfo.ArgumentList.Add(argument);
    }

    using Process? process = Process.Start(startInfo);
    if (process == null) {
      throw new InvalidOperationException("Failed to start " + executable);
    }

    process.WaitForExit();
    return process.ExitCode;
  }

  private static string MakeFullPath(string repoRoot, string path) {
    return Path.GetFullPath(Path.IsPathRooted(path) ? path : Path.Combine(repoRoot, path));
  }

  private static void EnsureFileExists(string path, string label) {
    if (!File.Exists(path)) {
      throw new InvalidOperationException($"{label} not found: {path}");
    }
  }

  private static string NormalizeAlias(string value) {
    return new string(value.Where(char.IsLetterOrDigit).Select(char.ToLowerInvariant).ToArray());
  }

  private static string ExistingPath(string path) {
    if (string.IsNullOrWhiteSpace(path)) {
      return "(not found)";
    }

    return File.Exists(path) || Directory.Exists(path) ? path : path + " (not found)";
  }

  private static string EmptyAsUnknown(string value) {
    return string.IsNullOrWhiteSpace(value) ? "(unknown)" : value;
  }

  private static string QuoteCommand(string executable, IReadOnlyList<string> arguments) {
    return string.Join(" ", new[] { Quote(executable) }.Concat(arguments.Select(Quote)));
  }

  private static string Quote(string value) {
    return value.Contains(' ') || value.Contains('\t') || value.Contains('"')
      ? "\"" + value.Replace("\"", "\\\"", StringComparison.Ordinal) + "\""
      : value;
  }

  private static void PrintUsage() {
    Console.WriteLine("""
Usage:
  dotnet run --project tools/VisualGDBBuild -- [target] [options]

Targets:
  l4 | pandora          Build projects/stm32/l4/stm32l475vetx/STM32L475VETX.vgdbcmake
  f1 | powerenvdaq      Build projects/stm32/f1/stm32f103vetx/STM32F103VETX.vgdbcmake

Options:
  --action <build|clean|rebuild>   VisualGDB action. Default: build
  --config <name>                  VisualGDB configuration. Default: Debug
  --platform <name>                VisualGDB platform. Default: VisualGDB
  --project <path>                 Explicit .vgdbcmake project path
  --solution <path>                Explicit .sln path
  --visualgdb <path>               Explicit VisualGDB.exe path
  --diagnose                       Print VisualGDB/CMake/Ninja/toolchain paths
  --list                           List .vgdbcmake projects
  --dry-run                        Print the VisualGDB command without running it
  --help                           Show this help
""");
  }

  private sealed class Options {
    public string? Target { get; private set; }
    public string Action { get; private set; } = "build";
    public string? Configuration { get; private set; }
    public string? Platform { get; private set; }
    public string? ProjectPath { get; private set; }
    public string? SolutionPath { get; private set; }
    public string? VisualGdbPath { get; private set; }
    public bool Diagnose { get; private set; }
    public bool DryRun { get; private set; }
    public bool ListProjects { get; private set; }
    public bool ShowHelp { get; private set; }

    public static Options Parse(IReadOnlyList<string> args) {
      Options options = new();
      for (int i = 0; i < args.Count; i++) {
        string arg = args[i];
        if (!arg.StartsWith("-", StringComparison.Ordinal)) {
          options.Target ??= arg;
          continue;
        }

        switch (arg.ToLowerInvariant()) {
          case "--action":
            options.Action = RequireValue(args, ref i, arg);
            break;
          case "--config":
          case "--configuration":
            options.Configuration = RequireValue(args, ref i, arg);
            break;
          case "--platform":
            options.Platform = RequireValue(args, ref i, arg);
            break;
          case "--project":
            options.ProjectPath = RequireValue(args, ref i, arg);
            break;
          case "--solution":
            options.SolutionPath = RequireValue(args, ref i, arg);
            break;
          case "--visualgdb":
            options.VisualGdbPath = RequireValue(args, ref i, arg);
            break;
          case "--diagnose":
            options.Diagnose = true;
            break;
          case "--dry-run":
            options.DryRun = true;
            break;
          case "--list":
            options.ListProjects = true;
            break;
          case "--help":
          case "-h":
          case "/?":
            options.ShowHelp = true;
            break;
          default:
            throw new InvalidOperationException("Unknown option: " + arg);
        }
      }

      return options;
    }

    private static string RequireValue(IReadOnlyList<string> args, ref int index, string option) {
      if (index + 1 >= args.Count) {
        throw new InvalidOperationException(option + " requires a value.");
      }

      index++;
      return args[index];
    }
  }

  private sealed record VisualGdbProject(string ToolchainId, string ToolchainVersion, string BinaryDirectory,
                                         string MakeCommand, string CMakeCommand);
}
