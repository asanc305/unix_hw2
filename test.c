#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {

  char *array[6];
  array[0] = "./sleep";
  array[1] = "20";
  array[2] = "10";
  array[3] = NULL;
  int err = execv("./sleep", array);
  printf("%i\n", err);
}

