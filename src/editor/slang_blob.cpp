#include "slang_blob.h"

using namespace godot;

void SlangBlob::_bind_methods() {
	BIND_GET_SET(SlangBlob, buffer, Variant::PACKED_BYTE_ARRAY)
	BIND_GET_SET(SlangBlob, diagnostic, Variant::STRING)
}

Ref<SlangBlob> SlangBlob::create(slang::IBlob* blob, slang::IBlob* diagnostic) {
	ERR_FAIL_NULL_V(blob, {});
	Ref ret = memnew(SlangBlob);
	if (void const* buffer_ptr = blob->getBufferPointer()) {
		PackedByteArray buffer;
		buffer.resize(blob->getBufferSize());
		memcpy(buffer.ptrw(), buffer_ptr, blob->getBufferSize());
		ret->set_buffer(buffer);
	}
	if (diagnostic) {
		if (void const* diagnostic_ptr = diagnostic->getBufferPointer()) {
			ret->set_diagnostic(String::utf8(static_cast<const char*>(diagnostic_ptr), diagnostic->getBufferSize()));
		}
	}
	return ret;
}

GET_SET_PROPERTY_IMPL(SlangBlob, godot::PackedByteArray, buffer)
GET_SET_PROPERTY_IMPL(SlangBlob, godot::String, diagnostic)