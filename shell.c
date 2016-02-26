/**
　* SIMPLE SHELL:
 * cd, pwd, ls, set/get env, redirect, POXIS
 */
  
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <sys/wait.h>
#include <fcntl.h>

#include "gosh.h"


extern int errno;

char buffer[512]; 

int numLiveChildren = 0;

void sigchldHandler(int sig) {
  int status, savedErrno;
  pid_t childPid;

  savedErrno = errno;

  while ((childPid = waitpid(-1, &status, WNOHANG)) > 0) {
    printf("handler: Reaped child %ld - \n", (long) childPid);
    numLiveChildren--;
  }

  if (childPid == -1 && errno != ECHILD){
    printf("waitpid error\n");
  }
  
  //sleep(2);
  errno = savedErrno;
}

int main() {

  int retval, input_stat;
  char *com;
  
  struct command_t command_a;

  /*  retval = start_sig_catchers(); */

  while(1) {

    retval = init_command(&command_a);

    printf("gosh> ");

    /*  retval = simple_accept_input(&command_a); */

    com = (char *) malloc(150 * sizeof(char *));
    gets(com);

    input_stat = simple_accept_input(&command_a, com);

    if(input_stat == 2){
      ;
    }else if(input_stat == 1){
      exit(0);
    }else{
      print_command(&command_a, "c1");
    }

    /* 
       If the input processing was sucessful, call a fork/exec
       function.  Otherwise, exit the program 
    */
    if (retval == 0 && input_stat == 0) {      

      /* 
	       simple_fork_command(&command_a);
      */
      simple_fork_command(&command_a);

    }else if(retval == 0 && input_stat == 3){
      /* 
        Support of pwd command
      */
      simple_pwd_command(&command_a);

    }else if(retval == 0 && input_stat == 4){
      /* 
        Support of cd command
      */
      simple_cd_command(&command_a);

    }else if(retval == 0 && input_stat == 5){
      /*
        support of get environ
      */
      simple_get_env(&command_a);

    }else if(retval == 0 && input_stat == 6){
      /* 
        support of set environ
      */
      simple_set_env(&command_a);

    }else if (retval == 1) {
      exit(0);
    }
    free(com);
  }
  exit(0);
}

/* Helper Function.  Initialized a command_t struct */
int init_command(struct command_t *cmd) {
  int i;
  for(i = 0; i < cmd->num_args; i++){
    cmd->args[i] = NULL;
  }
  cmd->num_args = 0;
  cmd->outfile[0] = '\0';
  cmd->infile[0] = '\0';
  return(0);
}

/* Helper Function.  Print out relevent info stored in a command_t struct */
int print_command(struct command_t *cmd, const char *tag) {
  int i;
  
  for (i=0; i<cmd->num_args; i++) {
    printf("%s %d: %s\n", tag, i, cmd->args[i]);
  }

  if (cmd->outfile[0] != '\0') {
    printf("%s outfile: %s\n", tag, cmd->outfile);
  }

  if (cmd->infile[0] != '\0') {
    printf("%s infile: %s\n", tag, cmd->infile);
  }

  return(0);
}


/* Problem 1: read input from stdin and split it up by " " characters.
   Store the pieces in the passed in cmd_a->args[] array.  If the user
   inputs 'exit', return a 1.  If the user inputs nothing (\n), return
   a value > 1.  If the user inputs somthing else, return a 0. */
int simple_accept_input(struct command_t *cmd_a, char *com) {
  if(strcmp(com, "exit") == 0){
    return (1); //exit command
  }else if(strlen(com) == 0){
    return(2); //\n command
  }

  int i, j, strt = 0, end = 0, row = 0, col = 0, count = 0;
  cmd_a->args[row] = (char *) malloc(500 * sizeof(char *));

  for(i = 0; i < strlen(com); i++){
    if(com[i] == '\'') {
      count++;
    }

    if(count % 2 == 0){
      if(com[i] == ' '){
        cmd_a->args[row][col] = '\0';
        row++;
        cmd_a->args[row] = (char *) malloc(500 * sizeof(char *));
        col = 0;
        continue;
      }
    }
    cmd_a->args[row][col++] = com[i];  
  }
  cmd_a->args[row][col] = '\0';
  cmd_a->num_args = row+1;
    
  if(cmd_a->num_args == 1 && strcmp(cmd_a->args[0], "pwd") == 0){
    return(3);//pwd command
  }else if((cmd_a->num_args == 2 || cmd_a->num_args == 1) && strcmp(cmd_a->args[0], "cd") == 0){
    return(4);//cd command
  }else if(cmd_a->num_args == 2 && strcmp(cmd_a->args[0], "echo") == 0){
    return(5);//get environment
  }else if(cmd_a->num_args == 1){
    int i;
    for(i = 0; i < strlen(cmd_a->args[0]); i++){
      if(cmd_a->args[0][i] == '=') {
        if(i != 0 && i != strlen(cmd_a->args[0])-1){
          return(6);
        }
      }
    }//set environment
  }

  return(0);
}

/* Simple fork/exec/wait procedure that executes the command described 
   in the passed in 'cmd' pointer. */
int simple_fork_command(struct command_t *cmd) {
  pid_t child;

  sigset_t blockMask, emptyMask;
  struct sigaction sa;

  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sa.sa_handler = sigchldHandler;

  if (sigaction(SIGCHLD, &sa, NULL) == -1){
    printf("sigaction error\n");
    exit(-1);
  }

  sigemptyset(&blockMask);
  sigaddset(&blockMask, SIGCHLD);
  if (sigprocmask(SIG_SETMASK, &blockMask, NULL) == -1){
    printf("sigprocmask error\n");
    exit(1);
  }

  numLiveChildren++;

  child = fork();

  if(child == 0){
    printf("Running Command\n");
    printf("---------------\n");
    if(cmd->num_args == 4 && strcmp(cmd->args[3], "2>&1") == 0 && strcmp(cmd->args[1], ">") == 0){
      int fd_out = open(cmd->args[2], O_CREAT | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
      if (fd_out > 0) {
        printf("dup file handle: success\n");
        dup2(fd_out, 1);  
        dup2(fd_out, 2); 
        close(fd_out);
        execvp(cmd->args[0], cmd->args);
      }
    }else{
      execvp(cmd->args[0], cmd->args);
    }
    printf("Command Not Found\n");
    exit(1);
  }
  else if(child == -1){
    exit(1);
  }
  else{

    sigemptyset(&emptyMask);

    while (numLiveChildren > 0) {
      if (sigsuspend(&emptyMask) == -1 && errno != EINTR) {
        printf("Child terminated abnormally\n");
        exit(-1);
      }
    }

    printf("---------------\n");
    printf("Command Returned Exit Code 0\n");

    /*
    int stat_val;
    pid_t child_pid;
    child_pid = wait(&stat_val);

    if(WIFEXITED(stat_val)){
      printf("---------------\n");
      printf("Command Returned Exit Code %d\n", WEXITSTATUS(stat_val));
    }
    else
      printf("Child terminated abnormally\n");*/
  }

  return(0);
}

int simple_pwd_command(struct command_t *cmd) {
  printf("Running Command\n");
  printf("---------------\n");
  memset(buffer, 0, sizeof(buffer));  
  getcwd(buffer, sizeof(buffer));  
  printf("%s\n", buffer);  
  printf("---------------\n");
  printf("Command Returned Exit Code 0\n");
  return(0);
}

int simple_cd_command(struct command_t *cmd) {
  printf("Running Command\n");
  printf("---------------\n");
  if(cmd->args[1] == NULL){
    chdir("/home");
  }else{
    chdir(cmd->args[1]);
  }
  printf("change dir: success\n");
  printf("---------------\n");
  printf("Command Returned Exit Code 0\n");
  return(0);
}

int simple_get_env(struct command_t *cmd){
  printf("Running Command\n");
  printf("---------------\n");
  int i, j = 0;
  char *name;
  name = (char *)malloc(200 * sizeof(char));
  if(cmd->args[1][0] != '$'){
    printf("format error\n");
    printf("---------------\n");
    printf("Command Returned Exit Code 1\n");
    return(1);
  }
  for(i = 1; i < strlen(cmd->args[1]); i++){
    name[j++] = cmd->args[1][i];
  }
  name[j]='\0';
  char * path = getenv(name);
  if(path != NULL){
    printf("%s\n", path);
  }else{
    printf("No such variable\n");
  }
  printf("---------------\n");
  printf("Command Returned Exit Code 0\n");
  free(name);
  return(0);
}

int simple_set_env(struct command_t *cmd){
  printf("Running Command\n");
  printf("---------------\n");
  int i, j, k = 0;
  char *myenv;
  char *envname;
  myenv = (char *)malloc(1024 * sizeof(char));
  envname = (char *)malloc(200 * sizeof(char));
  for(i = 0; i < strlen(cmd->args[0]); i++){
    if(cmd->args[0][i] == '='){
      for(j = 0; j < i; j++){
        envname[k++] = cmd->args[0][j];
      }
      envname[k]='\0';
      k = 0;
      for(j = i+1; j < strlen(cmd->args[0]); j++){
        if(cmd->args[0][j] == '\'') continue;
        myenv[k++] = cmd->args[0][j];
      }
      myenv[k]='\0';
      break;
    }
  }
  setenv(envname,myenv,0);
  printf("define new variable: success\n");
  printf("---------------\n");
  printf("Command Returned Exit Code 0\n");
  free(myenv);
  free(envname);
  return(0);
}

/* Problem 3: set up all of your signal actions here */
int start_sig_catchers(void) {
  return(0);
}
