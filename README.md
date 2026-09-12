# Task Tracker

<p align=center>
  <img src="./cover/cover-512.png" width=512>
</p>

<p align=center>
  <sub>Cover by <a href="https://github.com/rexim">rexim</a></sub>
</p>

This is an improvised Tasks system, because I needed something more powerful than just plane TODOs in the Source Code of my projects, yet I didn't want to install a full blown Issue Tracker System.

## The Spec

### Layout

Each project at the root has `tasks/` folder which contains sub-folders for each task.

```
project/
+-...
+-tasks/
| +-tags
| +-20260824-215300
| | +-TASK.md
| | +-...
| +-20260830-000403-rexim
| | +-TASK.md
| | +-screenshot.png
| | +-...
| +-...
+-...
```

### HUID

Each task sub-folder is named with a Task ID. The Task ID format is `[0-9]{8}-[0-9]{6}`. Just grab the current date and time and use it as the Task ID. Use UTC timezone so the current timezone is irrelevant. If your Task ID collides with an existing one, just wait one second and try again.

The full format is actually `[0-9]{8}-[0-9]{6}(-[a-zA-Z0-9\\-]*)?`. So if you work in a team you can agree on unique suffixes per individual to slap at the end like `20260829-235855-rexim` or `20260829-235902-01`. Those are valid Task IDs too.

I call this ID system HUID (Human-Unique IDentifier). It is fairly unique if you generate IDs at "Human-speed". That is for me personally the speed at which I need to generate them is never below one second.

Having a Unique Task ID is beneficial under such control systems as git, because you can generate tasks in parallel branches and then relatively easily merge them together.

### TASK.md

Inside of the task sub-folder there is one mandatory file `TASK.md` which is a markdown file describing the task. The folder may contain other files as attachments to the task. `TASK.md` should link to the attachments as necessary. Try to keep the size of the attachments small, since they are going to be committed to the git repo. Use [ffmpeg](https://ffmpeg.org/) to reencode any screencast to reduce their size as necessary.

The format of `TASK.md`:

```markdown
# <title>

- STATUS: (OPEN|CLOSED)
- PRIORITY: <number>
- TAGS: <comma-and-whitespace-separated-list-of-tags>
[other properties]

[description]
```

As you work on the task feel free to append any discovered details about the task to the `[description]`.

Use [git-blame](https://git-scm.com/docs/git-blame) and [git-log](https://git-scm.com/docs/git-log) to learn about when, how and by whom any changes to the task were made.

#### Task Properties

The `STATUS` property defines whether the task is done or not. There could be only two statuses. If you need more it is generally recommended to use `TAGS` for whatever you are trying to do.

The `PRIORITY` is generally used for sorting the tasks by the external tools. One thing I've seen people online do is stressing out about what priority to put. Don't think of priority as an absolute value. Think of it as the means of getting your list of tasks sorted in a specific way you want them to see.

The `TAGS` property contains the list of tags separated by commas and whitespaces. `TAGS: foo,bar,baz` defines 3 tags. `TAGS: foo,,, hello  world` also defines 3 tags. You can use these tags to group tasks into categories. Like `bug` or `enhancement`.

We allow to specify `[other properties]` (in addition to `STATUS`, `PRIORITY`, and `TAGS`) in a similar format (that is `- [NAME]: VALUE`), the official tool will ignore them but try its best to not disturb them too much during any mass update operations (like `tatr-untag`, etc).

Duplicated properties are not allowed. If a tasks specifies duplicated properties only the last value must be taken into account. Mass update operations of the official tool will remove any duplicated properties.

### Tags description file

There might be an optional `tasks/tags` file with the following format:

```
<tag-name> [,] <tag-description>
<tag-name> [,] <tag-description>
<tag-name> [,] <tag-description>
...
```

It serves as a documentation for each existing tag and the thirdparty tools may use it to display the tag descriptions.

## The Tool

The repo comes with a command line tool that helps to navigate and manipulate the `tasks/` folder:

```console
$ cc -o nob nob.c
$ ./nob
$ sudo cp ./build/tatr /usr/local/bin/
$ tatr help
```

It is specifically optimized to be run in compilation mode of Emacs. Not sure how useful it is outside of this use case.

We only support Linux right now But I have tasks to add [Windows](./tasks/20260825-170729/TASK.md) and [MacOS](./tasks/20260901-063204/TASK.md) support in the future.

You are welcome to make your own tools.

### Tatr Query Language (TQL)

The Query language that is used in `tatr ls` command to select a set of tasks.

#### Examples

Query everything with tag `bug`:

```console
$ tatr ls :bug
```

Everything with tag `bug`, but without tag `ui`:

```console
$ tatr ls :bug and not :ui
```

Everything that is not tagged:

```console
$ tatr ls not tagged
```

All the bugs with priority less than 50:

```console
$ tatr ls :bug and priority lt 50
```

#### Syntax

Here is the [Backus–Naur form](https://en.wikipedia.org/wiki/Backus%E2%80%93Naur_form) of the language:

```
<expr>            ::= <or>
<or>              ::= <and> *('or' <and>)
<and>             ::= <compare> *('and' <compare>)
<compare>         ::= <primary> *(<compare-op> <primary>)
<compare-op>      ::= 'lt' | 'le' | 'gt' | 'ge' | 'eq' | 'ne'
<primary>         ::= <tag>
                    | '[' <expr> ']'
                    | 'not' <primary>
                    | 'any'
                    | 'tagged'
                    | 'priority'
                    | <number>
<tag>             ::= ':' 1*<any-character-except-whitespaces-and-square-brackets>
<number>          ::= ['-'] 1*<digit>
```

The syntax is designed to be used in shell environment without requiring any special escaping.

In shells parenthsis usually have special meaning. Because of that we use square brackets for grouping expressions `'[' <expr> ']'`.

Another problematic symbols are `<` and `>`, which are usually used for file redirecting. Which means we can't easily use traditional comparison operators. So we are following the [test(1)](https://www.man7.org/linux/man-pages/man1/test.1.html) utility convention: `lt`, `le`, `gt`, `ge`, `eq`, and `ne`.

Since brackets have a special meaning in TQL, if you have any tags that contain them, you probably won't be able to filter by them (even though the specs do not explicitly prohibit square brackets in the tags). Just don't use square brackets in the tags if you are using `tatr` I guess. I may do something about that later.

#### Reference

| Expression | Description |
|-|-|
| `<a> or <b>` | True when `<a>` or `<b>` or both are true. Just a regular [logical disjunction](https://en.wikipedia.org/wiki/Logical_disjunction). |
| `<a> and <b>` | True when both `<a>` and `<b>` are true. Just a regular [logical conjunction](https://en.wikipedia.org/wiki/Logical_conjunction). |
| `:<tag>` | True when a task's `TAGS` property contains `<tag>`. |
| `not <expr>` | True when `<expr>` is false. |
| `tagged` | True when a task has at least one tag in its `TAGS` property. |
| `any` | Always true for any task. |
| `priority` | Priority of the task as an integer. |
| `<a> lt <b>` | True when `<a>` is less than `<b>`.|
| `<a> gt <b>` | True when `<a>` is greater than `<b>`.|
| `<a> le <b>` | True when `<a>` is less or equal to `<b>`.|
| `<a> ge <b>` | True when `<a>` is greater or equal to `<b>`.|
| `<a> eq <b>` | True when `<a>` is equal to `<b>`.|
| `<a> ne <b>` | True when `<a>` is not equal to `<b>`.|

## The License

All the code in this repo is released under [GNU General Public License, version 2](https://www.gnu.org/licenses/old-licenses/gpl-2.0.html) license unless stated otherwise (specifically, the files in [./thirdparty/](./thirdparty/) folder have their own corresponding licenses)

The cover art in the [./cover/](./cover/) folder is under the [CC BY-NC](https://creativecommons.org/licenses/by-nc/4.0/).
