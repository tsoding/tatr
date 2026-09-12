# TQL expression that matches a task with a specific ID

- STATUS: OPEN
- PRIORITY: 110
- TAGS: scope,tql

Extracted from 20260828-211200

It feels like sometimes mass update operations (like `tatr-untag`, etc) should be able to work on a single task.

Like I wanna be able to do:

```console
$ tatr close 20260912-102943
```

So we need to think how to accomdate this use case.

---

I'm thinking about extending TQL with `OP_ID` which is compiled from a keyword that is a valid HUID. That is something like `20260912-102943` is a syntactically valid TQL query which matches one task with that exact ID.
