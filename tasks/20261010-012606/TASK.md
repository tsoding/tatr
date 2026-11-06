# Some way to initialize priority, tags and body for tasks created with `tatr-todo`

- STATUS: OPEN
- PRIORITY: 100
- TAGS:

Introduced by TASK(20260919-171620)

Maybe with the following syntax:

```c
// TODO: Title
// - TAGS: scope,bug
// - PRIORITY: 69
// Body
```

But I'm not sure. This is just a rough idea.

---

Another idea is maybe do that through flags, but a single `tatr-todo` creates a bunch of tasks for which you may not want to have the same tag and priority, but who knows.

---

I just quickly added `-t` flag from `tatr-new` cause I needed it. I guess we are moving in that direction...
