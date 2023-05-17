#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <err.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <stddef.h>
#include <ctype.h>
#include <sys/wait.h>
#include <fcntl.h>


#ifndef MAX_WORDS
#define MAX_WORDS 512
#endif
// functions
char *wordarr[MAX_WORDS];
size_t wordsplit(char const *line);
char *expand(char const *phrase);

// GLOBALS
char *smallsh_pid = NULL;
char *fg_exit = "0";
char *bg_pid = "";



int main(int argc, char *argv[])
{

    // file for non-interactive mode
    FILE *input = stdin;
    char *file_name = "(stdin)";

    // checking if there's a file
    // 0 is command?, 1 is file/script title
    if (argc == 2){
        file_name = argv[1];

        // "re" opens files with the CLOEXEC flag
        input = fopen(file_name, "re");

        if (!input){
            errx(1, "%s", file_name);
        }
    } else if (argc > 2){
        errx(1, "too many arguments");
    }

    char *line = NULL;
    size_t n = 0;
    int background = 0;
    pid_t spawnpid = -5;
    int bgChildStatus;
    int bg_child = 0;
    //int mathResult = 0;
    //int signum = 0;

    for (;;){
mainloop:

      for(int z=0; z < MAX_WORDS; z++){
        // reset array for next loop/iteration
        if(wordarr[z]) free(wordarr[z]);
        wordarr[z] = NULL;
      }

      if (input == stdin){
        /*Check for Prompt String 1 aka PS1*/
        char *first_check;
        first_check = getenv("PS1");
        if (first_check == NULL){
          fprintf(stderr, "");
        } else{
          fprintf(stderr, "%s", first_check);
        }
      }


      /*Below reallocates the line*/
      ssize_t line_length = getline(&line, &n, input);
      // handle EOF here????
      if (feof(input)){
        goto exitjump;
      }
      if(line_length == 1 && line[0] == '\n'){
        goto mainloop;
      }

      if (line_length == -1){
        printf("\n");
        clearerr(input);
        errno = 0;
        continue;
      }

      if (line_length < 0){
        err(1, "%s", file_name);
      }

      // word split
      size_t num_words = wordsplit(line);
      for(size_t i = 0; i < num_words; i++){
        //fprintf(stderr, "Word %zu: %s --> ", i, wordarr[i]);
        char *expand_word = expand(wordarr[i]);
        free(wordarr[i]);
        wordarr[i] = expand_word;
        //fprintf(stderr, "%s\n", wordarr[i]);
        }
      // parsing?


      // built in commands
      if (wordarr[0] == NULL){
        // assuming nothing is entered so generate a new prompt
        free(line);
        line = NULL;
        errno = 0;
        goto mainloop;
      } // EXIT COMMAND
      else if ((strcmp(wordarr[0], "exit")) == 0){
        if(wordarr[2]){
          fprintf(stderr, "too many arguments\n");
          errno = 0;
          goto mainloop;
        } // exit command and assume expansion of $?
          if(wordarr[1]){
            for(int i=0; i < strlen(wordarr[1]); i++){
              // check every character is a number
              if(isdigit(wordarr[1][i]) == 0){
                fprintf(stderr, "invalid argument\n");
                errno = 0;
                goto mainloop;
              }
              // fprintf(stderr, "\nexit\n");
              exit(atoi(wordarr[1]));
            }
          } else {
exitjump:
            //fprintf(stderr, "\nexit\n");
            exit(atoi(fg_exit));
          }
      } // CD COMMAND
      else if((strcmp(wordarr[0], "cd") == 0)){
        // too many arguments
        if(wordarr[2]){
          fprintf(stderr, "too many arguments\n");
          errno = 0;
          goto mainloop;
        } if(wordarr[1] != NULL){
          if(chdir(wordarr[1]) == -1){
            printf("%s", strerror(errno));
            // reset errno
            errno=0;
            goto mainloop;
          } else{
            chdir(wordarr[1]);
          }
        } else{
          chdir(getenv("HOME"));
        }
      } // FORKING? CHILD?
      else{
        // non-built in?
        spawnpid = fork();
        switch(spawnpid){

          case -1:
            perror("fork()failed");
            exit(1);
            break;

          case 0:
            execvp(wordarr[0], wordarr);
            fprintf(stderr, "execvp failed\n");
            exit(EXIT_FAILURE);
            break;

          default:
            if(background == 1){
              bg_pid = malloc(8);
              sprintf(bg_pid, "%d", spawnpid);
              background = 0;
              goto mainloop;
            }
        }
      }
    }
}
char *wordarr[MAX_WORDS] = {0};

size_t wordsplit(char const *line){
  // create value holders
  size_t wordarr_len = 0;
  size_t wordarr_index = 0;

  char const *word = line;      // create the temp pointer 

  for(;*word && isspace(*word); ++word); // discard leading space
  
  for(;*word;){
    if(wordarr_index == MAX_WORDS) break;
    // read a word
    if(*word == '#') break;

    for(;*word && !isspace(*word); ++word){

      if(wordarr_index == '\\'){
        ++word;
      }
      // read a word
      void *tmp = realloc(wordarr[wordarr_index], sizeof **wordarr * (wordarr_len + 2));

      if(!tmp){
        err(1, "realloc");
      }
      // increment/update
      wordarr[wordarr_index] = tmp;
      wordarr[wordarr_index][wordarr_len++] = *word;
      wordarr[wordarr_index][wordarr_len] = '\0';
    }
    ++wordarr_index;
    wordarr_len = 0;
    for(;*word && isspace(*word); word++);
  }

  return wordarr_index;
}

char param_scan(char const *phrase, char **start, char **end)
{
  static char *prev;

  if(!phrase) phrase = prev;

  char ret = 0;
  *start = NULL;
  *end = NULL;

  char *stringThis = strchr(phrase, '$');

  if(stringThis){
    char *ch = strchr("$!?", stringThis[1]);
    if(ch){
      ret = *ch;
      *start = stringThis;
      *end = stringThis + 2;
    }
    else if(stringThis[1] == '{'){
      char *charEnd = strchr(stringThis + 2, '}');
      if(charEnd){
        ret = '{';
        *start = stringThis;
        *end = charEnd + 1;
      }
    }
  }
  prev = *end;
  return ret;
}

char *build_str(char const *start, char const *end){
  static size_t base_len = 0;
  static char *base = 0;

  if(!start){
    // reset base string and return old one
    char *ret = base;
    base = NULL;
    base_len = 0;
    return ret;
  }

  size_t n = end ? end - start : strlen(start);
  size_t newsize = sizeof *base *(base_len + n + 1);
  
  void *tmp = realloc(base, newsize);
  if(!tmp)err(1, "realloc");
  base = tmp;
  memcpy(base + base_len, start, n);
  base_len += n;
  base[base_len] = '\0';
  return base;

}

char *expand(char const *word){
  char const *position = word;
  char *start, *end;
  char ch = param_scan(position, &start, &end);
  build_str(NULL, NULL);
  build_str(position, start);

  // loop
  while(ch){
    if(ch == '!'){
      build_str(bg_pid, NULL);
    } else if (ch == '$'){
      build_str(smallsh_pid, NULL);
    } else if (ch == '?'){
      build_str(fg_exit, NULL);
    }
    position = end;
    ch = param_scan(position, &start, &end);
    build_str(position, start);
  }
  return build_str(start, NULL);
}

