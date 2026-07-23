#pragma once


#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <deque>

#include "../Value/Value.hxx"
#include "../Utils/utils.hxx"

#include <ffi.h>
#include <memory>
#include <ranges>


#ifndef FFI_TYPE_CSTRING 
#define FFI_TYPE_CSTRING 100
#endif


inline namespace pie {
namespace ffi {

// ============================================================================
// FFI shape tree.
//
// An `FFI` node describes the *C-level layout* of a single argument or
// return value: either a scalar libffi type, or (recursively) a struct made
// up of more `FFI` nodes. It never stores the actual Pie value - that's kept
// separate so the same shape can be reused for packing (Pie -> C bytes) and
// unpacking (C bytes -> Pie) alike (needed for struct return values).
// ============================================================================
class FFI {
public:
    std::vector<std::unique_ptr<FFI>> nested; // only populated when isStruct()
    ffi_type *type = nullptr;

    bool isStruct() const noexcept { return type->type == FFI_TYPE_STRUCT; }

    ~FFI() {
        if (isStruct()) {
            delete[] type->elements;
            delete type;
        }
    }
};


// Every scalar C type Pie's FFI layer understands, mapped to libffi's
// static type descriptors. FFI_TYPE_INT is treated as a plain 32-bit `int`,
// which is correct for every ABI libffi actually targets.
inline ffi_type* baseType(const BigInt type_id) noexcept {
    switch (type_id) {
        case FFI_TYPE_VOID      : return &ffi_type_void;
        case FFI_TYPE_INT       :
        case FFI_TYPE_SINT32    : return &ffi_type_sint32;
        case FFI_TYPE_UINT32    : return &ffi_type_uint32;
        case FFI_TYPE_SINT8     : return &ffi_type_sint8;
        case FFI_TYPE_UINT8     : return &ffi_type_uint8;
        case FFI_TYPE_SINT16    : return &ffi_type_sint16;
        case FFI_TYPE_UINT16    : return &ffi_type_uint16;
        case FFI_TYPE_SINT64    : return &ffi_type_sint64;
        case FFI_TYPE_UINT64    : return &ffi_type_uint64;
        case FFI_TYPE_FLOAT     : return &ffi_type_float;
        case FFI_TYPE_DOUBLE    : return &ffi_type_double;

        case FFI_TYPE_CSTRING   :
        case FFI_TYPE_POINTER   : return &ffi_type_pointer;
        default                 : return nullptr;
    }
}


inline const char* typeName(const int ffi_type_tag) noexcept {
    switch (ffi_type_tag) {
        case FFI_TYPE_VOID      : return "void";
        case FFI_TYPE_SINT32    : return "int32";
        case FFI_TYPE_UINT32    : return "uint32";
        case FFI_TYPE_SINT8     : return "int8";
        case FFI_TYPE_UINT8     : return "uint8";
        case FFI_TYPE_SINT16    : return "int16";
        case FFI_TYPE_UINT16    : return "uint16";
        case FFI_TYPE_SINT64    : return "int64";
        case FFI_TYPE_UINT64    : return "uint64";
        case FFI_TYPE_FLOAT     : return "float";
        case FFI_TYPE_DOUBLE    : return "double";
        case FFI_TYPE_POINTER   : return "pointer";
        case FFI_TYPE_CSTRING   : return "cstring";
        case FFI_TYPE_STRUCT    : return "struct";
        default                 : return "<unknown>";
    }
}


// ----------------------------------------------------------------------------
// Pie value -> C value coercion helpers. This is where the "smart unboxing"
// lives: Pie only has one integer type (BigInt / int64_t) and one float type
// (double). Every C integer width/signedness and both C float widths get
// derived from a single Pie value based purely on what the C side asked for.
// ----------------------------------------------------------------------------

template <std::integral T>
inline T narrowTo(const BigInt v, const int ffi_type_tag) {
    if (
        v < BigInt(std::numeric_limits<T>::min()) or
        ( sizeof(BigInt) > sizeof(T) and v > BigInt(std::numeric_limits<T>::max()))
    )
        util::error(
            "Value " + std::to_string(v) + " does not fit in C type `" +
            typeName(ffi_type_tag) + "`"
        );

    return static_cast<T>(v);
}


inline BigInt asInt(const value::Value& value, const int ffi_type_tag) {
    if (std::holds_alternative<BigInt>(value)) return get<BigInt>(value);
    if (std::holds_alternative<bool  >(value)) return get<bool  >(value);


    util::error(
        "C type `" + std::string{typeName(ffi_type_tag)} +
        "` expects a Pie Int (or Bool), got: " + value::stringify(value)
    );
}

inline double asNumber(const value::Value& value, const int ffi_type_tag) {
    if (std::holds_alternative<double>(value)) return get<double>(value);
    if (std::holds_alternative<BigInt>(value)) return get<BigInt>(value);

    util::error(
        "C type `" + std::string{typeName(ffi_type_tag)} +
        "` expects a Pie Int or Double, got: " + value::stringify(value)
    );
}

// Storage for anything a pointer needs to keep pointing at for the
// lifetime of a single ffi_call (currently: copied-out string bytes for
// `const char*` arguments). A deque so addresses handed out earlier stay
// valid no matter how much more gets appended later.


// Pie has no pointer type of its own.
// A "pointer" is either a raw address jammed into a BigInt (e.g. from dlopen/dlsym) or a Pie String. 
// A Pie String's bytes are NOT already sitting somewhere stable for the duration of the C call
// (the value::Value that owns them is often a short-lived temporary)
// so they get copied into `scratch` and the pointer to _that_ copy is what's actually passed.
inline void* asPointer(const value::Value& value, std::deque<std::vector<std::byte>>& scratch) {
    if (std::holds_alternative<BigInt>(value)) return reinterpret_cast<void*>(get<BigInt>(value));

    if (std::holds_alternative<std::string>(value)) {
        const auto& s = get<std::string>(value);
        auto& buf = scratch.emplace_back(s.size() + 1);
        std::memcpy(buf.data(), s.data(), s.size());
        buf[s.size()] = std::byte{0}; // NUL terminator
        return buf.data();
    }

    util::error(
        "C type `pointer` expects a Pie Int (raw address) or a Pie String, got: " +
        value::stringify(value)
    );
}


// filter out the `__types` bookkeeping member so struct field order lines up
// 1:1 with the type-id list every time we walk a Pie Object as a C struct.
inline auto structFields(const value::Object& obj) {
    return obj.second->members | std::views::filter([] (const auto& member) { return get<0>(member).name != "__types"; });
}


// ============================================================================
// prepareFFI: build the C-level shape (ffi_type tree) for a Pie value given
// its declared C type id. For scalars this is trivial; for structs it reads
// the `__types` member and recurses field-by-field (a struct field may
// itself be a struct).
// ============================================================================
inline std::unique_ptr<FFI> prepareFFI(const value::Value& value, const BigInt type_id) {
    if (type_id != FFI_TYPE_STRUCT) {
        auto* t = baseType(type_id);
        if (not t) util::error("Unknown/unsupported C type id: " + std::to_string(type_id));

        auto node = std::make_unique<FFI>();
        node->type = t;
        return node;
    }

    if (not std::holds_alternative<value::Object>(value))
        util::error("C Type indicated `struct`, but value passed was: " + value::stringify(value));

    const auto& obj = get<value::Object>(value);

    std::vector<BigInt> c_types;
    bool found_types = false;
    for (const auto& [name, _, member_value] : obj.second->members) {
        if (name.name != "__types") continue;
        found_types = true;

        if (not std::holds_alternative<value::ListValue>(*member_value))
            util::error("Special Member `__types` must be a list filled with C Types: " + value::stringify(value));

        for (const auto& elt : get<value::ListValue>(*member_value).elts->values) {
            if (not std::holds_alternative<BigInt>(elt))
                util::error("Special Member `__types` must be a list filled with C Types: " + value::stringify(value));

            c_types.push_back(get<BigInt>(elt));
        }
    }

    if (not found_types)
        util::error("Struct value passed to the FFI is missing its `__types` member: " + value::stringify(value));


    const size_t size = c_types.size();
    auto types = new ffi_type*[size + 1];
    types[size] = nullptr;

    auto ret = std::make_unique<FFI>();

    // zip shortens the members list to the type list length.
    // this is intended to allow the user to attach methods and other members to the Pie class
    for (size_t i{}; const auto& [id, member] : std::views::zip(c_types, structFields(obj))) {
        const auto& [name, _, member_value] = member;

        auto field = prepareFFI(*member_value, id);
        types[i++] = field->type;
        ret->nested.push_back(std::move(field));
    }

    auto* ffi_struct = new ffi_type{{}, {}, FFI_TYPE_STRUCT, types};

    // necessary call to `ffi_get_struct_offsets` to fill in .size and .alignment of `ffi_struct`
    size_t dummy[256];
    ffi_get_struct_offsets(FFI_DEFAULT_ABI, ffi_struct, dummy);

    ret->type = ffi_struct;
    return ret;
}


// ----------------------------------------------------------------------------
// pack: Pie value -> raw C bytes, according to an already-built FFI shape.
// Used for every argument (scalars and structs alike) and for the "template"
// object used to describe a struct return type.
// ----------------------------------------------------------------------------
inline void packScalar(std::byte* dst, const int ffi_type_tag, const value::Value& value, std::deque<std::vector<std::byte>>& scratch) {
    switch (ffi_type_tag) {
        case FFI_TYPE_SINT8  : *reinterpret_cast<int8_t  *>(dst) =      narrowTo<  int8_t>(asInt(value, ffi_type_tag), ffi_type_tag); return;
        case FFI_TYPE_UINT8  : *reinterpret_cast<uint8_t *>(dst) =      narrowTo< uint8_t>(asInt(value, ffi_type_tag), ffi_type_tag); return;
        case FFI_TYPE_SINT16 : *reinterpret_cast<int16_t *>(dst) =      narrowTo< int16_t>(asInt(value, ffi_type_tag), ffi_type_tag); return;
        case FFI_TYPE_UINT16 : *reinterpret_cast<uint16_t*>(dst) =      narrowTo<uint16_t>(asInt(value, ffi_type_tag), ffi_type_tag); return;
        case FFI_TYPE_SINT32 : *reinterpret_cast<int32_t *>(dst) =      narrowTo< int32_t>(asInt(value, ffi_type_tag), ffi_type_tag); return;
        case FFI_TYPE_UINT32 : *reinterpret_cast<uint32_t*>(dst) =      narrowTo<uint32_t>(asInt(value, ffi_type_tag), ffi_type_tag); return;
        case FFI_TYPE_SINT64 : *reinterpret_cast<int64_t *>(dst) = static_cast< int64_t>(asInt(value, ffi_type_tag)); return;
        case FFI_TYPE_UINT64 : *reinterpret_cast<uint64_t*>(dst) = static_cast<uint64_t>(asInt(value, ffi_type_tag)); return;

        case FFI_TYPE_FLOAT  : *reinterpret_cast<float   *>(dst) = static_cast<   float>(asNumber(value, ffi_type_tag)); return;
        case FFI_TYPE_DOUBLE : *reinterpret_cast<double  *>(dst) = asNumber(value, ffi_type_tag); return;


        case FFI_TYPE_CSTRING:
        case FFI_TYPE_POINTER: *reinterpret_cast<void**>(dst) = asPointer(value, scratch); return;

        default:
            util::error(std::string{"Cannot pack a value into unsupported C type `"} + typeName(ffi_type_tag) + "`");
    }
}


inline void pack(std::byte *buffer, const FFI *ffi, const value::Value& value, std::deque<std::vector<std::byte>>& scratch) {
    if (ffi->isStruct()) {
        if (not std::holds_alternative<value::Object>(value)) util::error("Expected a struct value while packing, got: " + value::stringify(value));

        size_t offset[256];
        ffi_get_struct_offsets(FFI_DEFAULT_ABI, ffi->type, offset);

        const auto& obj = get<value::Object>(value);

        for (size_t i{}; const auto& [name, _, member_value] : structFields(obj)) {
            if (i >= ffi->nested.size()) break; // extra Pie-side members beyond the C shape are fine

            pack(buffer + offset[i], ffi->nested[i].get(), *member_value, scratch);
            ++i;
        }
        return;
    }

    packScalar(buffer, ffi->type->type, value, scratch);
}


// ----------------------------------------------------------------------------
// unpack: raw C bytes -> Pie value, the mirror image of pack(). For structs,
// `shape_template` is a Pie Object (usually the class the caller declared as
// the return type) used purely as a mold: same member names/order/nested
// struct shapes, with fresh values filled in from `buffer`.
// ----------------------------------------------------------------------------
inline value::Value unpackScalar(const std::byte *src, const int ffi_type_tag) {
    switch (ffi_type_tag) {
        case FFI_TYPE_SINT8  : return static_cast<BigInt>(*reinterpret_cast<const std::int8_t  *>(src));
        case FFI_TYPE_UINT8  : return static_cast<BigInt>(*reinterpret_cast<const std::uint8_t *>(src));
        case FFI_TYPE_SINT16 : return static_cast<BigInt>(*reinterpret_cast<const std::int16_t *>(src));
        case FFI_TYPE_UINT16 : return static_cast<BigInt>(*reinterpret_cast<const std::uint16_t*>(src));
        case FFI_TYPE_SINT32 : return static_cast<BigInt>(*reinterpret_cast<const std::int32_t *>(src));
        case FFI_TYPE_UINT32 : return static_cast<BigInt>(*reinterpret_cast<const std::uint32_t*>(src));
        case FFI_TYPE_SINT64 : return static_cast<BigInt>(*reinterpret_cast<const std::int64_t *>(src));
        case FFI_TYPE_UINT64 : return static_cast<BigInt>(*reinterpret_cast<const std::uint64_t*>(src));

        case FFI_TYPE_FLOAT  : return static_cast<double>(*reinterpret_cast<const float *>(src));
        case FFI_TYPE_DOUBLE : return *reinterpret_cast<const double*>(src);

        case FFI_TYPE_CSTRING: return reinterpret_cast<BigInt>(*reinterpret_cast<void* const*>(src));
        case FFI_TYPE_POINTER: return reinterpret_cast<BigInt>(*reinterpret_cast<void* const*>(src));

        case FFI_TYPE_VOID   : return BigInt{0};

        default:
            util::error(std::string{"Cannot unpack unsupported C type `"} + typeName(ffi_type_tag) + "`");
    }
}

inline value::Value unpack(const std::byte *buffer, const FFI *ffi, const value::Value& shape_template) {
    if (not ffi->isStruct())
        return unpackScalar(buffer, ffi->type->type);

    if (not std::holds_alternative<value::Object>(shape_template))
        util::error("Struct return type must be described by an Object template: " + value::stringify(shape_template));


    const auto& tmpl = get<value::Object>(shape_template);

    size_t offset[256];
    ffi_get_struct_offsets(FFI_DEFAULT_ABI, ffi->type, offset);


    // deep-copy the template's members so filling in real values doesn't mutate the template
    auto out_members = std::make_shared<value::Members>(*tmpl.second);
    for (auto& [name, type, val] : out_members->members) val = std::make_shared<value::Value>(*val);


    for (size_t i{}; auto& [name, type, val] : out_members->members) {
        if (name.name == "__types") continue;
        if (i >= ffi->nested.size()) break;

        *val = unpack(buffer + offset[i], ffi->nested[i].get(), *val);
        ++i;
    }


    return value::Object{tmpl.first, out_members};
}



// unpackInto: like unpack(), but writes *through* an existing Pie value
// instead of building a fresh one. Used for `T*` (pointer-to-struct)
// arguments: since a Pie value the caller already holds went in by
// reference (no separate return value comes back for it), this is how the
// interpreter reflects whatever the C function wrote into that memory back
// into the Pie-visible object.
inline void unpackInto(const std::byte *buffer, const FFI *ffi, value::Value& target) {
    if (not ffi->isStruct()) {
        target = unpackScalar(buffer, ffi->type->type);
        return;
    }

    if (not std::holds_alternative<value::Object>(target))
        util::error("Expected a struct value to write C results back into, got: " + value::stringify(target));

    auto& obj = get<value::Object>(target);

    size_t offset[256];
    ffi_get_struct_offsets(FFI_DEFAULT_ABI, ffi->type, offset);

    size_t i{};
    for (auto& [name, _, member_value] : obj.second->members) {
        if (name.name == "__types") continue;
        if (i >= ffi->nested.size()) break;

        unpackInto(buffer + offset[i], ffi->nested[i].get(), *member_value);
        ++i;
    }
}


} // namespace ffi
} // namespace pie