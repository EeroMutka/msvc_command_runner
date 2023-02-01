#include "foundation/foundation.hpp"
#include "microsoft_craziness.h"

int main(int argc, const char** argv) {
	Allocator* a = temp_push();

	os_write_to_console_colored(LIT("-------- Running cl.exe from msvc_command_runner --------\n"), ConsoleAttribute_Blue | ConsoleAttribute_Green);

	WinSDK_Find_Result windows_sdk = WinSDK_find_visual_studio_and_windows_sdk();
	String msvc_directory = str_from_utf16(windows_sdk.vs_exe_path, a); // contains cl.exe, link.exe
	String windows_sdk_include_base_path = str_from_utf16(windows_sdk.windows_sdk_include_base, a); // contains <string.h>, etc
	String windows_sdk_um_library_path = str_from_utf16(windows_sdk.windows_sdk_um_library_path, a); // contains kernel32.lib, etc
	String windows_sdk_ucrt_library_path = str_from_utf16(windows_sdk.windows_sdk_ucrt_library_path, a); // contains libucrt.lib, etc
	String vs_library_path = str_from_utf16(windows_sdk.vs_library_path, a); // contains MSVCRT.lib etc
	String vs_include_path = str_from_utf16(windows_sdk.vs_include_path, a); // contains vcruntime.h
	
	// https://learn.microsoft.com/en-us/cpp/build/reference/compiler-command-line-syntax?view=msvc-170

	Slice<String> args = make_slice_garbage<String>(argc - 1, a);
	Slice<String> cl_args = args;
	Slice<String> link_args = {};

	for (uint i = 0; i < argc - 1; i++) {
		String arg = str_from_cstring(argv[i + 1]);
		args[i] = arg;
		if (str_equals_nocase(arg, LIT("/link")) || str_equals_nocase(arg, LIT("-link"))) {
			cl_args = slice_before(args, i);
			link_args = slice_after(args, i + 1);
		}
	}

	Array<String> msvc_args = make_array<String>(a);
	array_push(&msvc_args, STR_JOIN(a, msvc_directory, LIT("\\cl.exe")));
	array_push(&msvc_args, STR_JOIN(a, LIT("/I"), windows_sdk_include_base_path, LIT("\\ucrt")));
	array_push(&msvc_args, STR_JOIN(a, LIT("/I"), vs_include_path));

	for (uint i = 0; i < cl_args.len; i++) {
		array_push(&msvc_args, cl_args[i]);
	}

	array_push(&msvc_args, LIT("/link"));
	array_push(&msvc_args, STR_JOIN(a, LIT("/LIBPATH:"), windows_sdk_um_library_path));
	array_push(&msvc_args, STR_JOIN(a, LIT("/LIBPATH:"), windows_sdk_ucrt_library_path));
	array_push(&msvc_args, STR_JOIN(a, LIT("/LIBPATH:"), vs_library_path));

	for (uint i = 0; i < link_args.len; i++) {
		array_push(&msvc_args, link_args[i]);
	}

	//{
	//	for (uint i = 0; i < msvc_args.len; i++) {
	//		printf(" `%s` ", str_to_cstring(msvc_args[i], a));
	//	}
	//	os_write_to_console_colored(LIT("\n---------------------------------------------------------\n"), ConsoleAttribute_Blue | ConsoleAttribute_Green);
	//}

	if (!os_run_command(msvc_args.slice, {})) {
		return 1;
	}

	return 0;
}
