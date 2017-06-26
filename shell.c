//Note: No subhsell implementation and lbp(list background processes) exists

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

#define LINE_BUFSIZE 512
#define TOK_BUFSIZE 64
#define TOK_DELIM " \t\r\n\a"


void HandleSignal(int sig, siginfo_t *si, void *context)
{
  
   switch(sig)
   {
      case SIGINT:
         _exit(0);
         break;
      case SIGCHLD:
	if((si->si_code == CLD_EXITED || si->si_code == CLD_KILLED) && getpgid(si->si_pid) != -1) //is a background process
         {
	   printf("[%d] retval: %d\n", si->si_pid,si->si_status);
         }
         break;
   }
}

char *shell_read_line(void)
{
  int buf_size = LINE_BUFSIZE;
  int position = 0;
  char *buffer = malloc(sizeof(char) * buf_size);
  int c;

  if (!buffer) {
    fprintf(stderr, "shell: allocation error\n");
    exit(EXIT_FAILURE);
  }

  while (1) {
    //Get a char
    c = getchar();

    // If we hit EOF, replace it with a null character and return.
    if (c == EOF || c == '\n') {
      buffer[position] = '\0';
      return buffer;
    } else {
      buffer[position] = c;
    }
    position++;

    // If the command is too long get more space for buffer
    if (position >= buf_size) {
      buf_size += LINE_BUFSIZE;
      buffer = realloc(buffer, buf_size);
      if (!buffer) {
        fprintf(stderr, "shell: allocation error\n");
        exit(EXIT_FAILURE);
      }
    }
  }
}


char **shell_split_line(char *line) // same strategy like we did on reading line, expand the buffer dynamically
{
  int buf_size = TOK_BUFSIZE, position = 0;
  char **tokens = malloc(buf_size * sizeof(char*));
  char *token;

  if (!tokens) {
    fprintf(stderr, "shell: allocation error\n");
    exit(EXIT_FAILURE);
  }

  token = strtok(line, TOK_DELIM);
  while (token != NULL) {
    tokens[position] = token;
    position++;

    if (position >= buf_size) {
      buf_size += TOK_BUFSIZE;
      tokens = realloc(tokens, buf_size * sizeof(char*));
      if (!tokens) {
        fprintf(stderr, "shell: allocation error\n");
        exit(EXIT_FAILURE);
      }
    }

    token = strtok(NULL, TOK_DELIM);
  }
  tokens[position] = NULL;
  return tokens;
}


int shell_run(char **args)
{
  pid_t pid, wpid;
  int status,i,j,in=0,out=0,background=0;
  char input[128], output[128];
  struct sigaction sVal;

   // a signal handler that takes three arguments
   // instead of one, which is the default.
   sVal.sa_flags = SA_SIGINFO;

   // Indicate which function is the signal handler.
   sVal.sa_sigaction = HandleSignal;

  //assigns input and output redirection flags, also for background process(/*TO-DO*/)
  for(i=0; args[i] != '\0'; i++)
    {
      
        if(strcmp(args[i],"<") == 0)
        {        
            args[i] = NULL;
            strcpy(input, args[i+1]);
            in = 7;           
        }               
      
        else if(strcmp(args[i],">") == 0)
        {      
            args[i] = NULL;
            strcpy(output, args[i+1]);
            out = 7;
        }         
	else if(strcmp(args[i], "&") == 0)
	{
	  args[i] = NULL;
	  background = 7;
        }
    }

  fflush(0);
  pid = fork();
  if (pid == 0) {
    // We are in child process
       
     
    //input redirection, checks if we have '<', stdin from input file 
    if (in)
    {
        int fd0;
	if ((fd0 = open(input, O_RDONLY)) < 0) {
	  fprintf(stderr, "%s not found\n",input);
	  //perror("inputfile not found");  //some extra printing comes with this(shell err)
            exit(0);
	    }  
        dup2(fd0, 0);
        close(fd0);
	in = 0;
    }
    
    //output redirection, checks if we have '>', stdout to output file 
    
    if (out)
    {
        int fd1;
	fd1 = creat(output , 0644);
	dup2(fd1, 1);
	close(fd1);
        out = 0;
    }  
     
    if (background){
      setpgid(getpid(), getpgid(getppid()));
      if (execvp(args[0], args) == -1) {       
      perror("shell");
    }
    }
    
    
    //time to execute with error check
    if (execvp(args[0], args) == -1) {
      perror("shell");
    }
    exit(EXIT_FAILURE);
  } else if (pid < 0) {
    // Error forking
    perror("shell");
  } else {
    // Parent process
    if(!background){
    do {
      wpid = waitpid(pid, &status, WUNTRACED);
    } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }
    else{
      // Register for SIGINT
      sigaction(SIGINT, &sVal, NULL);
      // Register for SIGCHLD 
      sigaction(SIGCHLD, &sVal, NULL);
      
    }
   
  }

  return 1;
}

int creat_proc (int in, int out, char **args)
{
  pid_t pid;

  if ((pid = fork ()) == 0)
    {
      if (in != 0)
        {
          dup2 (in, 0);
          close (in);
        }

      if (out != 1)
        {
          dup2 (out, 1);
          close (out);
        }

      if (execvp(args[0], args) == -1) {       
      perror("shell");
      }
    }

  return pid;
}

int shell_run_pipe(char **args, int pipes){
  
  // The number of commands to run , #pipe symbols + 1
    const int commands = pipes + 1;
    int i = 0;

    int pipefds[2*pipes];

    for(i = 0; i < pipes; i++){
        if(pipe(pipefds + i*2) < 0) {
            perror("Couldn't Pipe");
            exit(EXIT_FAILURE);
        }
    }

    int pid;
    int status;

    int j = 0;
    int k = 0;
    int s = 1;
    int place;
    int commandStarts[10];
    commandStarts[0] = 0;

    // Set all the pipes to NULL
    // Create an array of where the next command starts

    while (args[k] != NULL){
        if(!strcmp(args[k], "|")){
            args[k] = NULL;
            commandStarts[s] = k+1;
            s++;
        }
        k++;
    }



    for (i = 0; i < commands; ++i) {
        // place is where in args the program should
        // start running when it gets to the execution
        // command
        place = commandStarts[i];

        pid = fork();
        if(pid == 0) {
            //if not last command
            if(i < pipes){
                if(dup2(pipefds[j + 1], 1) < 0){
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
            }

            //if not first command&& j!= 2*pipes
            if(j != 0 ){
                if(dup2(pipefds[j-2], 0) < 0){
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
            }

            int q;
            for(q = 0; q < 2*pipes; q++){
                    close(pipefds[q]);
            }

            // The commands are executed here, 
            // but it must be doing it a bit wrong          
            if( execvp(args[place], args+place) < 0 ){
                    perror(*args);
                    exit(EXIT_FAILURE);
            }
        }
        else if(pid < 0){
            perror("error");
            exit(EXIT_FAILURE);
        }

        j+=2;
    }

    for(i = 0; i < 2 * pipes; i++){
        close(pipefds[i]);
    }

    for(i = 0; i < pipes + 1; i++){
        wait(&status);
    }
}


int shell_run_seq(char **args){
  int i=0,j,prev=-1,num=1;
  pid_t pid;
  int in, fd [2];

  //parse the parsed arguments for piping
  char *cmds[20][20]; 
  i=0;
  num=0;
  while (args[i] != NULL){
    if (strcmp(args[i], ";") == 0){
      args[i] = NULL;
      for(j=0; j < i-prev;j++ ){
	cmds[num][j] = args[prev+j+1];
      }
      prev = i;
      num++;
    }
    i++;
  }
   i = prev +1;
   while(args[i] != NULL){
     for(j=0; j < i-prev;j++ ){
	cmds[num][j] = args[prev+j+1];
      }
     i++;
   }
   num++;

   for(i=0; i < num; i++){
     shell_run(cmds[i]);
   }
  return 1;
}


int shell_quit(char **args){
  int status;
  pid_t pid,wpid;
  // wait for background processes to complete then exit
   do {
     waitpid(-1, &status, 0);
      } while (!WIFEXITED(status) && !WIFSIGNALED(status));

  return 0;
}

//current directory is part of parent process, would be wise to implement 'cd' in shell's itself like quit function
int shell_cd(char **args)
{
  // if there is no argument
  if (args[1] == NULL) {
    fprintf(stderr, "shell: expected argument to \"cd\"\n");
  } 
  //else use chdir to change directory
  else {
    if (chdir(args[1]) != 0) {
      perror("shell");
    }
  }
  return 1;
}

char *shell_commands[] = {
  "cd",
  "quit"
};

int (*shell_func[]) (char **) = {
  &shell_cd,
  &shell_quit
};

int shell_numof_commands() {
  return sizeof(shell_commands) / sizeof(char *);
}


int shell_execute(char **args)
{
  int i,pipes=0,seq=0;

  //if command is empty
  if (args[0] == NULL) {
    return 1;
  }

  //if command is one of my commands I implemented 
  for (i = 0; i < shell_numof_commands(); i++) {
    if (strcmp(args[0], shell_commands[i]) == 0) {
      return (*shell_func[i])(args);
    }
  }
  
  // there exists some pipes
  i=0;
  while(args[i] != '\0'){
    if (strcmp(args[i], "|") == 0){
      pipes++;
    }
    else if (strcmp(args[i], ";") == 0){
      seq = 1;
    }
    i++;
  }
  
  if (pipes){
    return shell_run_pipe(args,pipes);
  }
  
  if (seq){
    return shell_run_seq(args);
}


  return shell_run(args);
}


void shell(void)
{
  char *line;
  char **args;
  int status;
  // pid_t childpids[64];
  //int nchildren = 0;

  do {
    printf("> ");
    fflush(stdout);
    //read a line, parse it then execute it
    line = shell_read_line();
    args = shell_split_line(line);
    status = shell_execute(args);
    //deallocate the line and arguments
    free(line);
    free(args);
  } while (status);
}


int main(int argc, char **argv){
  //any necessary initial configuration



  //shell loop
  shell();
  
  //Terminates
  return EXIT_SUCCESS;
}


