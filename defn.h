//prototype for expand
int expand(char* orig, char* new, int newsize);

//prototype for the built-in processing
int processBuiltin(char** line, int argcptr, int outfd);

//prototype for processline
int processline(char* line, int infd, int outfd, int flags);

