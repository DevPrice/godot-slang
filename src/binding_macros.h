#ifndef BINDING_MACROS_H
#define BINDING_MACROS_H

#define GET_SET_PROPERTY(Type, Name) \
private:                             \
	Type Name{};                     \
                                     \
public:                              \
	Type get_##Name() const;         \
	void set_##Name(Type p_##Name);  \
                                     \
private:

#define GET_SET_PROPERTY_IMPL(ClassName, Type, Name) \
	Type ClassName::get_##Name() const {             \
		return Name;                                 \
	}                                                \
	void ClassName::set_##Name(Type p_##Name) {      \
		Name = p_##Name;                             \
	}

#define GET_SET_OBJECT_PTR(Type, Name) \
private:                               \
	godot::ObjectID Name##_id;         \
                                       \
public:                                \
	Type* get_##Name() const;          \
	void set_##Name(Type* p_##Name);   \
                                       \
private:

#define GET_SET_OBJECT_PTR_IMPL(ClassName, Type, Name)                                 \
	Type* ClassName::get_##Name() const {                                              \
		return godot::Object::cast_to<Type>(godot::ObjectDB::get_instance(Name##_id)); \
	}                                                                                  \
	void ClassName::set_##Name(Type* p_##Name) {                                       \
		Name##_id = p_##Name ? p_##Name->get_instance_id() : godot::ObjectID();        \
	}

#define GET_SET_NODE_PATH(Type, Name) \
private:                              \
	godot::NodePath Name##_path;      \
                                      \
public:                               \
	Type* get_##Name() const;         \
	void set_##Name(Type* p_##Name);  \
                                      \
private:

#define GET_SET_RELATIVE_NODE_PATH_IMPL(ClassName, Type, Name)              \
	Type* ClassName::get_##Name() const {                                   \
		return get_node_or_null(Name##_path);                               \
	}                                                                       \
	void ClassName::set_##Name(Type* p_##Name) {                            \
		Name##_path = p_##Name ? get_path_to(p_##Name) : godot::NodePath(); \
	}

#define BIND_METHOD(ClassName, Name, ...) \
	godot::ClassDB::bind_method(D_METHOD(#Name, ##__VA_ARGS__), &ClassName::Name);

#define BIND_STATIC_METHOD(ClassName, Name, ...) \
	godot::ClassDB::bind_static_method(#ClassName, D_METHOD(#Name, ##__VA_ARGS__), &ClassName::Name);

#define BIND_GET_SET_METHOD(ClassName, Name)                                     \
	godot::ClassDB::bind_method(D_METHOD("get_" #Name), &ClassName::get_##Name); \
	godot::ClassDB::bind_method(D_METHOD("set_" #Name, "p_" #Name), &ClassName::set_##Name);

#define BIND_GET_SET(ClassName, Name, VariantType, ...) \
	BIND_GET_SET_METHOD(ClassName, Name)                \
	godot::ClassDB::add_property(#ClassName, godot::PropertyInfo(VariantType, #Name, ##__VA_ARGS__), "set_" #Name, "get_" #Name);

#define BIND_GET_SET_OBJECT(ClassName, Name, ObjectClass) \
	BIND_GET_SET_METHOD(ClassName, Name)                  \
	godot::ClassDB::add_property(#ClassName, godot::PropertyInfo(Variant::Type::OBJECT, #Name, godot::PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE, godot::ObjectClass::get_class_static()), "set_" #Name, "get_" #Name);

#define BIND_GET_SET_RESOURCE(ClassName, Name, Type, ...) \
	BIND_GET_SET_METHOD(ClassName, Name)                  \
	godot::ClassDB::add_property(#ClassName, godot::PropertyInfo(godot::Variant::OBJECT, #Name, godot::PROPERTY_HINT_RESOURCE_TYPE, #Type, ##__VA_ARGS__), "set_" #Name, "get_" #Name);

#define BIND_GET_SET_NODE(ClassName, Name, Type) \
	BIND_GET_SET_METHOD(ClassName, Name)         \
	godot::ClassDB::add_property(#ClassName, godot::PropertyInfo(godot::Variant::OBJECT, #Name, godot::PROPERTY_HINT_NODE_TYPE, #Type), "set_" #Name, "get_" #Name);

#define BIND_GET_SET_ENUM(ClassName, Name, Values) \
	BIND_GET_SET_METHOD(ClassName, Name)           \
	godot::ClassDB::add_property(#ClassName, godot::PropertyInfo(godot::Variant::INT, #Name, godot::PROPERTY_HINT_ENUM, Values), "set_" #Name, "get_" #Name);

#define BIND_GET_SET_ARRAY(ClassName, Name, Type) \
	BIND_GET_SET_METHOD(ClassName, Name)          \
	godot::ClassDB::add_property(#ClassName, godot::PropertyInfo(godot::Variant::ARRAY, #Name, godot::PROPERTY_HINT_TYPE_STRING, godot::String::num(Type)), "set_" #Name, "get_" #Name);

#define BIND_GET_SET_RESOURCE_ARRAY(ClassName, Name, Type) \
	BIND_GET_SET_METHOD(ClassName, Name)                   \
	godot::ClassDB::add_property(#ClassName, godot::PropertyInfo(godot::Variant::ARRAY, #Name, godot::PROPERTY_HINT_TYPE_STRING, godot::String::num(godot::Variant::OBJECT) + "/" + godot::String::num(PROPERTY_HINT_RESOURCE_TYPE) + ":" #Type), "set_" #Name, "get_" #Name);

#endif
