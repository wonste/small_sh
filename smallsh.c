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
char *expand(char const *word);

// GLOBALS
char *smallsh_pid = NULL;   // $$
char *fg_exit = "0";        // $?
char *bg_pid = "";          // $!
struct sigaction old_sigint = {0}, old_sigtstp = {0}, ignore_signals = {0}, manage_sigint = {0};

void handle_sigint(int sig){

}


int main(int argc, char *argv[])
{
  ignore_signals.sa_handler = SIG_IGN;
  manage_sigint.sa_handler = handle_sigint;
  sigaction(SIGINT, &manage_sigint, &old_sigint);
  sigaction(SIGTSTP, &ignore_signals, &old_sigtstp);
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
    char *array_to_feed[MAX_WORDS];
    int background = 0;
    pid_t spawnpid = -5;
    int bgChildStatus;
    int bg_child = 0;
    int mathResult = 0;
    int signum = 0;
    int j;
    
    for (;;){
mainloop:
      sigaction(SIGINT, &manage_sigint, NULL);
      // background processes
      while((bg_child = waitpid(0, &bgChildStatus, WNOHANG | WUNTRACED)) > 0){

        if(WIFEXITED(bgChildStatus)){
          fprintf(stderr, "Child process %jd done. Exit status %d.\n", (intmax_t) bg_child, WEXITSTATUS(bgChildStatus));
        } else if (WIFSIGNALED(bgChildStatus)){
          signum = WTERMSIG(bgChildStatus);
          fprintf(stderr, "Child process %jd done. Signaled %d.\n", (intmax_t)bg_child, signum);
        } else if (WIFSTOPPED(bgChildStatus)){
          kill(bg_child, SIGCONT);
          fprintf(stderr, "Child process %jd stopped. Continuing.\n", (intmax_t)bg_child);
        }
      }

      for(int i=0; i < MAX_WORDS; i++){
        // reset array for next loop/iteration
        if(wordarr[i]) free(wordarr[i]);
        wordarr[i] = NULL;
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
      sigaction(SIGINT, &ignore_signals, NULL);
      // handle EOF here????
      if (feof(input)){
        goto exitjump;
      }
      if(line_length == 1 && line[0] == '\n'){
        goto mainloop;
      }

      if (line_length == -1){
        if(errno == EINTR){
          // error interrupt
          clearerr(stdin);
          fprintf(stderr, "\n");
          errno = 0;
          goto mainloop;
        }
      }

      //if (line_length < 0){
      //  err(1, "%s", file_name);
      //}

      // word split
      size_t num_words = wordsplit(line);
      for(size_t i = 0; i < num_words; i++){
        //fprintf(stderr, "Word %zu: %s --> ", i, wordarr[i]);
        char *expand_word = expand(wordarr[i]);
        free(wordarr[i]);
        wordarr[i] = expand_word;
        //fprintf(stderr, "%s\n", wordarr[i]);
        }
      

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
        if(strcmp(wordarr[num_words - 1], "&")==0){
          background = 1;
          wordarr[num_words - 1] = NULL;
          num_words--;
        }
        spawnpid = fork();
        switch(spawnpid){

          case -1:
            perror("fork()failed");
            exit(1);
            break;

          case 0:
            // reset signals
            sigaction(SIGINT, &old_sigint, NULL);
            sigaction(SIGTSTP, &old_sigtstp, NULL);

            // parse for files to read, write, and append
            // this is because you want to truncate repeat redirection operators
            j = 0; // index for array_to_feed
            for(int i = 0 ; i < num_words; i++){
              
              // < "infile"
              if (strcmp(wordarr[i], "<") == 0){

                if(wordarr[i+1] != NULL){
                  int letmein = open(wordarr[i+1], O_RDONLY);

                  if (letmein == -1){
                    fprintf(stderr, "infile open()\n");
                    exit(1);
                  }

                  int resultIn = dup2(letmein, 0);
                  if (resultIn == -1){
                    fprintf(stderr, "infile dup2\n");
                    exit(2);
                  }
                } 
                i++;
              } // > "outfile" 
              else if(strcmp(wordarr[i], ">") == 0){

                if(wordarr[i+1] != NULL){
                  int letmeout = open(wordarr[i+1], O_WRONLY | O_CREAT | O_TRUNC, 0777);

                  if(letmeout == -1){
                    fprintf(stderr, "outfile open()\n");
                    exit(1);
                  }

                  int resultOut = dup2(letmeout, 1);
                  if(resultOut == -1){
                    fprintf(stderr, "outfile dup2\n");
                    exit(2);
                  }
                } 
                i++;
              } // >> "appendTHIS"
              else if(strcmp(wordarr[i], ">>") == 0){

                if(wordarr[i+1] != NULL){
                  int appendTHIS = open(wordarr[i+1], O_WRONLY| O_APPEND | O_CREAT, 0777);

                  if(appendTHIS == -1){
                    fprintf(stderr, "append file open()\n");
                    exit(1);
                  }

                  int appendVal = dup2(appendTHIS, 1);
                  if(appendVal == -1){
                    fprintf(stderr, "append dup2\n");
                      exit(2);
                  }
                }
                i++;
              } else{
                 // add to the clean array

                 array_to_feed[j] = strdup(wordarr[i]);
                 j++;
                 array_to_feed[j] = NULL;
               }
            
            }

            execvp(array_to_feed[0], array_to_feed);
            fprintf(stderr, "execvp failed\n");
            exit(EXIT_FAILURE);
            break;

          default:
            if(background == 0){
              waitpid(spawnpid, &bgChildStatus, WUNTRACED);

              if(WIFSTOPPED(bgChildStatus)){

                bg_pid = malloc(8);
                sprintf(bg_pid, "%d", spawnpid);
                kill(spawnpid, SIGCONT);
                waitpid(spawnpid, &bgChildStatus, WNOHANG | WUNTRACED);
                fprintf(stderr, "Child process %jd stopped. Continuing.\n", (intmax_t)spawnpid);

              } if(WIFSIGNALED(bgChildStatus)){

                mathResult = 128 + WTERMSIG(bgChildStatus);
                fg_exit = malloc(8);
                sprintf(fg_exit, "%d", mathResult);

              } if(WIFEXITED(bgChildStatus)){

                fg_exit = malloc(8);
                sprintf(fg_exit, "%d", WEXITSTATUS(bgChildStatus));

              }
              break;

            }
            if(background == 1){
              // let it goooo
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

      if(*word == '\\'){
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

char
param_scan(char const *word, char const **start, char const **end)
{
  static char const *prev;
  if (!word) word = prev;
  
  char ret = 0;
  *start = 0;
  *end = 0;
  for (char const *s = word; *s && !ret; ++s) {
    s = strchr(s, '$');
    if (!s) break;
    switch (s[1]) {
    case '$':
    case '!':
    case '?':
      ret = s[1];
      *start = s;
      *end = s + 2;
      break;
    case '{':;
      char *e = strchr(s + 2, '}');
      if (e) {
        ret = s[1];
        *start = s;
        *end = e + 1;
      }
      break;
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
  char const *start, *end;
  char ch = param_scan(position, &start, &end);
  build_str(NULL, NULL);
  build_str(position, start);

  // loop
  while(ch){
    if(ch == '!'){
      build_str(bg_pid, NULL);
    } else if (ch == '$'){
      smallsh_pid = malloc(8);
      sprintf(smallsh_pid, "%d", getpid());
      build_str(smallsh_pid, NULL);
    } else if (ch == '?'){
      build_str(fg_exit, NULL);
    } else if (ch == '{'){
      char param_array[200] = {'\0'};
      strncpy(param_array, start + 2, (end - 1) - (start + 2));
      char* env_var = getenv(param_array);
      if (env_var == NULL) build_str("", NULL);
      else build_str(env_var, NULL);
    }
    position = end;
    ch = param_scan(position, &start, &end);
    build_str(position, start);
  }
  return build_str(start, NULL);
}

