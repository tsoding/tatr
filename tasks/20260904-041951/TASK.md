# List both closed and open tasks somehow

- STATUS: OPEN
- PRIORITY: 100
- TAGS: scope

Some sort of flag for `tatr-ls` and `tatr-summary`. Not sure how it
should play out with `-c` flag of the corresponding commands.

Maybe filtering by status should be part of TQL, but than getting the
default behavior of empty `tatr-ls` and `tatr-summary` right is a bit
difficult. The default query is just `any`, but now it should be
`not closed`. But then if I provide a custom query I should not forget
to do `{custom} and not closed`. It's getting weird.

Also, making `tatr-summary` accept a query to summarize is an
interesting idea.
