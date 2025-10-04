import json
import inspect
from dataclasses import (
    asdict,
    fields,
    MISSING,
)
from typing import TypeVar, Type, Dict, Any, List, get_args, get_origin

# Define a generic type variable T for any dataclass
T = TypeVar("T")


def _map_value(value: Any, expected_type: Type) -> Any:
    """
    Recursively maps nested lists and objects during deserialization.
    This is the core logic for handling complex dataclass structures.
    """
    origin_type = get_origin(expected_type)

    # 1. Handle Lists (e.g., List[MyDataclass])
    if origin_type is list:
        # Get the inner type (e.g., MyDataclass from List[MyDataclass])
        inner_type = get_args(expected_type)[0]
        if inspect.isclass(inner_type) and hasattr(inner_type, "__dataclass_fields__"):
            return [from_json_generic(json.dumps(item), inner_type) for item in value]

    # 2. Handle Dictionaries (e.g., Dict[str, MyDataclass])
    if origin_type is dict:
        # Get the inner value type (e.g., MyDataclass from Dict[str, MyDataclass])
        value_type = get_args(expected_type)[1]
        if inspect.isclass(value_type) and hasattr(value_type, "__dataclass_fields__"):
            return {k: from_dict_generic(v, value_type) for k, v in value.items()}

    # 3. Handle Simple Dataclasses (The inner Char object)
    if inspect.isclass(expected_type) and hasattr(
        expected_type, "__dataclass_fields__"
    ):
        return from_dict_generic(value, expected_type)

    # 4. Handle Primitive types (float, str, int, etc.)
    return value


def from_dict_generic(data: Dict[str, Any], model_type: Type[T]) -> T:
    """
    [DESERIALIZATION HELPER] Transforms a Python dictionary (from JSON) into a dataclass object (T).
    Handles nested dataclasses and lists of dataclasses.
    """
    if not hasattr(model_type, "__dataclass_fields__"):
        raise TypeError(f"{model_type} must be a dataclass.")

    mapped_fields = {}

    # Iterate through the expected fields of the target dataclass
    for field in fields(model_type):
        field_name = field.name
        field_type = field.type

        # Get the raw value from the input data, use default if missing
        raw_value = data.get(field_name)

        if raw_value is not None:
            # Map the raw value to the specific type (handling nested conversions)
            mapped_fields[field_name] = _map_value(raw_value, field_type)
        elif field.default is not MISSING:  # Use imported MISSING
            # Use default value if provided in the dataclass
            mapped_fields[field_name] = field.default
        elif field.default_factory is not MISSING:  # Use imported MISSING
            # Use default factory if provided
            mapped_fields[field_name] = field.default_factory()

    # Instantiate the dataclass using the prepared fields
    return model_type(**mapped_fields)


def from_json_generic(json_string: str, model_type: Type[T]) -> T:
    """
    [DESERIALIZATION] Transforms a JSON string into a dataclass object of type T.
    This is the primary public deserialization function.
    """
    data = json.loads(json_string)
    return from_dict_generic(data, model_type)


def to_json_generic(model: T) -> str:
    """
    [SERIALIZATION] Transforms a dataclass object (T) into a formatted JSON string.
    This is the primary public serialization function.
    """
    if not hasattr(model, "__dataclass_fields__"):
        raise TypeError(f"Object {model} must be a dataclass.")

    # asdict() handles the conversion of nested dataclasses recursively to dicts
    data_dict = asdict(model)
    # Use indent=4 for clean, human-readable output
    return json.dumps(data_dict, indent=4)
