#include "foundation/foundation.hpp"
#include "microsoft_craziness.h"

int main(int argc, const char** argv) {
	Allocator* a = temp_push();
	
	os_write_to_console_colored(LIT("-------- Running devenv.exe from msvc_command_runner --------\n"), ConsoleAttribute_Blue | ConsoleAttribute_Green);
	
	WinSDK_Find_Result windows_sdk = WinSDK_find_visual_studio_and_windows_sdk();
	String vs_base_path = str_from_utf16(windows_sdk.vs_base_path, a);
	
	Array<String> args = make_array<String>(a);
	array_push(&args, STR_JOIN(a, vs_base_path, LIT("\\Common7\\IDE\\devenv.exe")));
	
	for (uint i = 1; i < argc; i++) {
		array_push(&args, str_from_cstring(argv[i]));
	}
	
	if (!os_run_command(args.slice, {})) {
		return 1;
	}
	
	return 0;
}
