#ifndef ARG_H
#define ARG_H

extern char *argv0;

#define ARGBEGIN							\
	for (argv0 = *argv, argv++, argc--;				\
	    argv[0] != NULL && argv[0][0] == '-' && argv[0][1] != '\0';	\
	    argc--, argv++) {						\
		char **_argv, *_a;					\
		if (argv[0][1] == '-' && argv[0][2] == '\0') {		\
			argv++, argc--;					\
			break;						\
		}							\
		for (_argv = argv, _a = *argv + 1; *_a != '\0'; _a++) {	\
			switch (*_a)

#define ARGEND								\
			if (_argv != argv)				\
				break;					\
		}							\
	}

#define	EARGF(x)							\
	((argv[1] == NULL) ? ((x), (char *)0) : (argc--, argv++, argv[0]))

#endif
