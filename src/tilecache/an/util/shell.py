def shell_escape(s):
    repl = ("\\", "|", "&", ";", "(", ")", "<", ">", " ", "\t", "\n", "'", "\"")
    # (note, "\\" must be first!)
    for x in repl:
        s = s.replace(x, '\\'+x)
    return s
