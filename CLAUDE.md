# CLAUDE.md - BuildSystem Module

This file provides guidance to Claude Code (claude.ai/code) when working with the Malterlib BuildSystem module.

## Module Overview

The BuildSystem module is the core of Malterlib's custom build system (mib/MTool). It provides a sophisticated build configuration DSL, cross-platform project generation, and repository management capabilities. The module implements parsing, evaluation, and generation of build configurations across multiple platforms (macOS, Windows, Linux) and IDEs (Xcode, Visual Studio).

## Architecture

### Core Components

#### Build System DSL
The build system uses a custom Domain-Specific Language (DSL) for describing build configurations:
- **File Types**:
  - `.MBuildSystem` - Root build system configuration
  - `.MHeader` - Module header files defining targets and properties
  - `.MTarget` - Target definitions
  - `.MConfig` - Configuration files
  - `.MSettings` - Settings files
  - `.MRepo` - Repository configuration

#### Main Classes
- `CBuildSystem` - Main build system orchestrator
- `CTarget` - Build target representation
- `CWorkspace` - Workspace (collection of targets) management
- `CProperty` - Property management and evaluation
- `CEntity` - Base class for build entities
- `CGenerator` - Base class for platform-specific generators
- `CGeneratorState` - Generation state management
- `CRegistry` - Build system registry for properties and settings

### Directory Structure

```
BuildSystem/
├── Include/Mib/BuildSystem/     # Public API headers
│   ├── BuildSystem               # Main build system API
│   ├── BuildSystemDependency     # Dependency management
│   ├── BuildSystemPreprocessor  # Preprocessor functionality
│   └── Registry                  # Registry API
├── Source/                       # Implementation
│   ├── Generators/               # Platform-specific generators
│   │   ├── VisualStudio/        # Visual Studio project generation
│   │   └── Xcode/               # Xcode project generation
│   ├── Malterlib_BuildSystem_*.cpp/h  # Core implementation files
│   └── Various subsystems (Parse, Evaluate, Generate, etc.)
├── Test/                         # Unit tests
└── TestFiles/                    # Test configuration files
```

## DSL Syntax Reference

### Entity Types
Entities are prefixed with special characters:
- `%` - Declares an entity (Target, Group, File, Dependency, etc.)
- `*` - Declares configuration tuple (Configuration, Platform, Architecture)
- No prefix - Property assignment

### Property Types
Properties support various types and modifiers based on EJSON:
- Basic types: `bool`, `string`, `int`, `float`
- Arrays: `[string]`, `[int]`, `[bool]`, etc.
- Maps/Objects: `{string: string}`, `{string: any}`
- Optional: `?` suffix (e.g., `bool?`)
- Functions: `function(parameters) return_type`
- Custom types: Defined with `Type` declarations
- Any type: `any` for dynamic values

#### Property Assignment and Default Values
- `Key "Value"` - Direct assignment, overwrites any previous value
- `Key: type = "Default"` - Sets a default value that only applies if the property remains undefined after all other processing
- `+=` - Prepends to existing value (if a property has `+=`, any default value set with `=` is ignored)
- `=+` - Appends to existing value (if a property has `=+`, any default value set with `=` is ignored)

### Conditions
Conditions filter when properties or entities apply:
```
Property
{
    OptimizationLevel "O3"
    {
        !!Configuration "Release"    // Double !! means condition
    }
}
```

Common condition operators:
- `!!` - Condition prefix
- `!` - NOT operator
- `&` - AND operator
- `|` - OR operator

### Code Examples Following Standards

#### Target Definition
```malterlib-build
%Target "Lib_Malterlib_BuildSystem"
{
	Target
	{
		Name "Lib_Malterlib_BuildSystem"
		Type "StaticLibrary"
	}

	Property
	{
		MalterlibNoShortcuts false
	}

	Compile
	{
		!!CompileDialect "C"
		PrefixHeader "Source/Malterlib_BuildSystem.h"->MakeAbsolute()
		{
			|
			{
				Compile.Type "C++"
				!GeneratorFamily "VisualStudio"
			}
		}
	}

	%Group "BuildSystem"
	{
		%Group "Include"
		{
			Compile.Type "C++"
			%File "Include/Mib/BuildSystem/^*"
		}
		%File "Source/^*"
	}

	%Dependency "Lib_Malterlib_Core"
}
```

#### Workspace Definition
```malterlib-build
%Workspace "BuildSystem"
{
	Workspace
	{
		Name "BuildSystem"
	}

	%Target "Lib_Malterlib_BuildSystem"
	%Target "Test_Malterlib_BuildSystem"
	{
		!!MalterlibEnableInternalTests true
	}
}
```

#### Property Declaration with Types
```malterlib-build
Property
{
	MalterlibBuildSystemEmbedCMake: bool?
	BuildOutputDirectory: string = "/opt/Deploy"
	IncludePaths: [string] = []
	CompilerFlags: [string] = ["-Wall", "-Werror"]  // Default only used if CompilerFlags is undefined
}

Property
{
	!!Configuration "Debug"
	CompilerFlags =+ ["-g", "-O0"]  // This appends, so default ["-Wall", "-Werror"] won't apply
}
```

### Built-in Functions

The DSL provides numerous built-in functions for path manipulation, string operations, and more:

#### Path Functions
- `MakeAbsolute(path)` - Convert to absolute path
- `MakeRelative(path, base)` - Convert to relative path
- `GetFile(path)` - Extract filename from path
- `GetPath(path)` - Extract directory from path
- `GetExtension(path)` - Extract file extension
- `GetFileNoExt(path)` - Get filename without extension
- `GetDrive(path)` - Get drive letter (Windows)
- `AppendPath(path, ...)` - Append path components
- `IsAbsolute(path)` - Check if path is absolute
- `WindowsPath(path)` - Convert to Windows path format
- `UnixPath(path)` - Convert to Unix path format
- `NativePath(path)` - Convert to native OS path format
- `ShortenPath(path)` - Shorten long paths (Windows)
- `RelativeBase(path)` - Get path relative to build system base
- `GetLastPaths(path, n)` - Get last n path components
- `RemoveStartPaths(path, n)` - Remove first n path components

#### String Functions
- `ToString(...)` - Convert values to string
- `ToStringCompact(...)` - Convert to compact string
- `EJsonToString(value, indent?)` - Convert to EJSON string
- `JsonToString(value, indent?)` - Convert to JSON string
- `ParseEJson(string, filename?)` - Parse EJSON string
- `ParseJson(string, filename?)` - Parse JSON string
- `Split(source, splitBy)` - Split string into array
- `Join(strings, joinBy)` - Join array into string
- `Replace(source, searchFor, replaceWith)` - Replace string occurrences
- `ReplaceChars(string, searchForChars, replaceWith)` - Replace characters
- `Trim(source)` - Trim whitespace
- `Escape(source)` - Escape special characters
- `EscapeHost(source...)` - Escape for host OS
- `EscapeWindows(source...)` - Escape for Windows
- `EscapeBash(source...)` - Escape for Bash
- `EscapeMSBuild(string)` - Escape for MSBuild

#### List Operations
- `ForEach(array, function, properties?)` - Apply function to each element
- `ContainsListElement(array, element)` - Check if array contains element
- `Length(array)` - Get length of array or string
- `IsEmpty(array)` - Check if array or string is empty
- `Unique(array)` - Remove duplicate elements
- `RemoveDuplicates(array, value)` - Remove duplicates of specific value
- `Sort(array)` - Sort array elements
- `Concat(arrays...)` - Concatenate multiple arrays

#### File System Operations
- `GeneratedFiles(wildcard)` - Get list of generated files matching wildcard
- `SourceFiles(wildcard)` - Get list of source files matching wildcard
- `ReadFile(filename, byDigest?)` - Read file contents
- `FileExists(filename)` - Check if file exists
- `LinkExists(filename)` - Check if symbolic link exists
- `ResolveSymbolicLink(filename)` - Resolve symbolic link
- `DirectoryExists(filename)` - Check if directory exists
- `FileOrDirectoryExists(filename)` - Check if file or directory exists
- `FindFilesIn(path, wildcard, excludeWildcards?)` - Find files in directory
- `FindDirectoriesIn(path, wildcard, excludeWildcards?)` - Find directories
- `FindFilesRecursiveIn(path, wildcard, excludeWildcards?)` - Find files recursively
- `FindDirectoriesRecursiveIn(path, wildcard, excludeWildcards?)` - Find directories recursively

## Generator System

### Supported Generators
- **Xcode** - macOS/iOS development
- **Visual Studio** - Windows development

### Generator Settings
Generators can be configured through `.MSettings` files:
```malterlib-build
GeneratorSetting
{
	XcodeVersion "14.0"
	VisualStudioVersion "2022"
	CMakeMinimumVersion "3.20"
}
```

## Repository Management

The BuildSystem includes integrated repository management:

### Repository Configuration
```malterlib-build
%Repository "Malterlib"
{
	Repository
	{
		Path "."
		Type "git"
		Remote "origin"
		DefaultBranch "master"
	}
}
```

### Repository Operations
- Status checking
- Branch management
- Synchronized operations across multiple repos
- Git LFS support for binary dependencies

## Testing

### Test Structure
Tests are organized in `Test/` directory:
- `Test_Malterlib_BuildSystem_General.cpp` - General functionality tests
- `Test_Malterlib_BuildSystem_Generate.cpp` - Generation tests
- `Test_Malterlib_BuildSystem_Parse.cpp` - Parser tests
- `Test_Malterlib_BuildSystem_Syntax.cpp` - Syntax tests

### Test Files
`TestFiles/` contains various test configurations demonstrating DSL features:
- Basic configurations (Simple, Empty)
- Advanced features (ForEach, Conditions, Functions)
- Type system (Types, DefaultedTypes, DynamicValue)
- Generation (GenerateFile, Expand)

## Integration with mib Tool

The BuildSystem module is the engine behind the `mib` command-line tool. Key integration points:

## Performance Considerations

### Parallel Processing
The BuildSystem uses parallel processing for:
- File parsing
- Dependency resolution
- Project generation
- Repository operations

### String Caching
The module uses `CStringCache` for efficient string handling:
```cpp
CStringCache StringCache;
CPropertyKey Key(StringCache, "PropertyName");
```

## Code Standards Specific to BuildSystem

### Naming Conventions for DSL Elements
- **Targets**: `Lib_`, `Exe_`, `Com_`, `Tool_` prefixes
- **Workspaces**: Descriptive names matching module or purpose
- **Properties**: CamelCase without prefixes in DSL
- **Functions**: CamelCase for DSL built-ins

### File Organization
- Keep `.MHeader` files at module root
- Place `.MTarget` files near their implementation
- Group related configurations in subdirectories

### DSL Best Practices
- Use conditions sparingly for clarity
- Prefer composition over complex conditions
- Document non-obvious property dependencies
- Use type annotations for custom types

### Example Following Standards
```malterlib-build
// Proper indentation and spacing in DSL files
%Target "Lib_Malterlib_BuildSystem"
{
	Target
	{
		Name "Lib_Malterlib_BuildSystem"
		Type "StaticLibrary"
	}

	// Group related properties
	Property
	{
		OutputDirectory: string = "/opt/Deploy/BuildSystem"
		IntermediateDirectory: string = "/opt/Build/BuildSystem"
	}

	// Use clear condition grouping
	Compile
	{
		PreprocessorDefines =+ ["DEBUG", "ASSERTIONS"]
		{
			!!Configuration "Debug"
		}

		PreprocessorDefines =+ ["NDEBUG", "OPTIMIZE"]
		{
			!!Configuration "Release"
		}
	}

	// Organize files logically
	%Group "Public"
	{
		%File "Include/^*"
	}

	%Group "Implementation"
	{
		%File "Source/^*"
	}

	// Clear dependency declarations
	%Dependency "Lib_Malterlib_Core"
}
```

## Debugging Build Configurations

### Trace Property Resolution
```malterlib-build
Property
{
	TestProperty "Value"
	{
		#Debug "TraceEval,TraceCondition"
	}
}
```

## Common Patterns

### Conditional Compilation
```malterlib-build
%File "platform_specific.cpp"
{
	!!PlatformFamily "Windows"
}

%File "platform_specific.mm"
{
	!!PlatformFamily "macOS"
}
```

## Important Notes

- The BuildSystem is the foundation of Malterlib's build infrastructure
- Changes to this module can affect the entire build process
- Always test generator changes on all supported platforms
- The DSL parser is recursive descent with backtracking
- String interpolation uses `` ` `` backticks with `@()` for expressions
- Property evaluation is lazy and cached for performance
- The module supports distributed builds through NConcurrency
- File patterns use `^` for recursive matching
- Conditions are evaluated at generation time, not build time
- The build system maintains a dependency graph for incremental builds
