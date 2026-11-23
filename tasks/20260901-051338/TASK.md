# Make the grep command configurable for `tatr-ref`

- STATUS: OPEN
- PRIORITY: 100
- TAGS: scope

Maybe some sort of a config file in home folder. It must be per user, not per repo. You may need to use https://github.com/tsoding/shlex.h for this.

---

Also put the suffix in there (the default value for the `-s` flag of `tatr-new`)

---

We could maybe even use environment variables instead of config files? Though, for example, git can do both envars and config.
