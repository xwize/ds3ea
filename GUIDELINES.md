* Prefer reference/pointer hugging the type e.g. int* var; over int *var;
* Variable names must be at least 3 letters, except x y z w dx dy dz dw i j k e n (clang-tidy)
* Functions stay simple: cognitive complexity at most 25, nesting at most 4 levels, at most 6 parameters. Split long
  functions into named helpers and group related parameters in a struct; only a fixed API signature (e.g. a Win32
  hook) may exceed these, with a comment giving the reason (clang-tidy)
* Three or more // lines in a row are a doxygen block instead:
	/** 
	 */
* Every trailing comment starts with a capital letter, unless it starts with code (an identifier, a keyword such as
  the `} // namespace` that closes every namespace, or an expression like `w = 1/R^2`). Trailing comments on struct
  members also share one column across the struct. A struct uses either trailing member comments or comments above members, never both:
  once one member needs a comment above it, every member comment goes above its member.

* Include files follow the form 
	#include "gen/supporting.hpp" // supportive code specific to this module (generated shaders for example)
	(include)
	#include "alpha.hpp" // other project headers 
	#include "beta.hpp"
	(intentional newline)
	#include <fmt/format.h> // third-party libraries (fmt, gtest, imgui etc)
	#include <gtest/gtest.h>
	(intentional newline)
	#include <system_header_a> // system headers (d3d, win32, reshade etc)
	#include <system_header_b>
	(intentional newline)
	#include <stdlib_header_a> // standard library / STL
	#include <stdlib_header_b>
	
* Lines must be 120 columns MAX 
* Free functions in .cpp files go into anon namespaces which end with } // namespace
* Namespace contents are not indented: everything inside a namespace (named or anonymous, including forward
  declaration blocks) starts in column 0
* Class members are ordered: constructors and destructor first, then deleted/defaulted copy and move operations, then
  the remaining public members (static factories included), then private. The only thing allowed above the
  constructors is a declaration they need, such as a passkey type, kept to a one-line forward declaration and
  defined in the .cpp
* A constructor's initialiser list starts on its own line with `:`, indented one level; further initialisers that
  don't fit go on their own `,`-prefixed lines
	Addon::Addon(HMODULE hmod, Key /*key*/)
		: m_module(hmod)
	{
* Each module or hpp that introduces a new namespace or interface should have a doxygen block explaining it
* Each struct or class has a summary doxygen explaining it 
* Every function declared in an hpp (the public API) has a doxygen block with an @param for each parameter and
  @returns for a non-void return (spelled @param and @returns, not @return or \param). Struct fields and enum values
  keep their trailing // comments. A header with functions outside any namespace needs a /** @file */ block.
* Follow the reshade style where possible (but treat this file as an override)
* Naming: PascalCase for types (classes, structs, enums, aliases); snake_case for namespaces, variables, functions,
  methods and enumerators (enum class Level { error, warning };) (clang-tidy)
* Prefix g_ for globals, m_ for private and protected class members and k_ for compile-time constants (constexpr
  values, and const values initialised from a constant expression); everything else is left unprefixed, including
  locals, statics, parameters, public struct fields and const variables or const references to runtime values
  e.g. constexpr int k_max_passes = 8;   but   const Settings& settings = get_settings();
  (clang-tidy checks the prefixes, except k_ on const values that aren't constexpr)
* Braceless if bodies are permitted provided they are followed by an empty newline 
*  e.g. if (x)
*	        return;
*	    (blank line here)
* A statement that follows a closing brace } is preceded by an empty newline, except after other closing braces or
  when it continues the same statement (else, __except, catch)
* Getters should generally start with get_ and be const where possible e.g. int get_age() const;
* Shaders live in shaders/<name>.hlsl, one per file with a doxygen block saying what it does and who draws it.
  They are compiled at build time (warnings are errors), before the C++, into src/gen/<name>.h holding the bytecode
  as k_<name> (generated, not in git; in the source tree so IDEs resolve it); include it as "gen/<name>.h" and create
  the shader from k_<name>.
  Game layouts are hardcoded for 1.15.x (the add-on only runs past the version gate), so shader source is never
  generated at runtime.
* No shader is compiled at runtime and no HLSL lives in C++ strings: settings that change a shader's output go in a
  constant buffer, not in generated source.
* Use RAII wherever possible, for D3D11 objects strongly consider automatic lifetime management via ComPtr 
* No naked new & delete: own objects with std::make_unique, a ComPtr or by value, and never skip cleanup (release
  explicitly when the device goes away) (clang-tidy)
* Use structs to group related variables 
* General structure of a cpp should be <types> <state> <functions>. In the anonymous namespace:
	1. types and constants: struct, enum, using, const/constexpr values, constexpr functions
	2. state: every static and thread_local variable together, one per line, with its lifetime and locking comments
	   (the struct's own doxygen block stays with the struct)
	3. helper functions
  followed by the module's public functions.
* One variable per declaration everywhere: locals, namespace-scope variables and struct members (clang-tidy)
* Casts are static_cast (or another named cast), never C-style or functional; arrays are std::array, never C arrays;
  every local is initialised where it is declared (clang-tidy)
* Prefer fmt for formatting 
* class is prefered for classes with methods
* struct is preferred for plain old data 
* One class per header file (but many pod structs are OK)
* Conan for packages 
* Errors: a failure is described by one ds3ea::Error (an ErrorCategory, a message and the source location where it
  was raised), reported one of two ways:
	- Result<T> (result.hpp) for functions deep in the call stack and called often (per frame, per draw, per
	  resource), or whose error handling is very simple (the caller checks it and falls back). Return
	  make_error(category, "format", args...). Never call .value() on a Result: check and dereference it, chain it
	  (and_then, map, or_else) or use get_or_throw()
	- Exceptions for every other failure: I/O, constructors that can't fully initialise, setup, anything that has to
	  travel up several levels. Throw with throw_error(category, "format", args...), which throws a
	  RuntimeException; capture() turns one back into a Result
	- assert (<cassert>) for programmer errors (null where non-null is required, an index out of range, a call in
	  the wrong state), never Result or an exception: a broken precondition is a bug, not a runtime error
* Exceptions never escape into ReShade, D3D11, the game or Windows: every boundary (each ReShade event callback,
  DllMain, add-on init) runs its work through call_at_boundary() (boundary.hpp), which logs and never rethrows.
  Where it makes sense the boundary also disables the affected feature
* A catch that handles only some categories switches on get_error().category, handles those and rethrows the rest
  with throw;. Never swallow unhandled categories in a default:
* Rules marked (clang-tidy) are checked on every build with the checks in .clang-tidy (test/.clang-tidy relaxes two
  for tests) and fail it. Configure with -DDS3EA_CLANG_TIDY=OFF for a faster local build without it.
  // NOLINT(check-name) is only for names or forms an external API forces (Win32, ReShade, DllMain), with the reason
