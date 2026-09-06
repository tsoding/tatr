# Task archiving

- STATUS: OPEN
- PRIORITY: 100
- TAGS:

As the amount of tasks grows over time I expect a lot of closed
irrelevant tasks accumulating in the `tasks/` folder slowing down
`tatr-ls` and `tatr-summary` since they need to visit each folder and
parse each `TASK.md` file. We should implement some sort of mechanism
(maybe in a form of `tatr-archive` subcommand) that zips them all up
into a single file.

Since we are using HUIDs which are based on the current UTC time it's
relatively straightforward to just archive old enough tasks based
entirely on their HUIDs.

This obviously evicts all the archived tasks metadata out of `tatr-ls`
and `tatr-summary` reports. We can solve that by placing all the
archived metadata into a single text file upon calling
`tatr-archive`. Parsing a single text file should be much faster than
a bunch of files scattered across several folders.

We haven't reached the point where this kind of stuff is necessary,
but in case we do I just wanna have some sort of plan for that. Hence
creating this task and putting all my thoughts about the problem in
it.
