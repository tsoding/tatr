# Editor-Tool vs Tool-Editor

The original design goal was to make the tool usable from the compilation mode of Emacs.
This is the Editor-Tool mode of operation. The Editor is primary and it invokes the Tool.

Recent introduction of `-e` flag to such commands as `tatr-new`, and `tatr-ls` drags the 
design towards Tool-Editor mode of operation. The Tool is invoking the Editor to change
some text somewhere. It is completely different from what we had before and somewhat
incompatible.

I think we want to support both modes as I sometimes use Tatr in a tmux+vim setup.
Command line git is probably a good example of a tool that tries to support both modes.
We may learn a thing or two from it.

Every time a new feature is introduced a question should be asked: "Is this for Editor-Tool 
or Tool-Editor mode of operation?" And the design should be adjusted accordingly to the mode.
