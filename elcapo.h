#define MAX_PATH_LENGTH 256
#define DEFAULT_RETRIES 3
#define MAX_PROCESSES 100
#define MAX_ARGS 10


struct program_options {
  char config_file[MAX_PATH_LENGTH];
  char log_file[MAX_PATH_LENGTH];
  int retries;
  bool daemonize;
};

struct process_info {
  char path[MAX_PATH_LENGTH];
  char *args[MAX_ARGS];
  pid_t pid;
  int tries;
};

void print_usage_and_exit(int status);
void parse_program_options(int argc, char **argv, struct program_options *options);
