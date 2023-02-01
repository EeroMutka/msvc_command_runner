#include "foundation/foundation.hpp"
#include "microsoft_craziness.h"

int main(int argc, const char** argv) {
	Allocator* a = temp_push();
	
	WinSDK_Find_Result windows_sdk = WinSDK_find_visual_studio_and_windows_sdk();
	String msvc_directory = str_from_utf16(windows_sdk.vs_exe_path, a); // contains cl.exe, link.exe
	String windows_sdk_include_base_path = str_from_utf16(windows_sdk.windows_sdk_include_base, a); // contains <string.h>, etc
	String windows_sdk_um_library_path = str_from_utf16(windows_sdk.windows_sdk_um_library_path, a); // contains kernel32.lib, etc
	String windows_sdk_ucrt_library_path = str_from_utf16(windows_sdk.windows_sdk_ucrt_library_path, a); // contains libucrt.lib, etc
	String vs_library_path = str_from_utf16(windows_sdk.vs_library_path, a); // contains MSVCRT.lib etc
	String vs_include_path = str_from_utf16(windows_sdk.vs_include_path, a); // contains vcruntime.h
	
	printf("msvc_directory: %s\n", str_to_cstring(msvc_directory, a));
	printf("windows_sdk_include_base_path: %s\n", str_to_cstring(windows_sdk_include_base_path, a));
	printf("windows_sdk_um_library_path: %s\n", str_to_cstring(windows_sdk_um_library_path, a));
	printf("windows_sdk_ucrt_library_path: %s\n", str_to_cstring(windows_sdk_ucrt_library_path, a));
	printf("vs_library_path: %s\n", str_to_cstring(vs_library_path, a));
	printf("vs_include_path: %s\n", str_to_cstring(vs_include_path, a));
	printf("\n");
	
	return 0;
}
