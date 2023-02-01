//
// The foundation is a minimal set of useful functions and utilities
// that come in handy everywhere, and which the C/C++ language doesn't
// provide you in a good way. Although I've tried to design this to be
// as universal as possible, it's still kind of an opinionated and personal
// codebase for myself, so it's not namespaced under any prefix; it's the foundation.
// 
// WARNING: THIS IS ALL CURRENTLY A WORK-IN-PROGRESS CODEBASE!! Some things aren't complete or fully tested,
// such as UTF8 support and some things might be implemented in a dumb way.

#ifdef FOUNDATION_INCLUDED
#error
#endif
#define FOUNDATION_INCLUDED

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS // should be defined before including initializer_list
#endif

#include <stdbool.h>
#include <stdint.h>

typedef uint8_t   u8;
typedef int8_t    s8;
typedef uint16_t  u16;
typedef int16_t   s16;
typedef uint32_t  u32;
typedef int32_t   s32;
typedef uint64_t  u64;
typedef int64_t   s64;
typedef uint64_t  uint;
typedef int64_t   sint;
typedef float     f32;
typedef double    f64;
typedef s32       rune;
typedef uint      uint_pow2; // must be a positive power-of-2. (zero is not allowed)

// This is used to mark optional pointers
#define OPT(x) x

#define LEN(x) (sizeof(x) / sizeof(x[0]))
#define OFFSET_OF(T, f) ((uint)&((T*)0)->f)

#define PAD(x) char _pad_##__COUNTER__[x]
#define ____CONCAT(x, y) x ## y
#define STRINGIFY(s) #s
#define CONCAT(x, y) ____CONCAT(x, y)

// https://www.wambold.com/Martin/writings/alignof.html
#ifdef __cplusplus
	template<typename T> struct alignment_trick { char c; T member; };
	#define ALIGN_OF(type) OFFSET_OF(alignment_trick<type>, member)
#else
#define ALIGN_OF(type) offsetof (struct CONCAT(_dummy, __COUNTER__) { char c; type member; }, member)
#endif

#ifdef __cplusplus
#define STRUCT_INIT(T) T
#define C_API extern "C"
#else
#define STRUCT_INIT(T) (T)
#define C_API
#endif

#ifndef Array
#define Array(T) ArrayRaw
#endif

#ifndef Slice
#define Slice(T) SliceRaw
#endif

#ifndef Map64
#define Map64(T) Map64Raw
#endif

#ifndef String
typedef struct {
	u8* data;
	uint len;
} String;
#define String String
#endif

// Useful for surpressing compiler warnings
#define UNUSED(name) ((void)(0 ? ((name) = (name)) : (name)))


//#define STACK_SLICE(T, ...) Slice<T>{ (T*)std::initializer_list<T>{__VA_ARGS__}.begin(), (s64)std::initializer_list<T>{__VA_ARGS__}.size() }
// TODO: MakeSlice with varargs that allocates a slice

#ifdef _DEBUG
#define ASSERT(x) { if (!(x)) __debugbreak(); }
#else
#define ASSERT(x) { if (x) {} }
#endif

inline static void _cast_check_fail() { ASSERT(false); }
#define CAST_CHK(T, x) ((T)(x) == (x) ? (T)(x) : (_cast_check_fail(), (T)0))

#define STATIC_ASSERT(x) enum { CONCAT(_static_assert_, __LINE__) = 1 / ((int)!!(x)) }

#define BP __debugbreak();

// Debugging helper that counts the number of hits and allows for breaking at a certain index
#define HITS(name, break_if_equals) \
	static uint CONCAT(name, _c) = 1; \
	CONCAT(name, _c)++; \
	uint name = CONCAT(name, _c); \
	if (name == (break_if_equals)) { BP; }

#ifndef LOG
#define LOG(s, ...) fprintf(stdout, s, __VA_ARGS__)
#endif

#define I8_MIN -128
#define I8_MAX 127
#define U8_MAX 255
#define I16_MIN -32768
#define I16_MAX 32767
#define U16_MAX 0xffff
#define I32_MIN 0x80000000
#define I32_MAX 0x7fffffff
#define U32_MAX 0xffffffffu
#define I64_MIN 0x8000000000000000ll
#define I64_MAX 0x7fffffffffffffffll
#define U64_MAX 0xffffffffffffffffllu

// https://stackoverflow.com/questions/6235847/how-to-generate-nan-infinity-and-infinity-in-ansi-c
inline f32 _get_f32_pos_infinity() { u32 x = 0x7F800000; return *(f32*)&x; }
inline f32 _get_f32_neg_infinity() { u32 x = 0xFF800000; return *(f32*)&x; }

#define F32_MAX _get_f32_pos_infinity()
#define F32_MIN _get_f32_neg_infinity()

#define KiB(x) ((uint)(x) << 10)
#define MiB(x) ((uint)(x) << 20)
#define GiB(x) ((uint)(x) << 30)
#define TiB(x) ((u64)(x) << 40)

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define CLAMP(x, minimum, maximum) ((x) < (minimum) ? (minimum) : (x) > (maximum) ? (maximum) : (x))

// e.g. 0b0010101000
// =>   0b0010100000
#define FLIP_RIGHTMOST_ONE_BIT(x) ((x) & ((x) - 1))

// e.g. 0b0010101111
// =>   0b0010111111
#define FLIP_RIGHTMOST_ZERO_BIT(x) ((x) | ((x) + 1))

// e.g. 0b0010101000
// =>   0b0010101111
#define FLIP_RIGHMOST_ZEROES(x) (((x) - 1) | (x))

// When x is a power of 2, it must only contain a single 1-bit
// x == 0 will output 1.
#define IS_POWER_OF_2(x) (FLIP_RIGHTMOST_ONE_BIT(x) == 0)

// `p` must be a power of 2.
// `x` is allowed to be negative as well.
#define ALIGN_UP_POW2(x, p) (((x) + (p) - 1) & ~((p) - 1)) // e.g. (x=30, p=16) -> 32
#define ALIGN_DOWN_POW2(x, p) ((x) & ~((p) - 1)) // e.g. (x=30, p=16) -> 16

#define HAS_ALIGNMENT_POW2(x, p) ((x) % (p) == 0) // p must be a power of 2

#define LERP(a, b, alpha) ((alpha)*(b) + (1.f - (alpha))*(a))

#define PEEK(x) (x)[(x).len - 1]

#define BITCAST(T, x) (*(T*)&x)

#define THREAD_LOCAL __declspec(thread)

// TODO: make it possible for an arena to grow.
// It should be an enum parameter for the make_arena function.
// In some cases, you might want the arena to have a max size instead of growing,
// because it could be a useful assumption to know that the addresses will be all in one contiguous block of memory.
// It should also be possible to allocate a growing child-arena out from an existing arena / slot arena

typedef struct _Allocator {
	void*(*proc)(struct _Allocator* allocator, OPT(void*) old_ptr, uint old_size, uint new_size, uint_pow2 new_alignment);
} Allocator;

typedef enum {
	ArenaMode_VirtualReserveFixed,
	//ArenaMode_VirtualGrowing, // do we really need this since we have UsingAllocatorGrowing?
	ArenaMode_UsingBufferFixed,
	ArenaMode_UsingAllocatorGrowing,
} ArenaMode;

struct _Arena;

typedef struct {
	ArenaMode mode;
	struct {
		OPT(u8*) reserve_base;
		uint reserve_size;
	} VirtualReserveFixed;
	struct {
		u8* base;
		uint size;
	} UsingBufferFixed;
	struct {
		u32 min_block_size;
		Allocator* alc;
	} UsingAllocatorGrowing;
} ArenaDesc;

typedef struct _ArenaBlockHeader {
	uint size_including_header;
	struct _ArenaBlockHeader* next;
	// the block memory comes right after the header
} ArenaBlockHeader;

typedef struct {
	u8* head;
	ArenaBlockHeader* current_block; // used only with ArenaMode_UsingAllocatorGrowing
} ArenaPosition;

typedef struct _Arena {
	Allocator alc; // Must be the first field for outwards-casting!
	ArenaDesc desc;
	
	struct { // should be union
		u8* internal_base;
		ArenaBlockHeader* first_block; // used only with ArenaMode_UsingAllocatorGrowing
	};

	u8* committed_end; // only used with ArenaMode_VirtualReserveFixed
	
	ArenaPosition pos;
} Arena;

struct SlotArenaRaw;

typedef struct {
	Allocator alc;

	Arena* backing_arena;
	//SlotArenaRaw* sub_arenas[12]; // slot arenas. the last arena allocates 4k blocks
	
	// we should store a doubly linked list of continuous block-ranges at the last arena.
	// 00 111   22 3 4 5   666
	// 00100011100101010111000
} Heap;

#define make_slice_one_elem(elem, allocator) {MemClone((elem), (allocator)), 1}

C_API void* mem_clone_size(uint size, const void* value, Allocator* allocator);
#define mem_clone(value, allocator) mem_clone_size(sizeof(value), &value, allocator)

#define mem_alloc(size, alignment, allocator) (void*)(allocator)->proc((allocator), NULL, 0, (size), (alignment))
#define mem_resize(ptr, old_size, new_size, new_alignment, allocator) (void*)(allocator)->proc((allocator), (ptr), (old_size), (new_size), (new_alignment))
#define mem_free(ptr, size, allocator) (allocator)->proc((allocator), (ptr), (size), 0, 1)

#define mem_alloc_count(T, count, allocator) (T*)(allocator)->proc((allocator), NULL, 0, (count) * sizeof(T), ALIGN_OF(T))
#define mem_resize_count(T, ptr, old_count, new_count, allocator) (T*)(allocator)->proc((allocator), ptr, (old_count) * sizeof(T), (new_count) * sizeof(T), ALIGN_OF(T))
#define mem_free_count(T, ptr, count, allocator) (allocator)->proc((allocator), (ptr), (count) * sizeof(T), 0, ALIGN_OF(T))

#define mem_zero(ptr) memset(ptr, 0, sizeof(*ptr))
#define mem_zero_slice(slice) memset((slice).data, 0, (slice).size_bytes())

// Slice, Array and String have the same binary layout, so they can be bitcasted between each other

typedef struct {
	void* data;
	uint len;
	uint capacity;
	Allocator* alc;
} ArrayRaw;

typedef struct {
	void* data;
	uint len;
} SliceRaw;

// In these hash-macros, the argument is multiplied by the corresponding golden ratio
// for that integer size, so that multiplying once makes the integer wrap the value range
// 0.61803398874989486662 times.
#define HASH_U64(x) ((x)*0x9E3779B97F4A7D69)
#define HASH_U8(x)  HASH_U64((u64)(x))
#define HASH_U16(x) HASH_U64((u64)(x))
#define HASH_U32(x) HASH_U64((u64)(x))
#define HASH_PTR(x) HASH_U64((u64)(x))

//#define HASH_STRING(x) MeowU64From(MeowHash(MeowDefaultSeed, x.len, x.data), 0)

// @speed: for release builds we could use #define HASH HASH_U64(__COUNTER__) instead, if that'd compile to better machine code.
// But for debug, we want everything to be 100% consistent between builds.
// Actually, for now, lets just use the counter.
#define HASH_LOC HASH_U64(__COUNTER__+1)
//#define HASH_LOC (((u64)__FILE__) ^ HASH_U64((u64)__LINE__))

#ifdef _DEBUG
C_API void _DEBUG_FILL_GARBAGE(void* ptr, uint len);
#else
#define _DEBUG_FILL_GARBAGE(ptr, len)
#endif

typedef s64 Tick;

#define NANOSECOND 1
#define MICROSECOND 1000 // 1000 * NANOSECOND
#define MILLISECOND 1000000 // 1000 * MICROSECOND
#define SECOND 1000000000 // 1000 * MILLISECOND
#define MINUTE 60000000000 // 60 * SECOND
#define HOUR 3600000000000 // 60 * MINUTE

typedef struct { void* handle; } OS_DynamicLibrary;

typedef enum {
	OS_FileOpenMode_Read,
	OS_FileOpenMode_Write,
	OS_FileOpenMode_Append,
} OS_FileOpenMode;

typedef struct { void* _handle; } File;


#define LIT(x) STRUCT_INIT(String){ (u8*)x, sizeof(x)-1 }
#define AS_BYTES(x) STRUCT_INIT(String){ (u8*)&x, sizeof(x) }
#define SLICE_AS_BYTES(x) STRUCT_INIT(String){ (u8*)(x).data, (x).len * sizeof((x).data[0]) }

// #define STRING_FROM_CSTR(x) String{ (u8*)x, strlen(x) } // this shouldn't be a macro

// ALLOCA_C_STRING probably shouldn't be used in actual programs. Use to_cstring() instead.
//inline const char* _temp_cstr(String string, void* out) { ZoneScoped; memcpy(out, string.data, string.len); ((char*)out)[string.len] = '\0'; return (const char*)out; }
//#define ALLOCA_C_STRING(str) _temp_cstr(str, alloca(str.len + 1))

#define __ACTIVATE_MANUAL_STRUCT_PADDING \
	_Pragma("warning(3:4820)") \
	_Pragma("warning(3:4121)")
#define __RESTORE_MANUAL_STRUCT_PADDING \
	_Pragma("warning(4:4820)") \
	_Pragma("warning(4:4121)")

typedef struct {
	union {
		u32 id; // 1 is the first valid index, 0 is invalid
		bool initialized;
	};
	u32 gen;
} SlotArrayHandle;

struct SlotArrayElemHeader {
	u32 gen;
	u32 next_free_item; // 0 means this slot is currently occupied
	// the value comes after the header
};


typedef struct {
	Allocator* alc;
	u32 value_size;

	u32 alive_count;
	
	u32 slot_count;
	u32 slot_count_log2; // if there are zero slots, this will be zero and the `slots` pointer will be null.
	OPT(void*) slots;
} Map64Raw;

typedef struct {
	String file;
	u32 line;
} LeakTrackerCallstackEntry;

typedef struct {
	Array(LeakTrackerCallstackEntry) callstack;

	/*u64 allocation_idx;

	i64 size;
	i64 alignment;
	// this could be a BucketArray
	Array<CallstackEntry> callstack;
	*/
} LeakTracker_Entry;

typedef struct {
	// Note: LeakTracker never frees any of its internals until deinit_leak_tracker() is called
	bool active;
	Arena* internal_arena;

	Map64(LeakTracker_Entry) active_allocations; // key is the address of the allocation

	/*Allocator allocator; // Must be the first field for outwards-casting!

	Allocator* passthrough_allocator;
	Arena internal_arena;

	u64 next_allocation_idx;

	Array<LeakTracker_BadFree> bad_free_array;
	*/
	// This is here so that we won't have to create a dozen of duplicate strings
	Map64(String) file_names_cache; // key is the string hashed
} LeakTracker;

//extern Allocator _global_allocator; // do we need this?
C_API THREAD_LOCAL void* _foundation_pass;
C_API THREAD_LOCAL Arena* _temp_arena;
C_API THREAD_LOCAL uint _temp_arena_scope_counter;
C_API THREAD_LOCAL LeakTracker _leak_tracker;

// Relies on PDB symbol info
C_API void get_stack_trace(void(*visitor)(String function, String file, u32 line, void* user_ptr), void* user_ptr);

//
// Leak tracker can be used to track anything that can be keyed using a 64-bit identifier.
// For example, this can be an address for a memory allocation, or an OS handle.
// All of the OS and memory allocation related functions inside the foundation call to
// leak_tracker_begin_entry and leak_tracker_end_entry whenever they're dealing with state
// that needs to be manually released. As a consequence, if you for example call OS_FileOpen()
// but forget to call OS_FileClose() at the end, the leak tracker will report that as a leak
// and will give you the callstack of where the leak was created.
// 
// If you want to detect the leaks, just call init_leak_tracker() in your application entry point
// and deinit_leak_tracker() when your application has finished running. deinit_leak_tracker()
// will assert on any leaks and print information about them.
//
C_API void leak_tracker_init();
C_API void leak_tracker_deinit();

// These functions do not do anything if leak tracker is not present
C_API void leak_tracker_begin_entry(void* address, uint skip_stackframes_count);
C_API void leak_tracker_assert_is_alive(void* address);
C_API void leak_tracker_end_entry(void* address);
/*
typedef struct {
	u32 elem_size;
	
	u32 num_elems_per_bucket;
	Allocator* alc;
	//bool is_using_arena;
	//union {
	//	struct {
	//	} using_alc;
	//	Arena* using_arena;
	//};

	uint num_active;
	uint num_freed;

	void* first_free;
} SlotArenaRaw;
*/

//#define SLOT_ARENA_EACH(ar, T, ptr) (T* ptr = (T*)((ar).arena.mem + 8); \
//	SlotArenaIteratorCondition((RawSlotArena*)&(ar), (void**)&ptr); ptr = (T*)((u8*)ptr + 8 + (ar).elem_size))
//
//inline bool SlotArenaIteratorCondition(const RawSlotArena* arena, void** ptr) {
//	// `ptr` is a pointer on the element itself.
//	for (; *ptr < arena->arena->internal_base + arena->arena->internal_pos;) {
//		if (*((void**)*ptr - 1) == 0) return true;
//
//		// Element is destroyed; it has a freelist pointer other than null.
//		// Skip to the next element.
//		*ptr = (u8*)*ptr + arena->elem_size + 8;
//	}
//	return false;
//}

// These can be combined as a mask
typedef enum {
	ConsoleAttribute_Blue = 0x0001,
	ConsoleAttribute_Green = 0x0002,
	ConsoleAttribute_Red = 0x0004,
	ConsoleAttribute_Intensify = 0x0008,
} ConsoleAttribute;
typedef int ConsoleAttributeFlags;

typedef enum {
	MapInsert_AssertUnique,
	MapInsert_DoNotOverride,
	MapInsert_Override,
} MapInsert;

typedef struct { void* value; bool added; } MapInsertResult;

typedef struct OS_VisitDirectoryInfo {
	String name;
	bool is_directory;
} OS_VisitDirectoryInfo;

typedef enum OS_VisitDirectoryResult {
	// TODO: OS_VisitDirectoryResult_Recurse,
	OS_VisitDirectoryResult_Continue,
} OS_VisitDirectoryResult;

typedef OS_VisitDirectoryResult(*OS_VisitDirectoryVisitor)(const OS_VisitDirectoryInfo* info, void* userptr);

//
// `temp_push`/`temp_pop` sets a convention for easily getting temporary memory
// in a thread-safe way for a duration of some scope. Internally, foundation.cpp
// declares a thread-local Arena.
// 
// If you have nested push/pop pairs, PopTemp doesn't actually pop the stack until
// the final pop is called (when the scope counter reaches zero).
// This is done to make sure you can safely pass the temp allocator to procedures that
// return allocated memory back to the outer scope. e.g:
// 
// foo:
//    Allocator* temp = temp_push();
//    MyArray<int> a = make_array(temp);
//    bar(&a);
//    temp_pop();
// 
// bar:
//    Allocator* temp = temp_push();
//    Baz* baz = some_procedure_that_allocates_memory(temp);
//    array_push(&a, baz->some_field)  // Potentially grow the array and require an allocation from the temp arena
//    temp_pop();                  // Whoops, we might have just corrupted the entire array!
// 
// ... If the temp arena was popped at the end of `bar` to where it was when entering the procedure,
// there would be a subtle bug. `array_push` would first use the temporary allocator to insert some value, requiring
// a potential allocation by the array. Then `temp_pop` would be called, and the memory in use by the array
// would marked available for subsequent temporary allocations, corrupting the entire array.
// 

C_API Allocator* temp_push();
C_API void temp_pop();

// temp_init and temp_deinit are not necessary and they only exist for performance.
// They keep the temp arena alive even after a final (scope counter reaching zero) call to temp_pop.
// You probably want to call temp_init/deinit in main, if your program has multiple independent temp_push/pop scopes.
// If you don't call temp_init, then each final call to temp_pop will release the arena back to the OS,
// and temp_push will in turn need to allocate a new arena from the OS.
C_API void temp_init();
C_API void temp_deinit();

C_API Arena* arena_make(u32 min_block_size, Allocator* alc);
C_API Arena* arena_make_virtual_reserve_fixed(uint reserve_size, OPT(void*) reserve_base);
C_API Arena* arena_make_using_buffer_fixed(void* base, uint size);
C_API Arena* arena_make_ex(ArenaDesc desc);
C_API void arena_free(Arena* arena);

C_API Heap* make_heap(ArenaDesc backing_arena_desc);

C_API String arena_push_size(Arena* arena, uint size, uint_pow2 alignment);
C_API u8* arena_push(Arena* arena, String data, uint_pow2 alignment);

C_API u8* arena_get_base_contiguous(Arena* arena);
C_API uint arena_get_cursor(Arena* arena);

C_API ArenaPosition arena_get_pos(Arena* arena);
C_API void arena_pop_to(Arena* arena, ArenaPosition pos);
// TODO: C_API void arena_shrink_memory(uint base_size) // the memory will not be reduced past base_size
C_API void arena_clear(Arena* arena);

C_API uint_pow2 next_pow_of_2(uint v);

#define HASHMAP64_EMPTY_KEY (0xFFFFFFFFFFFFFFFF)
#define HASHMAP64_DEAD_BUT_REQUIRED_FOR_CHAIN_KEY (0xFFFFFFFFFFFFFFFE)
#define HASHMAP64_LAST_VALID_KEY (0xFFFFFFFFFFFFFFFD)

inline bool map64_iterate(Map64Raw* map, uint* i, uint* key, void** value_ptr) {
	if (!map->slots) return false;
	u32 slot_size = map->value_size + 8;

	for (;;) {
		if (*i >= map->slot_count) return false;

		u64* key_ptr = (u64*)((u8*)map->slots + (*i) * slot_size);
		(*i)++;

		if (*key_ptr <= HASHMAP64_LAST_VALID_KEY) {
			*key = *key_ptr;
			*value_ptr = key_ptr + 1;
			return true;
		}
	}
}

#define MAP64_EACH_RAW(map, key, value_ptr) (uint _i=0, key=0; map64_iterate(map, &_i, &key, value_ptr); )

// WARNING: The largest 2 key values (see HASHMAP64_LAST_VALID_KEY)
// are reserved internally for marking empty/destroyed slots by the hashmap, so you cannot use them as valid keys.
// An assertion will fail if you try to insert a value with a reserved key.
// This is done for performance, but maybe we should have an option to default to a safe implementation.
C_API Map64Raw map64_make_raw(u32 value_size, Allocator* alc);
C_API Map64Raw make_map64_cap_raw(u32 value_size, uint_pow2 capacity, Allocator* alc);
C_API void free_map64_raw(Map64Raw* map);
C_API void resize_map64_raw(Map64Raw* map, u32 slot_count_log2);
C_API MapInsertResult map64_insert_raw(Map64Raw* map, u64 key, OPT(const void*) value, MapInsert mode);
C_API bool map64_remove_raw(Map64Raw* map, u64 key);
C_API OPT(void*) map64_get_raw(Map64Raw* map, u64 key);

C_API void mem_copy(void* dst, const void* src, uint size);

C_API ArrayRaw make_array_raw(Allocator* alc);
C_API ArrayRaw make_array_len_raw(u32 elem_size, uint len, const void* initial_value, Allocator* alc);
C_API ArrayRaw make_array_len_garbage_raw(u32 elem_size, uint len, Allocator* alc);
C_API ArrayRaw make_array_cap_raw(u32 elem_size, uint capacity, Allocator* alc);
C_API void free_array_raw(ArrayRaw* array, u32 elem_size);
C_API uint array_push_raw(ArrayRaw* array, const void* elem, u32 elem_size);
C_API void array_push_slice_raw(ArrayRaw* array, SliceRaw elems, u32 elem_size);
C_API void array_pop_raw(ArrayRaw* array, OPT(void*) out_elem, u32 elem_size);
C_API void array_reserve_raw(ArrayRaw* array, uint capacity, u32 elem_size);
C_API void array_resize_raw(ArrayRaw* array, uint len, OPT(const void*) value, u32 elem_size); // set value to NULL to not initialize the memory

//SlotArenaRaw* make_slot_arena_contiguous_raw(u32 elem_size, ArenaDesc arena_desc);
//SlotArenaRaw* make_slot_arena_raw(ArenaDesc arena_desc);
//void* slot_arena_add_garbage_raw(SlotArenaRaw* arena);
//void slot_arena_clear_raw(SlotArenaRaw* arena);
//bool slot_arena_remove_raw(SlotArenaRaw* arena, void* ptr);
//void delete_slot_arena_raw(SlotArenaRaw* arena);
//uint slot_arena_get_index_raw(const SlotArenaRaw* arena, void* ptr);

C_API u64 os_read_cycle_counter();
C_API void os_sleep_milliseconds(s64 ms);
C_API void os_write_to_console(String str);
C_API void os_write_to_console_colored(String str, ConsoleAttributeFlags attributes_mask);

// If `working_dir` is an empty string, the current working directory will be used.
// `args[0]` should be the path of the executable, where `\' and `/` are both accepted path separators.
// TODO: options to capture stdout and stderr
C_API bool os_run_command(Slice(String) args, String working_dir);

C_API bool os_set_working_dir(String dir);
C_API String os_get_working_dir(Allocator* allocator);

// these strings do not currently convert slashes - they will be windows specific `\`
//Slice<String> os_file_picker_multi(); // allocated with temp_allocator

C_API void os_error_message(String title, String message);

C_API OPT(u8*) os_mem_reserve(u64 size, OPT(void*) address);
C_API void os_mem_commit(u8* ptr, u64 size);
C_API void os_mem_decommit(u8* ptr, u64 size);
C_API void os_mem_release(u8* ptr);

C_API s64 round_to_s64(float x);
C_API s64 floor_to_s64(float x);

// -- String module -----------------------------------------------------------

#define STR_IS_UTF8_FIRST_BYTE(c) (((c) & 0xC0) != 0x80) /* is c the start of a utf8 sequence? */
#define STR_EACH(str, r, i) (uint i=0, r = 0, i##_next=0; r=str_next_rune(str, &i##_next); i=i##_next)
#define STR_EACH_REVERSE(str, r, i) (uint i=str.len; rune r=str_prev_rune(str, &i);)

C_API String str_format(Allocator* alc, const char* fmt, ...);
C_API void str_print(Array(u8)* buffer, String str);
C_API void str_print_repeat(Array(u8)* buffer, String str, uint count);
C_API void str_printf(Array(u8)* buffer, const char* fmt, ...);

C_API String str_advance(String* str, uint len);
C_API String str_clone(String str, Allocator* allocator);

C_API void str_copy(String dst, String src);

C_API String str_path_stem(String path); // Returns the name of a file without extension, e.g. "matty/boo/billy.txt" => "billy"
C_API String str_path_extension(String path); // returns the file extension, e.g. "matty/boo/billy.txt" => "txt"
C_API String str_path_tail(String path); // Returns the last part of a path, e.g. "matty/boo/billy.txt" => "billy.txt"
C_API String str_path_dir(String path); // returns the directory, e.g. "matty/boo/billy.txt" => "matty/boo"

C_API bool str_last_index_of_any_char(String str, String chars, uint* out_index);
C_API bool str_contains(String str, String substr);
C_API bool str_find_substring(String str, String substr, uint* out_index);

C_API String str_replace(Allocator* allocator, String str, String substr, String replace_with);
C_API String str_to_lower(String str, Allocator* a);
C_API rune str_rune_to_lower(rune r);

//Slice<String> str_split_by_char(String str, u8 character, Allocator* allocator);
C_API String str_join(Allocator* alc, Slice(String) args);

C_API bool str_equals(String a, String b);
C_API bool str_equals_nocase(String a, String b); // case-insensitive version of str_equals

C_API String str_slice(String str, uint lo, uint hi);
C_API String str_slice_before(String str, uint mid);
C_API String str_slice_after(String str, uint mid);

C_API bool str_to_u64(String s, u32 radix, u64* out_value);
C_API bool str_to_s64(String s, u32 radix, s64* out_value);
C_API bool str_to_f64(String s, f64* out);

C_API String str_from_uint(Allocator* allocator, String bytes);
C_API String str_from_int(Allocator* allocator, String bytes);
C_API String str_from_float(Allocator* allocator, String bytes);
C_API String str_from_float_ex(Allocator* allocator, String bytes, int num_decimals);

C_API char* str_to_cstring(String s, Allocator* alc);
C_API String str_from_cstring(const char* s);

C_API u64 str_hash(String s);

// Do we need this function...? It's very windows-specific.
C_API wchar_t* str_to_utf16(String str, Allocator* allocator, uint num_null_terminations, uint* out_len);
C_API String str_from_utf16(wchar_t* str_utf16, Allocator* allocator);

C_API uint str_encode_rune(u8* output, rune r); // returns the number of bytes written
// TODO: utf8_decode_rune

// Returns the character on `byteoffset`, then increments it.
// Will returns 0 if byteoffset >= str.len
C_API rune str_next_rune(String str, uint* byteoffset);

// Decrements `bytecounter`, then returns the character on it.
// Will returns 0 if byteoffset < 0
C_API rune str_prev_rune(String str, uint* byteoffset);

C_API uint str_rune_count(String str);

// -- Clipboard module --------------------------------------------------------

C_API String os_clipboard_get_text(Allocator* allocator);
C_API void os_clipboard_set_text(String str);

// -- DynLib module -----------------------------------------------------------

C_API OS_DynamicLibrary os_dynamic_library_load(String filepath);
C_API bool os_dynamic_library_unload(OS_DynamicLibrary dll);
C_API void* os_dynamic_library_sym_address(OS_DynamicLibrary dll, String symbol);

// -- File module -------------------------------------------------------------

// The path separator in the returned string will depend on the OS. On windows, it will be a backslash.
// If the provided path is invalid, an empty string will be returned.
// TODO: maybe it'd be better if these weren't os-specific functions, and instead could take an argument for specifying
//       windows-style paths / unix-style paths
C_API bool os_path_is_absolute(String path);

// If `working_dir` is an empty string, the current working directory will be used.
C_API String os_path_to_absolute(String working_dir, String path, Allocator* alc);

C_API bool os_visit_directory(String path, OS_VisitDirectoryVisitor visitor, void* visitor_userptr);

C_API bool os_directory_exists(String path);
C_API bool os_delete_directory(String path); // If the directory doesn't already exist, it's treated as a success.
C_API bool os_make_directory(String path); // If the directory already exists, it's treated as a success.

C_API bool os_file_read_whole(String filepath, Allocator* allocator, String* out_str);
C_API bool os_file_write_whole(String filepath, String data);

C_API File os_file_open(String filepath, OS_FileOpenMode mode);
C_API bool os_file_is_valid(File file);
C_API bool os_file_close(File file);

C_API uint os_file_size(File file);

C_API uint os_file_read(File file, void* dst, uint size);
C_API bool os_file_write(File file, String data);
C_API uint os_file_get_position(File file);
C_API bool os_file_set_position(File file, uint position);

C_API u64 os_file_get_modtime(String filepath); // it'd be nice if this used apollo time
C_API bool os_file_clone(String src_filepath, String dst_filepath);
C_API bool os_file_delete(String filepath);

C_API String os_file_picker_dialog(Allocator* allocator);

// -- Time module -------------------------------------------------------------

C_API Tick time_get_tick();

// -- Rand module -------------------------------------------------------------

C_API u32 rand_u32();
C_API u64 rand_u64();
C_API float rand_float_in_range(float minimum, float maximum);

#undef String
#undef Array
#undef Slice
#undef Map64
#undef C_API