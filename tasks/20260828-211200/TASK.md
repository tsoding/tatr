# Mass updating tasks by a query

- STATUS: OPEN
- PRIORITY: 110
- TAGS: scope,stream

## Scope of the task

Adding everything with priority 100 and higher to the scope:

```console
$ tatr tag -t scope priority gte 100
```

Removing everything from the scope:

```console
$ tatr untag -t scope .scope
```

Closing all the bugs:

```console
$ tatr close .bug
```

Only these commands for now. In the future we may add more as part of
separate tasks.

## Keeping the properties intact as we are modifying them

We implemented `tatr-untag` but it's currently rather bad at keeping the original order of the properties.

---

There is a little bit of a problem in here of keeping the parsed properties (`Task.status`, `Task.priority`, and `Task.tags`) and the unparse properties (`Task.properties`) in sync.

---

I think I solved the syncing problem by making sure `STATUS`, `PRIORITY`, and `TAGS` are always present in the `Task.properties` and just giving `Task.status`, `Task.priority`, and `Task.tags` a priority over whatever is in `Task.properties`.

It also helped a lot of with keep the original order of the properties

---

I'm still not sure how the whole system reacts to duplicated properties.

---

Interestingly we never really defined well the format of the properties in general. So it's a rather a spec gap right now.
