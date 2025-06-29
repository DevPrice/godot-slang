#ifndef BINDING_MACROS_H
#define BINDING_MACROS_H

#define GET_SET_PROPERTY(Type, Name) \
private: \
    Type Name; \
public: \
    Type get_##Name() const; \
    void set_##Name(Type p_##Name); \
private:

#define GET_SET_PROPERTY_IMPL(ClassName, Type, Name) \
Type ClassName::get_##Name() const { \
    return Name; \
} \
void ClassName::set_##Name(Type p_##Name) { \
    Name = p_##Name; \
}

#define GET_SET_OBJECT_PTR(Type, Name) \
private: \
    ObjectID Name##_id; \
public: \
    Type* get_##Name() const; \
    void set_##Name(Type* p_##Name); \
private:

#define GET_SET_OBJECT_PTR_IMPL(ClassName, Type, Name) \
Type* ClassName::get_##Name() const { \
    return Object::cast_to<Type>(ObjectDB::get_instance(Name##_id)); \
} \
void ClassName::set_##Name(Type* p_##Name) { \
    Name##_id = p_##Name ? p_##Name->get_instance_id() : ObjectID(); \
}

#define GET_SET_NODE_PATH(Type, Name) \
private: \
    NodePath Name##_path; \
public: \
    Type* get_##Name() const; \
    void set_##Name(Type* p_##Name); \
private:

#define GET_SET_RELATIVE_NODE_PATH_IMPL(ClassName, Type, Name) \
Type* ClassName::get_##Name() const { \
    return get_node_or_null(Name##_path); \
} \
void ClassName::set_##Name(Type* p_##Name) { \
    Name##_path = p_##Name ? get_path_to(p_##Name) : NodePath(); \
}

#define BIND_METHOD(ClassName, Name, ...) \
    ClassDB::bind_method(D_METHOD(#Name, ##__VA_ARGS__), &ClassName::Name);

#define BIND_STATIC_METHOD(ClassName, Name, ...) \
    ClassDB::bind_static_method(#ClassName, D_METHOD(#Name, ##__VA_ARGS__), &ClassName::Name);

#define BIND_GET_SET_METHOD(ClassName, Name) \
    ClassDB::bind_method(D_METHOD("get_" #Name), &ClassName::get_##Name); \
    ClassDB::bind_method(D_METHOD("set_" #Name, "p_" #Name), &ClassName::set_##Name);

#define BIND_GET_SET(ClassName, Name, VariantType, ...) \
    BIND_GET_SET_METHOD(ClassName, Name) \
    ClassDB::add_property(#ClassName, PropertyInfo(VariantType, #Name, ##__VA_ARGS__), "set_" #Name, "get_" #Name);

#define BIND_GET_SET_RESOURCE(ClassName, Name, Type, ...) \
    BIND_GET_SET_METHOD(ClassName, Name) \
    ClassDB::add_property(#ClassName, PropertyInfo(Variant::OBJECT, #Name, PROPERTY_HINT_RESOURCE_TYPE, #Type, ##__VA_ARGS__), "set_" #Name, "get_" #Name);

#define BIND_GET_SET_NODE(ClassName, Name, Type) \
    BIND_GET_SET_METHOD(ClassName, Name) \
    ClassDB::add_property(#ClassName, PropertyInfo(Variant::OBJECT, #Name, PROPERTY_HINT_NODE_TYPE, #Type), "set_" #Name, "get_" #Name);

#define BIND_GET_SET_ENUM(ClassName, Name, Values) \
    BIND_GET_SET_METHOD(ClassName, Name) \
    ClassDB::add_property(#ClassName, PropertyInfo(Variant::INT, #Name, PROPERTY_HINT_ENUM, Values), "set_" #Name, "get_" #Name);

#define BIND_GET_SET_STRING_ARRAY(ClassName, Name) \
    BIND_GET_SET_METHOD(ClassName, Name) \
    ClassDB::add_property(#ClassName, PropertyInfo(Variant::ARRAY, #Name, PROPERTY_HINT_TYPE_STRING, String::num(Variant::STRING)), "set_" #Name, "get_" #Name);

#define BIND_GET_SET_RESOURCE_ARRAY(ClassName, Name, Type) \
    BIND_GET_SET_METHOD(ClassName, Name) \
    ClassDB::add_property(#ClassName, PropertyInfo(Variant::ARRAY, #Name, PROPERTY_HINT_TYPE_STRING, String::num(Variant::OBJECT) + "/" + String::num(PROPERTY_HINT_RESOURCE_TYPE) + ":" #Type), "set_" #Name, "get_" #Name);

#endif
