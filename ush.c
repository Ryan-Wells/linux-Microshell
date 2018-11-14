/* CS 352 -- Micro Shell!  
 *
 *   Sept 21, 2000,  Phil Nelson
 *   Modified April 8, 2001 
 *   Modified January 6, 2003
 *   Modified January 8, 2017
 *
 *   April 12, 2018, Ryan Wells
 *   Modified April 23, 2018
 *   Modified May 6, 2018
 *   Modified May 21, 2018
 */

#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include "defn.h"

//Constants
#define LINELEN 1024

//Flags
#define WAIT     1
#define NOWAIT   2
#define EXPAND   4
#define NOEXPAND 8

//global variables
char signalString[5];
char** mainargv;
int interactive;
int had_Signal;
int wasBuiltin;
int startPoint;
int arguments;
int exitValue;
int mainargc;
int status;
int piping;

//Prototypes
int processline(char* line, int infd, int outfd, int flags);
char** arg_parse(char* line, int* argcptr);  
int countArguments(char* line);
void removeQuotes(char* str);
int countQuotes(char* line);
void sigintHandler(int sig);


//Shell main 
int main(int argc, char** argv){
   startPoint = 1;
   arguments = argc - 1;
   char buffer [LINELEN];
   FILE* myfile;
   myfile = stdin;     
   interactive = 1; 

   //if we run ush with an argument, read from a file instead of stdin
   if(argc > 1){
      myfile = fopen(argv[1], "r");
      interactive = 0;
      if(myfile == NULL){
         fprintf(stderr, "./ush: Can not open %s\n", argv[1]);
         exit(127);
      }
   }

   //signal handling
   signal(SIGINT, sigintHandler);


   int len;
   while(1){
      had_Signal = 0;
      //prompt and get line
      if(interactive == 1){
         fprintf (stderr, "%% ");
      }
      if(fgets (buffer, LINELEN, myfile) != buffer){
         break;
      }
      //Get rid of \n at end of buffer.
      len = strlen(buffer);
      if(buffer[len-1] == '\n'){
         buffer[len-1] = 0;
      }

      //Process comments
      int lastWas$ = 0;
      for(int i = 0; i < len; i++){
         if(buffer[i] == '$'){
            lastWas$ = 1;
         }
         else if(buffer[i] == '#'){
            if(lastWas$ == 0){
               buffer[i] = '\0';
               lastWas$ = 0;
            }
         }
         else{
            lastWas$ = 0;
         }
      }

      //set global variables
      mainargc = argc;
      mainargv = argv;

      //Run it ... 
      processline (buffer, 0, 1, 5);  

   }
   if(!feof(myfile)){
      perror("read");
   }
   return 0;		//Also known as exit (0);
}



//processline function
int processline(char* line, int infd, int outfd, int flags){

   pid_t cpid;
   int argcptr;
   int index = 0;
   char expandedLine[LINELEN];

   //call expand for a new line
   if(flags & EXPAND){
      int exp = expand(line, expandedLine, LINELEN);
      if(exp == -1){
         strcpy(expandedLine, " ");
      }
   }
   else{
      strcpy(expandedLine, line);
   }

   //test to see if we need to do pipelining
   int expLen = strlen(expandedLine);
   int pipeLining = 0;

   for(int i = 0; i < expLen; i++){
      if(expandedLine[i] == '|'){
         pipeLining++;
      }
   }

   int max = pipeLining;

   //process pipes
   if(pipeLining > 0){
      
      char* commands[max + 1];
      char* cmd = strtok(expandedLine, "|");
      //get our list of commands.
      while(cmd != NULL){
         commands[index++] = cmd;
         cmd = strtok(NULL, "|");
      }

      int oldinfd = 0;
      int mypipe[2];

      flags = 10; //NOWAIT, NOEXPAND

      //perform processline on the beginning commands
      for(int i = 0; i < max; i++){

         if(pipe(mypipe) == -1){
            perror("pipe");
         }
  
         processline(commands[i], oldinfd, mypipe[1], flags);  //check here for errors!

         if(oldinfd != 0){
            close(oldinfd);
         }

         close(mypipe[1]);
         oldinfd = mypipe[0];

      }

      flags = 9; //WAIT, NOEXPAND

      //perform processline on the final command
      processline(commands[max], oldinfd, 1, flags);  //check for errors here

      if(oldinfd != 0){
         close(oldinfd);
      }
   }

   else{
      //perform our arg_parse function
      char** argList = arg_parse(expandedLine, &argcptr);
      if(argcptr == 0){
         return -1;
      }
   
      //if it's a built-in, execute it and then don't fork, just free arglist.
      int test = processBuiltin(argList, argcptr, outfd);
   
   
      //otherwise, continue and fork 
      if(test < 0){ 
         //Start a new process to do the job. 
         cpid = fork();
         if(cpid < 0){
            //Fork wasn't successful 
            perror("fork");
            free(argList);
            return -1;
         }
       
         //Check for who we are!
         if(cpid == 0){
            //We are the child!
   
            if(outfd != 1){
               dup2(outfd, 1);
            }
            if(infd != 0){
               dup2(infd, 0);
            }
   
            execvp(argList[0], argList);         

            //execvp returned, wasn't successful
            perror("exec");
            fclose(stdin);  // avoid a linux stdio bug
            exit(127);
         }


         //Have the parent wait for child to complete, if flags allow
         if(flags & WAIT){
   
            if(wait (&status) < 0){
               //Wait wasn't successful
               perror("wait");
            }
            else{
   
               //If the command called _exit(2), use the value passed to that function
               if(WIFEXITED(status)){
                  sprintf(signalString, "%d", WEXITSTATUS(status));
               }
   
               //If the command was killed by a signal, use the number 128 plus the signal number.
               else if(WIFSIGNALED(status)){
                  sprintf(signalString, "%d", (128 + WTERMSIG(status)));
               }
   
               //report if a child has been terminated by a signal other than SIGINT
               if((WIFSIGNALED(status) && ((WTERMSIG(status) != SIGINT)))){

                  dprintf(outfd, "%s ", strsignal(status));
                  if(WCOREDUMP(status)){
                     dprintf(outfd, " (core dumped)");
                  }
                  dprintf(outfd, "\n");
               }


               free(argList);
               //since no child is running, return 0
               return 0;
            }
         }
         else{
            //if a child is running, return the child's pid
            return cpid;
         } 
   
      }

   }
   //it was a builtin, so exitValue was set to 0 or 1, depending on success
   wasBuiltin = 1;
   return exitValue;

}



//used to go through the line and find the arguments
char** arg_parse(char* line, int* argcptr){
   int argCount = 0;
   int lastWasSpace = 1;
   int j = 0;
   int quoteNum = 0;
   int argEnded = 1;
   char** error = (char**)malloc((1) * sizeof(char*));
   error[0] = NULL; //used to exit arg_parse if there is an odd number of quotes
   int length = strlen(line);
   //Count the quotes. If there was an odd amount, then we exit.
   if(countQuotes(line) & 1){
      fprintf(stderr, "odd number of \" in line.\n");
      return error;
   }
   //count the number of arguments
   argCount = countArguments(line);
   //set argcptr to equal argCount
   *argcptr = argCount;
   //malloc an array to put the pointers in
   char** argList = (char**)malloc((argCount+1) * sizeof(char*));
   if(argList == NULL){
      return error;
   }
   argList[argCount] = NULL;
   j = 0;
   for(int i = 0; i < length; i++){
      //skip spaces
      if(line[i] == ' '){
         lastWasSpace = 1;
      }
      //found an arg
      if(line[i] != ' ' && lastWasSpace == 1){
         //if it started with quote
         if(line[i] != '"'){
            argList[j++] = &line[i];
         }
         else{
            argList[j++] = &line[i++];
            quoteNum++;
         }
         argEnded = 0;
         while(argEnded == 0){
            //if we found a quote, increment quoteNum
            if(line[i] == '"'){
               quoteNum++;
            }
    
            //go until we find a space or '\0' with an even number of quotes
            if(((line[i] == ' ') || (line[i] == 0)) && (quoteNum & 1) == 0){
               line[i] = '\0';
               argEnded = 1;
               lastWasSpace = 1;
            }
            else{
               i++;
            }
         }
      }
   }
   //remove the quotes from each argument
   for(int i = 0; i < argCount; i++){
      removeQuotes(argList[i]);
   }
 
   //return our final list of arguments
   return argList;
}



//used to count the quotes in a line
int countQuotes(char* line){
   int j = 0;
   for(int i = 0; i < strlen(line); i++){
      if(line[i] == '"'){
         j++;
      }
   }
   return j;
}



//used to count the number of arguments in a line
int countArguments(char* line){
   int argCount = 0;
   int j = 0;
   for(int i = 0; i < strlen(line); i++){
      
      if(line[i] == '"'){
         i++;
         while(line[i] != '"'){
            i++;
         }
      }
      if(line[i] == ' '){
         j = 0;
      }
      else if(j == 0){
         j = 1;
         argCount++;
      }
   }
   return argCount;
}



//used to remove the quotes from a string
void removeQuotes(char* str){
 
   int j = 0;
   for(int i = 0; i < strlen(str); i++){
      if(str[i] == '\\'){
         str[j++] = str[i++];
         str[j++] = str[i];
         if(str[i] == '\0'){
            break;
         }
      }
       if(str[i] != '"'){
         str[j++] = str[i];
      }
   }
   str[j] = '\0';
}



//signal handler
void sigintHandler(int sig){
   had_Signal = 1;
   sprintf(signalString, "%d", 130);
   exitValue = -1;
}




