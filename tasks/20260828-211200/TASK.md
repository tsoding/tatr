# Mass updating tasks by a query (`tatr-untag`)

- STATUS: CLOSED
- PRIORITY: 120
- TAGS: stream

## Scope of the task

Removing everything from the scope:

```console
$ tatr untag -t scope :scope
```

Only this command for now. In the future we may add more as part of
separate tasks.

## Keeping the properties intact as we are modifying them

We implemented `tatr-untag` but it's currently rather bad at keeping the original order of the properties.

---

There is a little bit of a problem in here of keeping the parsed properties (`Task.status`, `Task.priority`, and `Task.tags`) and the unparse properties (`Task.properties`) in sync.

---

I think I solved the syncing problem by making sure `STATUS`, `PRIORITY`, and `TAGS` are always present in the `Task.properties` and just giving `Task.status`, `Task.priority`, and `Task.tags` a priority over whatever is in `Task.properties`.

It also helped a lot of with keeping the original order of the properties.

---

I'm still not sure how the whole system reacts to duplicated properties.

---

Interestingly we never really defined well the format of the properties in general. So it's a rather a spec gap right now.

---

Added some info about `[other properties]` to The Spec.

---

I think this part of the Task is closed. This is how we are treating the properties from now on.
