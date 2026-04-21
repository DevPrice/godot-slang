#include "slang_blob.h"

using namespace godot;

void SlangBlob::_bind_methods() {
	BIND_GET_SET(SlangBlob, buffer, Variant::PACKED_BYTE_ARRAY)
	BIND_GET_SET(SlangBlob, diagnostic, Variant::STRING)
}

Ref<SlangBlob> SlangBlob::create(slang::IBlob* blob, slang::IBlob* diagnostic) {
	ERR_FAIL_NULL_V(blob, {});
	Ref ret = memnew(SlangBlob);
	ret->set_buffer(blob_to_bytes(blob));
	if (diagnostic) {
		ret->set_diagnostic(blob_to_string(diagnostic));
	}
	return ret;
}

String SlangBlob::blob_to_string(slang::IBlob* blob) {
	if (blob && blob->getBufferPointer() && blob->getBufferSize()) {
		return String::utf8(
			static_cast<const char*>(blob->getBufferPointer()),
			static_cast<int64_t>(blob->getBufferSize()));
	}
	return String{};
}

PackedByteArray SlangBlob::blob_to_bytes(slang::IBlob* blob) {
	if (blob && blob->getBufferPointer() && blob->getBufferSize()) {
		PackedByteArray buffer;
		buffer.resize(static_cast<int64_t>(blob->getBufferSize()));
		memcpy(buffer.ptrw(), blob->getBufferPointer(), blob->getBufferSize());
		return buffer;
	}
	return PackedByteArray{};
}

GET_SET_PROPERTY_IMPL(SlangBlob, godot::PackedByteArray, buffer)
GET_SET_PROPERTY_IMPL(SlangBlob, godot::String, diagnostic)