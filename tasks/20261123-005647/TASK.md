# Automatically open TASK.md in external text editor

- STATUS: OPEN
- PRIORITY: 110
- TAGS: scope

Idea inspired by suggestions from Marijn Stevens over email.

---

Introduce `-e` flag to to several commands:

`tatr new -e ...`

This should open the newly created `TASK.md` in an external text editor.
We should probably do similar thing to what git is doing. Look up the editor
command line in `$TATR_EDITOR` then in `$EDITOR` and then in config. Though we
don't have config until TASK(20260901-051338) is done.

The command line string can be tokenized by https://github.com/tsoding/shlex.h

`tatr todo -e ...`

Open TASK.md after each `y` answer to `tatr-todo` prompt. Or maybe later we could 
have an option to open all of them at once when all TODOs a processed.

`tatr ls -e ...`

Open all selected TASK.md-s at once in the text editor. This is why the formatting
string has `%files` parameter. I do not want to open them sequentually because in
case a blocking editor like command line vim this process becomes uncancellable until
you visited all the files. And if you selected like 100+ this maybe become very 
annoying. In case of `tatr-todo` it is very easy to cancel the process between the file
visits.

---

It turned out that for `tatr-todo` it's better to introduce a prompt option
"yes and open in a text editor".

---

The way we can make the `tatr ls -e` cancellable is by having an interactive
prompt that asks if you wanna open the next file. Similar to `tatr-todo`. Such
prompt may have the `a` option that opens everything at once as was planned
before.

Though it's unclear if the `a` option should open the files one-by-one or just
shove all of them into a single command line.

---

Allowing `tatr-ls` to call to a text editor makes it even more complex imo. We
should introduce `tatr-edit` that accepts a query and does interactive prompting,
but that means we now have two commands that accept query and we need to decide
what do we do with the `-debug` flag again TASK(20260910-181239). Do we extract it
to a separate command?

While we are at it, should we maybe extract `-update` flag to a separate command too?
Though in that case how do we separate TQL and TUL expressions?

---

So far I just implemented `tatr-new` and `tatr-todo`. Opening existing TASK.md-s by
query is still brewing in my head.

---

`tatr-edit` would require reimplementing the same set of flags as in `tatr-ls`.
Namely `-c` and `-a` which does feel like a lot of duplication.

