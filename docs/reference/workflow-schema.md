# Workflow Schema Reference

This page defines the formal schema for NYX workflow JSON files. For a gentler introduction, see the [Workflow Authoring Guide](../workflows/overview.md).

## JSON Schema

```json
{
  "$schema": "http://json-schema.org/draft-07/schema#",
  "$id": "https://github.com/N3ur0sis/nyx/workflow.schema.json",
  "title": "NYX Workflow",
  "type": "object",
  "required": ["steps"],
  "properties": {
    "workflow": {
      "type": "object",
      "properties": {
        "id":          { "type": "string" },
        "name":        { "type": "string" },
        "version":     { "type": "string" },
        "description": { "type": "string" }
      }
    },
    "vars": {
      "type": "object",
      "additionalProperties": {
        "oneOf": [
          { "type": "string" },
          { "type": "number" },
          { "type": "boolean" }
        ]
      }
    },
    "steps": {
      "type": "array",
      "minItems": 1,
      "items": { "$ref": "#/definitions/step" }
    }
  },
  "definitions": {
    "step": {
      "type": "object",
      "required": ["id", "tool"],
      "properties": {
        "id": {
          "type": "string",
          "description": "Unique step identifier, referenced in expressions"
        },
        "tool": {
          "type": "string",
          "description": "Tool name (short or prefixed with nyx-)"
        },
        "params": {
          "type": "object",
          "description": "Tool parameters. Values may contain ${...} expressions.",
          "additionalProperties": true
        },
        "for_each": {
          "type": "string",
          "description": "Expression evaluating to an array. Step runs once per element."
        },
        "when": {
          "type": "string",
          "description": "Boolean expression. Step is skipped if false."
        },
        "meta": {
          "type": "object",
          "description": "Opaque metadata for GUI tools (position, color, etc.)",
          "additionalProperties": true
        }
      }
    }
  }
}
```

## Expression Syntax (Informal Grammar)

```
expression     = "${" path [pipe_chain] [comparison] "}"
path           = root ("." segment)*
root           = "vars" | "each" | step_id
segment        = identifier | identifier "[" integer "]"
pipe_chain     = ("|" pipe_op)*
pipe_op        = "filter" identifier ("==" | "!=") value
               | "select" identifier
               | "count"
               | "first"
               | "flat"
comparison     = ("==" | "!=" | ">" | "<" | ">=" | "<=") value
value          = number | "true" | "false" | quoted_string | identifier
```

## Validation Rules

The engine validates the following before execution:

1. `steps` is a non-empty array
2. Every step has a non-empty `id` field
3. Every step has a non-empty `tool` field
4. All step IDs are unique within the workflow
5. All step IDs referenced in `${...}` expressions exist in the workflow
6. The dependency graph is acyclic (no circular references)

## Execution Semantics

1. Parse the workflow JSON
2. Scan all expressions to build the dependency graph
3. Validate the graph (uniqueness, references, acyclicity)
4. Topological sort via Kahn's algorithm
5. For each step in order:
   a. Evaluate `when` (if present). Skip if false.
   b. Evaluate `for_each` (if present). If array, iterate.
   c. For each iteration (or once if no `for_each`):
      - Resolve all `${...}` expressions in `params`
      - Look up the tool in the registry
      - Invoke the tool in-process
      - Store the output envelope
6. Collect all step results

## Tool Name Resolution

The engine strips a `nyx-` prefix when looking up tools. Both `"pingsweep"` and `"nyx-pingsweep"` resolve to the same registered tool.
