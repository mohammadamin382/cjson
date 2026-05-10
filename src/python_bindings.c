#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include "json_parser.h"

static void json_value_destructor(PyObject* capsule) {
    JsonValue* value = (JsonValue*)PyCapsule_GetPointer(capsule, "JsonValue");
    if (value) {
        json_free(value);
    }
}

static void sqlite_db_destructor(PyObject* capsule) {
    JsonSqliteDB* db = (JsonSqliteDB*)PyCapsule_GetPointer(capsule, "JsonSqliteDB");
    if (db) {
        sqlite_close(db);
    }
}

static PyObject* py_parse(PyObject* self, PyObject* args) {
    const char* json_string;
    if (!PyArg_ParseTuple(args, "s", &json_string)) {
        return NULL;
    }
    
    if (!json_string) {
        PyErr_SetString(PyExc_ValueError, "JSON string cannot be NULL");
        return NULL;
    }
    
    JsonValue* value = json_parse(json_string);
    if (!value) {
        JsonError* error = json_get_last_error();
        char error_msg[1024];
        snprintf(error_msg, sizeof(error_msg), "JSON Parse Error at line %zu, column %zu: %s", 
                 error->line, error->column, error->message);
        PyErr_SetString(PyExc_ValueError, error_msg);
        return NULL;
    }
    
    PyObject* result = PyCapsule_New(value, "JsonValue", json_value_destructor);
    if (!result) {
        json_free(value);
        PyErr_SetString(PyExc_RuntimeError, "Failed to create Python capsule");
        return NULL;
    }
    
    return result;
}

static PyObject* py_stringify(PyObject* self, PyObject* args) {
    PyObject* capsule;
    int pretty = 0;
    
    if (!PyArg_ParseTuple(args, "O|i", &capsule, &pretty)) {
        return NULL;
    }
    
    if (!capsule || !PyCapsule_CheckExact(capsule)) {
        PyErr_SetString(PyExc_TypeError, "First argument must be a valid JsonValue capsule");
        return NULL;
    }
    
    JsonValue* value = (JsonValue*)PyCapsule_GetPointer(capsule, "JsonValue");
    if (!value) {
        PyErr_SetString(PyExc_ValueError, "Invalid or NULL JSON value in capsule");
        return NULL;
    }
    
    char* result = json_stringify(value, pretty);
    if (!result) {
        JsonError* error = json_get_last_error();
        char error_msg[1024];
        snprintf(error_msg, sizeof(error_msg), "Stringify Error: %s", error->message);
        PyErr_SetString(PyExc_RuntimeError, error_msg);
        return NULL;
    }
    
    PyObject* py_result = PyUnicode_FromString(result);
    free(result);
    
    if (!py_result) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to create Python string from JSON");
        return NULL;
    }
    
    return py_result;
}

static PyObject* py_to_xml(PyObject* self, PyObject* args) {
    PyObject* capsule;
    if (!PyArg_ParseTuple(args, "O", &capsule)) {
        return NULL;
    }
    
    if (!capsule || !PyCapsule_CheckExact(capsule)) {
        PyErr_SetString(PyExc_TypeError, "Argument must be a valid JsonValue capsule");
        return NULL;
    }
    
    JsonValue* value = (JsonValue*)PyCapsule_GetPointer(capsule, "JsonValue");
    if (!value) {
        PyErr_SetString(PyExc_ValueError, "Invalid or NULL JSON value in capsule");
        return NULL;
    }
    
    char* result = json_to_xml(value);
    if (!result) {
        JsonError* error = json_get_last_error();
        char error_msg[1024];
        snprintf(error_msg, sizeof(error_msg), "XML Conversion Error: %s", error->message);
        PyErr_SetString(PyExc_RuntimeError, error_msg);
        return NULL;
    }
    
    PyObject* py_result = PyUnicode_FromString(result);
    free(result);
    
    if (!py_result) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to create Python string from XML");
        return NULL;
    }
    
    return py_result;
}

static PyObject* py_to_yaml(PyObject* self, PyObject* args) {
    PyObject* capsule;
    if (!PyArg_ParseTuple(args, "O", &capsule)) {
        return NULL;
    }
    
    if (!capsule || !PyCapsule_CheckExact(capsule)) {
        PyErr_SetString(PyExc_TypeError, "Argument must be a valid JsonValue capsule");
        return NULL;
    }
    
    JsonValue* value = (JsonValue*)PyCapsule_GetPointer(capsule, "JsonValue");
    if (!value) {
        PyErr_SetString(PyExc_ValueError, "Invalid or NULL JSON value in capsule");
        return NULL;
    }
    
    char* result = json_to_yaml(value);
    if (!result) {
        JsonError* error = json_get_last_error();
        char error_msg[1024];
        snprintf(error_msg, sizeof(error_msg), "YAML Conversion Error: %s", error->message);
        PyErr_SetString(PyExc_RuntimeError, error_msg);
        return NULL;
    }
    
    PyObject* py_result = PyUnicode_FromString(result);
    free(result);
    
    if (!py_result) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to create Python string from YAML");
        return NULL;
    }
    
    return py_result;
}

static PyObject* py_to_csv(PyObject* self, PyObject* args) {
    PyObject* capsule;
    if (!PyArg_ParseTuple(args, "O", &capsule)) {
        return NULL;
    }
    
    JsonValue* value = (JsonValue*)PyCapsule_GetPointer(capsule, "JsonValue");
    if (!value) {
        PyErr_SetString(PyExc_ValueError, "Invalid JSON value");
        return NULL;
    }
    
    char* result = json_to_csv(value);
    if (!result) {
        PyErr_SetString(PyExc_ValueError, "Cannot convert to CSV");
        return NULL;
    }
    
    PyObject* py_result = PyUnicode_FromString(result);
    free(result);
    
    return py_result;
}

static PyObject* py_to_ini(PyObject* self, PyObject* args) {
    PyObject* capsule;
    if (!PyArg_ParseTuple(args, "O", &capsule)) {
        return NULL;
    }
    
    JsonValue* value = (JsonValue*)PyCapsule_GetPointer(capsule, "JsonValue");
    if (!value) {
        PyErr_SetString(PyExc_ValueError, "Invalid JSON value");
        return NULL;
    }
    
    char* result = json_to_ini(value);
    if (!result) {
        PyErr_SetString(PyExc_ValueError, "Cannot convert to INI");
        return NULL;
    }
    
    PyObject* py_result = PyUnicode_FromString(result);
    free(result);
    
    return py_result;
}

static PyObject* py_validate(PyObject* self, PyObject* args) {
    const char* json_string;
    if (!PyArg_ParseTuple(args, "s", &json_string)) {
        return NULL;
    }
    
    bool valid = json_validate(json_string);
    return PyBool_FromLong(valid);
}

static PyObject* py_free(PyObject* self, PyObject* args) {
    PyObject* capsule;
    if (!PyArg_ParseTuple(args, "O", &capsule)) {
        return NULL;
    }
    
    JsonValue* value = (JsonValue*)PyCapsule_GetPointer(capsule, "JsonValue");
    if (value) {
        json_free(value);
    }
    
    Py_RETURN_NONE;
}

static PyObject* py_parse_file(PyObject* self, PyObject* args) {
    const char* filename;
    if (!PyArg_ParseTuple(args, "s", &filename)) {
        return NULL;
    }
    
    if (!filename || strlen(filename) == 0) {
        PyErr_SetString(PyExc_ValueError, "Filename cannot be NULL or empty");
        return NULL;
    }
    
    JsonValue* value = json_parse_file(filename);
    if (!value) {
        JsonError* error = json_get_last_error();
        char error_msg[1024];
        snprintf(error_msg, sizeof(error_msg), "File Parse Error: %s", error->message);
        PyErr_SetString(PyExc_IOError, error_msg);
        return NULL;
    }
    
    PyObject* result = PyCapsule_New(value, "JsonValue", json_value_destructor);
    if (!result) {
        json_free(value);
        PyErr_SetString(PyExc_RuntimeError, "Failed to create capsule for parsed value");
        return NULL;
    }
    
    return result;
}

static PyObject* py_save_file(PyObject* self, PyObject* args) {
    const char* filename;
    PyObject* capsule;
    int pretty = 1;
    
    if (!PyArg_ParseTuple(args, "sO|i", &filename, &capsule, &pretty)) {
        return NULL;
    }
    
    if (!filename || strlen(filename) == 0) {
        PyErr_SetString(PyExc_ValueError, "Filename cannot be NULL or empty");
        return NULL;
    }
    
    if (!capsule || !PyCapsule_CheckExact(capsule)) {
        PyErr_SetString(PyExc_TypeError, "Second argument must be a valid JsonValue capsule");
        return NULL;
    }
    
    JsonValue* value = (JsonValue*)PyCapsule_GetPointer(capsule, "JsonValue");
    if (!value) {
        PyErr_SetString(PyExc_ValueError, "Invalid or NULL JSON value in capsule");
        return NULL;
    }
    
    bool success = json_save_file(filename, value, pretty);
    if (!success) {
        JsonError* error = json_get_last_error();
        char error_msg[1024];
        snprintf(error_msg, sizeof(error_msg), "File Save Error: %s", error->message);
        PyErr_SetString(PyExc_IOError, error_msg);
        return NULL;
    }
    
    return PyBool_FromLong(success);
}

static PyObject* py_xml_to_json(PyObject* self, PyObject* args) {
    const char* xml;
    if (!PyArg_ParseTuple(args, "s", &xml)) return NULL;
    
    JsonValue* value = xml_to_json(xml);
    if (!value) {
        PyErr_SetString(PyExc_ValueError, "Invalid XML");
        return NULL;
    }
    
    return (PyObject*)PyCapsule_New(value, "JsonValue", json_value_destructor);
}

static PyObject* py_yaml_to_json(PyObject* self, PyObject* args) {
    const char* yaml;
    if (!PyArg_ParseTuple(args, "s", &yaml)) return NULL;
    
    JsonValue* value = yaml_to_json(yaml);
    if (!value) {
        PyErr_SetString(PyExc_ValueError, "Invalid YAML");
        return NULL;
    }
    
    return (PyObject*)PyCapsule_New(value, "JsonValue", json_value_destructor);
}

static PyObject* py_csv_to_json(PyObject* self, PyObject* args) {
    const char* csv;
    if (!PyArg_ParseTuple(args, "s", &csv)) return NULL;
    
    JsonValue* value = csv_to_json(csv);
    if (!value) {
        PyErr_SetString(PyExc_ValueError, "Invalid CSV");
        return NULL;
    }
    
    return (PyObject*)PyCapsule_New(value, "JsonValue", json_value_destructor);
}

static PyObject* py_ini_to_json(PyObject* self, PyObject* args) {
    const char* ini;
    if (!PyArg_ParseTuple(args, "s", &ini)) return NULL;
    
    JsonValue* value = ini_to_json(ini);
    if (!value) {
        PyErr_SetString(PyExc_ValueError, "Invalid INI");
        return NULL;
    }
    
    return (PyObject*)PyCapsule_New(value, "JsonValue", json_value_destructor);
}

static PyObject* py_to_sqlite(PyObject* self, PyObject* args) {
    PyObject* capsule;
    const char* db_path;
    
    if (!PyArg_ParseTuple(args, "Os", &capsule, &db_path)) return NULL;
    
    JsonValue* value = (JsonValue*)PyCapsule_GetPointer(capsule, "JsonValue");
    if (!value) {
        PyErr_SetString(PyExc_ValueError, "Invalid JSON value");
        return NULL;
    }
    
    JsonSqliteDB* db = json_to_sqlite(value, db_path);
    if (!db) {
        PyErr_SetString(PyExc_IOError, "Cannot create SQLite database");
        return NULL;
    }
    
    return (PyObject*)PyCapsule_New(db, "JsonSqliteDB", sqlite_db_destructor);
}

static PyObject* py_sqlite_insert(PyObject* self, PyObject* args) {
    PyObject* db_capsule;
    const char* table;
    PyObject* value_capsule;
    
    if (!PyArg_ParseTuple(args, "OsO", &db_capsule, &table, &value_capsule)) return NULL;
    
    JsonSqliteDB* db = (JsonSqliteDB*)PyCapsule_GetPointer(db_capsule, "JsonSqliteDB");
    JsonValue* value = (JsonValue*)PyCapsule_GetPointer(value_capsule, "JsonValue");
    
    bool success = sqlite_insert(db, table, value);
    return PyBool_FromLong(success);
}

static PyObject* py_sqlite_query(PyObject* self, PyObject* args) {
    PyObject* db_capsule;
    const char* table;
    const char* key;
    const char* value;
    
    if (!PyArg_ParseTuple(args, "Osss", &db_capsule, &table, &key, &value)) return NULL;
    
    JsonSqliteDB* db = (JsonSqliteDB*)PyCapsule_GetPointer(db_capsule, "JsonSqliteDB");
    JsonValue* result = sqlite_query(db, table, key, value);
    
    return (PyObject*)PyCapsule_New(result, "JsonValue", json_value_destructor);
}

static PyObject* py_sqlite_close(PyObject* self, PyObject* args) {
    PyObject* capsule;
    if (!PyArg_ParseTuple(args, "O", &capsule)) return NULL;
    
    JsonSqliteDB* db = (JsonSqliteDB*)PyCapsule_GetPointer(capsule, "JsonSqliteDB");
    if (db) sqlite_close(db);
    
    Py_RETURN_NONE;
}

static PyObject* py_path_query(PyObject* self, PyObject* args) {
    PyObject* capsule;
    const char* path;
    
    if (!PyArg_ParseTuple(args, "Os", &capsule, &path)) return NULL;
    
    JsonValue* root = (JsonValue*)PyCapsule_GetPointer(capsule, "JsonValue");
    JsonValue* result = json_path_query(root, path);
    
    if (!result) Py_RETURN_NONE;
    
    return (PyObject*)PyCapsule_New(result, "JsonValue", json_value_destructor);
}

static PyObject* py_get_last_error(PyObject* self, PyObject* args) {
    JsonError* error = json_get_last_error();
    
    if (error->code == JSON_ERROR_NONE) {
        Py_RETURN_NONE;
    }
    
    PyObject* error_dict = PyDict_New();
    PyDict_SetItemString(error_dict, "code", PyLong_FromLong(error->code));
    PyDict_SetItemString(error_dict, "message", PyUnicode_FromString(error->message));
    PyDict_SetItemString(error_dict, "line", PyLong_FromSize_t(error->line));
    PyDict_SetItemString(error_dict, "column", PyLong_FromSize_t(error->column));
    
    return error_dict;
}

static PyObject* py_clear_error(PyObject* self, PyObject* args) {
    json_clear_error();
    Py_RETURN_NONE;
}

static PyObject* py_array_size(PyObject* self, PyObject* args) {
    PyObject* capsule;
    if (!PyArg_ParseTuple(args, "O", &capsule)) return NULL;
    
    JsonValue* value = (JsonValue*)PyCapsule_GetPointer(capsule, "JsonValue");
    if (!value || value->type != JSON_ARRAY) {
        PyErr_SetString(PyExc_TypeError, "Not an array");
        return NULL;
    }
    
    return PyLong_FromSize_t(json_array_size(value));
}

static PyObject* py_object_size(PyObject* self, PyObject* args) {
    PyObject* capsule;
    if (!PyArg_ParseTuple(args, "O", &capsule)) return NULL;
    
    JsonValue* value = (JsonValue*)PyCapsule_GetPointer(capsule, "JsonValue");
    if (!value || value->type != JSON_OBJECT) {
        PyErr_SetString(PyExc_TypeError, "Not an object");
        return NULL;
    }
    
    return PyLong_FromSize_t(json_object_size(value));
}

static PyObject* py_object_has(PyObject* self, PyObject* args) {
    PyObject* capsule;
    const char* key;
    
    if (!PyArg_ParseTuple(args, "Os", &capsule, &key)) return NULL;
    
    JsonValue* value = (JsonValue*)PyCapsule_GetPointer(capsule, "JsonValue");
    if (!value) {
        PyErr_SetString(PyExc_ValueError, "Invalid JSON value");
        return NULL;
    }
    
    return PyBool_FromLong(json_object_has(value, key));
}

static PyObject* py_array_get(PyObject* self, PyObject* args) {
    PyObject* capsule;
    size_t index;
    
    if (!PyArg_ParseTuple(args, "On", &capsule, &index)) {
        return NULL;
    }
    
    if (!capsule || !PyCapsule_CheckExact(capsule)) {
        PyErr_SetString(PyExc_TypeError, "First argument must be a valid JsonValue capsule");
        return NULL;
    }
    
    JsonValue* array = (JsonValue*)PyCapsule_GetPointer(capsule, "JsonValue");
    if (!array) {
        PyErr_SetString(PyExc_ValueError, "Invalid or NULL JSON value in capsule");
        return NULL;
    }
    
    if (array->type != JSON_ARRAY) {
        PyErr_SetString(PyExc_TypeError, "Value is not a JSON array");
        return NULL;
    }
    
    JsonValue* item = json_array_get(array, index);
    if (!item) {
        JsonError* err = json_get_last_error();
        char error_msg[512];
        snprintf(error_msg, sizeof(error_msg), "Array access error: %s", err->message);
        PyErr_SetString(PyExc_IndexError, error_msg);
        return NULL;
    }
    
    // Note: We don't use destructor here because item is owned by the array
    PyObject* result = PyCapsule_New(item, "JsonValue", NULL);
    if (!result) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to create capsule for array item");
        return NULL;
    }
    
    return result;
}

static PyObject* py_deep_copy(PyObject* self, PyObject* args) {
    PyObject* capsule;
    if (!PyArg_ParseTuple(args, "O", &capsule)) return NULL;
    
    JsonValue* value = (JsonValue*)PyCapsule_GetPointer(capsule, "JsonValue");
    if (!value) {
        PyErr_SetString(PyExc_ValueError, "Invalid JSON value");
        return NULL;
    }
    
    JsonValue* copy = json_deep_copy(value);
    if (!copy) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to deep copy");
        return NULL;
    }
    
    return (PyObject*)PyCapsule_New(copy, "JsonValue", json_value_destructor);
}

static PyObject* py_equals(PyObject* self, PyObject* args) {
    PyObject* capsule1;
    PyObject* capsule2;
    
    if (!PyArg_ParseTuple(args, "OO", &capsule1, &capsule2)) return NULL;
    
    JsonValue* val1 = (JsonValue*)PyCapsule_GetPointer(capsule1, "JsonValue");
    JsonValue* val2 = (JsonValue*)PyCapsule_GetPointer(capsule2, "JsonValue");
    
    return PyBool_FromLong(json_equals(val1, val2));
}

static PyObject* py_merge(PyObject* self, PyObject* args) {
    PyObject* capsule1;
    PyObject* capsule2;
    
    if (!PyArg_ParseTuple(args, "OO", &capsule1, &capsule2)) return NULL;
    
    JsonValue* obj1 = (JsonValue*)PyCapsule_GetPointer(capsule1, "JsonValue");
    JsonValue* obj2 = (JsonValue*)PyCapsule_GetPointer(capsule2, "JsonValue");
    
    JsonValue* merged = json_merge(obj1, obj2);
    if (!merged) {
        PyErr_SetString(PyExc_ValueError, "Failed to merge objects");
        return NULL;
    }
    
    return (PyObject*)PyCapsule_New(merged, "JsonValue", json_value_destructor);
}

static PyObject* py_memory_usage(PyObject* self, PyObject* args) {
    PyObject* capsule;
    if (!PyArg_ParseTuple(args, "O", &capsule)) return NULL;
    
    JsonValue* value = (JsonValue*)PyCapsule_GetPointer(capsule, "JsonValue");
    if (!value) {
        PyErr_SetString(PyExc_ValueError, "Invalid JSON value");
        return NULL;
    }
    
    return PyLong_FromSize_t(json_memory_usage(value));
}

static PyObject* py_optimize_memory(PyObject* self, PyObject* args) {
    PyObject* capsule;
    if (!PyArg_ParseTuple(args, "O", &capsule)) return NULL;
    
    JsonValue* value = (JsonValue*)PyCapsule_GetPointer(capsule, "JsonValue");
    if (!value) {
        PyErr_SetString(PyExc_ValueError, "Invalid JSON value");
        return NULL;
    }
    
    json_optimize_memory(value);
    Py_RETURN_NONE;
}

static PyObject* py_validate_schema(PyObject* self, PyObject* args) {
    PyObject* data_capsule;
    PyObject* schema_capsule;
    
    if (!PyArg_ParseTuple(args, "OO", &data_capsule, &schema_capsule)) return NULL;
    
    JsonValue* data = (JsonValue*)PyCapsule_GetPointer(data_capsule, "JsonValue");
    JsonValue* schema = (JsonValue*)PyCapsule_GetPointer(schema_capsule, "JsonValue");
    
    return PyBool_FromLong(json_validate_schema(data, schema));
}

static PyObject* py_diff(PyObject* self, PyObject* args) {
    PyObject* capsule1;
    PyObject* capsule2;
    
    if (!PyArg_ParseTuple(args, "OO", &capsule1, &capsule2)) return NULL;
    
    JsonValue* val1 = (JsonValue*)PyCapsule_GetPointer(capsule1, "JsonValue");
    JsonValue* val2 = (JsonValue*)PyCapsule_GetPointer(capsule2, "JsonValue");
    
    JsonValue* diff = json_diff(val1, val2);
    return (PyObject*)PyCapsule_New(diff, "JsonValue", json_value_destructor);
}

static PyObject* py_patch(PyObject* self, PyObject* args) {
    PyObject* target_capsule;
    PyObject* patch_capsule;
    
    if (!PyArg_ParseTuple(args, "OO", &target_capsule, &patch_capsule)) return NULL;
    
    JsonValue* target = (JsonValue*)PyCapsule_GetPointer(target_capsule, "JsonValue");
    JsonValue* patch = (JsonValue*)PyCapsule_GetPointer(patch_capsule, "JsonValue");
    
    JsonValue* result = json_patch(target, patch);
    return (PyObject*)PyCapsule_New(result, "JsonValue", json_value_destructor);
}

typedef struct {
    PyObject* callback;
    PyObject* results;
} StreamCallbackData;

static bool stream_python_callback(JsonStreamEvent* event, void* user_data) {
    StreamCallbackData* data = (StreamCallbackData*)user_data;
    
    PyGILState_STATE gstate = PyGILState_Ensure();
    
    PyObject* event_dict = PyDict_New();
    if (!event_dict) {
        PyGILState_Release(gstate);
        return false;
    }
    
    const char* event_type_str = "unknown";
    switch (event->type) {
        case JSON_EVENT_OBJECT_START: event_type_str = "object_start"; break;
        case JSON_EVENT_OBJECT_END: event_type_str = "object_end"; break;
        case JSON_EVENT_ARRAY_START: event_type_str = "array_start"; break;
        case JSON_EVENT_ARRAY_END: event_type_str = "array_end"; break;
        case JSON_EVENT_KEY: event_type_str = "key"; break;
        case JSON_EVENT_VALUE: event_type_str = "value"; break;
        case JSON_EVENT_ERROR: event_type_str = "error"; break;
        case JSON_EVENT_EOF: event_type_str = "eof"; break;
    }
    
    PyObject* type_str = PyUnicode_FromString(event_type_str);
    if (!type_str) {
        Py_DECREF(event_dict);
        PyGILState_Release(gstate);
        return false;
    }
    PyDict_SetItemString(event_dict, "type", type_str);
    Py_DECREF(type_str);
    
    if (event->key) {
        PyObject* key_str = PyUnicode_FromString(event->key);
        if (!key_str) {
            Py_DECREF(event_dict);
            PyGILState_Release(gstate);
            return false;
        }
        PyDict_SetItemString(event_dict, "key", key_str);
        Py_DECREF(key_str);
    } else {
        PyDict_SetItemString(event_dict, "key", Py_None);
    }
    
    if (event->value) {
        PyObject* value_capsule = PyCapsule_New(event->value, "JsonValue", NULL);
        if (!value_capsule) {
            Py_DECREF(event_dict);
            PyGILState_Release(gstate);
            return false;
        }
        PyDict_SetItemString(event_dict, "value", value_capsule);
        Py_DECREF(value_capsule);
    } else {
        PyDict_SetItemString(event_dict, "value", Py_None);
    }
    
    bool continue_parsing = true;
    
    if (data->callback && data->callback != Py_None) {
        PyObject* result = PyObject_CallFunction(data->callback, "O", event_dict);
        Py_DECREF(event_dict);
        
        if (!result) {
            PyGILState_Release(gstate);
            return false;
        }
        
        continue_parsing = PyObject_IsTrue(result);
        Py_DECREF(result);
    } else {
        PyList_Append(data->results, event_dict);
        Py_DECREF(event_dict);
    }
    
    PyGILState_Release(gstate);
    return continue_parsing;
}

static PyObject* py_stream_parse_chunk(PyObject* self, PyObject* args) {
    const char* chunk;
    Py_ssize_t length;
    PyObject* callback = Py_None;
    
    if (!PyArg_ParseTuple(args, "s#|O", &chunk, &length, &callback)) {
        return NULL;
    }
    
    if (!chunk) {
        PyErr_SetString(PyExc_ValueError, "Chunk data cannot be NULL");
        return NULL;
    }
    
    if (length < 0) {
        PyErr_SetString(PyExc_ValueError, "Chunk length cannot be negative");
        return NULL;
    }
    
    if (callback != Py_None && !PyCallable_Check(callback)) {
        PyErr_SetString(PyExc_TypeError, "Callback must be callable or None");
        return NULL;
    }
    
    PyObject* results_list = PyList_New(0);
    if (!results_list) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to create results list");
        return NULL;
    }
    
    StreamCallbackData data = {callback, results_list};
    
    JsonStreamParser* parser = json_stream_parser_create(stream_python_callback, &data);
    if (!parser) {
        JsonError* error = json_get_last_error();
        char error_msg[1024];
        snprintf(error_msg, sizeof(error_msg), "Failed to create stream parser: %s", error->message);
        PyErr_SetString(PyExc_RuntimeError, error_msg);
        Py_DECREF(results_list);
        return NULL;
    }
    
    bool success = json_stream_parse_chunk(parser, chunk, length);
    json_stream_parser_free(parser);
    
    if (!success) {
        JsonError* error = json_get_last_error();
        char error_msg[1024];
        snprintf(error_msg, sizeof(error_msg), "Stream Parse Error at line %zu, column %zu: %s", 
                 error->line, error->column, error->message);
        PyErr_SetString(PyExc_ValueError, error_msg);
        Py_DECREF(results_list);
        return NULL;
    }
    
    return results_list;
}

static PyObject* py_stream_parse_file(PyObject* self, PyObject* args) {
    const char* filename;
    PyObject* callback = Py_None;
    
    if (!PyArg_ParseTuple(args, "s|O", &filename, &callback)) return NULL;
    
    StreamCallbackData data = {callback, PyList_New(0)};
    
    JsonStreamParser* parser = json_stream_parser_create(stream_python_callback, &data);
    if (!parser) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to create stream parser");
        Py_DECREF(data.results);
        return NULL;
    }
    
    bool success = json_stream_parse_file(parser, filename);
    json_stream_parser_free(parser);
    
    if (!success) {
        JsonError* error = json_get_last_error();
        PyErr_SetString(PyExc_IOError, error->message);
        Py_DECREF(data.results);
        return NULL;
    }
    
    return data.results;
}

static PyObject* py_array_remove(PyObject* self, PyObject* args) {
    PyObject* capsule;
    size_t index;
    
    if (!PyArg_ParseTuple(args, "On", &capsule, &index)) return NULL;
    
    JsonValue* array = (JsonValue*)PyCapsule_GetPointer(capsule, "JsonValue");
    if (!array) {
        PyErr_SetString(PyExc_ValueError, "Invalid JSON array");
        return NULL;
    }
    
    bool success = json_array_remove(array, index);
    return PyBool_FromLong(success);
}

static PyObject* py_object_remove(PyObject* self, PyObject* args) {
    PyObject* capsule;
    const char* key;
    
    if (!PyArg_ParseTuple(args, "Os", &capsule, &key)) return NULL;
    
    JsonValue* object = (JsonValue*)PyCapsule_GetPointer(capsule, "JsonValue");
    if (!object) {
        PyErr_SetString(PyExc_ValueError, "Invalid JSON object");
        return NULL;
    }
    
    bool success = json_object_remove(object, key);
    return PyBool_FromLong(success);
}

static PyObject* py_object_keys(PyObject* self, PyObject* args) {
    PyObject* capsule;
    
    if (!PyArg_ParseTuple(args, "O", &capsule)) return NULL;
    
    JsonValue* object = (JsonValue*)PyCapsule_GetPointer(capsule, "JsonValue");
    if (!object) {
        PyErr_SetString(PyExc_ValueError, "Invalid JSON object");
        return NULL;
    }
    
    size_t count;
    const char** keys = json_object_keys(object, &count);
    
    if (!keys) {
        Py_RETURN_NONE;
    }
    
    PyObject* keys_list = PyList_New(count);
    for (size_t i = 0; i < count; i++) {
        PyList_SetItem(keys_list, i, PyUnicode_FromString(keys[i]));
    }
    
    free(keys);
    return keys_list;
}

static PyObject* py_create_null(PyObject* self, PyObject* args) {
    JsonValue* value = json_create_null();
    if (!value) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to create null value");
        return NULL;
    }
    return (PyObject*)PyCapsule_New(value, "JsonValue", NULL);
}

static PyObject* py_create_bool(PyObject* self, PyObject* args) {
    int val;
    if (!PyArg_ParseTuple(args, "p", &val)) return NULL;
    
    JsonValue* value = json_create_bool(val);
    if (!value) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to create bool value");
        return NULL;
    }
    return (PyObject*)PyCapsule_New(value, "JsonValue", NULL);
}

static PyObject* py_create_number(PyObject* self, PyObject* args) {
    double val;
    if (!PyArg_ParseTuple(args, "d", &val)) return NULL;
    
    JsonValue* value = json_create_number(val);
    if (!value) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to create number value");
        return NULL;
    }
    return (PyObject*)PyCapsule_New(value, "JsonValue", NULL);
}

static PyObject* py_create_string(PyObject* self, PyObject* args) {
    const char* val;
    if (!PyArg_ParseTuple(args, "s", &val)) return NULL;
    
    JsonValue* value = json_create_string(val);
    if (!value) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to create string value");
        return NULL;
    }
    return (PyObject*)PyCapsule_New(value, "JsonValue", NULL);
}

static PyObject* py_create_array(PyObject* self, PyObject* args) {
    JsonValue* value = json_create_array();
    if (!value) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to create array");
        return NULL;
    }
    return (PyObject*)PyCapsule_New(value, "JsonValue", NULL);
}

static PyObject* py_create_object(PyObject* self, PyObject* args) {
    JsonValue* value = json_create_object();
    if (!value) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to create object");
        return NULL;
    }
    return (PyObject*)PyCapsule_New(value, "JsonValue", NULL);
}

static PyObject* py_array_append(PyObject* self, PyObject* args) {
    PyObject* array_capsule;
    PyObject* value_capsule;
    
    if (!PyArg_ParseTuple(args, "OO", &array_capsule, &value_capsule)) return NULL;
    
    JsonValue* array = (JsonValue*)PyCapsule_GetPointer(array_capsule, "JsonValue");
    JsonValue* value = (JsonValue*)PyCapsule_GetPointer(value_capsule, "JsonValue");
    
    if (!array || !value) {
        PyErr_SetString(PyExc_ValueError, "Invalid JSON values");
        return NULL;
    }
    
    bool success = json_array_append(array, value);
    return PyBool_FromLong(success);
}

static PyObject* py_object_set(PyObject* self, PyObject* args) {
    PyObject* object_capsule;
    const char* key;
    PyObject* value_capsule;
    
    if (!PyArg_ParseTuple(args, "OsO", &object_capsule, &key, &value_capsule)) return NULL;
    
    JsonValue* object = (JsonValue*)PyCapsule_GetPointer(object_capsule, "JsonValue");
    JsonValue* value = (JsonValue*)PyCapsule_GetPointer(value_capsule, "JsonValue");
    
    if (!object || !value) {
        PyErr_SetString(PyExc_ValueError, "Invalid JSON values");
        return NULL;
    }
    
    bool success = json_object_set(object, key, value);
    return PyBool_FromLong(success);
}

static PyObject* py_object_get(PyObject* self, PyObject* args) {
    PyObject* capsule;
    const char* key;
    
    if (!PyArg_ParseTuple(args, "Os", &capsule, &key)) return NULL;
    
    JsonValue* object = (JsonValue*)PyCapsule_GetPointer(capsule, "JsonValue");
    if (!object) {
        PyErr_SetString(PyExc_ValueError, "Invalid JSON object");
        return NULL;
    }
    
    JsonValue* value = json_object_get(object, key);
    if (!value) {
        Py_RETURN_NONE;
    }
    
    return (PyObject*)PyCapsule_New(value, "JsonValue", NULL);
}

static PyObject* py_get_type(PyObject* self, PyObject* args) {
    PyObject* capsule;
    if (!PyArg_ParseTuple(args, "O", &capsule)) return NULL;
    
    JsonValue* value = (JsonValue*)PyCapsule_GetPointer(capsule, "JsonValue");
    if (!value) {
        PyErr_SetString(PyExc_ValueError, "Invalid JSON value");
        return NULL;
    }
    
    const char* type_str;
    switch (value->type) {
        case JSON_NULL: type_str = "null"; break;
        case JSON_BOOL: type_str = "bool"; break;
        case JSON_NUMBER: type_str = "number"; break;
        case JSON_STRING: type_str = "string"; break;
        case JSON_ARRAY: type_str = "array"; break;
        case JSON_OBJECT: type_str = "object"; break;
        default: type_str = "unknown"; break;
    }
    
    return PyUnicode_FromString(type_str);
}

static PyObject* json_value_to_python_helper(JsonValue* value);

static PyObject* json_value_to_python_helper(JsonValue* value) {
    if (!value) {
        Py_RETURN_NONE;
    }
    
    switch (value->type) {
        case JSON_NULL:
            Py_RETURN_NONE;
        case JSON_BOOL:
            return PyBool_FromLong(value->data.bool_value);
        case JSON_NUMBER:
            return PyFloat_FromDouble(value->data.number_value);
        case JSON_STRING:
            return PyUnicode_FromString(value->data.string_value);
        case JSON_ARRAY: {
            JsonArray* arr = value->data.array_value;
            PyObject* py_list = PyList_New(arr->count);
            if (!py_list) return NULL;
            
            for (size_t i = 0; i < arr->count; i++) {
                PyObject* item = json_value_to_python_helper(arr->values[i]);
                if (!item) {
                    Py_DECREF(py_list);
                    return NULL;
                }
                PyList_SetItem(py_list, i, item);
            }
            return py_list;
        }
        case JSON_OBJECT: {
            JsonObject* obj = value->data.object_value;
            PyObject* py_dict = PyDict_New();
            if (!py_dict) return NULL;
            
            for (size_t i = 0; i < obj->count; i++) {
                PyObject* py_val = json_value_to_python_helper(obj->pairs[i].value);
                if (!py_val) {
                    Py_DECREF(py_dict);
                    return NULL;
                }
                PyDict_SetItemString(py_dict, obj->pairs[i].key, py_val);
                Py_DECREF(py_val);
            }
            return py_dict;
        }
    }
    
    Py_RETURN_NONE;
}

static PyObject* py_to_python(PyObject* self, PyObject* args) {
    PyObject* capsule;
    if (!PyArg_ParseTuple(args, "O", &capsule)) return NULL;
    
    JsonValue* value = (JsonValue*)PyCapsule_GetPointer(capsule, "JsonValue");
    if (!value) {
        PyErr_SetString(PyExc_ValueError, "Invalid JSON value");
        return NULL;
    }
    
    return json_value_to_python_helper(value);
}

static JsonValue* python_object_to_json_with_visited(PyObject* obj, PyObject* visited);

static JsonValue* python_object_to_json_with_visited(PyObject* obj, PyObject* visited) {
    JsonValue* value = NULL;
    
    if (obj == Py_None) {
        value = json_create_null();
    } else if (PyBool_Check(obj)) {
        value = json_create_bool(obj == Py_True);
    } else if (PyLong_Check(obj)) {
        value = json_create_number(PyLong_AsDouble(obj));
    } else if (PyFloat_Check(obj)) {
        value = json_create_number(PyFloat_AsDouble(obj));
    } else if (PyUnicode_Check(obj)) {
        const char* str = PyUnicode_AsUTF8(obj);
        if (!str) return NULL;
        value = json_create_string(str);
    } else if (PyList_Check(obj)) {
        PyObject* obj_id = PyLong_FromVoidPtr(obj);
        int contains = PySet_Contains(visited, obj_id);
        
        if (contains == 1) {
            Py_DECREF(obj_id);
            PyErr_SetString(PyExc_ValueError, "Circular reference detected in list");
            return NULL;
        } else if (contains == -1) {
            Py_DECREF(obj_id);
            return NULL;
        }
        
        PySet_Add(visited, obj_id);
        Py_DECREF(obj_id);
        
        value = json_create_array();
        if (!value) return NULL;
        
        Py_ssize_t size = PyList_Size(obj);
        for (Py_ssize_t i = 0; i < size; i++) {
            PyObject* item = PyList_GetItem(obj, i);
            JsonValue* val = python_object_to_json_with_visited(item, visited);
            if (!val) {
                json_free(value);
                return NULL;
            }
            json_array_append(value, val);
        }
    } else if (PyDict_Check(obj)) {
        PyObject* obj_id = PyLong_FromVoidPtr(obj);
        int contains = PySet_Contains(visited, obj_id);
        
        if (contains == 1) {
            Py_DECREF(obj_id);
            PyErr_SetString(PyExc_ValueError, "Circular reference detected in dict");
            return NULL;
        } else if (contains == -1) {
            Py_DECREF(obj_id);
            return NULL;
        }
        
        PySet_Add(visited, obj_id);
        Py_DECREF(obj_id);
        
        value = json_create_object();
        if (!value) return NULL;
        
        PyObject *key, *val;
        Py_ssize_t pos = 0;
        while (PyDict_Next(obj, &pos, &key, &val)) {
            const char* key_str = PyUnicode_AsUTF8(key);
            if (!key_str) {
                json_free(value);
                return NULL;
            }
            
            JsonValue* json_val = python_object_to_json_with_visited(val, visited);
            if (!json_val) {
                json_free(value);
                return NULL;
            }
            json_object_set(value, key_str, json_val);
        }
    } else {
        PyErr_SetString(PyExc_TypeError, "Unsupported Python type");
        return NULL;
    }
    
    return value;
}

static JsonValue* python_object_to_json_helper(PyObject* obj) {
    PyObject* visited = PySet_New(NULL);
    if (!visited) return NULL;
    
    JsonValue* result = python_object_to_json_with_visited(obj, visited);
    Py_DECREF(visited);
    
    return result;
}

static PyObject* py_from_python(PyObject* self, PyObject* args) {
    PyObject* obj;
    if (!PyArg_ParseTuple(args, "O", &obj)) return NULL;
    
    JsonValue* value = python_object_to_json_helper(obj);
    if (!value) {
        if (!PyErr_Occurred()) {
            PyErr_SetString(PyExc_RuntimeError, "Failed to create JSON value");
        }
        return NULL;
    }
    
    return (PyObject*)PyCapsule_New(value, "JsonValue", json_value_destructor);
}

static PyObject* py_array_insert(PyObject* self, PyObject* args) {
    PyObject* array_capsule;
    size_t index;
    PyObject* value_capsule;
    
    if (!PyArg_ParseTuple(args, "OnO", &array_capsule, &index, &value_capsule)) return NULL;
    
    JsonValue* array = (JsonValue*)PyCapsule_GetPointer(array_capsule, "JsonValue");
    JsonValue* value = (JsonValue*)PyCapsule_GetPointer(value_capsule, "JsonValue");
    
    if (!array || array->type != JSON_ARRAY) {
        PyErr_SetString(PyExc_TypeError, "Not an array");
        return NULL;
    }
    
    JsonArray* arr = array->data.array_value;
    if (index > arr->count) {
        PyErr_SetString(PyExc_IndexError, "Index out of range");
        return NULL;
    }
    
    if (arr->count >= arr->capacity) {
        size_t new_capacity = arr->capacity == 0 ? 16 : arr->capacity * 2;
        JsonValue** new_values = realloc(arr->values, new_capacity * sizeof(JsonValue*));
        if (!new_values) {
            PyErr_SetString(PyExc_MemoryError, "Failed to expand array");
            return NULL;
        }
        memset(new_values + arr->capacity, 0, (new_capacity - arr->capacity) * sizeof(JsonValue*));
        arr->values = new_values;
        arr->capacity = new_capacity;
    }
    
    for (size_t i = arr->count; i > index; i--) {
        arr->values[i] = arr->values[i - 1];
    }
    arr->values[index] = value;
    arr->count++;
    
    Py_RETURN_TRUE;
}

static PyObject* py_sqlite_get_all(PyObject* self, PyObject* args) {
    PyObject* db_capsule;
    const char* table;
    
    if (!PyArg_ParseTuple(args, "Os", &db_capsule, &table)) return NULL;
    
    JsonSqliteDB* db = (JsonSqliteDB*)PyCapsule_GetPointer(db_capsule, "JsonSqliteDB");
    JsonValue* result = sqlite_get_all(db, table);
    
    if (!result) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to query table");
        return NULL;
    }
    
    return (PyObject*)PyCapsule_New(result, "JsonValue", json_value_destructor);
}

static PyObject* py_sqlite_update(PyObject* self, PyObject* args) {
    PyObject* db_capsule;
    const char* table;
    const char* key;
    const char* key_value;
    PyObject* new_data_capsule;
    
    if (!PyArg_ParseTuple(args, "OsssO", &db_capsule, &table, &key, &key_value, &new_data_capsule)) return NULL;
    
    JsonSqliteDB* db = (JsonSqliteDB*)PyCapsule_GetPointer(db_capsule, "JsonSqliteDB");
    JsonValue* new_data = (JsonValue*)PyCapsule_GetPointer(new_data_capsule, "JsonValue");
    
    bool success = sqlite_update(db, table, key, key_value, new_data);
    return PyBool_FromLong(success);
}

static PyObject* py_sqlite_delete(PyObject* self, PyObject* args) {
    PyObject* db_capsule;
    const char* table;
    const char* key;
    const char* value;
    
    if (!PyArg_ParseTuple(args, "Osss", &db_capsule, &table, &key, &value)) return NULL;
    
    JsonSqliteDB* db = (JsonSqliteDB*)PyCapsule_GetPointer(db_capsule, "JsonSqliteDB");
    
    bool success = sqlite_delete(db, table, key, value);
    return PyBool_FromLong(success);
}

static PyObject* py_sqlite_optimize(PyObject* self, PyObject* args) {
    PyObject* capsule;
    if (!PyArg_ParseTuple(args, "O", &capsule)) return NULL;
    
    JsonSqliteDB* db = (JsonSqliteDB*)PyCapsule_GetPointer(capsule, "JsonSqliteDB");
    if (db) sqlite_optimize(db);
    
    Py_RETURN_NONE;
}

static PyMethodDef JsonMethods[] = {
    {"parse", py_parse, METH_VARARGS, "Parse JSON string"},
    {"parse_file", py_parse_file, METH_VARARGS, "Parse JSON from file"},
    {"save_file", py_save_file, METH_VARARGS, "Save JSON to file"},
    {"stringify", py_stringify, METH_VARARGS, "Convert JSON to string"},
    {"to_xml", py_to_xml, METH_VARARGS, "Convert JSON to XML"},
    {"to_yaml", py_to_yaml, METH_VARARGS, "Convert JSON to YAML"},
    {"to_csv", py_to_csv, METH_VARARGS, "Convert JSON to CSV"},
    {"to_ini", py_to_ini, METH_VARARGS, "Convert JSON to INI"},
    {"xml_to_json", py_xml_to_json, METH_VARARGS, "Convert XML to JSON"},
    {"yaml_to_json", py_yaml_to_json, METH_VARARGS, "Convert YAML to JSON"},
    {"csv_to_json", py_csv_to_json, METH_VARARGS, "Convert CSV to JSON"},
    {"ini_to_json", py_ini_to_json, METH_VARARGS, "Convert INI to JSON"},
    {"to_sqlite", py_to_sqlite, METH_VARARGS, "Convert JSON to SQLite database"},
    {"sqlite_insert", py_sqlite_insert, METH_VARARGS, "Insert into SQLite"},
    {"sqlite_query", py_sqlite_query, METH_VARARGS, "Query SQLite database"},
    {"sqlite_get_all", py_sqlite_get_all, METH_VARARGS, "Get all rows from table"},
    {"sqlite_update", py_sqlite_update, METH_VARARGS, "Update SQLite row"},
    {"sqlite_delete", py_sqlite_delete, METH_VARARGS, "Delete SQLite row"},
    {"sqlite_optimize", py_sqlite_optimize, METH_VARARGS, "Optimize SQLite database"},
    {"sqlite_close", py_sqlite_close, METH_VARARGS, "Close SQLite database"},
    {"path_query", py_path_query, METH_VARARGS, "Query JSON using path"},
    {"validate", py_validate, METH_VARARGS, "Validate JSON string"},
    {"get_last_error", py_get_last_error, METH_VARARGS, "Get last error details"},
    {"clear_error", py_clear_error, METH_VARARGS, "Clear error state"},
    {"array_size", py_array_size, METH_VARARGS, "Get array size"},
    {"object_size", py_object_size, METH_VARARGS, "Get object size"},
    {"object_has", py_object_has, METH_VARARGS, "Check if object has key"},
    {"array_get", py_array_get, METH_VARARGS, "Get array element"},
    {"array_remove", py_array_remove, METH_VARARGS, "Remove array element"},
    {"array_insert", py_array_insert, METH_VARARGS, "Insert into array at index"},
    {"object_remove", py_object_remove, METH_VARARGS, "Remove object key"},
    {"object_keys", py_object_keys, METH_VARARGS, "Get all object keys"},
    {"deep_copy", py_deep_copy, METH_VARARGS, "Deep copy JSON value"},
    {"equals", py_equals, METH_VARARGS, "Compare two JSON values"},
    {"merge", py_merge, METH_VARARGS, "Merge two JSON objects"},
    {"memory_usage", py_memory_usage, METH_VARARGS, "Get memory usage"},
    {"optimize_memory", py_optimize_memory, METH_VARARGS, "Optimize memory usage"},
    {"validate_schema", py_validate_schema, METH_VARARGS, "Validate against schema"},
    {"diff", py_diff, METH_VARARGS, "Get difference between values"},
    {"patch", py_patch, METH_VARARGS, "Apply patch to value"},
    {"stream_parse_chunk", py_stream_parse_chunk, METH_VARARGS, "Parse JSON chunk in streaming mode"},
    {"stream_parse_file", py_stream_parse_file, METH_VARARGS, "Parse JSON file in streaming mode"},
    {"create_null", py_create_null, METH_VARARGS, "Create null value"},
    {"create_bool", py_create_bool, METH_VARARGS, "Create bool value"},
    {"create_number", py_create_number, METH_VARARGS, "Create number value"},
    {"create_string", py_create_string, METH_VARARGS, "Create string value"},
    {"create_array", py_create_array, METH_VARARGS, "Create array"},
    {"create_object", py_create_object, METH_VARARGS, "Create object"},
    {"array_append", py_array_append, METH_VARARGS, "Append to array"},
    {"object_set", py_object_set, METH_VARARGS, "Set object key-value"},
    {"object_get", py_object_get, METH_VARARGS, "Get object value by key"},
    {"get_type", py_get_type, METH_VARARGS, "Get JSON value type"},
    {"to_python", py_to_python, METH_VARARGS, "Convert JSON to Python object"},
    {"from_python", py_from_python, METH_VARARGS, "Convert Python object to JSON"},
    {"free", py_free, METH_VARARGS, "Free JSON value"},
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef jsonmodule = {
    PyModuleDef_HEAD_INIT,
    "cjson",
    "Advanced JSON library written in C",
    -1,
    JsonMethods
};

PyMODINIT_FUNC PyInit_cjson(void) {
    return PyModule_Create(&jsonmodule);
}
