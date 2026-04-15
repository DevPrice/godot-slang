#include "slang_blob.h"

using namespace godot;

void SlangBlob::_bind_methods() {
	BIND_GET_SET(SlangBlob, buffer, Variant::PACKED_BYTE_ARRAY)
	BIND_GET_SET(SlangBlob, diagnostic, Variant::STRING)
}

GET_SET_PROPERTY_IMPL(SlangBlob, godot::PackedByteArray, buffer)
GET_SET_PROPERTY_IMPL(SlangBlob, godot::String, diagnostic)