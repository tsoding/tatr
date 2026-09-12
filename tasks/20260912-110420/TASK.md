# Invalid tasks are filter out when `tatr-ls` is called with non-default query

- STATUS: OPEN
- PRIORITY: 100
- TAGS: bug

The idea was that incorrectly formatted tasks are included in the `tatr-ls` result but have high priority and screaming title so to force you to go and fix them. But currently such invalid tasks are easily filtered out by `OP_TAG` (since they don't have any tags). We need to make them unfliterable. Maybe introduce a special flag into `Task` structure like `Task.invalid` which makes it very special during filtering and rendering and stuff.
