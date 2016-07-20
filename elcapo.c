#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include "elcapo.h"

static char *program_name;

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

// -- Read configuration file
  config = fopen(options.config_file, "r");
  if (config == NULL) {
    printf("Error opening file\n");
    exit(2);
  }

  process_count = 0;
  arg_count = 0;

  while (!( feof(config) )) {
    fgets(line, 260, config);

    if (strlen(line) > 0) {
      token = strtok(line, " ");
      if (*token == 'R')
        process[process_count].tries = 0;
      else
        process[process_count].tries = -1;

      token = strtok(NULL, " ");
      strncpy(process[process_count].path, token, MAX_PATH_LENGTH-1);

      process[process_count].args[arg_count] = malloc(strlen(token)+1);
      strcpy(process[process_count].args[arg_count], token);
      arg_count++;

      while ((token = strtok(NULL, " ")) != NULL) {
        if (token[0] == '\n')
          printf("dfd\n");
        process[process_count].args[arg_count] = malloc(strlen(token)+1);
        strcpy(process[process_count].args[arg_count], token);
        arg_count++;
        if (token[strlen(token)-1] == '\n')
          printf("d: %s %lu\n", token, strlen(token));
      }

      process[process_count].args[arg_count] = NULL;
      arg_count = 0; 
      process_count++;     
    }
    line[0] = '\0';    
  }

  

  

  // -- If daemonized, create session, process group, and go to background
  //   -- Then close file descriptors and redirect stdout / stderr to /dev/null or log file

// -- Start child processes
  int i;
  pid_t id;
  
  for (i=0; i<process_count; i++) {
    id = fork();
    if (id == 0) {
      int e = execv(process[process_count].path, process[process_count].args);
      //printf("child %s %s %s %i\n", process[i].args[0], process[i].args[1], process[i].args[2],  e);
      _exit(0);
    }
    else 
      process[process_count].pid = id;  
  }  

  // -- Set up signal handlers for SIGHUP to show the number of running processes

	// -- Wait for processes to terminate
  //   -- When a process terminates, check if it needs to be restarted.
  //   -- If it does, check if the exit status is != 0 and if it already used max retries
  int status;
  while ( (id = wait(&status)) != -1) {
    //id = wait(&status);
    //printf("%i %i\n", id, WIFEXITED(status));
    
    if (id == -1)
      break;
  }

  // -- After waiting for all processes (including those that were restarted), terminate

}
