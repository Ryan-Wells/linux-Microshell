#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>
#include "defn.h"

//global variables
char** mainargv;
int interactive;
int startPoint;
int arguments;
int exitValue;
int mainargc;

//prototypes
int processBuiltin(char** line, int argcptr, int outfd);
void printStats(char* fileName, int outfd);


//used to check for and execute specific built-ins
int processBuiltin(char** line, int argcptr, int outfd){

   char* bin = " ";
   char* arg1 = " ";
   char* arg2 = " ";
   if(line[0] != NULL){
      bin = line[0];
   }
   if(line[1] != NULL){
      arg1 = line[1];
   }
   if(line[2] != NULL){
      arg2 = line[2];
   }

   //***exit [value]*** :  exit the shell with the value. if no value given, exit with value 0.
   if(strcmp(bin, "exit") == 0){
  
      if(strcmp(arg1, " ") == 0){
         exitValue = 0;
         exit(0);
      }
      else{
         int exitNum = atoi(arg1);
         exitValue = 0;
         exit(exitNum);
      }
      exitValue = 1;
   }

   //***envset NAME VALUE*** :  sets the environment variable of the same name to the given value
   else if(strcmp(bin, "envset") == 0){
      if(strcmp(arg1, " ") == 0){
         fprintf(stderr, "envset error: cannot make set an environment variable with an empty string!\n");
         exitValue = 1;
      }
      else{
         if(strcmp(arg2, " ") == 0){
            fprintf(stderr, "envset error: cannot set '%s' to a null value!\n", arg1);
            exitValue = 1;
         }
         else{
            int test = setenv(arg1, arg2, 1);
            if(test == 0){
               exitValue = 0;
            }
            else{
               exitValue = 1;
               perror("envset error: ");
            }
         }
      }
      return 1;
   }

   //***envunset NAME*** :  removes the variable NAME from the current environment
   else if(strcmp(bin, "envunset") == 0){
      
      if(strcmp(arg1, " ") == 0){
         exitValue = 1;
         fprintf(stderr, "envunset error: cannot unset nothing.\n");
      }
      if(strcmp(arg2, " ") != 0){
         exitValue = 1;
         fprintf(stderr, "envunset error: too many commands.\n");
      }
      else{
         int test = unsetenv(arg1);
         if(test != 0){
            exitValue = 1;
            perror("envunset error: ");
         }
         else{
            exitValue = 0;
         }
      }
      return 1;
   }

   //***cd [directory]*** :  uses chdir to change the working directory for the shell. 
   else if(strcmp(bin, "cd") == 0){
      //if nothing is specified, cd to home
      if(strcmp(arg1, " ") == 0){
         if(getenv("HOME")){
            exitValue = 0;
            chdir(getenv("HOME"));
         }
         else{
            exitValue = 1;
            perror("cd error: ");
         }
      }
      //if something is specified, try to cd to it
      else{
         int test = chdir(arg1);
         if(test == -1){
            exitValue = 1;
            perror("cd error: ");
         }
         else{
            exitValue = 0;
         }
      }
      return 1;
   }



   //***sstat file [file]... : print out the statistics of one or more given files. 
   else if(strcmp(bin, "sstat") == 0){
      int argnum = 2;
      if(line[1] != NULL){
         FILE * test = fopen(line[1], "r");
         if(test){
            fclose(test);
            printStats(line[1], outfd);
            exitValue = 0;
         }
         else{
            exitValue = 1;
            perror("sstat error: ");
         }
      }
      while(line[argnum] != NULL){
         if(line[1] != NULL){
            FILE * test = fopen(line[argnum], "r");
            if(test){
               fclose(test);
               printStats(line[argnum], outfd);
               exitValue = 0;
            }
            else{
               exitValue = 1;
               perror("sstat error: ");
            }
            argnum++;
         }
      }
      return 1;
   }



   //***shift [n]: strange shenanigans.   
   else if(strcmp(bin, "shift") == 0){

      //if an argument is supplied, shift by given argument.
      if(line[1] != NULL){
         int shiftAmount = atoi(line[1]);
         if((shiftAmount + startPoint) < mainargc){
            startPoint += shiftAmount;
            arguments -= shiftAmount;
            exitValue = 0;
         }
         else{
            exitValue = 1;
            fprintf(stderr, "shift error: not enough parameters for a shift of %d!\n", shiftAmount);
         }
      }

      //if no argument is given, shift by 1
      else{
         if(arguments > 1){
            startPoint++;
            arguments--;
            exitValue = 0;
         }
         else{
            exitValue = 1;
            fprintf(stderr, "shift error: not enough parameters for a shift of 1!\n");
         }
      }
      return 1;
   }



   //***unshift [n]: ...
   else if(strcmp(bin, "unshift") == 0){

      //if an argument is supplied, unshift by the given argument
      if(line[1] != NULL){

         int shiftAmount = atoi(line[1]);
         if(shiftAmount < startPoint){
            startPoint -= shiftAmount;
            arguments += shiftAmount;
            exitValue = 0;
         }
         else{
            exitValue = 1;
            fprintf(stderr, "unshift error: not enough parameters for an unshift of %d!\n", shiftAmount);
         }
      }

      //if no argument is given, reset
      else{
         exitValue = 0;
         arguments += (startPoint - 1);
         startPoint = 1;
      }  
      return 1;
   }


   //the line didn't contain any built-ins, so return -1
   else{
      return -1;
   }
}


//used to print the stats required in sstat
void printStats(char* fileName, int outfd){
   struct stat fileStats;       
   struct tm *modTime;
   struct passwd *pwd;
   struct group *grp;

   stat(fileName, &fileStats);
   modTime = localtime(&fileStats.st_mtime);
   int perms = fileStats.st_mode;

   //start printing
   dprintf(outfd, "%s ", fileName);

   if((pwd = getpwuid(fileStats.st_uid)) != NULL){
      dprintf(outfd, "%s ", pwd->pw_name);
   }
   else{
      dprintf(outfd, "%d ", fileStats.st_uid);
   }

   if((grp = getgrgid(fileStats.st_gid)) != NULL){
      dprintf(outfd, "%s ", grp->gr_name);
   }
   else{
      dprintf(outfd, "%d ", fileStats.st_gid);
   }

   //print permissions
   dprintf(outfd, "-");
   (perms & S_IRUSR) ? dprintf(outfd, "r") : dprintf(outfd, "-");
   (perms & S_IWUSR) ? dprintf(outfd, "w") : dprintf(outfd, "-");
   (perms & S_IXUSR) ? dprintf(outfd, "x") : dprintf(outfd, "-");

   (perms & S_IRGRP) ? dprintf(outfd, "r") : dprintf(outfd, "-");
   (perms & S_IWGRP) ? dprintf(outfd, "w") : dprintf(outfd, "-");
   (perms & S_IXGRP) ? dprintf(outfd, "x") : dprintf(outfd, "-");

   (perms & S_IROTH) ? dprintf(outfd, "r") : dprintf(outfd, "-");
   (perms & S_IWOTH) ? dprintf(outfd, "w") : dprintf(outfd, "-");
   (perms & S_IXOTH) ? dprintf(outfd, "x") : dprintf(outfd, "-");
   dprintf(outfd, " ");

   dprintf(outfd, "%lu ", fileStats.st_nlink);
   dprintf(outfd, "%lu ", fileStats.st_size);
   dprintf(outfd, "%s", asctime(modTime));
   //done printing

}
