# Mass updating tasks by a query

- STATUS: OPEN
- PRIORITY: 110
- TAGS: scope

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

---

We implemented `tatr-untag` but it's currently rather bad at keeping the original order of the properties.
