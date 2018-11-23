# linux-Microshell

This is the result of a quarter-long project in a computer systems course. As time went on more and more functionality was added, and the microshell became more powerful.
As it stands, it is not robust enough to be applicable in any sense of the word, but building it from scratch was a great learning experience.

Instructions:
The program can be compiled with the "make" command. After that, it can be run using ush. (./ush)

-can read in command line arguments with and without quotes (arg1 "arg 2" arg"  "3)
-has environmental variable processing (${HOME})
-ignores comments (input line starting with #)
-can read in scripts through command line arguments (./ush script arg1 arg2 will read in commands through the script file)
-replaces $# with the number of arguments
-replaces $n with the n'th argument
-replaces $? with the terminating value
-expands commands within $()   (envset VAR $(expr ${VAR} + 1))
-processes pipelines to an extent  (finger | tail -n +2 | cut -c1-8 | uniq | sort)

commands:
exit [value]: exit the shell with the value, returning 0 if none specified.
envset NAME value: sets the given environment variable
envunset NAME: removes the variable from the current environment
cd [directory]: moves to the specified directory, or HOME if none are given.
shift [n]: shifts all arguments by a value of n. (by default, 1)
unshift [n]: unshifts all arguments by a value of n. (by default, resets all shifts)
sstat [file]: prints information such as file name, user id, permissions, size...

beyond that, common commands such as echo also should work with the shell.

