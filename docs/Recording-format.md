# Recording JSON schema 1

```json
{
  "schema": 1,
  "name": "A key pulse",
  "type": "keyboard",
  "events": [
    {"at_us": 0, "code": 30, "flags": 0, "buttons": 0, "data": 0, "x": 0, "y": 0},
    {"at_us": 100000, "code": 30, "flags": 1, "buttons": 0, "data": 0, "x": 0, "y": 0}
  ]
}
```

All properties are required; unknown/duplicate properties are rejected. `type` is keyboard or mouse. `at_us` is nondecreasing elapsed microseconds, at most one hour. Keyboard flags are KeyUp=1, E0=2 and E1=4; code is a scan code, not a character or virtual key. Keyboard mouse-fields must be zero. Mouse code must be zero; flags/buttons/data correspond to the Interceptor raw mouse stroke, and X/Y are signed 32-bit raw units. Mouse buttons contain down/up transitions, not a persistent button mask.

Name is 1–120 UTF-8 bytes without control characters. Limits: 100000 events, 32 MiB JSON, 16 nesting levels. No device token is persisted: the operator explicitly selects a current capture-scoped target before replay. Files cannot execute scripts or reference external resources.
