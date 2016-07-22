#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

#include "elcapo.h"

static char *program_name;
int running_count;

void print_usage_and_exit(int status) {
  
  printf("Usage: %s -c config_file [-h] [-d] [-r retries]\n", program_name);
  printf("Where\n");
  printf("\t-c config_file\t\tSet elCAPO configuration file\n");
  printf("\t-h\t\t\tShow this help and terminate\n");
  printf("\t-d\t\t\tRun as a daemon\n");
  printf("\t-r retries\t\tRetry failed commands for `retries` times\n");
  printf("\t-l log_file\t\tUse log_file for stdout and stderr when running as daemon\n");
  exit(status);
}

void interrupt_handler(int signo) {
  printf("Number of processes running: %i\n", running_count);
}

void parse_program_options(int argc, char **argv, struct program_options *options) {

  bool set_config = false, set_logfile = false;
  char opt;

  bzero(options->config_file, MAX_PATH_LENGTH);
  bzero(options->log_file, MAX_PATH_LENGTH);
  options->daemonize = false;
  options->retries = DEFAULT_RETRIES;

  while((opt = getopt(argc, argv, "hdc:r:l:")) != -1) {
    switch(opt) {
    case 'd':
      options->daemonize = true;
      break;
    case 'c':
      set_config = true;
      strncpy(options->config_file, optarg, MAX_PATH_LENGTH-1);
      break;
    case 'r':
      options->retries = atoi(optarg);
      if (options->retries < 1) {
        fprintf(stderr, "Invalid retries number: %d\n", options->retries);
        print_usage_and_exit(1);
      }
      break;
    case 'l':
      set_logfile = true;
      strncpy(options->log_file, optarg, MAX_PATH_LENGTH-1);
      break;
    case 'h':
      print_usage_and_exit(0);
    default:
      print_usage_and_exit(1);
    }
  }

  if (!set_config) {
    fprintf(stderr, "Must specify configuration file\n");
    print_usage_and_exit(1);
  }

}


int main(int argc, char **argv) {

  struct program_options options;

  program_name = argv[0];

  // -- Parse command line options
  parse_program_options(argc, argv, &options);

#ifdef DEBUG
  printf("*** Starting ElCAPO ***\n");
  printf("*** PID : %d ***\n", getpid());
  printf("*** PPID: %d ***\n", getppid());
  printf("*** PGID: %d ***\n", getpgrp());
  printf("*** SSID: %d ***\n", getsid(0));

  printf("*** Config file: %s ***\n", options.config_file);
  printf("*** Daemonize: %d ***\n", options.daemonize);
  printf("*** Retries: %d ***\n", options.retries);
#endif

  struct process_info process[MAX_PROCESSES];
  FILE *config;
  char line[260];
  char *token;
  char argument[20];
  int process_count, arg_count;
  char delimit[] = " \t\r\n\v\f";	

  // -- Read configuration file
  config = fopen(options.config_file, "r");
  if (config == NULL) {
    printf("Error opening file\n");
    exit(-1);
  }

  process_count = 0;
  arg_count = 0;

  while (!( feof(config) )) {
    fgets(line, 260, config);

    if (strlen(line) > 0) {
      token = strtok(line, delimit);
      if (*token == 'R')
        process[process_count].tries = 0;
      else
        process[process_count].tries = -1;

      token = strtok(NULL, delimit);
      strncpy(process[process_count].path, token, MAX_PATH_LENGTH-1);

      process[process_count].args[arg_count] = malloc(strlen(token)+1);
      strcpy(process[process_count].args[arg_count], token);
      arg_count++;

      while ((token = strtok(NULL, delimit)) != NULL) {
        process[process_count].args[arg_count] = malloc(strlen(token)+1);
        strcpy(process[process_count].args[arg_count], token);
        arg_count++;
      }

      process[process_count].args[arg_count] = NULL;
      arg_count = 0; 
      process_count++;     
    }
    line[0] = '\0';    
  }

  

  // -- If daemonized, create session, process group, and go to background
  //   -- Then close file descriptors and redirect stdout / stderr to /dev/null or log file
  if (options.daemonize) {
    pid_t child_id, session_id;

    child_id = fork();
    if (child_id != 0) 
      _exit(0);
     
    session_id = setsid();
    
    fclose(stdin);
    fclose(stdout);
    fclose(stderr);
    fclose(config);

    if (strlen(options.log_file) == 0) {
      freopen("/dev/null","w",stderr);
      freopen("/dev/null","w",stdout);
    }
    else {
      freopen(options.log_file, "w", stderr);
      freopen(options.log_file, "w", stdout);
    }
  }

  // -- Start child processes
  int i;
  pid_t id;
  running_count = 0;
  for (i=0; i<process_count; i++) {
    id = fork();
    if (id == 0) {
      int e = execv(process[i].path, process[i].args);
      if (e==-1)
        printf("path: %s\n", strerror(errno));
      _exit(0);
    }
    
    process[i].pid = id;
    running_count++;
    printf("|Started|: %s pid: %i\n", process[i].path, process[i].pid);  
  }
  

  // -- Set up signal handlers for SIGHUP to show the number of running processes
  signal(SIGHUP, interrupt_handler);
  
  // -- Wait for processes to terminate
  //   -- When a process terminates, check if it needs to be restarted.
  //   -- If it does, check if the exit status is != 0 and if it already used max retries
  int status;
  pid_t newId;
  while ( (id = wait(&status)) != -1) {
    printf("|FINISHED| pid:%i exit:%i\n", id, WIFEXITED(status));
    running_count--;
    if (WIFEXITED(status))
      continue;

    for (i=0; i<process_count; i++) {
      if (process[i].pid == id) {
        if (process[i].tries == -1 || process[i].tries >= options.retries)
          break;
        else {
          newId = fork();
          if (newId == 0) {
            int e = execv(process[i].path, process[i].args);
            if (e==-1)
              printf("path: %s\n", strerror(errno));
            _exit(0);
          }
          
          process[i].pid = newId;
	  running_count++;
          printf("|REStarted|: %s pid: %i try: %i\n", process[i].path, process[i].pid, process[i].tries); 
          process[i].tries++;
        } 
      }
    }
  }

  // -- After waiting for all processes (including those that were restarted), terminate

}
