# CJSON Python API Documentation

## Installation

```bash
pip install cjson
```

## Core Functions

### json_parse(json_string: str) -> capsule
Parse a JSON string and return a JsonValue capsule.

**Parameters:**
- `json_string` (str): JSON string to parse

**Returns:**
- capsule: JsonValue object or None on error

**Raises:**
- `ValueError`: Invalid JSON syntax

```python
data = cjson.parse('{"name":"test","value":123}')
```

### json_stringify(value: capsule, pretty: bool = False) -> str
Convert a JsonValue to JSON string.

**Parameters:**
- `value` (capsule): JsonValue object
- `pretty` (bool): Enable pretty printing

**Returns:**
- str: JSON string

```python
json_str = cjson.stringify(data, pretty=True)
```

### json_validate(json_string: str) -> bool
Validate JSON string without parsing.

**Parameters:**
- `json_string` (str): JSON to validate

**Returns:**
- bool: True if valid, False otherwise

```python
is_valid = cjson.validate('{"key":"value"}')
```

### json_free(value: capsule) -> None
Free memory allocated for JsonValue.

**Parameters:**
- `value` (capsule): JsonValue to free

```python
cjson.free(data)
```

## File I/O

### json_parse_file(filename: str) -> capsule
Parse JSON from file.

**Parameters:**
- `filename` (str): Path to JSON file

**Returns:**
- capsule: Parsed JsonValue

**Raises:**
- `IOError`: File not found or read error

```python
data = cjson.parse_file('data.json')
```

### json_save_file(filename: str, value: capsule, pretty: bool = True) -> bool
Save JsonValue to file.

**Parameters:**
- `filename` (str): Output file path
- `value` (capsule): JsonValue to save
- `pretty` (bool): Pretty print

**Returns:**
- bool: True on success

```python
success = cjson.save_file('output.json', data, pretty=True)
```

## Value Creation

### json_create_null() -> capsule
Create null value.

**Returns:**
- capsule: JsonValue representing null

```python
null_val = cjson.create_null()
```

### json_create_bool(value: bool) -> capsule
Create boolean value.

**Parameters:**
- `value` (bool): Boolean value

**Returns:**
- capsule: JsonValue

```python
bool_val = cjson.create_bool(True)
```

### json_create_number(value: float) -> capsule
Create number value.

**Parameters:**
- `value` (float): Numeric value

**Returns:**
- capsule: JsonValue

```python
num = cjson.create_number(42.5)
```

### json_create_string(value: str) -> capsule
Create string value.

**Parameters:**
- `value` (str): String value

**Returns:**
- capsule: JsonValue

```python
str_val = cjson.create_string("hello")
```

### json_create_array() -> capsule
Create empty array.

**Returns:**
- capsule: JsonValue array

```python
arr = cjson.create_array()
```

### json_create_object() -> capsule
Create empty object.

**Returns:**
- capsule: JsonValue object

```python
obj = cjson.create_object()
```

## Array Operations

### json_array_append(array: capsule, value: capsule) -> bool
Append value to array.

**Parameters:**
- `array` (capsule): Array JsonValue
- `value` (capsule): Value to append

**Returns:**
- bool: True on success

```python
arr = cjson.create_array()
cjson.array_append(arr, cjson.create_number(1))
```

### json_array_get(array: capsule, index: int) -> capsule
Get array element by index.

**Parameters:**
- `array` (capsule): Array JsonValue
- `index` (int): Element index

**Returns:**
- capsule: Element value

**Raises:**
- `IndexError`: Index out of bounds

```python
item = cjson.array_get(arr, 0)
```

### json_array_remove(array: capsule, index: int) -> bool
Remove array element.

**Parameters:**
- `array` (capsule): Array JsonValue
- `index` (int): Element index

**Returns:**
- bool: True on success

```python
cjson.array_remove(arr, 0)
```

### json_array_size(array: capsule) -> int
Get array size.

**Parameters:**
- `array` (capsule): Array JsonValue

**Returns:**
- int: Number of elements

**Raises:**
- `TypeError`: Not an array

```python
size = cjson.array_size(arr)
```

## Object Operations

### json_object_set(obj: capsule, key: str, value: capsule) -> bool
Set object key-value pair.

**Parameters:**
- `obj` (capsule): Object JsonValue
- `key` (str): Property key
- `value` (capsule): Property value

**Returns:**
- bool: True on success

```python
obj = cjson.create_object()
cjson.object_set(obj, "name", cjson.create_string("test"))
```

### json_object_get(obj: capsule, key: str) -> capsule
Get object value by key.

**Parameters:**
- `obj` (capsule): Object JsonValue
- `key` (str): Property key

**Returns:**
- capsule: Property value or None

```python
name = cjson.object_get(obj, "name")
```

### json_object_remove(obj: capsule, key: str) -> bool
Remove object property.

**Parameters:**
- `obj` (capsule): Object JsonValue
- `key` (str): Property key

**Returns:**
- bool: True on success

```python
cjson.object_remove(obj, "name")
```

### json_object_has(obj: capsule, key: str) -> bool
Check if object has key.

**Parameters:**
- `obj` (capsule): Object JsonValue
- `key` (str): Property key

**Returns:**
- bool: True if key exists

```python
has_name = cjson.object_has(obj, "name")
```

### json_object_keys(obj: capsule) -> list[str]
Get all object keys.

**Parameters:**
- `obj` (capsule): Object JsonValue

**Returns:**
- list[str]: List of keys

```python
keys = cjson.object_keys(obj)
```

### json_object_size(obj: capsule) -> int
Get object size.

**Parameters:**
- `obj` (capsule): Object JsonValue

**Returns:**
- int: Number of properties

**Raises:**
- `TypeError`: Not an object

```python
size = cjson.object_size(obj)
```

## Conversion Functions

### json_to_xml(value: capsule) -> str
Convert JSON to XML.

**Parameters:**
- `value` (capsule): JsonValue to convert

**Returns:**
- str: XML string

```python
xml = cjson.to_xml(data)
```

### json_to_yaml(value: capsule) -> str
Convert JSON to YAML.

**Parameters:**
- `value` (capsule): JsonValue to convert

**Returns:**
- str: YAML string

```python
yaml = cjson.to_yaml(data)
```

### json_to_csv(value: capsule) -> str
Convert JSON array of objects to CSV.

**Parameters:**
- `value` (capsule): Array of objects

**Returns:**
- str: CSV string

**Raises:**
- `ValueError`: Invalid structure

```python
csv = cjson.to_csv(array_of_objects)
```

### json_to_ini(value: capsule) -> str
Convert JSON object to INI format.

**Parameters:**
- `value` (capsule): Object JsonValue

**Returns:**
- str: INI string

**Raises:**
- `ValueError`: Invalid structure

```python
ini = cjson.to_ini(obj)
```

### xml_to_json(xml: str) -> capsule
Convert XML to JSON.

**Parameters:**
- `xml` (str): XML string

**Returns:**
- capsule: JsonValue

**Raises:**
- `ValueError`: Invalid XML

```python
data = cjson.xml_to_json('<root><name>test</name></root>')
```

### yaml_to_json(yaml: str) -> capsule
Convert YAML to JSON.

**Parameters:**
- `yaml` (str): YAML string

**Returns:**
- capsule: JsonValue

**Raises:**
- `ValueError`: Invalid YAML

```python
data = cjson.yaml_to_json('name: test\nvalue: 123')
```

### csv_to_json(csv: str) -> capsule
Convert CSV to JSON array.

**Parameters:**
- `csv` (str): CSV string

**Returns:**
- capsule: Array JsonValue

**Raises:**
- `ValueError`: Invalid CSV

```python
data = cjson.csv_to_json('name,age\nAlice,30\nBob,25')
```

### ini_to_json(ini: str) -> capsule
Convert INI to JSON.

**Parameters:**
- `ini` (str): INI string

**Returns:**
- capsule: Object JsonValue

**Raises:**
- `ValueError`: Invalid INI

```python
data = cjson.ini_to_json('[section]\nkey=value')
```

## SQLite Integration

### json_to_sqlite(value: capsule, db_path: str) -> capsule
Convert JSON to SQLite database.

**Parameters:**
- `value` (capsule): JsonValue to convert
- `db_path` (str): Database file path

**Returns:**
- capsule: Database handle

**Raises:**
- `IOError`: Cannot create database

```python
db = cjson.to_sqlite(data, 'data.db')
```

### sqlite_insert(db: capsule, table: str, value: capsule) -> bool
Insert JSON object into SQLite table.

**Parameters:**
- `db` (capsule): Database handle
- `table` (str): Table name
- `value` (capsule): Object to insert

**Returns:**
- bool: True on success

```python
cjson.sqlite_insert(db, 'users', user_obj)
```

### sqlite_query(db: capsule, table: str, key: str, value: str) -> capsule
Query SQLite database.

**Parameters:**
- `db` (capsule): Database handle
- `table` (str): Table name
- `key` (str): Column name
- `value` (str): Value to match

**Returns:**
- capsule: Array of results

```python
results = cjson.sqlite_query(db, 'users', 'name', 'Alice')
```

### sqlite_close(db: capsule) -> None
Close SQLite database.

**Parameters:**
- `db` (capsule): Database handle

```python
cjson.sqlite_close(db)
```

## Streaming Parser

### stream_parse_chunk(chunk: bytes, callback: callable = None) -> list
Parse JSON chunk in streaming mode.

**Parameters:**
- `chunk` (bytes): Data chunk
- `callback` (callable): Event handler

**Returns:**
- list: List of events

**Raises:**
- `ValueError`: Parse error

```python
def handler(event):
    print(event['type'])
    return True

events = cjson.stream_parse_chunk(b'{"key":', handler)
```

### stream_parse_file(filename: str, callback: callable = None) -> list
Parse JSON file in streaming mode.

**Parameters:**
- `filename` (str): File path
- `callback` (callable): Event handler

**Returns:**
- list: List of events

**Raises:**
- `IOError`: File error

```python
events = cjson.stream_parse_file('large.json', handler)
```

## Utility Functions

### json_deep_copy(value: capsule) -> capsule
Create deep copy of JsonValue.

**Parameters:**
- `value` (capsule): JsonValue to copy

**Returns:**
- capsule: Copied JsonValue

**Raises:**
- `RuntimeError`: Copy failed

```python
copy = cjson.deep_copy(original)
```

### json_equals(val1: capsule, val2: capsule) -> bool
Compare two JsonValues.

**Parameters:**
- `val1` (capsule): First value
- `val2` (capsule): Second value

**Returns:**
- bool: True if equal

```python
equal = cjson.equals(val1, val2)
```

### json_merge(obj1: capsule, obj2: capsule) -> capsule
Merge two JSON objects.

**Parameters:**
- `obj1` (capsule): First object
- `obj2` (capsule): Second object

**Returns:**
- capsule: Merged object

**Raises:**
- `ValueError`: Not objects

```python
merged = cjson.merge(obj1, obj2)
```

### json_memory_usage(value: capsule) -> int
Calculate memory usage.

**Parameters:**
- `value` (capsule): JsonValue

**Returns:**
- int: Bytes used

```python
bytes_used = cjson.memory_usage(data)
```

### json_optimize_memory(value: capsule) -> None
Optimize memory usage.

**Parameters:**
- `value` (capsule): JsonValue to optimize

```python
cjson.optimize_memory(data)
```

## Advanced Features

### json_path_query(root: capsule, path: str) -> capsule
Query JSON using path expression.

**Parameters:**
- `root` (capsule): Root JsonValue
- `path` (str): JSONPath expression

**Returns:**
- capsule: Query result or None

```python
name = cjson.path_query(data, '$.users[0].name')
all_users = cjson.path_query(data, '$.users[*]')
filtered = cjson.path_query(data, '$.users[?(@.age>25)]')
```

### json_validate_schema(data: capsule, schema: capsule) -> bool
Validate data against schema.

**Parameters:**
- `data` (capsule): Data to validate
- `schema` (capsule): JSON schema

**Returns:**
- bool: True if valid

```python
schema = cjson.parse('{"type":"object"}')
valid = cjson.validate_schema(data, schema)
```

### json_diff(val1: capsule, val2: capsule) -> capsule
Get difference between values.

**Parameters:**
- `val1` (capsule): First value
- `val2` (capsule): Second value

**Returns:**
- capsule: Diff object

```python
diff = cjson.diff(old_data, new_data)
```

### json_patch(target: capsule, patch: capsule) -> capsule
Apply patch to value.

**Parameters:**
- `target` (capsule): Target value
- `patch` (capsule): Patch object

**Returns:**
- capsule: Patched value

```python
patched = cjson.patch(target, patch)
```

## Error Handling

### json_get_last_error() -> dict | None
Get last error details.

**Returns:**
- dict: Error info with keys: code, message, line, column
- None: No error

```python
error = cjson.get_last_error()
if error:
    print(f"Error at {error['line']}:{error['column']}: {error['message']}")
```

### json_clear_error() -> None
Clear error state.

```python
cjson.clear_error()
```

## Event Types (Streaming)

Events returned by streaming parser:

- `object_start`: Object begins
- `object_end`: Object ends
- `array_start`: Array begins
- `array_end`: Array ends
- `key`: Object key
- `value`: Value parsed
- `error`: Parse error
- `eof`: End of input

Event structure:
```python
{
    'type': str,
    'key': str | None,
    'value': capsule | None
}
```

## Complete Example

```python
import cjson

# Parse JSON
data = cjson.parse('{"users":[{"name":"Alice","age":30}]}')

# Access values
users = cjson.object_get(data, 'users')
first_user = cjson.array_get(users, 0)
name = cjson.object_get(first_user, 'name')

# Modify
new_user = cjson.create_object()
cjson.object_set(new_user, 'name', cjson.create_string('Bob'))
cjson.object_set(new_user, 'age', cjson.create_number(25))
cjson.array_append(users, new_user)

# Convert
xml = cjson.to_xml(data)
yaml = cjson.to_yaml(data)

# Save
cjson.save_file('output.json', data, pretty=True)

# Path query
bob = cjson.path_query(data, '$.users[?(@.name=="Bob")]')

# Cleanup
cjson.free(data)
cjson.free(bob)
```

## License

GPL-3.0

## Author

Mohammad Amin
moahmmad.hosseinii@gmail.com
