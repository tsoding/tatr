# Automatically open TASK.md in external text editor

- STATUS: OPEN
- PRIORITY: 110
- TAGS: scope

Idea inspired by suggestions from Marijn Stevens over email.

---

Introduce `-edit` flag to to several commands:

`tatr new -edit ...`

This should open the newly created `TASK.md` in an external text editor.
We should probably do similar thing to what git is doing. Look up the editor
command line in `$TATR_EDITOR` then in `$EDITOR` and then in config. Though we
don't have config until TASK(20260901-051338) is done.

The command line string can be tokenized by https://github.com/tsoding/shlex.h

`tatr todo -edit ...`

Open TASK.md after each `y` answer to `tatr-todo` prompt. Or maybe later we could 
have an option to open all of them at once when all TODOs a processed.

`tatr ls -edit ...`

Open all selected TASK.md-s at once in the text editor. This is why the formatting
string has `%files` parameter. I do not want to open them sequentually because in
case a blocking editor like command line vim this process becomes uncancellable until
you visited all the files. And if you selected like 100+ this maybe become very 
annoying. In case of `tatr-todo` it is very easy to cancel the process between the file
visits.
