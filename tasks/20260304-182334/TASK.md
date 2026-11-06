# Accidental double `tatr new` in Emacs

- STATUS: CLOSED
- PRIORITY: 200
- TAGS: bug

There is a weird UX problem I keep experiencing in Emacs' compilation
mode with `tatr new`. I have a habit of switching to `*compilation*`
buffer and hitting `g` to rerun the last commands. Sometimes such
commands is `tatr new` which leads to creating the same task
twice. I'm not sure what to do about it as I feel like I'm probably
the only one who will experience this problem even after the tool goes
public.

It probably has a similar problem in Bash if you have a habit of
hitting up and enter.

---

With VCS it's not that big of a deal actually.

---

It is annoying though! What I'm thinking could be a passable solution
is to just add friction when you create a task with the title that
already exists (literal byte level equality). Something like a yorn
prompt "are you sure?" would suffice.
