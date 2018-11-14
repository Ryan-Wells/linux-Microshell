#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <stdio.h>
#include <ctype.h>
#include "defn.h"

#include <fcntl.h> 

//global variables
char stdoutString[1024];
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
int tested = 0;

//prototypes
int expand(char* orig, char* new, int newsize);
int substring(char* str, char * substr);


//used to provide expansion capabilities to my microshell
int expand(char* orig, char* new, int newsize){

   char parenString[255];	//used to call processline on the string within $()
   char temp[newsize]; 		//string to copy words within braces into, and to copy file names
   char env[newsize];		//string to copy the environment variable into
   char dollars[25];  		//string to copy the PID into
   char sub[255];      		//string to copy characters into, to check for substrings
   int length = strlen(orig); 	//the length of the original line
   int matchedParen = 0;	//used to make sure we match our parentheses
   int dollarsIndex = 0;	//used for dollar processing
   int parenIndex = 0;		//used to iterate through our braces string
   int newIndex = 0;   		//used for adding characters into the new string
   int tmpIndex = 0;		//used to travel through temp
   int inBraces = 0;   		//used to check whether or not we are within a set of braces
   int subindex = 0;   		//used to insert file names into the string
   int templen = 0;    		//the length of a file name we find
   int added = 0;      		//used to make sure we don't add too many file names to the string
   int subd = 0;  		//used to help remove *xyz when we search for files


   for(int i = 0; i < length; i++){

      if(had_Signal){
         new[0] = '\0';
         exitValue = -1;
      }
      else{
   
         if(orig[i] == '\\'){
            i++;
            new[newIndex++] = orig[i];
         }
   
         else if(orig[i] == ')'){
            i++;
            matchedParen--;
            new[newIndex++] = ' ';
         }
   
         //abcd... = just copy into the new string
         else if((orig[i] != '*' && orig[i] != '$' && orig[i] != '\\') || (orig[i] == '*' && orig[i-1] != ' ')){
            if(newIndex == newsize){
               fprintf(stderr, "expand error: size is larger than allowed\n");
               new[0] = '\0';
               return -2;
            }
            new[newIndex++] = orig[i];         
         }
   
         // *	                prev was space    ||  beginning ||  backslash
         if(orig[i] == '*' && (((((orig[i-1] == ' ') || (i-1 == 0) || (orig[i-1] == '\\')))))){
   
            // \*
            if(orig[i-1] == '\\'){	//store * in the expanded line
               if(newIndex == newsize){
                  fprintf(stderr, "expand error: size is larger than allowed\n");
                  new[0] = '\0';
                  return -2;
               }
               new[newIndex - 1] = orig[i+1];
               i++;    
            }
   
            else{
               DIR *direct;
               struct dirent *directory;
   
               // * appears by itself: replace * with all names of files in directory that do not start with a period
               if((orig[i+1] == ' ') || (orig[i+1] == '\0')){
   
                  //open the current directory
                  direct = opendir(".");
                  if(direct != NULL){
                     while((directory = readdir(direct)) != NULL){
                        char* temp = directory->d_name;
                        if(temp[0] != '.'){
                           templen = strlen(temp);
                           added += (templen + 1);
                           if(newIndex + added >= newsize){
                              fprintf(stderr, "expand error: size is larger than allowed\n");
                              new[0] = '\0';
                              return -2;
                           }
                           //add the file name to the new string
                           for(int i = 0; i < templen; i++){
                              if(newIndex == newsize){
                                 fprintf(stderr, "expand error: size is larger than allowed\n");
                                 new[0] = '\0';
                                 return -2;
                              }
                              new[newIndex++] = temp[i];
                           }
                           new[newIndex++] = ' ';
                        }
                     }
                     closedir(direct);
                  }
               }
   
               //*xyz appears: replace * with the names of files in the directory that don't start with a period, and end with xyz
               else{
                  subindex = 0;
                  i++;
                  //find the letters after *
                  while(orig[i] != ' ' && orig[i] != '\0'){
                     if(orig[i] == '/'){
                        fprintf(stderr, "./ush: no match\n");
                        return -1;
                     }
                     sub[subindex++] = orig[i++];
                  }
                  sub[subindex] = '\0';
                  new[newIndex++] = '*';
                  for(int i = 0; i < subindex; i++){
                     if(newIndex == newsize){
                        fprintf(stderr, "expand error: size is larger than allowed\n");
                        new[0] = '\0';
                        return -2;
                     }
                     new[newIndex++] = sub[i];
                  }
   
                  //open the current directory
                  direct = opendir(".");
                  if(direct != NULL){
                     while((directory = readdir(direct)) != NULL){
                        char* temp = directory->d_name;
                        if(temp[0] != '.'){   
      
       			   //if a filename ends with xyz, add it to the string
                           if(substring(temp, sub)){
                              if(subd == 0){
                                 newIndex -= subindex + 1;
                                 subd = 1;
                              }
                              templen = strlen(temp);
                              added += (templen + 1);
                              if(newIndex + added >= newsize){
                                 fprintf(stderr, "expand error: size is larger than allowed\n");
                                 new[0] = '\0';
                                 return -2;
                              }
                              for(int i = 0; i < templen; i++){
                                 new[newIndex++] = temp[i];
                              }
                              new[newIndex++] = ' ';
                           }
                        }
                     }
                     closedir(direct);
                  }
               }
            }
         }
   
   
         //$
         if(orig[i] == '$'){
   
            if(newIndex == newsize){
               fprintf(stderr, "expand error: size is larger than allowed\n");
               new[0] = '\0';
               return -2;
            }
   
            new[newIndex++] = orig[i++];
   
   
            //$? = replace $? with the exit value of the previous command.
            if(orig[i] == '?'){
   
               char myVal[5];
               newIndex--;
   
               //if the command was a built-in command, use 0 for a successful built-in, otherwise use 1
               if(wasBuiltin == 1){
                  wasBuiltin = 0;
                  sprintf(myVal, "%d", exitValue);
                  int len = strlen(myVal);
   
                  if(newIndex + len > newsize){
                     fprintf(stderr, "expand error: size is larger than allowed ( $? processing )\n");
                     new[0] = '\0';
                     return -2;
                  }
                  else{
                     for(int i = 0; i < len; i++){
                        new[newIndex++] = myVal[i];
                     }
                  }
               }
               else{
                  int len = strlen(signalString);
                  if(newIndex + len > newsize){
                     fprintf(stderr, "expand error: size is larger than allowed ( $? processing )\n");
                     new[0] = '\0';
                     return -2;
                  }
                  else{
                     for(int i = 0; i < len; i++){
                        new[newIndex++] = signalString[i];
                     }
                  }
               }
            }
   
   
   
            //$( = process commands between the parentheses
            else if(orig[i] == '('){
               int t = 0;
 
               //check for missing parentheses
               if(tested == 0){
                  for(int i = 0; i < length; i++){
                     if(orig[i] == '('){
                        t++;
                     }
                     if(orig[i] == ')'){
                        t--;
                     }
                  }
                  if(t > 0){
                     fprintf(stderr, "expand error: missing )\n");
                     new[0] = '\0';
                     return -2;
                  }
                  tested++;
               }
               int size = 0;
               newIndex--;
               matchedParen++;
               i++;
               //allow for 200,000 characters
               newsize = 200000;
   
               while(matchedParen > 0 && orig[i] != ')' && size < 500){
   
                  parenString[parenIndex++] = orig[i++];
                  size++;
                  if(orig[i] == ')'){
                     matchedParen --;
                  }
                  else if(orig[i] == '('){
                     matchedParen ++;
                  }   
               }
               parenString[parenIndex++] = ')';
               parenString[parenIndex] = '\0';

   
               //create pipe
               int myPipe[2];	
               if(pipe(myPipe) == -1){
                  perror("pipe");
               }
               piping = 1;
   
               //call processline, passing read and write end of pipe.    
    	       processline(parenString, 0, myPipe[1], 6);
   
	       //close write end of pipe
               close(myPipe[1]);

               //read from the pipe and put it directly into the new string
               int x;
                  while((x = read(myPipe[0], &new[newIndex], (newsize - newIndex - 1))) != 0 && !had_Signal){
   	          newIndex += x;
      	       }
            
               close(myPipe[0]);
               parenIndex = 0;
   
               //remove newline characters
               for(int i = x; i < newIndex; i++){
                  if(new[i] == '\n'){
                     if(i < newIndex - 1){
                        new[i] = ' ';
                     }
                     else{
                        newIndex--;
                     }
                  }
               }
   
               //if processline started a child running, now wait on it

//waitpid!!
               waitpid(1, &status, 0);
               piping = 0;
   
            }
   
   
            //$$ = replace $$ with the PID
            else if(orig[i] == '$'){
               newIndex--;
               int pid = getpid();
               snprintf(dollars, sizeof(dollars), "%d", pid);
               for(int k = 0; k < strlen(dollars); k++){
                  new[newIndex++] = dollars[k];
               }
            }

            //$n... (NON-INTERACTIVE) = replace $n with argc[startPoint + n] 
            else if(isdigit(orig[i]) && interactive == 0){
               dollarsIndex = 0;
               newIndex--;
               dollars[dollarsIndex++] = orig[i++];
   
               //walk forward until we find a non-number, all the while putting digits into dollars
               while(isdigit(orig[i])){
                  dollars[dollarsIndex++] = orig[i++];
               }
   
               //then, atoi what we got. 
               int number = atoi(dollars);
   
               //if n + startPoint + > arguments, replace with empty string
               if((number + startPoint) > mainargc - 1){
                  new[newIndex++] = orig[i];
               }
   
               //replace $0 with the script name
               else if(number == 0){
                  char* script = mainargv[1];
                  int len = strlen(script);
                  i--;
                  for(int id = 0; id < len; id++){
                     new[newIndex++] = script[id];
                  }
               }
   
               else{
                  //get the (startPoint + number)th argument
                  char* nthWord = mainargv[startPoint + number];
   
                  //finally, replace $n with the argument.
                  if(nthWord != NULL){
                     int len = strlen(nthWord);
                     i--;
                     for(int id = 0; id < len; id++){
                        if(newIndex == newsize){
                           fprintf(stderr, "expand error: size is larger than allowed\n");
                           new[0] = '\0';
                           return -2;
                        }
                        new[newIndex++] = nthWord[id];
                     }
                  }
               }
            }

            //$n... (INTERACTIVE) = replace $0 with shell name, replace $n with " "
            else if(isdigit(orig[i]) && interactive == 1){
   
               newIndex--;
               //$0 = shell name
               if(orig[i] == '0'){
                  char* shell = mainargv[0];
                  int shellLength = strlen(shell);
                  for(int sh = 0; sh < shellLength; sh++){
                     if(newIndex == newsize){
                        fprintf(stderr, "expand error: size is larger than allowed\n");
                        new[0] = '\0';
                        return -2;
                     }
                     new[newIndex++] = shell[sh];
                  }
               }   
   
               //$n = " "  
               if(isdigit(orig[i]) && orig[i] != '0'){
                  while(isdigit(orig[i + 1])){
                     i++;
                  }
                  new[newIndex++] = ' ';
               }
            }


            //$# = replace $# with the number of arguments 
            else if(orig[i] == '#' && interactive == 0){
               char args[10];
               sprintf(args, "%d", arguments);
               int argsLength = strlen(args);
               args[argsLength] = '\0';
               newIndex--;
               for(int i = 0; i < argsLength; i++){
                  if(args[i] != 0){
                     new[newIndex++] = args[i];
                  }
               }
            }
   	 
   	    //$# (interactive) = replace with 1, because we supplied one argument.
            else if(orig[i] == '#' && interactive == 1){
               //newIndex--;
               new[newIndex++] = '1';
            }


            //${ = copy everything until '}' into temp, then replace it with getenv(temp)
            else if(orig[i] == '{'){
               inBraces = 1;
               newIndex--;
            
               while(inBraces == 1){
                  //until we reach the second brace, copy everything into temp 
                  if(orig[i] != '{'){
                     temp[tmpIndex++] = orig[i];			
                  }
                  i++;
                  //we never find the second }, print error
                  if(orig[i] == '\0'){
                     fprintf(stderr, "expand error: no matching }\n");
                     return -1;
                  }
               
                  //we have found the second '}', so turn temp into the environment variable
                  if(orig[i] == '}'){
                     temp[tmpIndex] = 0;
                     if(getenv(temp) != NULL){
                        strcpy(env, getenv(temp));	
                     }
                     inBraces = 0;
                     //if what we expanded is too large, then we quit
                     if(((int)strlen(env) + newIndex) >= newsize){
                        fprintf(stderr, "expand error: size is larger than allowed\n");
                        new[0] = '\0';
                        return -2;
                     }
                     //then we put that env string into 'new', replacing the brackets and their contents
                     for(int j = 0; j < strlen(env); j++){
                        new[newIndex++] = env[j];
                     }
                  }
               }
               
            }
            //$a = continue copying, there were no braces found.
            else{
               if(newIndex == newsize){
                  fprintf(stderr, "expand error: size is larger than allowed\n");
                  new[0] = '\0';
                  return -2;
               }
               new[newIndex++] = orig[i];
            }
         }
      }
   }

   new[newIndex] = '\0';

   return 1;
}


//used to check if a string ends with a subtring
int substring(char* str, char * substr){
   int len = strlen(str);
   int len2 = strlen(substr);
   int dif = len - len2;

   if((len >= len2) && (strcmp(str + dif, substr) == 0)){
      return 1;
   }
   else{
      return 0;
   }
}


