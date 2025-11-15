#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include "json_parser.h"

static PyObject* py_parse(PyObject* self, PyObject* args) {
    const char* json_string;
    if (!PyArg_ParseTuple(args, "s", &json_string)) {
        return NULL;
    }
    
    JsonValue* value = json_parse(json_string);
    if (!value) {
        PyErr_SetString(PyExc_ValueError, "Invalid JSON");
        return NULL;
    }
    
    PyObject* result = (PyObject*)PyCapsule_New(value, "JsonValue", NULL);
    return result;
}

static PyObject* py_stringify(PyObject* self, PyObject* args) {
    PyObject* capsule;
    int pretty = 0;
    
    if (!PyArg_ParseTuple(args, "O|i", &capsule, &pretty)) {
        return NULL;
    }
    
    JsonValue* value = (JsonValue*)PyCapsule_GetPointer(capsule, "JsonValue");
    if (!value) {
        PyErr_SetString(PyExc_ValueError, "Invalid JSON value");
        return NULL;
    }
    
    char* result = json_stringify(value, pretty);
    PyObject* py_result = PyUnicode_FromString(result);
    free(result);
    
    return py_result;
}

static PyObject* py_to_xml(PyObject* self, PyObject* args) {
    PyObject* capsule;
    if (!PyArg_ParseTuple(args, "O", &capsule)) {
        return NULL;
    }
    
    JsonValue* value = (JsonValue*)PyCapsule_GetPointer(capsule, "JsonValue");
    if (!value) {
        PyErr_SetString(PyExc_ValueError, "Invalid JSON value");
        return NULL;
    }
    
    char* result = json_to_xml(value);
    PyObject* py_result = PyUnicode_FromString(result);
    free(result);
    
    return py_result;
}

static PyObject* py_to_yaml(PyObject* self, PyObject* args) {
    PyObject* capsule;
    if (!PyArg_ParseTuple(args, "O", &capsule)) {
        return NULL;
    }
    
    JsonValue* value = (JsonValue*)PyCapsule_GetPointer(capsule, "JsonValue");
    if (!value) {
        PyErr_SetString(PyExc_ValueError, "Invalid JSON value");
        return NULL;
    }
    
    char* result = json_to_yaml(value);
    PyObject* py_result = PyUnicode_FromString(result);
    free(result);
    
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
    if (!PyArg_ParseTuple(args, "s", &filename)) return NULL;
    
    JsonValue* value = json_parse_file(filename);
    if (!value) {
        PyErr_SetString(PyExc_IOError, "Cannot read file");
        return NULL;
    }
    
    return (PyObject*)PyCapsule_New(value, "JsonValue", NULL);
}

static PyObject* py_save_file(PyObject* self, PyObject* args) {
    const char* filename;
    PyObject* capsule;
    int pretty = 1;
    
    if (!PyArg_ParseTuple(args, "sO|i", &filename, &capsule, &pretty)) return NULL;
    
    JsonValue* value = (JsonValue*)PyCapsule_GetPointer(capsule, "JsonValue");
    if (!value) {
        PyErr_SetString(PyExc_ValueError, "Invalid JSON value");
        return NULL;
    }
    
    bool success = json_save_file(filename, value, pretty);
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
    
    return (PyObject*)PyCapsule_New(value, "JsonValue", NULL);
}

static PyObject* py_yaml_to_json(PyObject* self, PyObject* args) {
    const char* yaml;
    if (!PyArg_ParseTuple(args, "s", &yaml)) return NULL;
    
    JsonValue* value = yaml_to_json(yaml);
    if (!value) {
        PyErr_SetString(PyExc_ValueError, "Invalid YAML");
        return NULL;
    }
    
    return (PyObject*)PyCapsule_New(value, "JsonValue", NULL);
}

static PyObject* py_csv_to_json(PyObject* self, PyObject* args) {
    const char* csv;
    if (!PyArg_ParseTuple(args, "s", &csv)) return NULL;
    
    JsonValue* value = csv_to_json(csv);
    if (!value) {
        PyErr_SetString(PyExc_ValueError, "Invalid CSV");
        return NULL;
    }
    
    return (PyObject*)PyCapsule_New(value, "JsonValue", NULL);
}

static PyObject* py_ini_to_json(PyObject* self, PyObject* args) {
    const char* ini;
    if (!PyArg_ParseTuple(args, "s", &ini)) return NULL;
    
    JsonValue* value = ini_to_json(ini);
    if (!value) {
        PyErr_SetString(PyExc_ValueError, "Invalid INI");
        return NULL;
    }
    
    return (PyObject*)PyCapsule_New(value, "JsonValue", NULL);
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
    
    return (PyObject*)PyCapsule_New(db, "JsonSqliteDB", NULL);
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
    
    return (PyObject*)PyCapsule_New(result, "JsonValue", NULL);
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
    
    return (PyObject*)PyCapsule_New(result, "JsonValue", NULL);
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
    
    if (!PyArg_ParseTuple(args, "On", &capsule, &index)) return NULL;
    
    JsonValue* array = (JsonValue*)PyCapsule_GetPointer(capsule, "JsonValue");
    if (!array) {
        PyErr_SetString(PyExc_ValueError, "Invalid JSON array");
        return NULL;
    }
    
    JsonValue* item = json_array_get(array, index);
    if (!item) {
        JsonError* err = json_get_last_error();
        PyErr_SetString(PyExc_IndexError, err->message);
        return NULL;
    }
    
    return (PyObject*)PyCapsule_New(item, "JsonValue", NULL);
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
    
    return (PyObject*)PyCapsule_New(copy, "JsonValue", NULL);
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
    
    return (PyObject*)PyCapsule_New(merged, "JsonValue", NULL);
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
    return (PyObject*)PyCapsule_New(diff, "JsonValue", NULL);
}

static PyObject* py_patch(PyObject* self, PyObject* args) {
    PyObject* target_capsule;
    PyObject* patch_capsule;
    
    if (!PyArg_ParseTuple(args, "OO", &target_capsule, &patch_capsule)) return NULL;
    
    JsonValue* target = (JsonValue*)PyCapsule_GetPointer(target_capsule, "JsonValue");
    JsonValue* patch = (JsonValue*)PyCapsule_GetPointer(patch_capsule, "JsonValue");
    
    JsonValue* result = json_patch(target, patch);
    return (PyObject*)PyCapsule_New(result, "JsonValue", NULL);
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
    {"sqlite_close", py_sqlite_close, METH_VARARGS, "Close SQLite database"},
    {"path_query", py_path_query, METH_VARARGS, "Query JSON using path"},
    {"validate", py_validate, METH_VARARGS, "Validate JSON string"},
    {"get_last_error", py_get_last_error, METH_VARARGS, "Get last error details"},
    {"clear_error", py_clear_error, METH_VARARGS, "Clear error state"},
    {"array_size", py_array_size, METH_VARARGS, "Get array size"},
    {"object_size", py_object_size, METH_VARARGS, "Get object size"},
    {"object_has", py_object_has, METH_VARARGS, "Check if object has key"},
    {"array_get", py_array_get, METH_VARARGS, "Get array element"},
    {"deep_copy", py_deep_copy, METH_VARARGS, "Deep copy JSON value"},
    {"equals", py_equals, METH_VARARGS, "Compare two JSON values"},
    {"merge", py_merge, METH_VARARGS, "Merge two JSON objects"},
    {"memory_usage", py_memory_usage, METH_VARARGS, "Get memory usage"},
    {"optimize_memory", py_optimize_memory, METH_VARARGS, "Optimize memory usage"},
    {"validate_schema", py_validate_schema, METH_VARARGS, "Validate against schema"},
    {"diff", py_diff, METH_VARARGS, "Get difference between values"},
    {"patch", py_patch, METH_VARARGS, "Apply patch to value"},
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
