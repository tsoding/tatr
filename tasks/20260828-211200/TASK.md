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
