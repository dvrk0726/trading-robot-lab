"""
Validate basic_flow test vectors against shared JSON schemas.

Usage:
    python shared/schemas/validate_examples.py

Requirements:
    pip install jsonschema
"""

import json
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
SCHEMAS_DIR = Path(__file__).resolve().parent
VECTORS_DIR = REPO_ROOT / "shared" / "test_vectors" / "basic_flow"

SCHEMA_MAP = {
    "01_market_event.json": "market_event.schema.json",
    "02_feature_snapshot.json": "feature_snapshot.schema.json",
    "03_strategy_signal.json": "strategy_signal.schema.json",
    "04_order_intent.json": "order_intent.schema.json",
    "05_risk_decision.json": "risk_decision.schema.json",
}


def main():
    try:
        from jsonschema import validate, ValidationError
    except ImportError:
        print("ERROR: 'jsonschema' package is not installed.")
        print()
        print("Install it with:")
        print("    pip install jsonschema")
        print()
        sys.exit(1)

    errors = []
    validated = 0

    for example_name, schema_name in SCHEMA_MAP.items():
        example_path = VECTORS_DIR / example_name
        schema_path = SCHEMAS_DIR / schema_name

        if not example_path.exists():
            errors.append(f"MISSING: {example_path}")
            continue
        if not schema_path.exists():
            errors.append(f"MISSING: {schema_path}")
            continue

        with open(example_path, "r", encoding="utf-8") as f:
            example_data = json.load(f)
        with open(schema_path, "r", encoding="utf-8") as f:
            schema_data = json.load(f)

        try:
            validate(instance=example_data, schema=schema_data)
            print(f"  OK   {example_name}  ->  {schema_name}")
            validated += 1
        except ValidationError as e:
            path = " -> ".join(str(p) for p in e.absolute_path) or "(root)"
            errors.append(f"FAIL  {example_name}  ->  {schema_name}: {e.message} at {path}")

    print()

    if errors:
        for err in errors:
            print(f"  {err}")
        print()
        print(f"Validated: {validated}/{len(SCHEMA_MAP)}")
        sys.exit(1)

    print(f"All {validated} examples valid.")
    sys.exit(0)


if __name__ == "__main__":
    main()
