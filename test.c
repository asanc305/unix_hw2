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
  array[3] = '\0';
  printf("%s\n", array[3]);
  int err = execv("./sleep", array);
  
}

