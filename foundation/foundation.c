// required libraries
#pragma comment(lib, "gdi32.lib") // required by glfw3
#pragma comment(lib, "shell32.lib") // required by glfw3
#pragma comment(lib, "user32.lib") // required by glfw3

#pragma comment(lib, "Dbghelp.lib") // os_get_stack_trace

#include "foundation.h"

#define Array(T) ArrayRaw
#define Slice(T) SliceRaw
#define Map64(T) Map64Raw

#include <stdio.h> // would be nice to get rid of this
#include <stdarg.h> // for va_list
#include <string.h> // for memcmp
#include <stdlib.h> // for strtod

//#include "vendor/meow_hash/meow_hash_x64_aesni.h"
//#include "vendor/meow_hash/meow_hash_x64_aesni.h"

// -- All global state held by the foundation -------------------------------
//Arena temp_allocator_arena;
u64 global_rand = 0;

static u8 SLOT_ARENA_FREELIST_END = 0;

THREAD_LOCAL void* _foundation_pass; // TODO: get rid of this!

THREAD_LOCAL Arena* _temp_arena;
THREAD_LOCAL uint _temp_arena_scope_counter = 0;
THREAD_LOCAL bool _temp_arena_keep_alive = false;
THREAD_LOCAL LeakTracker _leak_tracker;

// --------------------------------------------------------------------------


#define ARRAY_IDX(T, arr, idx) ((T*)arr.data)[i]

#define SEPARATOR_CHARS LIT("/\\")

typedef struct {
	u8 bytes[32];
} ASCII_Set;

// @noutf
ASCII_Set ascii_set_make(String chars) {
	//ZoneScoped;
	ASCII_Set set = {0};
	for (uint i = 0; i < chars.len; i++) {
		u8 c = chars.data[i];
		set.bytes[c / 8] |= 1 << (c % 8);
	}
	return set;
}

bool ascii_set_contains(ASCII_Set set, u8 c) {
	//ZoneScoped;
	return (set.bytes[c / 8] & 1 << (c % 8)) != 0;
}


// @noutf
bool str_last_index_of_any_char(String str, String chars, uint* out_index) {
	//ZoneScoped;
	ASCII_Set char_set = ascii_set_make(chars);

	for (uint i = str.len - 1; i < str.len; i--) {
		if (ascii_set_contains(char_set, str.data[i])) {
			*out_index = i;
			return true;
		}
	}
	return false;
}

bool str_equals(String a, String b) {
	return a.len == b.len && memcmp(a.data, b.data, a.len) == 0;
}

bool str_equals_nocase(String a, String b) {
	if (a.len != b.len) return false;
	for (uint i = 0; i < a.len; i++) {
		if (str_rune_to_lower(a.data[i]) != str_rune_to_lower(b.data[i])) {
			return false;
		}
	}
	return true;
}

String str_slice(String str, uint lo, uint hi) {
	ASSERT(hi >= lo && hi <= str.len);
	return (String){str.data + lo, hi - lo};
}

String str_slice_after(String str, uint mid) {
	ASSERT(mid <= str.len);
	return (String){str.data + mid, str.len - mid};
}

String str_slice_before(String str, uint mid) {
	ASSERT(mid <= str.len);
	return (String){str.data, mid};
}

bool str_find_substring(String str, String substr, uint* out_index) {
	// ZoneScoped;
	for (uint i = 0; i < str.len; i++) {
		if (str_equals(str_slice(str, i, i + substr.len), substr)) {
			*out_index = i;
			return true;
		}
	}
	return false;
};

bool str_contains(String str, String substr) {
	uint _idx;
	return str_find_substring(str, substr, &_idx);
}

String str_path_extension(String path) {
	//ZoneScoped;
	s64 idx;
	if (str_last_index_of_any_char(path, LIT("."), &idx)) {
		return str_slice_after(path, idx + 1);
	}
	return (String){0};
}

String str_path_dir(String path) {
	uint last_sep;
	if (str_last_index_of_any_char(path, SEPARATOR_CHARS, &last_sep)) {
		return str_slice_before(path, last_sep);
	}
	return path;
}

String str_format_va_list(Allocator* allocator, const char* fmt, va_list args) {
	va_list _args = args;

	uint needed_bytes = vsnprintf(0, 0, fmt, args) + 1;
	String result = (String){ mem_alloc_count(u8, needed_bytes, allocator), needed_bytes };
	result.len -= 1;
	result.data[result.len] = 0;

	vsnprintf((char*)result.data, needed_bytes, fmt, _args);
	return result;
}

void str_print(Array(u8)* buffer, String str) {
	array_push_slice_raw(buffer, BITCAST(SliceRaw, str), 1);
}

void str_print_repeat(Array(u8)* buffer, String str, uint count) {
	for (uint i = 0; i < count; i++) str_print(buffer, str);
}

String str_format(Allocator* alc, const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	String result = str_format_va_list(alc, fmt, args);
	va_end(args);
	return result;
}

void str_printf(Array(u8)* buffer, const char* fmt, ...) {
	Allocator* temp = temp_push();
	
	va_list args;
	va_start(args, fmt);
	String str = str_format_va_list(temp, fmt, args);
	va_end(args);
	
	str_print(buffer, str);
	temp_pop();
}

void* mem_clone_size(uint size, const void* value, Allocator* allocator) {
	void* result = mem_alloc_count(u8, size, allocator);
	memcpy(result, value, size);
	return result;
}

void mem_copy(void* dst, const void* src, uint size) { memcpy(dst, src, size); }

String str_path_tail(String path) {
	uint last_sep;
	if (str_last_index_of_any_char(path, SEPARATOR_CHARS, &last_sep)) {
		path = str_slice_after(path, last_sep + 1);
	}
	return path;
}

String str_path_stem(String path) {
	//ZoneScoped;
	//if (path.len == 0 || str_contains(SEPARATOR_CHARS, STR_FROM_CHAR(path[path.len - 1]))) {
	//	return {};
	//}

	uint last_sep;
	if (str_last_index_of_any_char(path, SEPARATOR_CHARS, &last_sep)) {
		path = str_slice_after(path, last_sep + 1);
	}

	uint last_dot;
	if (str_last_index_of_any_char(path, LIT("."), &last_dot)) {
		path = str_slice_before(path, last_dot);
	}

	return path;
}

//const char* temp_cstr(String str) {
//	Slice<u8> mem = mem_alloc(str.len + 1, TEMP_ALLOCATOR);
//	memcpy(mem.data, str.data, str.len);
//	mem.data[str.len] = '\0';
//
//	return (char*)mem.data;
//}


//void slot_array_test() {
//	Slot_Array<int> arr = {};
//
//	Slot_Array_Handle<int> first_handle = slot_array_add(arr, 123);
//	Slot_Array_Handle<int> bah = slot_array_add(arr, 521);
//
//	assert(*arr[first_handle] == 123);
//	assert(*arr[bah] == 521);
//
//	*arr[first_handle] = 1293;
//	assert(*arr[first_handle] == 1293);
//
//}

//u32 hash_fnv32(Slice<u8> data, u32 seed) {
//	for (i64 i = 0; i < data.len; i++) {
//		seed = (seed * 0x01000193) ^ data[i];
//	}
//	return seed;
//}


// idea for a bucketed slice
// 
// template<typename T>
// struct BucketedIndex {
//   void* bucket_base;
//   u32 elem_offset;
// 
//   // Bucket array metadata
//   u8 bucket_start_offset; // at offset 0, we store a pointer to the previous bucket. Maybe this field is not needed.
//   u24 bucket_end_offset; // at this offset, we store a pointer to the next bucket.
// }
// 
//template<typename T>
//struct BucketedSlice {
//  BucketedIndex lo;
//  BucketedIndex hi;
//};

//void ArrayReserveRaw(RawArray* arr, uint elem_size, uint capacity) {
//	ZoneScoped;

//}

#define make_str(len, allocator) (String){ (u8*)(allocator)->proc((allocator), NULL, 0, (len), 1), len }

String str_clone(String str, Allocator* allocator) {
	//ZoneScoped;
	String copied = make_str(str.len, allocator);
	str_copy(copied, str);
	return copied;
}

//String str_clone(const char* cstr, Allocator* allocator) {
//	ZoneScoped;
//	return str_clone(String{ (u8*)cstr, (isize)strlen(cstr) }, allocator);
//}

//String _str_join(Slice<String> strings, Allocator* allocator) {
//	ZoneScoped;
//	i64 total_size = 0;
//	for (i64 i = 0; i < strings.len; i++)
//		total_size += strings.data[i].len;
//
//	String result = MakeSlice(u8, total_size, allocator);
//
//	u8* ptr = result.data;
//	for (i64 i = 0; i < strings.len; i++) {
//		memcpy(ptr, (const void*)strings.data[i].data, strings.data[i].len);
//		ptr += strings.data[i].len;
//	}
//
//	return result;
//}

//Slice<String> str_split_by_char(String str, u8 character, Allocator* allocator) {
//	ZoneScoped;
//	uint required_len = 1;
//	for (uint i = 0; i < str.len; i++) if (str.data[i] == character) required_len++;
//
//	Array<String> substrings;
//	InitArrayCap(&substrings, required_len, allocator);
//
//	uint prev = 0;
//	for (uint i = 0; i < str.len; i++) {
//		if (str.data[i] == character) {
//			array_push(&substrings, slice(str, prev, i));
//			prev = i + 1;
//		}
//	}
//	array_push(&substrings, slice(str, prev));
//	
//	return substrings.slice;
//}


//void* slot_array_subscript_raw(SlotArrayRaw* arr, SlotArrayHandle handle, usize elem_size) {
//	ZoneScoped;
//	if (handle.id == 0) return NULL;
//
//	handle.id -= 1;
//	u32 bucket_index = handle.id / arr->num_items_per_bucket; // if we made `num_items_per_bucket` a power of 2, we could get rid of this divide
//	u32 item_index = handle.id % arr->num_items_per_bucket;
//
//	const usize item_size = sizeof(SlotArrayElemHeader) + elem_size;
//	SlotArrayElemHeader* item = (SlotArrayElemHeader*)((u8*)arr->buckets.data[bucket_index] + item_index * item_size);
//	if (item->gen != handle.gen) return NULL;
//
//	return item + 1;
//}

//SlotArrayHandle slot_array_add_raw(SlotArrayRaw* arr, usize size, const void* value) {
//	ZoneScoped;
//	ASSERT(arr->num_items_per_bucket != 0); // Did you call slot_array_init?
//
//	const isize item_size = sizeof(SlotArrayElemHeader) + size;
//
//	arr->num_alive += 1;
//	if (arr->first_removed) {
//		// take a previously freed handle
//
//		u32 id = arr->first_removed;
//		u32 bucket_index = (id - 1) / arr->num_items_per_bucket;
//		u32 item_index = (id - 1) % arr->num_items_per_bucket;
//
//		SlotArrayElemHeader* item = (SlotArrayElemHeader*)((u8*)arr->buckets.data[bucket_index] + item_size * item_index);
//		arr->first_removed = item->next_free_item;
//
//		item->next_free_item = 0;
//		memcpy(item + 1, value, size);
//
//		return { id, item->gen };
//	}
//
//	if (arr->last_bucket_cursor >= arr->num_items_per_bucket) {
//		arr->last_bucket_cursor = 0;
//		arr->last_bucket++;
//	}
//	if (arr->last_bucket >= arr->buckets.len) {
//		//if (!arr->allocator.proc)
//		//	slot_array_init(arr);
//
//		Slice<u8> bucket_allocation = MemAllocSlice(u8, arr->num_items_per_bucket * item_size, arr->allocator);
//		array_append(&arr->buckets, (void*)bucket_allocation.data);
//	}
//
//	// allocate a new handle
//
//	u32 id = arr->last_bucket * arr->num_items_per_bucket + arr->last_bucket_cursor + 1;
//	SlotArrayElemHeader* item = (SlotArrayElemHeader*)((u8*)arr->buckets.data[arr->last_bucket] + item_size * arr->last_bucket_cursor);
//	item->gen = 0;
//	item->next_free_item = 0;
//	memcpy(item + 1, value, size);
//	arr->last_bucket_cursor++;
//
//	return { id, item->gen };
//}

//inline u64 leak_tracker_next_allocation(LeakTracker* t) {
//	u64 new_idx = t->next_allocation_idx;
//	t->next_allocation_idx++;
//	return new_idx;
//}

#if 0
Slice<u8> tracker_alloc(Allocator* allocator, i64 size, i64 alignment) {
	LeakTracker* tracker = (LeakTracker*)allocator;
	Slice<u8> allocation = tracker->passthrough_allocator->alloc(tracker->passthrough_allocator, size, alignment);
	
	// Even with zero sized allocations, we still want a unique allocated address at least for memory debugging purposes.
	ASSERT(allocation.data != NULL);

	LeakTracker_Entry entry;
	entry.allocation_idx = leak_tracker_next_allocation(tracker);
	entry.size = size;
	entry.alignment = alignment;
	array_init(&entry.callstack, 8, &tracker->internal_arena.allocator);

	{
		struct Pass {
			int i;
			LeakTracker* tracker;
			LeakTracker_Entry* entry;
		} pass = {0, tracker, &entry};
		_foundation_pass = &pass;

		os_get_stack_trace([](String function, String file, u32 line) {
			Pass* pass = (Pass*)_foundation_pass;
			if (pass->i > 0) {
				String* filepath_cached = pass->tracker->file_names_cache[file];
				if (!filepath_cached) {
					String cloned = str_clone(file, &pass->tracker->internal_arena.allocator);
					filepath_cached = map_insert(&pass->tracker->file_names_cache, cloned, cloned).ptr;
				}

				array_append(&pass->entry->callstack, { *filepath_cached, line });
			}
			pass->i++;
		});
	}

	map_insert(&tracker->active_allocations, (void*)allocation.data, entry);
	return allocation;
	BP;
	return {};
}
#endif

#if 0
void tracker_free(Allocator* allocator, Slice<u8> allocation) {
	BP;
	LeakTracker* tracker = (LeakTracker*)allocator;
	if (allocation.data) {
		LeakTracker_Entry* entry = tracker->active_allocations[(void*)allocation.data];
		if (entry && entry->size == allocation.len) {
			map_remove(&tracker->active_allocations, (void*)allocation.data);
			tracker->passthrough_allocator->free(tracker->passthrough_allocator, allocation);
		}
		else {
			ASSERT(false); // not sure if we should have bad_free_array or assert
			LeakTracker_BadFree bad_free = { (void*)allocation.data };
			array_append(&tracker->bad_free_array, bad_free);
		}
	}
}
#endif

#if 0
void tracker_resize(Allocator* allocator, Slice<u8>* allocation, i64 new_size, i64 alignment) {
	LeakTracker* tracker = (LeakTracker*)allocator;
	u8* old_address = allocation->data;
	ASSERT(old_address);

	tracker->passthrough_allocator->resize(tracker->passthrough_allocator, allocation, new_size, alignment);
	
	LeakTracker_Entry entry = {};

	if (old_address) {
		entry = *tracker->active_allocations[(void*)old_address];
		
		if (old_address != allocation->data) {
			ASSERT(map_remove(&tracker->active_allocations, (void*)old_address));
		}
	}

	//entry.location = loc;
	//entry.memory = (void*)allocation->data;
	entry.size = new_size;
	entry.alignment = alignment;
	map_insert(&tracker->active_allocations, (void*)allocation->data, entry, MapInsertMode_Overwrite);
	BP;
}
#endif

//void tracker_begin(Allocator* allocator, void* address) {
//	LeakTracker* tracker = (LeakTracker*)allocator;
//	map_insert(&tracker->active_allocations, address, (LeakTracker_Entry) { leak_tracker_next_allocation(tracker) });
//}
//
//void tracker_end(Allocator* allocator, void* address) {
//	LeakTracker* tracker = (LeakTracker*)allocator;
//	if (address) {
//		LeakTracker_Entry* entry = tracker->active_allocations[address];
//		if (entry) {
//			map_remove(&tracker->active_allocations, address);
//		}
//		else {
//			LeakTracker_BadFree bad_free = { address };
//			array_append(&tracker->bad_free_array, bad_free);
//		}
//	}
//}

void leak_tracker_init() {
	//ZoneScoped;
	ASSERT(!_leak_tracker.active);
	
	_leak_tracker.internal_arena = arena_make_virtual_reserve_fixed(GiB(1), NULL);
	_leak_tracker.active_allocations = map64_make_raw(sizeof(LeakTracker_Entry), &_leak_tracker.internal_arena->alc);
	_leak_tracker.file_names_cache = map64_make_raw(sizeof(String), &_leak_tracker.internal_arena->alc);
	_leak_tracker.active = true;
}

void leak_tracker_deinit() {
	ASSERT(_leak_tracker.active);
	_leak_tracker.active = false;
	//ZoneScoped;
	
	Allocator* temp = temp_push();
	LeakTracker_Entry* entry;
	for MAP64_EACH_RAW(&_leak_tracker.active_allocations, key, &entry) {
		printf("Leak tracker still has an active entry! Original callstack:\n");
		for (uint i = 0; i < entry->callstack.len; i++) {
			LeakTrackerCallstackEntry stackframe = ARRAY_IDX(LeakTrackerCallstackEntry, entry->callstack, i);
			printf("   - file: %s, line: %u\n", str_to_cstring(stackframe.file, temp), stackframe.line);
		}
	}

	temp_pop();
	//BP;
	//for (uint i = 0; i < .alive_count; i++) {
	//	BP;
	//}

	//if (_leak_tracker.active_allocations.alive_count > 0) {
	//}
	//ASSERT(_leak_tracker.active_allocations.alive_count == 0); // TODO: print a summary

	arena_free(_leak_tracker.internal_arena);
}

typedef struct {
	LeakTracker_Entry* entry;
	uint skip_stackframes_count;
	uint i;
} LeakTrackerBeginEntryPass;

// Currently uses the fvn64 hash algorithm. We could use something a lot faster.
u64 str_hash(String data) {
	u64 h = 0xcbf29ce484222325;
	for (u64 i =0; i < data.len; i++) {
		h = (h * 0x100000001b3) ^ data.data[i];
	}
	return h;
}

static void leak_tracker_begin_entry_stacktrace_visitor(String function, String file, u32 line, void* user_ptr) {
	LeakTrackerBeginEntryPass* pass = user_ptr;
	if (pass->i > pass->skip_stackframes_count) {
		u64 filepath_hash = str_hash(file);
		
		MapInsertResult map_entry = map64_insert_raw(&_leak_tracker.file_names_cache, filepath_hash, NULL, MapInsert_DoNotOverride);		
		String* filepath_cached = (String*)map_entry.value;
		if (map_entry.added) {
			*filepath_cached = str_clone(file, &_leak_tracker.internal_arena->alc);
		}

		array_push_raw(&pass->entry->callstack, &(LeakTrackerCallstackEntry){ *filepath_cached, line }, sizeof(LeakTrackerCallstackEntry));
	}
	pass->i++;
}

void leak_tracker_begin_entry(void* address, uint skip_stackframes_count) {
	if (!_leak_tracker.active) return;
	_leak_tracker.active = false; // disable leak tracker for the duration of this function
	
	LeakTracker_Entry entry = {0};
	entry.callstack = make_array_cap_raw(sizeof(LeakTracker_Entry), 8, &_leak_tracker.internal_arena->alc);
	
	// We could even store the function name and use the file_names_cache. Maybe rename it to just "string_table"
	get_stack_trace(leak_tracker_begin_entry_stacktrace_visitor, &(LeakTrackerBeginEntryPass) { &entry, skip_stackframes_count, 0});
	
	map64_insert_raw(&_leak_tracker.active_allocations, (u64)address, &entry, MapInsert_AssertUnique);
	_leak_tracker.active = true;
}

ArrayRaw make_array_raw(Allocator* alc) { return (ArrayRaw) { .alc = alc }; }

ArrayRaw make_array_len_raw(u32 elem_size, uint len, const void* initial_value, Allocator* alc) {
	ArrayRaw array = make_array_len_garbage_raw(elem_size, len, alc);
	for (uint i = 0; i < len; i++) {
		memcpy((u8*)array.data + elem_size*i, initial_value, elem_size);
	}
	return array;
}

ArrayRaw make_array_len_garbage_raw(u32 elem_size, uint len, Allocator* alc) {
	return (ArrayRaw) {
		.data = mem_alloc_count(u8, len * elem_size, alc), // TODO: go with the next power of 2
		.len = len,
		.capacity = len,
		.alc = alc,
	};
}

ArrayRaw make_array_cap_raw(u32 elem_size, uint capacity, Allocator* alc) {
	return (ArrayRaw) {
		.data = mem_alloc_count(u8, capacity * elem_size, alc), // TODO: go with the next power of 2
		.len = 0,
		.capacity = capacity,
		.alc = alc,
	};
}

void array_reserve_raw(ArrayRaw* array, uint capacity, u32 elem_size) {
	if (capacity > array->capacity) {
		ASSERT(array->alc); // Did you call make_array?
		array->data = mem_resize_count(u8, array->data, array->capacity * elem_size, capacity * elem_size, array->alc);
		array->capacity = capacity;
	}
}

void free_array_raw(ArrayRaw* array, u32 elem_size) {
	mem_free_count(u8, array->data, elem_size * array->capacity, array->alc);
	_DEBUG_FILL_GARBAGE(array, sizeof(*array));
}

#ifdef _DEBUG
void _DEBUG_FILL_GARBAGE(void* ptr, uint len) { memset(ptr, 0xCC, len); }
#endif

void slice_copy_raw(SliceRaw dst, SliceRaw src) {
}

void array_push_slice_raw(ArrayRaw* array, SliceRaw elems, u32 elem_size) {
	array_reserve_raw(array, array->len + elems.len, elem_size);
	for (uint i = 0; i < elems.len; i++) {
		array_push_raw(array, (const void*)((u8*)elems.data + elem_size * i), elem_size);
	}
}

void array_resize_raw(ArrayRaw* array, uint len, const void* value, u32 elem_size) {
	array_reserve_raw(array, len, elem_size);
	for (uint i = array->len; i < len; i++) {
		memcpy((u8*)array->data + i * elem_size, value, elem_size);
	}
	array->len = len;
}

uint array_push_raw(ArrayRaw* array, const void* elem, u32 elem_size) {
	if (array->len >= array->capacity) {
		// grow the array
		uint new_capacity = MAX(8, array->capacity * 2);
		array->data = mem_resize_count(u8, array->data, elem_size * array->capacity, elem_size * new_capacity, array->alc);
		array->capacity = new_capacity;
	}
	memcpy((u8*)array->data + array->len * elem_size, elem, elem_size);
	return array->len++;
}

void array_pop_raw(ArrayRaw* array, OPT(void*) out_elem, u32 elem_size) {
	ASSERT(array->len >= 1);
	array->len--;
	if (out_elem) {
		memcpy(out_elem, (u8*)array->data + array->len * elem_size, elem_size);
	}
}

void leak_tracker_assert_is_alive(void* address) {
	if (!_leak_tracker.active) return;
	ASSERT(map64_get_raw(&_leak_tracker.active_allocations, (u64)address));
}

void leak_tracker_end_entry(void* address) {
	if (!_leak_tracker.active) return;
	_leak_tracker.active = false; // disable leak tracker for the duration of this function
	bool ok = map64_remove_raw(&_leak_tracker.active_allocations, (u64)address);
	ASSERT(ok);

	_leak_tracker.active = true;
}

u32 rand_u32() {
	//ZoneScoped;
	BP;
	//ASSERT(false);
	//return rand();
	return 0;
	//if (global_rand == 0) {
	//	global_rand = read_cycle_counter();
	//}
	//
	//return (u32)rand() + (u32)global_rand;
}

u64 rand_u64() {
	BP;
	//return (((u64)rand_u32()) << 32) | (u64)rand_u32();
	return 0;
}


float rand_float_in_range(float minimum, float maximum) {
	BP;
	//ZoneScoped;
	//return minimum + (maximum - minimum) * (((float)rand()) / (float)RAND_MAX);
	return 0;
}



//void _custom_print_f(void(*append_fn)(String), String fmt, std::initializer_list<String> args) {
//	Slice<String> args_slice = { (String*)args.begin(), args.size() };
//
//	uint reserve_len = fmt.len;
//	for (int i = 0; i < args.size(); i++) reserve_len += args_slice[i].len;
//
//	for (uint offset = 0, offset_next = 0; rune r = str_next_rune(fmt, &offset_next); offset = offset_next) {
//		if (r == '%') {
//			offset = offset_next;
//			r = str_next_rune(fmt, &offset_next);
//
//			if (r == '%') { // escape %
//				append_fn(LIT("%"));
//				continue;
//			}
//
//			ASSERT(r >= '0' && r <= '9');
//			append_fn(args_slice[r - '0']);
//			continue;
//		}
//
//		append_fn(slice(fmt, offset, offset_next));
//	}
//}

//String __aprint(Allocator* allocator, const char* fmt, ...) {
//	//ZoneScoped;
//	va_list args;
//	va_start(args, fmt);
//	String result = __aprint_va_list(allocator, fmt, args);
//	va_end(args);
//	return result;
//	//__crt_va_start(args, fmt); defer(__crt_va_end(args));
//}


//void _print_fmt(String fmt, std::initializer_list<String> args) {
//	_custom_print_f([](String str) {
//		os_write_to_console(str);
//	}, fmt, args);
//}

//String _aprint_fmt(Allocator* allocator, String fmt, std::initializer_list<String> args) {
//	Slice<String> args_slice = { (String*)args.begin(), args.size() };
//
//	uint size = 0;
//	_foundation_pass = &size;
//	_custom_print_f([](String str) { *(uint*)_foundation_pass += 1; }, fmt, args);
//	
//	String result = MakeSlice(u8, size, allocator);
//	
//	_foundation_pass = result.data;
//	_custom_print_f([](String str) {
//		memcpy(_foundation_pass, str.data, str.len);
//		_foundation_pass = (u8*)_foundation_pass + str.len;
//	}, fmt, args);
//
//	return result;
//}

//void _bprint_fmt(Array<u8>* buffer, String fmt, std::initializer_list<String> args) {
//	_foundation_pass = buffer;
//	_custom_print_f([](String str) {
//		array_push_slice((Array<u8>*)_foundation_pass, str);
//		}, fmt, args);
//}
//
//void _Print(std::initializer_list<String> args) {
//	for (String arg : args) {
//		os_write_to_console(arg);
//	}
//}

//void _PrintB(Array<u8>* buffer, std::initializer_list<String> args) {
//	for (String arg : args) {
//		array_push_slice(buffer, arg);
//	}
//}

//String _PrintA(Allocator* allocator, std::initializer_list<String> args) {
//	uint size = 0;
//	for (String arg : args) size += arg.len;
//
//	Array<u8> buf;
//	InitArrayCap(&buf, size, allocator);
//	for (String arg : args) array_push_slice(&buf, arg);
//	return buf.slice;
//}

String str_from_uint(Allocator* allocator, String bytes) {
	ASSERT(bytes.len == 1 || bytes.len == 2 || bytes.len == 4 || bytes.len == 8);
	return bytes.len == 1 ?
			str_format(allocator, "%hhu", *(u8*)bytes.data) : bytes.len == 2 ?
			str_format(allocator, "%hu", *(u16*)bytes.data) : bytes.len == 4 ?
			str_format(allocator, "%u", *(u32*)bytes.data) :
			str_format(allocator, "%llu", *(u64*)bytes.data);
}

String str_from_int(Allocator* allocator, String bytes) {
	ASSERT(bytes.len == 1 || bytes.len == 2 || bytes.len == 4 || bytes.len == 8);
	return bytes.len == 1 ?
			str_format(allocator, "%hhd", *(s8*)bytes.data) : bytes.len == 2 ?
			str_format(allocator, "%hd", *(s16*)bytes.data) : bytes.len == 4 ?
			str_format(allocator, "%d", *(s32*)bytes.data) :
			str_format(allocator, "%lld", *(s64*)bytes.data);
}

String str_from_float_ex(Allocator* allocator, String bytes, int num_decimals) {
	//ZoneScoped;
	ASSERT(bytes.len == 4 || bytes.len == 8);
	char fmt_string[5];
	fmt_string[0] = '%';
	fmt_string[1] = '.';
	fmt_string[2] = '0' + (char)MIN(num_decimals, 9);
	fmt_string[3] = 'f';
	fmt_string[4] = 0;
	return str_format(allocator, fmt_string, bytes.len == 4 ? *(f32*)bytes.data : *(f64*)bytes.data);
}

String str_from_float(Allocator* allocator, String bytes) {
	ASSERT(bytes.len == 4 || bytes.len == 8);
	return str_format(allocator, "%f", bytes.len == 4 ? *(f32*)bytes.data : *(f64*)bytes.data);
}


//Slice<u8> global_allocator_alloc(Allocator* allocator, uint size, uint alignment) {
//	Slice<u8> allocation;
//	allocation.data = (u8*)malloc(size);
//	allocation.len = size;
//
//#ifdef _DEBUG
//	memset(allocation.data, 0xCC, allocation.len);
//#endif
//
//	// About capacity:
//	// As we're providing the user with the size of the allocation, we could potentially supply more than the requested size there.
//	// The only problem is that some places, such as `new_clone` will not end up using that memory / storing the allocated size anywhere
//	// and will instead provide the requested size back to free.
//	// Maybe we could have additional `AllocatorMode_Alloc_Flexible` and `AllocatorMode_Resize_Flexible` modes
//
//	return allocation;
//}

//void global_allocator_free(Allocator* allocator, Slice<u8> allocation) {
//#ifdef _DEBUG
//	memset(allocation.data, 0xCC, allocation.len); // debug; trigger data-breakpoints and make use-after-free bugs evident.
//#endif
//	free(allocation.data);
//}

//void global_allocator_resize(Allocator* allocator, Slice<u8>* allocation, uint new_size, uint alignment) {
//	if (new_size <= allocation->len) return;
//
//	ASSERT(allocation->data != NULL);
//
//#ifdef _DEBUG
//	u8* new_data = (u8*)malloc(new_size);
//	memcpy(new_data, allocation->data, allocation->len);
//	memset(new_data + allocation->len, 0xCC, new_size - allocation->len);
//
//	memset(allocation->data, 0xCC, allocation->len); // debug; trigger data-breakpoints and make use-after-free bugs evident.
//	free(allocation->data);
//
//	allocation->data = new_data;
//	allocation->len = new_size;
//#else
//	allocation->data = (u8*)realloc(allocation->data, new_size);
//	allocation->len = new_size;
//#endif
//}

void str_copy(String dst, String src) {
	ASSERT(dst.len >= src.len);
	memcpy(dst.data, src.data, src.len);
}

u8* arena_push(Arena* arena, String data, uint_pow2 alignment) {
	String result = arena_push_size(arena, data.len, alignment);
	str_copy(result, data);
	return result.data;
}

static void error_out_of_memory() {
	os_error_message(LIT("Error!"), LIT("Program ran out of available memory.\nThis is likely the fault of the program itself."));
	exit(1);
}

String arena_push_size(Arena* arena, uint size, uint_pow2 alignment) {
	ASSERT(IS_POWER_OF_2(alignment));
	//ZoneScoped;
	u8* allocation_pos = NULL;

	HITS(_c, 0);
	// TODO: get rid of this switch for the allocator vtable

	switch (arena->desc.mode) {
	case ArenaMode_VirtualReserveFixed: {
		//uint allocation_pos = ALIGN_UP_POW2(arena->pos.offset, alignment);
		//arena->pos.offset = allocation_pos + size;
		allocation_pos = (u8*)ALIGN_UP_POW2((uint)arena->pos.head, alignment);
		arena->pos.head = allocation_pos + size;

		if (arena->pos.head > arena->committed_end) {
			os_mem_commit(arena->committed_end, (uint)arena->pos.head - (uint)arena->committed_end);
			arena->committed_end = (u8*)ALIGN_UP_POW2((uint)arena->pos.head, KiB(4));
		}
		
		if (arena->pos.head > arena->internal_base + arena->desc.VirtualReserveFixed.reserve_size) {
			error_out_of_memory();
		}

#ifdef _DEBUG
		memset(allocation_pos, 0xCC, size);
#endif
	} break;
	
	case ArenaMode_UsingBufferFixed: {
		allocation_pos = (u8*)ALIGN_UP_POW2((uint)arena->pos.head, alignment);
		arena->pos.head = allocation_pos + size;

		ASSERT(arena->internal_base);
		if (arena->pos.head > arena->internal_base + arena->desc.UsingBufferFixed.size) {
			error_out_of_memory();
		}
	} break;
	
	case ArenaMode_UsingAllocatorGrowing: {
		// form a linked list of allocation blocks

		allocation_pos = (u8*)ALIGN_UP_POW2((uint)arena->pos.head, alignment);

		ArenaBlockHeader* curr_block = arena->pos.current_block;
		if (!curr_block || (allocation_pos + size > (u8*)curr_block + curr_block->size_including_header)) {
			//HITS(_cccc, 31);
			// The allocation doesn't fit in this block.

			OPT(ArenaBlockHeader*) next_block = curr_block ? curr_block->next : NULL;
			
			uint block_size = MAX(arena->desc.UsingAllocatorGrowing.min_block_size, sizeof(ArenaBlockHeader) + size);

			// If the allocation overflows the block size, let's free at least as much memory from above the head as we allocate.
			// This is to make sure that the arena's memory usage never grows past the maximum of what the user has asked for.
			//ArenaBlockHeader* b = next_block;
			for (uint amount_of_memory_released = 0;
				next_block && block_size > next_block->size_including_header && amount_of_memory_released < block_size;)
			{
				ArenaBlockHeader* after = next_block->next;
				amount_of_memory_released += next_block->size_including_header;
				mem_free(next_block, next_block->size_including_header, arena->desc.UsingAllocatorGrowing.alc);
				next_block = after;
				curr_block->next = next_block;
			}

			if (next_block && block_size <= next_block->size_including_header) {
				curr_block = next_block;
			}
			else {
				// allocate a new block
				ASSERT(alignment <= 16); // Let's align each block to 16 bytes.
				ArenaBlockHeader* new_block = mem_alloc(block_size, 16, arena->desc.UsingAllocatorGrowing.alc);
				new_block->size_including_header = block_size;
				new_block->next = next_block;

				//printf("_c is %llu\n", _c);

				if (curr_block) curr_block->next = new_block;
				else arena->first_block = new_block;
				curr_block = new_block;
			}

			arena->pos.current_block = curr_block;
			allocation_pos = (u8*)(curr_block + 1);
		}

		arena->pos.head = allocation_pos + size;
		ASSERT(arena->pos.head <= (u8*)arena->pos.current_block + arena->pos.current_block->size_including_header);
	} break;
	
	default: BP;
	}

	return (String){ allocation_pos, size };
}


Map64Raw map64_make_raw(u32 value_size, Allocator* alc) { return make_map64_cap_raw(value_size, 0, alc); }

static u32 hashmap64_get_slot_index(u64 key, u32 slot_count_log2) {
	const u64 golden_ratio_64 = 0x9E3779B97F4A7D69;

	u32 slot_index = (u32)((key * golden_ratio_64) >> (64 - slot_count_log2)); // fibonacci hash
	return slot_index;
}

OPT(void*) map64_get_raw(Map64Raw* map, u64 key) {
	ASSERT(key <= HASHMAP64_LAST_VALID_KEY);
	if (!map->slots) return NULL;

	//HITS(_c, 628);
	u32 slot_index = hashmap64_get_slot_index(key, map->slot_count_log2);
	u32 slot_size = map->value_size + 8;
	u8* slots = (u8*)map->slots;

	u32 wrapping_mask = (1u << map->slot_count_log2) - 1;
	for (;;) {
		ASSERT(slot_index < (1u << map->slot_count_log2));
		u64* key_ptr = (u64*)(slots + slot_index * slot_size);
		if (*key_ptr == key) {
			return key_ptr + 1;
		}

		if (*key_ptr == HASHMAP64_EMPTY_KEY) return NULL;
		
		slot_index = (slot_index + 1) & wrapping_mask;
	}
}

void resize_map64_raw(Map64Raw* map, u32 slot_count_log2) {
	slot_count_log2 = MAX(slot_count_log2, 2); // always have at minimum 4 slots
	u32 slot_size = map->value_size + 8;
	
	OPT(u8*) slots_before = map->slots;
	u32 slot_count_before = (1 << map->slot_count_log2);

	map->alive_count = 0;
	map->slot_count = (1 << slot_count_log2);
	map->slot_count_log2 = slot_count_log2;
	
	//((uint) & ((T*)0)->f)
	//uint test = ((struct _dummy { char c; Arena member; }*)0)->member;

	int test = ALIGN_OF(int);

	HITS(_c, 0);
	map->slots = mem_alloc_count(u8, slot_size * map->slot_count, map->alc);
	//if (map->slots == (void*)0x0000020000004440) BP;
	memset(map->slots, 0xFFFFFFFF, map->slot_count * slot_size); // fill all keys with HASHMAP64_EMPTY_KEY

	if (slots_before) {
		for (uint i = 0; i < slot_count_before; i++) {
			u64* key_ptr = (u64*)(slots_before + i * slot_size);
			void* value_ptr = key_ptr + 1;
			if (*key_ptr <= HASHMAP64_LAST_VALID_KEY) {
				map64_insert_raw(map, *key_ptr, value_ptr, MapInsert_AssertUnique);
			}
		}

		mem_free_count(u8, slots_before, slot_size * slot_count_before, map->alc);
	}
}

uint_pow2 next_pow_of_2(uint v) {
	// https://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	v |= v >> 32;
	v++;
	return v;
}

uint log2(uint_pow2 value) {
	ASSERT(IS_POWER_OF_2(value));
	uint result = 0;
	for (; value > 1;) {
		value >>= 1;
		result++;
	}
	return result;
}

#define slice_get(T, slice, index) ((T*)slice.data)[index]

String str_join(Allocator* alc, SliceRaw args) {
	uint offset = 0;
	for (uint i = 0; i < args.len; i++) {
		offset += slice_get(String, args, i).len;
	}

	String result = { mem_alloc_count(u8, offset, alc), offset };

	offset = 0;
	for (uint i = 0; i < args.len; i++) {
		String arg = slice_get(String, args, i);
		str_copy(str_slice(result, offset, offset + arg.len), arg);
		offset += arg.len;
	}
	return result;
}

Map64Raw make_map64_cap_raw(u32 value_size, uint_pow2 capacity, Allocator* alc) {
	Map64Raw map = (Map64Raw){
		.alc = alc,
		.value_size = ALIGN_UP_POW2(value_size, 8),
	};
	uint slot_count_log2 = log2(capacity);
	resize_map64_raw(&map, CAST_CHK(u32, slot_count_log2));
	return map;
}

MapInsertResult map64_insert_raw(Map64Raw* map, u64 key, OPT(const void*) value, MapInsert mode) {
	ASSERT(key <= HASHMAP64_LAST_VALID_KEY);
	HITS(_c, 0);

	//     filled / allocated >= 70/100
	// <=> filled * 100 >= allocated * 70

	// Note that `slot_count_before` will be 1 even when it's actually zero, becase we can't represent 0 in the _log2 form.
	// However this is fine, because the map will expand in that case too.
	u32 slot_count_before = 1u << map->slot_count_log2;
	if ((map->alive_count + 1) * 100 >= slot_count_before * 70) {
		// expand the map
		resize_map64_raw(map, map->slot_count_log2 + 1);
	}

	u32 slot_index = hashmap64_get_slot_index(key, map->slot_count_log2);
	u32 slot_size = map->value_size + 8;
	u8* slots = (u8*)map->slots;
	u32 wrapping_mask = (1u << map->slot_count_log2) - 1;

	void* first_dead_value = NULL;
	
	for (;;) {
		u64* key_ptr = (u64*)(slots + slot_index * slot_size);
		void* value_ptr = key_ptr + 1;

		if (*key_ptr > HASHMAP64_LAST_VALID_KEY) {
			// We can't stop yet, because the key might still exist after this.
			if (!first_dead_value) first_dead_value = value_ptr;
			if (*key_ptr == HASHMAP64_EMPTY_KEY) break; // Don't have to continue further, we know that this key does not exist in the map.
		}
		else if (*key_ptr == key) {
			if (mode == MapInsert_Override) {
				memcpy(value_ptr, value, map->value_size);
				return (MapInsertResult) { value_ptr, .added = false };
			}
			else if (mode == MapInsert_DoNotOverride) {
				return (MapInsertResult) { value_ptr, .added = false };
			}
			else { // Element already exists, and the behaviour of the map is set to AssertUnique!
				ASSERT(false);
			}
		}
		slot_index = (slot_index + 1) & wrapping_mask;
	}

	map->alive_count++;
	*((u64*)first_dead_value - 1) = key;
	if (value) memcpy(first_dead_value, value, map->value_size);
	return (MapInsertResult) { first_dead_value, .added = true };
}

bool map64_remove_raw(Map64Raw* map, u64 key) {
	if (map->alive_count == 0 || !map->slots) return false;

	u32 slot_index = hashmap64_get_slot_index(key, map->slot_count_log2);
	u32 slot_size = map->value_size + 8;
	u8* slots = (u8*)map->slots;
	u32 wrapping_mask = (1u << map->slot_count_log2) - 1;

	for (;;) {
		u64* key_ptr = (u64*)(slots + slot_index * slot_size);
		if (*key_ptr == HASHMAP64_EMPTY_KEY) {
			return false; // key does not exist in the table!
		}

		u32 next_slot_index = (slot_index + 1) & wrapping_mask;

		if (*key_ptr == key) {
			void* value_ptr = key_ptr + 1;
#ifdef _DEBUG
			memset(value_ptr, 0xCC, map->value_size); // debug; trigger data-breakpoints
#endif
			u64* next_key_ptr = (u64*)(slots + next_slot_index * slot_size);
			if (*next_key_ptr == HASHMAP64_EMPTY_KEY) {
				// If the next slot is empty and not required for the chain, this slot is not required for the chain either.
				*key_ptr = HASHMAP64_EMPTY_KEY;
				// TODO: We could release the entries before this from the chain duty as well
			}
			else {
				*key_ptr = HASHMAP64_DEAD_BUT_REQUIRED_FOR_CHAIN_KEY;
			}

			map->alive_count--;
			return true;
		}

		slot_index = next_slot_index;
	}
}

void free_map64_raw(Map64Raw* map) {
	u32 slot_size = map->value_size + 8;
	if (map->slots) {
		mem_free_count(u8, map->slots, slot_size * map->slot_count, map->alc);
	}
	_DEBUG_FILL_GARBAGE(map, sizeof(*map));
}

//SlotArenaRaw* make_slot_arena_contiguous_raw(u32 elem_size, ArenaDesc arena_desc) {
//	SlotArenaRaw _slot_arena = {
//		.first_free = &SLOT_ARENA_FREELIST_END,
//		.is_using_arena = true,
//		.using_arena = make_arena(arena_desc),
//		.elem_size = elem_size,
//	};
//	return mem_clone(_slot_arena, &_slot_arena.arena->alc);
//}

//SlotArenaRaw* make_slot_arena_raw(u32 elem_size, u32 num_elems_per_bucket, Allocator* alc) {
//	SlotArenaRaw _slot_arena = {
//		.first_free = &SLOT_ARENA_FREELIST_END,
//		.alc = alc,
//		.num_elems_per_bucket = num_elems_per_bucket,
//		.elem_size = elem_size,
//	};
//	return mem_clone(_slot_arena, alc);
//}

/*

void slot_arena_clear_raw(RawSlotArena* arena) {
	arena_clear(arena->arena);
	arena->num_active = 0;
	arena->num_freed = 0;
	arena->first_free = &SLOT_ARENA_FREELIST_END;
}

void* slot_arena_add_garbage_raw(RawSlotArena* arena) {
	//ZoneScoped;
	ASSERT(arena->arena->reserved != 0); // have you called slot_arena_create?
	arena->num_active++;
	
	// should the freelist items point to the actual content or the freelist pointer?
	// I think I should switch it to point to the content, because it's less steps in the iterator.

	if (arena->first_free != &SLOT_ARENA_FREELIST_END) {
		void* ptr = arena->first_free;
		void** next_free = (void**)((u8*)ptr - sizeof(uint));
		arena->first_free = *next_free;
		*next_free = NULL;
		return ptr;
	}
	
	// TODO: 8 byte alignment?
	// For that we'd have to update SLOT_ARENA_EACH
	uint* next_free = (uint*)arena_push_size(arena->arena, sizeof(uint) + arena->elem_size, 1).data;
	*next_free = 0;
	return next_free + 1;
}

bool slot_arena_remove_raw(RawSlotArena* arena, void* ptr) {
	//ZoneScoped;
	BP;//ASSERT(ptr > arena->arena->mem && ptr < arena->arena->mem + arena->arena->pos);

	void** next_free = (void**)((u8*)ptr - sizeof(uint));
	ASSERT(*next_free == NULL); // If this fails, it means you tried to remove an element that was already removed.

	void* first_removed = arena->first_free;
	arena->first_free = ptr;
	*next_free = first_removed;
	arena->num_active--;
	return true;
}

void delete_slot_arena_raw(RawSlotArena* arena) {
	//ZoneScoped;
	delete_arena(arena->arena);
	_DEBUG_FILL_GARBAGE(arena, sizeof(RawSlotArena));
}

u64 slot_arena_get_index_raw(const RawSlotArena* arena, void* ptr) {
	//ZoneScoped;
	//ASSERT(ptr >= arena->arena->mem && ptr < arena->arena->mem + arena->arena->pos);
	//uint offset = (uint)ptr - (uint)arena->arena->mem;
	//return offset / (arena->elem_size + 8);
	BP;
	return 0;
}
*/

u8* arena_get_base_contiguous(Arena* arena) {
	ASSERT(arena->desc.mode == ArenaMode_UsingBufferFixed || arena->desc.mode == ArenaMode_VirtualReserveFixed);
	return arena->internal_base + sizeof(Arena); }

uint arena_get_cursor(Arena* arena) {
	ASSERT(arena->desc.mode == ArenaMode_UsingBufferFixed || arena->desc.mode == ArenaMode_VirtualReserveFixed);
	return arena->pos.head - (arena->internal_base + sizeof(Arena));
}

ArenaPosition arena_get_pos(Arena* arena) { return arena->pos; }

void arena_pop_to(Arena* arena, ArenaPosition pos) {
	//ZoneScoped;
#ifdef _DEBUG
	if (arena->desc.mode == ArenaMode_UsingAllocatorGrowing) {
		ArenaBlockHeader* last = arena->pos.current_block;
		for (ArenaBlockHeader* block = pos.current_block->next; block && block != last->next; block = block->next) {
			_DEBUG_FILL_GARBAGE(block + 1, block->size_including_header - sizeof(ArenaBlockHeader));
		}
		ASSERT(pos.head >= (u8*)(pos.current_block + 1));
		_DEBUG_FILL_GARBAGE(pos.head, ((u8*)pos.current_block + pos.current_block->size_including_header) - pos.head);
	}
	else {
		ASSERT(pos.head <= arena->pos.head);
		_DEBUG_FILL_GARBAGE(pos.head, arena->pos.head - pos.head); // debug; trigger data-breakpoints and garbage-fill the memory
	}
#endif

	// maybe we should also decommit memory. WARNING: if we do this, remember to update delete_arena()!!!
	arena->pos = pos;
}

void arena_clear(Arena* arena) {
	//ZoneScoped;
	if (arena->desc.mode == ArenaMode_UsingAllocatorGrowing) {
		arena_pop_to(arena, (ArenaPosition) {
			.head = (u8*)(arena->first_block + 1) + sizeof(Arena),
			.current_block = arena->first_block
		});
	} else {
		arena_pop_to(arena, (ArenaPosition) { .head = arena->internal_base + sizeof(Arena) });
	}
}

void* arena_allocator_proc(struct _Allocator* alc, OPT(u8*) old_ptr, uint old_size, uint new_size, uint new_alignment) {
	//ZoneScoped;

	HITS(_c, 0);
	Arena* arena = (Arena*)alc;

	ArenaBlockHeader* headers[2048] = { 0 };
	uint _i = 0;
	for (ArenaBlockHeader* h = arena->first_block; h; h = h->next) {
		headers[_i] = h;
		_i++;
	}

	if (new_size > old_size) {
		ASSERT(new_alignment > 0);
		ASSERT(IS_POWER_OF_2(new_alignment));

		String new_allocation = arena_push_size(arena, new_size, new_alignment);

		// TODO: Reuse the end of the arena if possible
		//if (old_ptr + old_size == arena->internal_base + arena->internal_pos &&
		//	HAS_ALIGNMENT_POW2((uint)old_ptr, new_alignment))
		//{
		//	uint difference = new_size - old_size;
		//	arena_push_size(arena, difference, 1);
		//	
		//	_DEBUG_FILL_GARBAGE(old_ptr + old_size, difference);
		//	return old_ptr;
		//}

		if (old_ptr) {
			memcpy(new_allocation.data, old_ptr, old_size); // first do the copy, then fill old with garbage
			_DEBUG_FILL_GARBAGE(old_ptr, old_size);
			_DEBUG_FILL_GARBAGE(new_allocation.data + old_size, new_size - old_size);
		}

		return new_allocation.data;
	}
	else {
		_DEBUG_FILL_GARBAGE(old_ptr + new_size, old_size - new_size); // erase the top
	}

	return old_ptr;
}

uint round_up_pow_of_2(uint x) {
	// https://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
	x--;
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> 8;
	x |= x >> 16;
	x |= x >> 32;
	return x + 1;
}

void* heap_allocator_proc(struct _Allocator* alc, OPT(u8*) old_ptr, uint old_size, uint new_size, uint new_alignment) {
	// 0b0010110
	//
	//uint arena_index = log2(round_up_pow_of_2(new_size)); // there's probably a better way to calculate this
	// the last arena is for 4k blocks
	BP;
	return NULL;
}

Heap* make_heap(ArenaDesc backing_arena_desc) {
	Heap _heap = { .alc = { heap_allocator_proc } };
	_heap.backing_arena = arena_make_ex(backing_arena_desc);
	Heap* heap = (Heap*)arena_push(_heap.backing_arena, AS_BYTES(_heap), 1);
	return heap;
}

Arena* arena_make(u32 min_block_size, Allocator* alc) {
	return arena_make_ex((ArenaDesc) {
		.mode = ArenaMode_UsingAllocatorGrowing,
		.UsingAllocatorGrowing = { .min_block_size = min_block_size, .alc = alc },
	});
}

Arena* arena_make_virtual_reserve_fixed(uint reserve_size, OPT(void*) reserve_base) {
	return arena_make_ex((ArenaDesc) {
		.mode = ArenaMode_VirtualReserveFixed,
		.VirtualReserveFixed = { .reserve_size = reserve_size, .reserve_base = reserve_base },
	});
}

Arena* arena_make_using_buffer_fixed(void* base, uint size) {
	return arena_make_ex((ArenaDesc) {
		.mode = ArenaMode_UsingBufferFixed,
		.UsingBufferFixed = {.base = base, .size = size },
	});
}

Arena* arena_make_ex(ArenaDesc desc) {	
	//HITS(_c, 3);
	Arena _arena = {
		.alc = (Allocator){ .proc = arena_allocator_proc },
		.desc = desc,
	};
	
	switch (desc.mode) {
	case ArenaMode_VirtualReserveFixed: {
		_arena.internal_base = os_mem_reserve(desc.VirtualReserveFixed.reserve_size, desc.VirtualReserveFixed.reserve_base);
		if (!_arena.internal_base) error_out_of_memory();
		_arena.pos.head = _arena.internal_base;
		_arena.committed_end = _arena.internal_base;
	} break;
	case ArenaMode_UsingBufferFixed: {
		_arena.internal_base = desc.UsingBufferFixed.base;
		_arena.pos.head = _arena.internal_base;
	} break;
	case ArenaMode_UsingAllocatorGrowing: {} break;
	default: BP;
	}

	Arena* arena = (Arena*)arena_push_size(&_arena, sizeof(Arena), 1).data;
	*arena = _arena;
	
	leak_tracker_begin_entry(arena, 1);
	return arena;
}

//Arena* make_arena_supplied_contiguous(void* base, uint size) {
//	return make_arena((ArenaDesc) {
//		.mode = ArenaMode_UsingBufferFixed,
//		.SuppliedContiguous = {
//			.base = base,
//			.size = size,
//		},
//	});
//}

//Arena* make_arena_virtual_contiguous(uint reserve_size) {
//	return make_arena_virtual_contiguous_ex(reserve_size, NULL);
//}

//Arena* make_arena_virtual_contiguous_ex(uint reserve_size, void* custom_base) {
//	return make_arena((ArenaDesc) {
//		.mode = ArenaMode_VirtualReserveFixed,
//		.VirtualContiguous = {
//			.reserve_size = reserve_size,
//			.reserve_base = custom_base,
//		},
//	});
//}

void arena_free(Arena* arena) {
	//ZoneScoped;
	leak_tracker_end_entry(arena);
	arena_clear(arena); // this will fill the arena memory with garbage in debug builds
	if (arena->desc.mode == ArenaMode_UsingBufferFixed) {
		_DEBUG_FILL_GARBAGE(arena, sizeof(Arena));
	}
	else if (arena->desc.mode == ArenaMode_VirtualReserveFixed) {
		os_mem_release(arena->internal_base);
	}
	else if (arena->desc.mode == ArenaMode_UsingAllocatorGrowing) {
		Allocator* alc = arena->desc.UsingAllocatorGrowing.alc;
		ArenaBlockHeader* block = arena->first_block;
		for (; block;) {
			ArenaBlockHeader* next = block->next;
			mem_free(block, block->size_including_header, alc);
			block = next;
		}
	}
	else BP;
}

s64 round_to_s64(float x) {
	//ZoneScoped;
	return floor_to_s64(x + 0.5f);
}

s64 floor_to_s64(float x) {
	//ZoneScoped;
	ASSERT(x > (float)I64_MIN && x < (float)I64_MAX);

	s64 x_i64 = (s64)x;
	if (x < 0) {
		float fraction = (float)x_i64 - x;
		if (fraction != 0) x_i64 -= 1;
	}
	return x_i64;
}

static const u32 offsetsFromUTF8[6] = {
	0x00000000UL, 0x00003080UL, 0x000E2080UL,
	0x03C82080UL, 0xFA082080UL, 0x82082080UL
};

// Taken and altered from https://www.cprogramming.com/tutorial/utf8.c
uint str_encode_rune(u8* output, rune r) {
	//ZoneScoped;
	u32 ch;

	ch = r;
	if (ch < 0x80) {
		*output++ = (char)ch;
		return 1;
	}
	else if (ch < 0x800) {
		*output++ = (ch >> 6) | 0xC0;
		*output++ = (ch & 0x3F) | 0x80;
		return 2;
	}
	else if (ch < 0x10000) {
		*output++ = (ch >> 12) | 0xE0;
		*output++ = ((ch >> 6) & 0x3F) | 0x80;
		*output++ = (ch & 0x3F) | 0x80;
		return 3;
	}
	else if (ch < 0x110000) {
		*output++ = (ch >> 18) | 0xF0;
		*output++ = ((ch >> 12) & 0x3F) | 0x80;
		*output++ = ((ch >> 6) & 0x3F) | 0x80;
		*output++ = (ch & 0x3F) | 0x80;
		return 4;
	}
	ASSERT(false);
	return 0;
}

// Taken and altered from https://www.cprogramming.com/tutorial/unicode.html
rune str_next_rune(String str, uint* byteoffset) {
	//ZoneScoped;
	if (*byteoffset >= str.len) return 0;
	ASSERT(*byteoffset >= 0);

	u32 ch = 0;
	int sz = 0;

	do {
		ch <<= 6;
		ch += str.data[(*byteoffset)++];
		sz++;
	} while (*byteoffset < str.len && !STR_IS_UTF8_FIRST_BYTE(str.data[*byteoffset]));
	ch -= offsetsFromUTF8[sz - 1];

	return (rune)ch;
}

rune str_prev_rune(String str, uint* byteoffset) {
	//ZoneScoped;
	if (*byteoffset <= 0) return 0;

	(void)(STR_IS_UTF8_FIRST_BYTE(str.data[--(*byteoffset)]) ||
		STR_IS_UTF8_FIRST_BYTE(str.data[--(*byteoffset)]) ||
		STR_IS_UTF8_FIRST_BYTE(str.data[--(*byteoffset)]) || --(*byteoffset));

	uint b = *byteoffset;
	return str_next_rune(str, &b);
}

uint str_rune_count(String str) {
	//ZoneScoped;
	uint i = 0;
	for STR_EACH(str, r, offset) i++;
	return i;
}

static u8 char_to_value[] = {
	0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
	0x08,0x09,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
	0xFF,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,0xFF,
	0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
};

static u8 char_is_integer[] = {
	0,0,0,0,0,0,1,1,
	1,0,0,0,1,0,0,0,
	0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,
};

static bool Str_IsU64(String s, u32 radix) {
	//ZoneScoped;
	// License: @DionSystems, modified from `MD_StringIsU64`
	bool result = false;
	if (s.len > 0) {
		result = true;
		for (u8* ptr = s.data, *opl = s.data + s.len;
			ptr < opl;
			ptr += 1)
		{
			u8 c = *ptr;
			if (!char_is_integer[c >> 3]) {
				result = false;
				break;
			}
			if (char_to_value[(c - 0x30) & 0x1F] >= radix) {
				result = false;
				break;
			}
		}
	}
	return result;
}

bool str_to_u64(String s, u32 radix, u64* out_value) {
	if (!Str_IsU64(s, radix)) return false; // TODO

	//ZoneScoped;
	ASSERT(2 <= radix && radix <= 16);
	u64 value = 0;
	for (uint i = 0; i < s.len; i++) {
		value *= radix;
		u8 c = s.data[i];
		value += char_to_value[(c - 0x30) & 0x1F];
	}
	*out_value = value;
	return true;
}



bool _str_is_i64(String s, u32 radix) {
	//ZoneScoped;
	// License: @DionSystems, modified from `MD_StringIsCStyleInt`

	u8* ptr = s.data;
	u8* opl = s.data + s.len;

	// consume sign
	for (; ptr < opl && (*ptr == '+' || *ptr == '-'); ptr += 1);

	// check integer "digits"
	String digits_substr = (String){ ptr, (uint)opl - (uint)ptr };
	return Str_IsU64(digits_substr, radix);
}

bool str_to_s64(String s, u32 radix, s64* out_value) {
	if (!_str_is_i64(s, radix)) return false;

//	ZoneScoped;
	s64 sign = 1;
	if (s.len > 0 && s.data[0] == '-') {
		sign = -1;
		s = str_slice_after(s, 1);
	}
	
	u64 val_u64;
	if (!str_to_u64(s, radix, &val_u64)) return false;
	
	*out_value = sign * (s64)val_u64;
	return true;
}

char* str_to_cstring(String s, Allocator* alc) {
	char* bytes = make_str(s.len + 1, alc).data;
	memcpy(bytes, s.data, s.len);
	bytes[s.len] = 0;
	return bytes;
}

bool str_to_f64(String s, f64* out) {
	//ZoneScoped;
	Allocator* temp = temp_push();
	char* cstr = str_to_cstring(s, temp);
	char* end;
	*out = strtod(cstr, &end);
	temp_pop();
	return s.len > 0 && end == cstr + s.len;
}

// supports UTF8
String str_replace(Allocator* allocator, String str, String substr, String replace_with) {
	Array(u8) result = make_array_cap_raw(1, str.len * 2, allocator);
	s64 last = str.len - substr.len;
	
	for (s64 i = 0; i <= last;) {
		if (str_equals(str_slice(str, i, i + substr.len), substr)) {
			array_push_slice_raw(&result, BITCAST(SliceRaw, replace_with), 1);
			i += substr.len;
		}
		else {
			array_push_raw(&result, &str.data[i], 1);
			i++;
		}
	}
	return BITCAST(String, result);
}

String str_from_cstring(const char* s) { return (String){(u8*)s, strlen(s)}; }

// Detect which OS we're compiling on
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
	#define OS_WINDOWS
#else
	#error "Sorry, only windows is supported for now!"
#endif

#ifdef OS_WINDOWS
	#define NOMINMAX
	#include <Windows.h>
	#include <DbgHelp.h>

	#pragma comment(lib, "Comdlg32.lib") // for GetOpenFileName

	void os_write_to_console(String str) {
		Allocator* temp = temp_push();

		uint str_utf16_len;
		wchar_t* str_utf16 = str_to_utf16(str, temp, 1, &str_utf16_len);
		ASSERT((u32)str_utf16_len == str_utf16_len);

		DWORD num_chars_written;
		BOOL ok = WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), str_utf16, (u32)str_utf16_len, &num_chars_written, NULL);
		temp_pop();
	}

	// colored write to console
	// WriteConsoleOutputAttribute
	void os_write_to_console_colored(String str, ConsoleAttributeFlags attributes_mask) {
		if (str.len == 0) return;
		//ASSERT(str.len < U32_MAX);
		Allocator* temp = temp_push();
		HANDLE stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);

		CONSOLE_SCREEN_BUFFER_INFO console_info;
		if (!GetConsoleScreenBufferInfo(stdout_handle, &console_info)) return;
		
		str = str_replace(temp, str, LIT("\t"), LIT("    "));

		uint str_utf16_len;
		wchar_t* str_utf16 = str_to_utf16(str, temp, 1, &str_utf16_len);

		DWORD num_chars_written;
		if (!WriteConsoleW(stdout_handle, str_utf16, CAST_CHK(u32, str_utf16_len), &num_chars_written, NULL)) return;
		
		WORD* attributes = mem_alloc_count(WORD, str.len, temp);
		for (u32 i = 0; i < str.len; i++) {
			attributes[i] = (u16)attributes_mask;
		}

		DWORD num_attributes_written;
		BOOL ok = WriteConsoleOutputAttribute(stdout_handle, attributes, CAST_CHK(u32, str.len), console_info.dwCursorPosition, &num_attributes_written);
		temp_pop();
	}

	u64 os_read_cycle_counter() {
		//ZoneScoped;
		u64 counter = 0;
		BOOL res = QueryPerformanceCounter((LARGE_INTEGER*)&counter);
		ASSERT(res == TRUE);
		return counter;
	}

	u64 os_file_get_modtime(String filepath) {
		//ZoneScoped;
		Allocator* temp = temp_push();

		HANDLE h = CreateFileA(str_to_cstring(filepath, temp), 0, FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY | FILE_FLAG_BACKUP_SEMANTICS, NULL);
		if (h == INVALID_HANDLE_VALUE) return 0;

		FILETIME write_time;
		GetFileTime(h, NULL, NULL, &write_time);

		//LARGE_INTEGER file_size;
		//GetFileSizeEx(h, &file_size);

		CloseHandle(h);
		temp_pop();
		return BITCAST(u64, write_time);
	}

	bool os_file_clone(String src_filepath, String dst_filepath) {
		//ZoneScoped;
		Allocator* temp = temp_push();

		BOOL ok = CopyFileA(str_to_cstring(src_filepath, temp), str_to_cstring(dst_filepath, temp), 0) == TRUE;
		temp_pop();
		return ok;
	}

	bool os_file_delete(String filepath) {
		Allocator* temp = temp_push();

		bool ok = DeleteFileA(str_to_cstring(filepath, temp)) == TRUE;
		temp_pop();
		return ok;
	}

	void os_sleep_milliseconds(s64 ms) {
		//ZoneScoped;
		ASSERT(ms < U32_MAX);
		Sleep((DWORD)ms);
	}

	OS_DynamicLibrary os_dynamic_library_load(String filepath) {
		//ZoneScoped;
		Allocator* temp = temp_push();
		HANDLE handle = LoadLibraryA(str_to_cstring(filepath, temp));
		leak_tracker_begin_entry(handle, 1);
		temp_pop();
		return (OS_DynamicLibrary){ .handle = handle };
	}

	bool os_dynamic_library_unload(OS_DynamicLibrary dll) {
		//ZoneScoped;
		leak_tracker_end_entry(dll.handle);
		return FreeLibrary((HMODULE)dll.handle) == TRUE;
	}

	void* os_dynamic_library_sym_address(OS_DynamicLibrary dll, String symbol) {
		//ZoneScoped;
		Allocator* temp = temp_push();
		void* addr = GetProcAddress((HMODULE)dll.handle, str_to_cstring(symbol, temp));
		temp_pop();
		return addr;
	}

	String os_file_picker_dialog(Allocator* allocator) {
		Allocator* temp = temp_push();

		//ZoneScoped;
		String buffer = make_str(4096, temp);
		buffer.data[0] = '\0';

		OPENFILENAMEA ofn = (OPENFILENAMEA){
			.lStructSize = sizeof(OPENFILENAMEA),
			.hwndOwner = NULL,
			.lpstrFile = (char*)buffer.data,
			.nMaxFile = CAST_CHK(u32, buffer.len) - 1,
			.lpstrFilter = "All\0*.*\0Text\0*.TXT\0",
			.nFilterIndex = 1,
			.lpstrFileTitle = NULL,
			.nMaxFileTitle = 0,
			.lpstrInitialDir = NULL,
			.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR,
		};

		GetOpenFileNameA(&ofn);
		temp_pop();
		return str_clone(str_from_cstring(buffer.data), allocator);
	}

	
	//Slice<String> os_file_picker_multi() {
	//	ZoneScoped;
	//	ASSERT(false); // cancelling does not work!!!!
	//	return {};

		//Slice<u8> buffer = mem_alloc(16*KB, TEMP_ALLOCATOR);
		//buffer[0] = '\0';
		//
		//OPENFILENAMEA ofn = {};
		//ofn.lStructSize = sizeof(ofn);
		//ofn.hwndOwner = NULL;
		//ofn.lpstrFile = (char*)buffer.data;
		//ofn.nMaxFile = (u32)buffer.len - 1;
		//ofn.lpstrFilter = "All\0*.*\0Text\0*.TXT\0";
		//ofn.nFilterIndex = 1;
		//ofn.lpstrFileTitle = NULL;
		//ofn.nMaxFileTitle = 0;
		//ofn.lpstrInitialDir = NULL;
		//ofn.Flags = OFN_ALLOWMULTISELECT | OFN_EXPLORER | OFN_NOCHANGEDIR;
		//GetOpenFileNameA(&ofn);
		//
		//Array<String> items = {};
		//items.allocator = TEMP_ALLOCATOR;
		//
		//String directory = String{ buffer.data, (isize)strlen((char*)buffer.data) };
		//u8* ptr = buffer.data + directory.len + 1;
		//
		//if (*ptr == NULL) {
		//	if (directory.len == 0) return {};
		//	
		//	array_append(items, directory);
		//	return items.slice;
		//}
		//
		//i64 i = 0;
		//while (*ptr) {
		//	String filename = String{ ptr, (isize)strlen((char*)ptr) };
		//
		//	String fullpath = str_join(slice({ directory, LIT("\\"), filename }), TEMP_ALLOCATOR);
		//	assert(fullpath.len < 270);
		//	array_append(items, fullpath);
		//
		//	ptr += filename.len + 1;
		//	i++;
		//	assert(i < 1000);
		//}
		//
		//return items.slice;
	//}

	wchar_t* str_to_utf16(String str, Allocator* allocator, uint num_null_terminations, uint* out_len) {
		if (str.len == 0) return NULL;
		ASSERT(str.len < I32_MAX);
		
		wchar_t* w_text = NULL;
		
		int w_len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, (const char*)str.data, (int)str.len, NULL, 0);
		w_text = mem_alloc_count(wchar_t, 2 * (w_len + num_null_terminations), allocator);
		
		int w_len1 = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, (const char*)str.data, (int)str.len, (wchar_t*)w_text, w_len);
		
		ASSERT(w_len != 0 && w_len1 == w_len);
		
		memset(&w_text[w_len], 0, num_null_terminations * sizeof(wchar_t));

		*out_len = w_len;
		return w_text;
	}

	String str_from_utf16(wchar_t* str_utf16, Allocator* allocator) {
		if (*str_utf16 == 0) return (String){0};
		
		int length = WideCharToMultiByte(CP_UTF8, 0, str_utf16, -1, NULL, 0, NULL, NULL);
		if (length <= 0) return (String) { 0 };

		String result = make_str(length, allocator); // length includes the null-termination.
		int length2 = WideCharToMultiByte(CP_UTF8, 0, str_utf16, -1, result.data, (int)result.len, NULL, NULL);
		if (length2 <= 0) return (String) { 0 };
		
		result.len--;
		return result;
	}

	//void os_allocator_proc(Slice<u8>* allocation, AllocatorMode mode, i64 size, i64 alignment, void* allocator_data, Code_Location loc) {
	//	switch (mode) {
	//	case AllocatorMode_VirtualReserve:
	//		assert(allocation->data == NULL);
	//		allocation->len = size; // we could give more space to this!
	//		allocation->data = (u8*)VirtualAlloc(NULL, size, MEM_RESERVE, PAGE_READWRITE);
	//		return;
	//
	//	case AllocatorMode_VirtualRelease:
	//		return;
	//	case AllocatorMode_Alloc:
	//		return;
	//	case AllocatorMode_Free:
	//		return;
	//	}
	//	assert(false);
	//}

	void os_error_message(String title, String message) {
		u8 buf[4096]; // we can't use temp_push/temp_pop here, because we might be reporting an error about running over the temporary arena
		Arena* stack_arena = arena_make_using_buffer_fixed(buf, sizeof(buf));
		uint _;
		wchar_t* title_utf16 = str_to_utf16(title, &stack_arena->alc, 1, &_);
		wchar_t* message_utf16 = str_to_utf16(message, &stack_arena->alc, 1, &_);

		MessageBoxW(0, message_utf16, title_utf16, MB_OK);
		arena_free(stack_arena);
	}

	OPT(u8*) os_mem_reserve(u64 size, OPT(void*) address) {
		OPT(u8*) ptr = (u8*)VirtualAlloc(address, size, MEM_RESERVE, PAGE_READWRITE);
		return ptr;
	}
	
	void os_mem_commit(u8* ptr, u64 size) {
		VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE);
	}

	void os_mem_decommit(u8* ptr, u64 size) {
		VirtualFree(ptr, size, MEM_DECOMMIT);
	}

	void os_mem_release(u8* ptr) {
		VirtualFree(ptr, 0, MEM_RELEASE);
	}

	bool os_set_working_dir(String dir) {
		Allocator* temp = temp_push();
		uint _;
		BOOL ok = SetCurrentDirectoryW(str_to_utf16(dir, temp, 1, &_));
		temp_pop();
		return ok;
	}

	String os_get_working_dir(Allocator* allocator) {
		wchar_t buf[MAX_PATH];
		buf[0] = 0;
		GetCurrentDirectoryW(MAX_PATH, buf);
		return str_from_utf16(buf, allocator);
	}

	String os_clipboard_get_text(Allocator* allocator) {
		String text = (String){0};
		if (OpenClipboard(NULL)) {
			HANDLE hData = GetClipboardData(CF_UNICODETEXT);
			
			u8* buffer = (u8*)GlobalLock(hData);
			
			int length = WideCharToMultiByte(CP_UTF8, 0, (wchar_t*)hData, -1, NULL, 0, NULL, NULL);
			if (length > 0) {
				text = make_str((uint)length, allocator);
				WideCharToMultiByte(CP_UTF8, 0, (wchar_t*)hData, -1, (char*)text.data, length, NULL, NULL);
				text.len -= 1;
				ASSERT(text.data[text.len] == 0);
			}

			GlobalUnlock(hData);
			CloseClipboard();
		}
		return text;
	}

	void os_clipboard_set_text(String text) {
		//ZoneScoped;
		if (!OpenClipboard(NULL)) return;

		Allocator* temp = temp_push();
		{
			EmptyClipboard();

			//int h = 0;
			//int length = MultiByteToWideChar(CP_UTF8, 0, (const char*)text.data, (int)text.len, NULL, 0);
			//
			//u8* utf16 = MakeSlice(u8, (uint)length * 2 + 2, temp);
			//
			//ASSERT(MultiByteToWideChar(CP_UTF8, 0, (const char*)text.data, (int)text.len, (wchar_t*)utf16.data, length) == length);
			//((u16*)utf16.data)[length] = 0;
			uint utf16_len;
			wchar_t* utf16 = str_to_utf16(text, temp, 1, &utf16_len);


			HANDLE clipbuffer = GlobalAlloc(0, utf16_len * 2 + 2);
			u8* buffer = (u8*)GlobalLock(clipbuffer);
			memcpy(buffer, utf16, utf16_len * 2 + 2);
		
			GlobalUnlock(clipbuffer);
			SetClipboardData(CF_UNICODETEXT, clipbuffer);

			CloseClipboard();
		}
		temp_pop();
	}

	bool os_directory_exists(String path) {
		Allocator* temp = temp_push();
		
		uint path_utf16_len;
		wchar_t* path_utf16 = str_to_utf16(path, temp, 1, &path_utf16_len);
		DWORD dwAttrib = GetFileAttributesW(path_utf16);

		temp_pop();
		return (dwAttrib != INVALID_FILE_ATTRIBUTES && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
	}

	bool os_path_is_absolute(String path) {
		return path.len > 2 && path.data[1] == ':';
	}

	String os_path_to_absolute(String working_dir, String path, Allocator* alc) {
		Allocator* temp = temp_push();
		
		String working_dir_before;
		if (working_dir.len > 0) {
			working_dir_before = os_get_working_dir(temp);
			os_set_working_dir(working_dir);
		}

		uint path_utf16_len;
		wchar_t* path_utf16 = str_to_utf16(path, temp, 1, &path_utf16_len);
		
		wchar_t buf[MAX_PATH + 1];
		DWORD length = GetFullPathNameW(path_utf16, MAX_PATH, buf, NULL);
		
		String result = {0};
		if (length > 0 && length <= MAX_PATH) {
			result = str_from_utf16(buf, alc);
		}
		
		if (working_dir.len > 0) {
			os_set_working_dir(working_dir_before);
		}

		temp_pop();
		return result;
	}

	bool os_visit_directory(String path, OS_VisitDirectoryVisitor visitor, void* visitor_userptr) {
		Allocator* temp = temp_push();

		String match_str = make_str(path.len + 2, temp);
		mem_copy(match_str.data, path.data, path.len);
		match_str.data[path.len] = '\\';
		match_str.data[path.len+1] = '*';

		uint match_str_utf16_len;
		wchar_t* match_str_utf16 = str_to_utf16(match_str, temp, 1, &match_str_utf16_len);

		WIN32_FIND_DATAW find_info;
		HANDLE handle = FindFirstFileW(match_str_utf16, &find_info);
		if (handle == INVALID_HANDLE_VALUE) return false;

		for (; FindNextFileW(handle, &find_info);) {
			OS_VisitDirectoryInfo info = {
				.name = str_from_utf16(find_info.cFileName, temp),
				.is_directory = find_info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY,
			};
			
			if (str_equals(info.name, LIT(".."))) continue;
			
			OS_VisitDirectoryResult result = visitor(&info, visitor_userptr);
		}

		bool ok = GetLastError() == ERROR_NO_MORE_FILES;
		FindClose(handle);
		
		temp_pop();
		return ok;
	}

	bool os_delete_directory(String path) {
		if (!os_directory_exists(path)) return true;

		Allocator* temp = temp_push();
		uint path_utf16_len;

		// NOTE: path must be double null-terminated!
		wchar_t* path_utf16 = str_to_utf16(path, temp, 2, &path_utf16_len);
		
		SHFILEOPSTRUCTW file_op = {
			.hwnd = NULL,
			.wFunc = FO_DELETE,
			.pFrom = path_utf16,
			.pTo = NULL,
			.fFlags = FOF_NO_UI,
			.fAnyOperationsAborted = false,
			.hNameMappings = 0,
			.lpszProgressTitle = NULL,
		};

		int result = SHFileOperationW(&file_op);

		temp_pop();
		return result == 0;
	}

	bool os_make_directory(String path) {
		Allocator* temp = temp_push();
		
		uint path_utf16_len;
		wchar_t* path_utf16 = str_to_utf16(path, temp, 1, &path_utf16_len);

		BOOL ok = CreateDirectoryW(path_utf16, NULL);
		temp_pop();
		
		if (ok) return true;

		DWORD err = GetLastError();
		if (err == ERROR_ALREADY_EXISTS) return true;

		return false;
	}

	bool os_file_read_whole(String filepath, Allocator* allocator, String* out_str) {
		//ZoneScoped;
		File file = os_file_open(filepath, OS_FileOpenMode_Read);
		if (!os_file_is_valid(file)) return false;

		uint size = os_file_size(file);
		
		String result = make_str(size, allocator);

		ASSERT(os_file_read(file, result.data, size) == size);

		os_file_close(file);
		*out_str = result;
		return true;
	}

	static s64 mul_div_u64(s64 val, s64 num, s64 den) {
		//ZoneScoped;
		// Implementation taken from the odin core library @license_todo
		s64 q = val / den;
		s64 r = val % den;
		return q * num + r * num / den;
	}

	Tick time_get_tick() {
		//ZoneScoped;
		// Implementation taken from the odin core library @license_todo
		LARGE_INTEGER freq, now;
		ASSERT(QueryPerformanceFrequency(&freq) == TRUE);
		ASSERT(QueryPerformanceCounter(&now) == TRUE);

		return mul_div_u64(now.QuadPart, 1000000000, freq.QuadPart);
	}

	bool os_file_is_valid(File file) { return file._handle != 0; }
	
	File os_file_open(String filepath, OS_FileOpenMode mode) {
		HANDLE handle;

		Allocator* temp = temp_push();
		uint filepath_utf16_len;
		wchar_t* filepath_utf16 = str_to_utf16(filepath, temp, 1, &filepath_utf16_len);

		if (mode == OS_FileOpenMode_Read) {
			handle = CreateFileW(filepath_utf16, FILE_GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
		}
		else {
			u32 creation = mode == OS_FileOpenMode_Append ? OPEN_ALWAYS : CREATE_ALWAYS;
			handle = CreateFileW(filepath_utf16, FILE_GENERIC_READ|FILE_GENERIC_WRITE, FILE_SHARE_READ, NULL, creation, 0, NULL);
		}

		if (handle == INVALID_HANDLE_VALUE) handle = 0;
		else leak_tracker_begin_entry(handle, 1);

		temp_pop();
		return (File){ handle };
	}

	uint os_file_read(File file, void* dst, uint size) {
		if (dst == NULL) return 0;
		if (size <= 0) return 0;

		for (uint read_so_far = 0; read_so_far < size;) {
			uint remaining = size - read_so_far;
			u32 to_read = remaining >= U32_MAX ? U32_MAX : (u32)remaining;

			DWORD bytes_read;
			BOOL ok = ReadFile(file._handle, (u8*)dst + read_so_far, to_read, &bytes_read, NULL);
			read_so_far += bytes_read;
			
			if (ok != TRUE || bytes_read < to_read) {
				return read_so_far;
			}
		}
		return size;
	}

	uint os_file_size(File file) {
		LARGE_INTEGER size;
		if (GetFileSizeEx(file._handle, &size) != TRUE) return -1;
		return size.QuadPart;
	}
	
	bool os_file_write(File file, String data) {
		if (data.len >= U32_MAX) return false; // TODO: writing files greater than 4 GB
		
		DWORD bytes_written;
		return WriteFile(file._handle, data.data, (DWORD)data.len, &bytes_written, NULL) == TRUE && bytes_written == data.len;
	}

	uint os_file_get_position(File file) {
		LARGE_INTEGER offset;
		if (SetFilePointerEx(file._handle, (LARGE_INTEGER){0}, &offset, FILE_CURRENT) != TRUE) return -1;
		return offset.QuadPart;
	}

	bool os_file_set_position(File file, uint position) {
		LARGE_INTEGER offset;
		offset.QuadPart = position;
		return SetFilePointerEx(file._handle, offset, NULL, FILE_BEGIN) == TRUE;
	}

	bool os_file_close(File file) {
		bool ok = CloseHandle(file._handle) == TRUE;
		leak_tracker_end_entry(file._handle);
		return ok;
	}

	void get_stack_trace(void(*visitor)(String function, String file, u32 line, void* user_ptr), void* user_ptr) {
		HANDLE process = GetCurrentProcess();
		
		// This is a bit sloppy and technically incorrect...
		static bool has_called_sym_initialize = false;
		if (!has_called_sym_initialize) {
			SymInitialize(process, NULL, true);
		}

		CONTEXT ctx;
		RtlCaptureContext(&ctx);

		STACKFRAME64 stack_frame = (STACKFRAME64){
			.AddrPC.Offset = ctx.Rip,
			.AddrPC.Mode = AddrModeFlat,
			.AddrFrame.Offset = ctx.Rsp,
			.AddrFrame.Mode = AddrModeFlat,
			.AddrStack.Offset = ctx.Rsp,
			.AddrStack.Mode = AddrModeFlat,
		};

		for (uint i = 0; i < 64; i++) {
			if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, GetCurrentThread(), &stack_frame, &ctx,
				NULL, SymFunctionTableAccess64, SymGetModuleBase64, NULL)) {
				// Maybe it failed, maybe we have finished walking the stack.
				break;
			}

			if (stack_frame.AddrPC.Offset == 0) break;
			if (i == 0) continue; // ignore this function

			struct {
				IMAGEHLP_SYMBOL64 s;
				u8 name_buf[64];
			} sym;
			
			sym.s.SizeOfStruct = sizeof(IMAGEHLP_SYMBOL64);
			sym.s.MaxNameLength = 64;

			if (SymGetSymFromAddr(process, stack_frame.AddrPC.Offset, NULL, &sym.s) != TRUE) break;

			String function_name = str_from_cstring(sym.s.Name);
			
			IMAGEHLP_LINE64 Line64;
			Line64.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
			DWORD dwDisplacement;
			if (SymGetLineFromAddr64(process, ctx.Rip, &dwDisplacement, &Line64) != TRUE) break;
			
			visitor(function_name, str_from_cstring(Line64.FileName), Line64.LineNumber, user_ptr);
			
			if (str_equals(function_name, LIT("main"))) break; // Don't care about anything beyond main
		}
	}

	bool os_run_command(SliceRaw args, String working_dir) {
		Allocator* temp = temp_push();
		String working_dir_before;
		if (working_dir.len > 0) {
			working_dir_before = os_get_working_dir(temp);
			os_set_working_dir(working_dir);
		}
		
		// Windows expects a single space-separated string that encodes a list of the passed command-line arguments.
		// In order to support spaces within an argument, we must enclose it with quotation marks (").
		// https://learn.microsoft.com/en-us/cpp/c-language/parsing-c-command-line-arguments?redirectedfrom=MSDN&view=msvc-170

		String* arg_strings = args.data;
		ArrayRaw cmd_string = make_array_raw(temp);

		for (uint i = 0; i < args.len; i++) {
			String arg = arg_strings[i];
			
			u8 quotation = '\"';
			array_push_raw(&cmd_string, &quotation, 1);
			
			for (uint j = 0; j < arg.len; j++) {
				if (arg.data[j] == '\"') {
					array_push_raw(&cmd_string, &quotation, 1); // escape quotation mark with another quotation mark
				}
				array_push_raw(&cmd_string, &arg.data[j], 1);
			}

			array_push_raw(&cmd_string, &quotation, 1);
			
			if (i < args.len - 1) array_push_raw(&cmd_string, &(u8){' '}, 1); // Separate each argument with a space
		}

		uint cmd_string_utf16_len;
		wchar_t* cmd_string_utf16 = str_to_utf16((String) { cmd_string.data, cmd_string.len }, temp, 1, & cmd_string_utf16_len);

		PROCESS_INFORMATION process_info = { 0 };

		STARTUPINFOW startup_info = { 0 };
		startup_info.cb = sizeof(STARTUPINFOW);

		HANDLE IN_Rd = NULL;
		HANDLE IN_Wr = NULL;
		HANDLE OUT_Rd = NULL;
		HANDLE OUT_Wr = NULL;

		// Initialize pipes
		SECURITY_ATTRIBUTES security_attrs = {
			.nLength = sizeof(SECURITY_ATTRIBUTES),
			.bInheritHandle = TRUE,
			.lpSecurityDescriptor = NULL,
		};

		if (!CreatePipe(&OUT_Rd, &OUT_Wr, &security_attrs, 0)) goto end;
		if (!SetHandleInformation(OUT_Rd, HANDLE_FLAG_INHERIT, 0)) goto end;
			
		if (!CreatePipe(&IN_Rd, &IN_Wr, &security_attrs, 0)) goto end;
		if (!SetHandleInformation(IN_Rd, HANDLE_FLAG_INHERIT, 0)) goto end;

		// TODO: capture output
		//startup_info.hStdError = OUT_Wr;
		//startup_info.hStdOutput = OUT_Wr;
		//startup_info.hStdInput = IN_Rd;
		//startup_info.dwFlags |= STARTF_USESTDHANDLES;

		//char* cmd_line[] = { "C:\\Program Files\\Notepad++\\notepad++.exe" };
		//wchar_t cmd_line[] = TEXT("C:\\Program Files\\Notepad++\\notepad++.exe");
		//wchar_t* cmd_line = TEXT("libtrans.exe");

		if (!CreateProcessW(NULL,
			cmd_string_utf16, // command line
			NULL,             // process security attributes 
			NULL,             // primary thread security attributes 
			TRUE,             // handles are inherited 
			0,                // creation flags 
			NULL,             // use parent's environment 
			NULL,             // use parent's current directory 
			&startup_info,    // STARTUPINFO pointer 
			&process_info     // receives PROCESS_INFORMATION 
		)) goto end;

		CloseHandle(process_info.hProcess);
		CloseHandle(process_info.hThread);
		
		// Close the handles to the pipe ends that are in the child processes hands - we don't need them.
		CloseHandle(OUT_Wr);
		CloseHandle(IN_Rd);

		// Close our own handles
		CloseHandle(IN_Wr);
		CloseHandle(OUT_Rd);

	end:;
		if (working_dir.len > 0) {
			os_set_working_dir(working_dir_before);
		}

		temp_pop();
		return true;
	}


#endif // OS_WINDOWS

String str_advance(String* str, uint len) {
	String result = str_slice_before(*str, len);
	*str = str_slice_after(*str, len);
	return result;
}

bool os_file_write_whole(String filepath, String data) {
	//ZoneScoped;

	File file = os_file_open(filepath, OS_FileOpenMode_Write);
	if (!os_file_is_valid(file)) return false;
	if (!os_file_write(file, data)) return false;

	os_file_close(file);
	return true;
}

void temp_init() {
	ASSERT(_temp_arena_keep_alive == false); // temp_init can only be called once!
	_temp_arena_keep_alive = true;
}

void temp_deinit() {
	ASSERT(_temp_arena_scope_counter == 0);
	ASSERT(_temp_arena_keep_alive);
	
	_temp_arena_keep_alive = false;
	if (_temp_arena) {
		arena_free(_temp_arena);
		_temp_arena = NULL;
	}
}

Allocator* temp_push() {
	if (_temp_arena == NULL) {
		ASSERT(_temp_arena_scope_counter == 0);
		// Allocate temp arena at a deterministic memory address
		_temp_arena = arena_make_virtual_reserve_fixed(GiB(1), (void*)TiB(2));
	}
	
	_temp_arena_scope_counter += 1;
	return &_temp_arena->alc;
}

void temp_pop() {
	//ASSERT(_temp_arena_scope_counter == temp.scope_counter);
	_temp_arena_scope_counter -= 1;
	if (_temp_arena_scope_counter == 0) {
		if (_temp_arena_keep_alive) {
			arena_clear(_temp_arena);
		}
		else {
			arena_free(_temp_arena);
			_temp_arena = NULL;
		}
	}
}

rune str_rune_to_lower(rune r) { // TODO: utf8
	return r >= 'A' && r <= 'Z' ? r + 32 : r;
}

String str_to_lower(String str, Allocator* a) {
	String out = str_clone(str, a);
	for (uint i = 0; i < out.len; i++) {
		out.data[i] = str_rune_to_lower(out.data[i]);
	}
	return out;
}


/* ============ LICENSES ============

@DionSystems:

	Copyright 2021 Dion Systems LLC

	Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/