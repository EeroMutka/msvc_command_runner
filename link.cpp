#include "foundation/foundation.hpp"
#include "microsoft_craziness.h"

int main(int argc, const char** argv) {
	Allocator* a = temp_push();

	os_write_to_console_colored(LIT("-------- Running link.exe from msvc_command_runner --------\n"), ConsoleAttribute_Blue | ConsoleAttribute_Green);

	WinSDK_Find_Result windows_sdk = WinSDK_find_visual_studio_and_windows_sdk();
	String msvc_directory = str_from_utf16(windows_sdk.vs_exe_path, a); // contains cl.exe, link.exe
	String windows_sdk_um_library_path = str_from_utf16(windows_sdk.windows_sdk_um_library_path, a); // contains kernel32.lib, etc
	String windows_sdk_ucrt_library_path = str_from_utf16(windows_sdk.windows_sdk_ucrt_library_path, a); // contains libucrt.lib, etc
	String vs_library_path = str_from_utf16(windows_sdk.vs_library_path, a); // contains MSVCRT.lib etc

	// https://learn.microsoft.com/en-us/cpp/build/reference/linking?view=msvc-170
	
	Array<String> args = make_array<String>(a);
	array_push(&args, STR_JOIN(a, msvc_directory, LIT("\\link.exe")));
	array_push(&args, STR_JOIN(a, LIT("/LIBPATH:"), windows_sdk_um_library_path));
	array_push(&args, STR_JOIN(a, LIT("/LIBPATH:"), windows_sdk_ucrt_library_path));
	array_push(&args, STR_JOIN(a, LIT("/LIBPATH:"), vs_library_path));
	
	for (uint i = 1; i < argc; i++) {
		array_push(&args, str_from_cstring(argv[i]));
	}
	
	u32 exit_code;
	if (!os_run_command(args.slice, {}, &exit_code)) {
		printf("Failed to run the command!\n");
		return 1;
	}

	return exit_code;
}
