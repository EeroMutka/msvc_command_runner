#pragma once

// Thin wrapper over the foundation C-API (foundation.h), providing some useful C++ features,
// such as visual studio natvis support to let you visualize slices and maps in the debugger.

#define C_ARRAY_SLICE(x) {x, sizeof(x) / sizeof(x[0])}

struct _Allocator;
typedef unsigned long long uint;
typedef unsigned int u32;

template<typename T>
struct Slice {
	T* data;
	uint len;

	inline T& operator [] (uint i) {
#ifdef _DEBUG
		if (i >= len) __debugbreak();
#endif
		return data[i];
	}

	inline uint size_bytes() { return len * sizeof(T); }
};

// NOTE: Must have the same binary layout as Array_Raw
template<typename T>
struct Array {
	union {
		struct {
			T* data;
			uint len;
		};
		Slice<T> slice;
	};
	uint capacity;
	
	_Allocator* allocator;

	inline T& operator [] (uint i) {
		if (i >= len) __debugbreak();
		return data[i];
	}
};

// NOTE: Must have the same binary layout as Map64_Raw
template<typename T>
struct Map64 {
	_Allocator* alc;
	u32 value_size;
	u32 alive_count;
	u32 slot_count; // visual studio natvis doesn't support bitshifts, so in order to visualize it we need this here. But it might be good to store nonetheless.
	u32 slot_count_log2;
	void* slots;
};

#define String Slice<u8>
#define Array(T) Array<T>
#define Slice(T) Slice<T>
#define Map64(T) Map64<T>

// We want to give structs that include templated fields different link-names between C/C++ versions,
// because otherwise the visual studio debugger might think to display the C version without natvis support.
#define LeakTracker LeakTracker_cpp

template<typename T>
inline T* mem_clone(const T& value, struct _Allocator* allocator) { return (T*)mem_clone_size(sizeof(T), &value, allocator); }

// include the foundation C-api
#include "foundation.h"

#undef mem_clone

// The reason we have to #define and can't just typedef in the first place,
// is that then the C++ compiler wouldn't allow us to compile our program, which is dumb.
typedef Slice<u8> String;

inline bool operator == (String a, String b) { return str_equals(a, b); }
inline bool operator != (String a, String b) { return !str_equals(a, b); }

template<typename T>
inline Slice<T> make_slice_garbage(uint len, Allocator* alc) {
	return Slice<T>{ (T*)(alc)->proc((alc), NULL, 0, (len) * sizeof(T), ALIGN_OF(T)), len };
}

template<typename T>
inline Slice<T> make_slice(uint len, const T& initial_value, Allocator* alc) {
	Slice<T> result = make_slice_garbage<T>(len, alc);
	for (uint i = 0; i < len; i++) result[i] = initial_value;
	return result;
}

//#define resize_slice(allocation, new_len, allocator) _MemResize((allocation), (new_len), (allocator))
//#define delete_slice(allocation, allocator) _MemFree(allocation, allocator)

//template<typename T>
//inline T* arena_push_value(Arena* arena, const T& value) { return (T*)arena_push(arena, AS_BYTES(value), ALIGN_OF(T)); }

template<typename T>
inline Slice<T> clone_slice(Slice<T> x, Allocator* allocator) {
	return Slice<T>{ (T*)mem_clone_size(x.len * sizeof(T), x.data, allocator), x.len };
}

template<typename T>
inline Slice<T> slice(Array<T> other, uint lo, uint hi) {
	ASSERT(hi >= lo);
	return Slice<T>{ other.data + lo, hi - lo };
}

template<typename T>
inline Slice<T> slice(Slice<T> other, uint lo, uint hi) {
	ASSERT(hi >= lo);
	return Slice<T>{ other.data + lo, hi - lo };
}

template<typename T>
inline Slice<T> slice_before(Array<T> other, uint mid) {
	return Slice<T>{ other.data, mid};
}

template<typename T>
inline Slice<T> slice_before(Slice<T> other, uint mid) {
	return Slice<T>{ other.data, mid };
}

template<typename T>
inline Slice<T> slice_after(Array<T> other, uint mid) {
	ASSERT(other.len >= mid);
	return Slice<T>{ other.data + mid, other.len - mid};
}

template<typename T>
inline Slice<T> slice_after(Slice<T> other, uint mid) {
	ASSERT(other.len >= mid);
	return Slice<T>{ other.data + mid, other.len - mid};
}

template<typename T>
inline void slice_set(Slice<T> dst, T value) {
	//ZoneScoped;
	for (uint i = 0; i < dst.len; i++) {
		dst.data[i] = value;
	}
}

template<typename T>
inline void slice_copy(Slice<T> dst, Slice<T> src) {
	ASSERT(src.len <= dst.len);
	mem_copy(dst.data, src.data, src.len * sizeof(T));
}

#define MAP64_EACH(map, key, value_ptr) MAP64_EACH_RAW((Map64Raw*)map, key, (void**)value_ptr)

template<typename T>
inline Map64<T> make_map64(Allocator* alc) { return BITCAST(Map64<T>, map64_make_raw(sizeof(T), alc)); }

template<typename T>
inline Map64<T> make_map64_cap(uint capacity, Allocator* alc) { return BITCAST(Map64<T>, make_map64_cap_raw(sizeof(T), capacity, alc)); }

template<typename T>
inline void free_map64(Map64<T>* map) { return free_map64_raw((Map64Raw*)map); }

template<typename T>
inline void resize_map64(Map64<T>* map, u32 slot_count_log2) { resize_map64_raw((Map64Raw*)map, slot_count_log2); }

template<typename T>
struct MapInsertResult_cpp { T* _unstable_ptr; bool added; };
STATIC_ASSERT(sizeof(MapInsertResult_cpp<void>) == sizeof(MapInsertResult));

template<typename T>
inline MapInsertResult_cpp<T> map64_insert(Map64<T>* map, u64 key, const T& value, MapInsert mode = MapInsert_AssertUnique) {
	return BITCAST(MapInsertResult_cpp<T>, map64_insert_raw((Map64Raw*)map, key, &value, mode));
}

template<typename T>
bool map64_remove(Map64<T>* map, u64 key) { return map64_remove_raw((Map64Raw*)map, key); }

template<typename T>
inline OPT(T*) map64_get(Map64<T>* map, u64 key) { return (T*)map64_get_raw((Map64Raw*)map, key); }



template<typename T>
inline Array<T> make_array(Allocator* alc) { return BITCAST(Array<T>, make_array_raw(alc)); }

template<typename T>
inline Array<T> make_array_len(uint len, const T& initial_value, Allocator* alc) {
	return BITCAST(Array<T>, make_array_len_raw(sizeof(T), len, &initial_value, alc));
}

template<typename T>
inline Array<T> make_array_len_garbage(uint len, Allocator* alc) {
	return BITCAST(Array<T>, make_array_len_garbage_raw(sizeof(T), len, alc));
}

template<typename T>
inline Array<T> make_array_cap(uint capacity, Allocator* alc) {
	return BITCAST(Array<T>, make_array_cap_raw(sizeof(T), capacity, alc));
}

template<typename T>
inline void free_array(Array<T>* array) { free_array_raw((ArrayRaw*)array, sizeof(T)); }

template<typename T>
inline void array_resize(Array<T>* array, uint len, const T& value) { array_resize_raw((ArrayRaw*)array, len, &value, sizeof(T)); }

template<typename T>
inline void array_resize_garbage(Array<T>* array, uint len) { array_resize_raw((ArrayRaw*)array, len, NULL, elem_size); }

template<typename T>
inline uint array_push(Array<T>* array, const T& elem) { return array_push_raw((ArrayRaw*)array, &elem, sizeof(T)); }

template<typename T>
inline void array_push_slice(Array<T>* array, Slice<T> elems) {
	array_push_slice_raw((ArrayRaw*)array, BITCAST(SliceRaw, elems), sizeof(T));
}

template<typename T>
inline T array_pop(Array<T>* array) {
	T elem;
	array_pop_raw((ArrayRaw*)array, &elem, sizeof(T));
	return elem;
}

template<typename T>
inline T& array_peek(Array<T>* array) {
	ASSERT(array->len > 0);
	return array->data[array->len - 1];
}

#include <initializer_list>

#define STR_JOIN(alc, ...) str_join_il(alc, {__VA_ARGS__})
inline String str_join_il(Allocator* alc, std::initializer_list<String> args) {
	return str_join(alc, { (String*)args.begin(), args.size() });
}

inline void str_print_il(Array<u8>* buffer, std::initializer_list<String> args) {
	for (auto arg : args) str_print(buffer, arg);
}

//
//////////////////////////////////////////////////////////////////
//
// Defer statement, taken from https://github.com/gingerBill/gb/blob/master/gb.h
// Akin to D's SCOPE_EXIT or
// similar to Go's defer but scope-based
//
// NOTE: C++11 (and above) only!
//
extern "C++" {
	// NOTE(bill): Stupid fucking templates
	template <typename T> struct gbRemoveReference { typedef T ffzType; };
	template <typename T> struct gbRemoveReference<T&> { typedef T ffzType; };
	template <typename T> struct gbRemoveReference<T&&> { typedef T ffzType; };

	/// NOTE(bill): "Move" semantics - invented because the C++ committee are idiots (as a collective not as indiviuals (well a least some aren't))
	template <typename T> inline T&& gb_forward(typename gbRemoveReference<T>::ffzType& t) { return static_cast<T&&>(t); }
	template <typename T> inline T&& gb_forward(typename gbRemoveReference<T>::ffzType&& t) { return static_cast<T&&>(t); }
	template <typename T> inline T&& gb_move(T&& t) { return static_cast<typename gbRemoveReference<T>::ffzType&&>(t); }
	template <typename F>
	struct gbprivDefer {
		F f;
		gbprivDefer(F&& f) : f(gb_forward<F>(f)) {}
		~gbprivDefer() { f(); }
	};
	template <typename F> gbprivDefer<F> gb__defer_func(F&& f) { return gbprivDefer<F>(gb_forward<F>(f)); }

#define GB_DEFER_1(x, y) x##y
#define GB_DEFER_2(x, y) GB_DEFER_1(x, y)
#define GB_DEFER_3(x)    GB_DEFER_2(x, __COUNTER__)
#define defer(code)      auto GB_DEFER_3(_defer_) = gb__defer_func([&]()->void{code;})
}
////////////////////////////////////////////////////////////////
//
//
//
//

/*
#define DECLARE_COMPARISON(T, expr) \
	inline bool operator==(const T& a, const T& b) { return expr; } \
	inline bool operator!=(const T& a, const T& b) { return !(expr); }

#define DECLARE_HASHER(T, expr) template<> struct Hasher<T> { inline u64 get(const T& x) { return (expr); } };

#define DECLARE_POD_STRUCT(T, elements) \
	__ACTIVATE_MANUAL_STRUCT_PADDING \
	struct T {elements;}; \
	DECLARE_COMPARISON(T, memcmp(&a, &b, sizeof(T)) == 0) \
	DECLARE_HASHER(T, MeowU64From(MeowHash(MeowDefaultSeed, sizeof(T), (void*)&x), 0)) \
	__RESTORE_MANUAL_STRUCT_PADDING

template<typename T>
struct Hasher {
	inline u64 get(const T& k) {
		ASSERT(false); // hash is not defined anywhere for this `T`
		return 0;
	}
};

#define HASHER_GET(T, x) (Hasher<T>{}.get(x))

template<> struct Hasher<u8> { inline u64 get(u8 x) { return HASH_U8(x); } };
template<> struct Hasher<u16> { inline u64 get(u16 x) { return HASH_U16(x); } };
template<> struct Hasher<u32> { inline u64 get(u32 x) { return HASH_U32(x); } };
template<> struct Hasher<u64> { inline u64 get(u64 x) { return HASH_U64(x); } };
template<> struct Hasher<s8> { inline u64 get(s8 x) { return HASH_U8((u8)x); } };
template<> struct Hasher<s16> { inline u64 get(s16 x) { return HASH_U16((u16)x); } };
template<> struct Hasher<s32> { inline u64 get(s32 x) { return HASH_U32((u32)x); } };
template<> struct Hasher<s64> { inline u64 get(s64 x) { return HASH_U64((u64)x); } };

template<typename T>
struct Hasher<T*> {
	inline u64 get(T* x) { return HASH_U64((u64)x); }
};

template<typename T> struct Hasher<Slice<T>> {
	inline u64 get(Slice<T> x) {
		u64 seed = 0xf778ac35da8c86f4;
		for (uint i = 0; i < x.len; i++) {
			u64 elem_hash = Hasher<T>().get(x.data[i]);
			seed = (seed * 0x01000193) ^ elem_hash;
		}
		return seed;
	}
};

template<typename T> struct Hasher<Array<T>> {
	inline u32 get(Array<T> x) { return HASHER_GET(Slice<T>, x.slice); }
};


template<typename T>
inline bool SliceIterateCondition(Slice<T> const& slice, uint* idx, T* ptr) {
	if (slice.len == 0)
		return false;

	*ptr = slice.data[*idx];
	return *idx < slice.len;
}

template<typename T>
inline bool SliceIterateCondition(Slice<T> const& slice, uint* idx, T** ptr) {
	*ptr = slice.data + *idx;
	return *idx < slice.len;
}


//
// In order to use an array or map, you must first manually initialize it by
// calling a InitArray/Map function. But why? Why not (A.) use constructors/destructors,
// or (B.) initialize it implicitly when inserting elements to it, the way for
// example jai and odin does it?
// 
// A. There are multiple reasons for this. The biggest reason is that having the
//    code be explicit is very useful. If we ban constructors and destructors,
//    the reader can safely assume that no code is happening behind their back when
//    reading a function. You want to know what your function is doing anyway,
//    so why not just type it right in so it's obvious to everyone? Another
//    reason is that this way we can treat *everything* as POD (plain old data)
//    structs, which is really nice. For example, copying an array implies no
//    hidden performance cost - you must manually copy the array, which also
//    makes the intent over the ownership of the data more clear. We also avoid
//    getting sucked into the rabbit hole of C++ where for every struct you're
//    now required to write constructors, destructors, copy constructors, move
//    constructors, etc etc, if you want your code to be "correct". No thanks :)
//    Of course, some might argue that using C++ smart pointers would solve this,
//    but there are good arguments against those too.
//    https://floooh.github.io/2018/06/17/handles-vs-pointers.html
//    https://www.rfleury.com/p/untangling-lifetimes-the-arena-allocator
//
// B. There are multiple reasons for this too. The first reason is that we don't
//    have an implicit "context" system, simply because I've found it unnecessary.
//    Again, the goal is to be explicit and follow the principle of least surprise.
//    For example, if you want to pass an allocator around, it's better to pass it as a
//    parameter, or store it inside of a context struct for your specific problem
//    that you pass around instead. That way, it will be obvious when a function
//    or a subsystem needs to allocate memory, and where exactly it happens.
// 
//    Since there's no implicit "context allocator", like in jai/odin, you must
//    explicitly provide an allocator to all functions that allocate memory.
//    Same goes for things like loggers. If we didn't have the InitArray/Map
//    functions, it'd be required to pass an allocator to all functions that
//    operate on the container, such as when array_push. Having a single InitArray/Map
//    where the allocator for the container is stored is a lot nicer. This
//    also gets rid of the bug-prone scenario where you're accidentally inserting
//    to a container from inside some code that uses an implicit context allocator
//    that's different from what the container is supposed to use. If this happens,
//    the (wrong) context allocator is assigned to the container, and things go south.
// 
//    Another benefit of having an explicit Make function is that the leak
//    tracker can pinpoint leaks to the codepath of where the array/map was
//    initialized, instead of some random piece of code that operated on the
//    container and was the first one to insert elements to it. (See LeakTracker)
//    
//    Plus, it's nice to be symmetric, since you're also required to call DestroyArray/Map!
//
template<typename T>
inline void InitArrayCap(Array<T>* arr, uint capacity, Allocator* allocator) {
	ZoneScoped;
	arr->allocator = allocator;
	arr->data = MakeSlice(T, capacity, allocator).data; // TODO: delay the reserve/capacity until you actually push to the array. We can still store the capacity
	arr->len = 0;
	arr->capacity = capacity;
}

template<typename T>
inline void InitArrayLen(Array<T>* arr, uint len, const T& value, Allocator* allocator) {
	ZoneScoped;
	arr->allocator = allocator;
	arr->data = MakeSlice(T, len, allocator).data;
	arr->len = len;
	arr->capacity = len;

	for (uint i = 0; i < len; i++) {
		arr->data[i] = value;
	}
}

template<typename T>
inline void InitArrayLenGarbage(Array<T>* arr, uint len, Allocator* allocator) {
	ZoneScoped;
	arr->allocator = allocator;
	arr->data = MakeSlice(T, len, allocator).data;
	arr->len = len;
	arr->capacity = len;
}

template<typename T>
inline void ArrayReserve(Array<T>* arr, uint capacity) {
	ArrayReserveRaw((RawArray*)arr, sizeof(T), capacity);
}

template<typename T>
inline void array_resize(Array<T>* arr, uint len, T value) {
	ZoneScoped;
	ArrayReserve(arr, len);

	for (uint i = arr->len; i < len; i++) {
		arr->data[i] = value;
	}

	arr->len = len;
}

template<typename T>
inline void ArrayResizeGarbage(Array<T>* arr, uint len) {
	ZoneScoped;
	ArrayReserve(arr, len);
	arr->len = len;
}

template<typename T>
inline void DestroyArray(Array<T> arr) {
	ZoneScoped;
	Slice<u8> allocation = { (u8*)arr.data, arr.capacity * sizeof(T) };
	mem_release(allocation, arr.allocator);
}

template<typename T>
inline void ArrayClear(Array<T>* arr) {
	ZoneScoped;
#ifdef _DEBUG
	memset(arr->data, 0xCC, arr->len * sizeof(T)); // debug; trigger data-breakpoints
#endif
	arr->len = 0;
}

template<typename T>
inline void array_push(Array<T>* arr, T elem) {
	ZoneScoped;
	if (arr->capacity < arr->len + 1) {
		// needs to grow!
		uint doubled_cap = 2 * arr->capacity;
		ArrayReserve(arr, MAX(doubled_cap, 8));
	}

	arr->data[arr->len] = elem;
	arr->len += 1;
}

template<typename T>
inline void array_pop(Array<T>* arr) { ASSERT(arr->len > 0); arr->len--; }

template<typename T>
inline void ArrayInsert(Array<T>* arr, uint position, T elem) {
	ZoneScoped;
	if (arr->capacity < arr->len + 1) {
		// needs to grow!
		uint doubled_cap = MAX(8, 2 * arr->capacity);
		ArrayReserve(arr, doubled_cap);
	}

	for (uint i = arr->len; i >= position; i--) {
		arr->data[i] = arr->data[i - 1];
	}
	arr->data[position] = elem;
	arr->len += 1;
}

template<typename T>
inline void ArrayInsertSlice(Array<T>* arr, uint position, Slice<T> elems) {
	ZoneScoped;
	if (arr->capacity < arr->len + elems.len) {
		// needs to grow!
		uint doubled_cap = MAX(8, 2 * arr->capacity + elems.len);
		ArrayReserve(arr, MAX(doubled_cap, arr->len + elems.len));
	}

	for (uint i = arr->len + elems.len - 1; i >= position; i--) {
		arr->data[i] = arr->data[i - elems.len];
	}
	memcpy(arr->data + position, elems.data, elems.len * sizeof(T));
	arr->len += elems.len;
}

template<typename T>
inline void ArrayRemove(Array<T>* arr, uint index) {
	ZoneScoped;
	ASSERT(arr.len > 0 && index >= 0 && index < arr.len);
	for (uint i = index + 1; i < arr->len; i++) {
		arr->data[i - 1] = arr->data[i];
	}
	arr->len -= 1;
}

template<typename T>
inline void ArrayRemoveSlice(Array<T>* arr, uint lo, uint hi) {
	ZoneScoped;
	uint count = hi - lo;
	for (uint i = lo + count; i < arr->len; i++) {
		arr->data[i - count] = arr->data[i];
	}
	arr->len -= count;
}

template<typename T>
inline void array_push_slice(Array<T>* arr, Slice<T> elems) {
	ZoneScoped;
	if (arr->capacity < arr->len + elems.len) {
		// needs to grow!
		ArrayReserve(arr, 2 * arr->capacity + elems.len);
	}

	memcpy(&arr->data[arr->len], elems.data, elems.len * sizeof(T));
	arr->len += elems.len;
}

//template<typename T>
//inline T* mem_new(Allocator* allocator) {
//	ZoneScoped;
//	Slice<u8> allocation = mem_alloc(sizeof(T), allocator);
//	return (T*)allocation.data;
//}
//

template<typename T>
inline void _MemFree(Slice<T> allocation, Allocator* allocator) {
	allocator->proc(allocator, SLICE_BYTES(allocation), 0, ALIGN_OF(T));
}

template<typename T>
inline void _MemResize(Slice<T>* allocation, uint new_len, Allocator* allocator) {
	allocation->data = (T*)allocator->proc(allocator, SLICE_BYTES(*allocation), new_len * sizeof(T), ALIGN_OF(T));
	allocation->len = new_len;
}

template<typename T>
inline T* _MemClone(const T& value, Allocator* allocator) {
	ZoneScoped;
	T* ptr = (T*)(allocator)->proc((allocator), {}, sizeof(T), ALIGN_OF(T));
	*ptr = value;
	return ptr;
}

template<typename T>
inline Slice<T> clone_slice(Slice<T> slice, Allocator* allocator) {
	ZoneScoped;
	Slice<T> cloned = MakeSlice(T, slice.len, allocator);
	memcpy(cloned.data, slice.data, slice.len * sizeof(T));
	return cloned;
}

#if 0
//template<typename T>
//struct SlotArray {
//	u32 num_items_per_bucket;
//
//	Allocator* allocator;
//
//	Array<void*> buckets;
//
//	u32 num_alive;
//	u32 first_removed;
//
//	u32 last_bucket;
//	u32 last_bucket_cursor;
//
//	inline T* operator [] (SlotArrayHandle handle) {
//		return (T*)slot_array_subscript_raw((SlotArray<void>*)this, handle, sizeof(T));
//	}
//};

//typedef SlotArray<void> SlotArrayRaw;
template<typename T>
inline void slot_array_init(SlotArray<T>* arr, Allocator* allocator, u32 num_items_per_bucket = 32) {
	ZoneScoped;
	ASSERT(num_items_per_bucket > 0);
	arr->allocator = allocator;
	arr->num_items_per_bucket = num_items_per_bucket;
}

template<typename T>
void slot_array_destroy(const SlotArray<T>& arr) {
	ZoneScoped;
	for (i64 i = 0; i < arr.buckets.len; i++) {
		u8* bucket = (u8*)((Array<void*>)arr.buckets)[i];

		Slice<u8> allocation = { bucket, arr.num_items_per_bucket * (sizeof(SlotArrayElemHeader) + sizeof(T)) };
		mem_release(allocation, arr.allocator);
	}
	array_release(arr.buckets);
}

template<typename T>
void slot_array_remove(SlotArray<T>* arr, SlotArrayHandle handle) {
	ZoneScoped;
	ASSERT(false);
	//LOG("TODO: slot_array_remove\n");
}

template<typename T>
SlotArrayHandle slot_array_add(SlotArray<T>* arr, const T& value) {
	return slot_array_add_raw((SlotArrayRaw*)arr, sizeof(T), &value);
}
#endif

#define MAP_ENTRY_EMPTY 0
#define MAP_ENTRY_DEAD_REQUIRED_FOR_CHAIN 1
#define MAP_ENTRY_ALIVE 2

template<typename KEY, typename VALUE>
struct Map_Entry {
	KEY key;
	u8 state;
	VALUE value;
};

template<typename KEY, typename VALUE>
struct Map {
	uint len;
	Slice<Map_Entry<KEY, VALUE>> entries;
	Allocator* allocator;

	inline VALUE* KeyAndValue(KEY key, KEY* out_key) {
		ZoneScoped;
		if (entries.len == 0) return NULL;

		u32 hashed = (u32)HASHER_GET(KEY, key);
		for (u32 i = hashed;; i++) {
			Map_Entry<KEY, VALUE>* entry = &entries.data[i % entries.len];
			if (entry->state == MAP_ENTRY_EMPTY)
				return NULL;

			if (entry->state == MAP_ENTRY_ALIVE && entry->key == key) {
				if (out_key) *out_key = entry->key;
				return &entry->value;
			}
		}
	}

	inline VALUE* operator [] (KEY key) { return KeyAndValue(key, NULL); }
};

template<typename KEY, typename VALUE>
inline void MapClear(Map<KEY, VALUE>* map) {
	ZoneScoped;
#ifdef _DEBUG
	memset(map->entries.data, 0xCC, map->entries.len * sizeof(Map_Entry<KEY, VALUE>)); // debug; trigger data-breakpoints
#endif
	for (s64 i = 0; i < map->entries.len; i++) {
		map->entries[i].state = 0;
	}
	map->len = 0;
}

// should we call anything that just releases memory, X_release? That'd make map_release a bit nicer
template<typename KEY, typename VALUE>
inline void map_release(Map<KEY, VALUE> map) {
	ZoneScoped;
	DeleteSlice(map.entries, map.allocator);
}

enum MapInsertMode {
	MapInsertMode_AssertUnique,
	MapInsertMode_Overwrite,
	MapInsertMode_DoNotOverwrite, // why isnt this Overwrite?
};

template<typename KEY, typename VALUE>
struct MapInsertResult {
	KEY key;
	VALUE* ptr;

	bool added;
};


template<typename KEY, typename VALUE>
void MapResize(Map<KEY, VALUE>* map, uint capacity) {
	ZoneScoped;
	ASSERT(capacity >= map->entries.len);

	map->len = 0;

	typedef Map_Entry<KEY, VALUE> entry_type;
	Slice<entry_type> old_entries = map->entries;

	ASSERT(map->allocator); // did you call InitMap?
	//if (!map->allocator.proc)
	//	map->allocator = GLOBAL_ALLOCATOR;

	map->entries = MakeSlice(entry_type, capacity, map->allocator);

	for (uint i = 0; i < map->entries.len; i++)
		map->entries[i].state = MAP_ENTRY_EMPTY;

	for (uint i = 0; i < old_entries.len; i++) {
		entry_type& entry = old_entries[i];
		if (entry.state == MAP_ENTRY_ALIVE) {
			MapInsert(map, entry.key, entry.value);
		}
	}

	DeleteSlice(old_entries, map->allocator);
}

template<typename KEY, typename VALUE>
inline void InitMap(Map<KEY, VALUE>* map, uint capacity, Allocator* allocator) {
	ZoneScoped;
	*map = {};
	map->allocator = allocator;
	MapResize(map, capacity);
}

// This assumes that `key` doesn't exist in the map already.
template<typename KEY, typename VALUE>
inline MapInsertResult<KEY, VALUE> MapInsert(Map<KEY, VALUE>* map, KEY key, VALUE value, MapInsertMode mode = MapInsertMode_AssertUnique) {
	ZoneScoped;
	u32 hashed = (u32)HASHER_GET(KEY, key);
	//if (hashed < MAP_FIRST_VALID_HASH) hashed = MAP_FIRST_VALID_HASH;

	// filled / allocated >= 70/100 ... therefore
	// filled * 100 >= allocated * 70
	if ((map->len + 1) * 100 >= map->entries.len * 70) {
		// expand the map
		MapResize(map, MAX(map->entries.len * 2, 8));
	}

	// NOTE: We cannot just stop and occupy the first destroyed slot when we encounter one, because the key might exist in the map AFTER a destroyed slot.
	// So we have to keep looking until we find an empty spot

	Map_Entry<KEY, VALUE>* first_dead = NULL;

	for (u32 i = hashed;; i++) {
		Map_Entry<KEY, VALUE>* entry = &map->entries[i % map->entries.len];

		if (entry->state != MAP_ENTRY_ALIVE) {
			if (!first_dead) first_dead = entry;
			if (entry->state == MAP_ENTRY_EMPTY) break; // Don't have to continue further, we know that this key does not exist in the map.
		}
		else if (entry->key == key) {
			if (mode == MapInsertMode_Overwrite) {
				entry->value = value;
				return MapInsertResult<KEY, VALUE>{ entry->key, & entry->value, false };
			}
			else if (mode == MapInsertMode_DoNotOverwrite) {
				return MapInsertResult<KEY, VALUE>{ entry->key, & entry->value, false };
			}
			else ASSERT(false); // Element already exists, and the behaviour of the map is set to MapInsertMode_AssertUnique!
		}
	}

	// Insert new item
	map->len += 1;
	first_dead->state = MAP_ENTRY_ALIVE;
	first_dead->key = key;
	first_dead->value = value;
	return MapInsertResult<KEY, VALUE>{ key, & first_dead->value, true };
}

template<typename KEY, typename VALUE>
inline bool MapRemove(Map<KEY, VALUE>* map, KEY key) {
	ZoneScoped;
	if (map->len == 0) return false;

	u32 hashed = (u32)HASHER_GET(KEY, key);

	u32 i = hashed;
	Map_Entry<KEY, VALUE>* entry;
	for (;;) { // A map is guaranteed to either be empty or have at least one empty slot, thus this will never infinite loop
		i %= map->entries.len;
		entry = &map->entries[i];

		if (entry->state == MAP_ENTRY_EMPTY)
			return false; // key does not exist in the table!

		if (entry->state == MAP_ENTRY_ALIVE && entry->key == key) {
#ifdef _DEBUG
			memset(entry, 0xCC, sizeof(*entry)); // debug; trigger data-breakpoints
#endif
			if (map->entries[(i + 1) % map->entries.len].state == MAP_ENTRY_EMPTY) {
				// If the next slot is empty and not required for the chain, this slot is not required for the chain either.
				entry->state = MAP_ENTRY_EMPTY;

				// Release the entries before this from the chain duty as well
				for (;;) {
					if (i == 0) i = (u32)map->entries.len;
					i -= 1;
					entry = &map->entries[i];
					if (entry->state != MAP_ENTRY_DEAD_REQUIRED_FOR_CHAIN) break;
					entry->state = MAP_ENTRY_EMPTY;
				}
			}
			else {
				entry->state = MAP_ENTRY_DEAD_REQUIRED_FOR_CHAIN;
			}

			map->len -= 1;
			return true;
		}

		i++;
	}
}

#define MAP_EACH(map, key, value) (s64 key##_i = 0; MapIteratorCondition(map, &key, &value, key##_i);)
#define MAP_EACH_PTR(map, key, value) (s64 key##_i = 0; MapIteratorConditionPtr(map, &key, &value, key##_i);)

template<typename KEY, typename VALUE>
inline bool MapIteratorCondition(Map<KEY, VALUE>& map, KEY* key, VALUE* value, s64& i) {
	while (i < map.entries.len) {
		Map_Entry<KEY, VALUE>* entry = &map.entries[i];
		i += 1;
		if (entry->state == MAP_ENTRY_ALIVE) {
			*key = entry->key;
			*value = entry->value;
			return true;
		}
	}
	return false;
}

template<typename KEY, typename VALUE>
inline bool MapIteratorConditionPtr(Map<KEY, VALUE>& map, KEY* key, VALUE** value_ptr, s64& i) {
	while (i < map.entries.len) {
		Map_Entry<KEY, VALUE>* entry = &map.entries[i];
		i += 1;
		if (entry->state != MAP_ENTRY_ALIVE) continue;

		*key = entry->key;
		*value_ptr = &entry->value;
		return true;
	}
	return false;
}


template<typename T>
struct SlotArena {
	// SlotArena
};

template<typename T>
inline void MakeSlotArena(SlotArena<T>* arena, uint reserve_size, Allocator* allocator) { make_slot_arena_raw((RawSlotArena*)arena, sizeof(T), reserve_size, allocator); }

template<typename T>
inline T* SlotArenaAdd(SlotArena<T>* arena) {
	T* result = (T*)slot_arena_add_garbage_raw((RawSlotArena*)arena);
	*result = {};
	return result;
}

template<typename T>
inline void SlotArenaClear(SlotArena<T>* arena) { slot_arena_clear_raw((RawSlotArena*)arena); }

template<typename T>
inline bool SlotArenaRemove(SlotArena<T>* arena, T* ptr) { return slot_arena_remove_raw((RawSlotArena*)arena, ptr); }

template<typename T>
inline void DestroySlotArena(SlotArena<T>* arena) { delete_slot_arena_raw((RawSlotArena*)arena); }

template<typename T>
inline u64 SlotArenaGetIndex(const SlotArena<T>* arena, void* ptr) { return slot_arena_get_index_raw((const RawSlotArena*)arena, ptr); }

#define Print(...) _Print({__VA_ARGS__})
#define PrintB(buffer, ...) _PrintB(buffer, {__VA_ARGS__})
#define PrintA(allocator, ...) _PrintA(allocator, {__VA_ARGS__})

void _Print(std::initializer_list<String> args);
void _PrintB(Array<u8>* buffer, std::initializer_list<String> args);
String _PrintA(Allocator* allocator, std::initializer_list<String> args);

*/
