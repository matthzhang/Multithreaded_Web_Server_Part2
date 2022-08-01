#ifndef _REENTRANT
#define _REENTRANT
#endif

#include <stdio.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <ctype.h>

#include "util.h"

//Global variables
static int master_fd = -1; // Global var used to store socket
pthread_mutex_t lock;


/**********************************************
 * makeargv
 * @param s is the buffer that needs to be parsed.
 * @param delimeters is used to split the buffer. Will be used by strtok_r 
 * @param argvp is adress to string array (so charr*** type) -> Try to reason why this needs to be char** type
 * 
 * Returns number of tokens on sucess and sets argvp -1 otherwise
 * Usage example: 
 *    char* buf = "This is the string I want to parse \n";
 *    char **result;
 *    makeargv(buf," \n",&result)
 *    result contains => ["This","is","the","string","I","want","to","parse"]
************************************************/
int makeargv(const char *s, const char *delimiters, char ***argvp) {
   int error;
   int i;
   int numtokens;
   const char *snew;
   char *t;
   char* context;

   if ((s == NULL) || (delimiters == NULL) || (argvp == NULL)) {
      errno = EINVAL;
      return -1;
   }

   *argvp = NULL;
   snew = s + strspn(s, delimiters);

   if ((t = malloc(strlen(snew) + 1)) == NULL)
      return -1;
   strcpy(t,snew);
   
   numtokens = 0;
   if (strtok_r(t, delimiters,&context) != NULL)
      for (numtokens = 1; strtok_r(NULL, delimiters,&context) != NULL; numtokens++) ;
   
   
   if ((*argvp = malloc((numtokens + 1)*sizeof(char *))) == NULL) {
      error = errno;
      free(t);
      errno = error;
      return -1;
   }

   if (numtokens == 0)
      free(t);
   else {
      strcpy(t,snew);
      **argvp = strtok_r(t,delimiters,&context);
      for (i=1; i<numtokens; i++){
         *((*argvp) +i) = strtok_r(NULL,delimiters,&context);
      }
         
   }
   *((*argvp) + numtokens) = NULL;

   return numtokens;
}

void freemakeargv(char **argv) {
   if (argv == NULL)
      return;
   if (*argv != NULL)
      free(*argv);
   free(argv);
}

/**********************************************
 * init
   - port is the number of the port you want the server to be
     started on
   - initializes the connection acception/handling system
   - YOU MUST CALL THIS EXACTLY ONCE (not once per thread,
     but exactly one time, in the main thread of your program)
     BEFORE USING ANY OF THE FUNCTIONS BELOW
   - if init encounters any errors, it will call exit().
************************************************/
void init(int port) {
   // set up sockets using bind, listen
   // also do setsockopt(SO_REUSEADDR)
   // exit if the port number is already in use

   
   //Create socket, store return value in master_fd
   int true = 1;
   master_fd = -1;
   //Set masster_fd
   master_fd = socket(AF_INET, SOCK_STREAM, 0);
   
   //Use setsockopt with SO_REUSEADDR
   if(setsockopt(master_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&true, sizeof(true)) == -1) {
      printf("Error setsockopt()\n");
      while((close(master_fd) == -1) && (errno == EINTR));
      exit(-1);
   }
   //setsockopt and error checks
   
   //Bind socket
   struct sockaddr_in server;
   server.sin_family = AF_INET;
   server.sin_addr.s_addr = htonl(INADDR_ANY);
   server.sin_port = htons((short)port);
   if(bind(master_fd, (struct sockaddr *)&server, sizeof(server)) != 0) {
      printf("Error bind()\n");
      exit(-1);
   }
   //Binds socket and error checks

   //Listen
  if(listen(master_fd, 20) != 0) {
     printf("Error listen()\n");
     exit(-1);
  }
  //listens and error checks
   
   //If successful should print
   printf("UTILS.O: Server Started on Port %d\n",port);

}

/**********************************************
 * accept_connection - takes no parameters
   - returns a file descriptor for further request processing.
     DO NOT use the file descriptor of your own -- use
     get_request() instead.
   - if the return value is negative, the thread calling
     accept_connection must should ignore request.
***********************************************/
int accept_connection(void) {
   pthread_mutex_lock(&lock);
   //lock

   int toReturn;
   struct sockaddr_in client;
   socklen_t client_size = sizeof(struct sockaddr_in);
   /* 
      Should we use locks?    
   */
  
   while((toReturn = accept(master_fd, (struct sockaddr*) &client, &client_size)) < 0){
      continue;
   }
   //accept connection and error check

   pthread_mutex_unlock(&lock);
   //unlocks
   return toReturn;
}

/**********************************************
 * get_request
   - parameters:
      - fd is the file descriptor obtained by accept_connection()
        from where you wish to get a request
      - filename is the location of a character buffer in which
        this function should store the requested filename. (Buffer
        should be of size 1024 bytes.)
   - returns 0 on success, nonzero on failure. You must account
     for failures because some connections might send faulty
     requests. This is a recoverable error - you must not exit
     inside the thread that called get_request. After an error, you
     must NOT use a return_request or return_error function for that
     specific 'connection'.
************************************************/
int get_request(int fd, char *filename) {
   /*
      Read from the socketfd and parse the first line for the GET info
      Error checks: 
         - if it isn't a GET request
         - if the file starts with .. or // -> Security breach (Accessing file outside server roor dir)
         - if the file path length is greater than 1024 (Possible buffer overflow)
         In all the cases just dump it and return -1.
      
      otherwise, store the path/filename into 'filename'.
   */
   //Read first line from fd (The rest does not matter to us)
   char *buffer;
   buffer = (char *)malloc(MAX_HTTP_REQUEST_SIZE);
   //allocate memeory to buffer
   //Look at fgets
   if(read(fd, buffer, MAX_HTTP_REQUEST_SIZE) < 1) {
      printf("empty request\n");
      free(buffer);
      return -1;
   }
   //read into buffer and error check

   //Pass the first line read into makeargv
   //Look at makeargv comments for usage
   char **tokens = NULL;
   int numtokens = makeargv(buffer, " ", &tokens);
   if(numtokens < 2) {
      if(numtokens < 0) {
         printf("Error: Unable to Tokenize Argument\n");
         freemakeargv(tokens);
         free(buffer);
         return -1;
      }
      printf("Error: Too Few Arguments\n");
      freemakeargv(tokens);
      free(buffer);
      return -1;
   }
   //tokenize arguments and error check
   free(buffer);

   //Error checks
   // - Check for valid request type
   if((strcmp(tokens[0], "GET") != 0) & (strcmp(tokens[0], "wget") != 0)) {
      printf("Error: Invalid Request\n");
      freemakeargv(tokens);
      return -1;
   // - Check for minimum path length for further error checking
   } else if(strlen(tokens[1]) > 1) {
   // - Check for maximum path length/overflow
      if(strlen(tokens[1]) > 1024) {
         printf("Error: Path Overflow\n");
         freemakeargv(tokens);
         return -1;
      }
   // - Check for out of bounds
      char temp[2];
      temp[0] = tokens[1][0];
      temp[1] = tokens[1][1];
      if((strcmp(temp, "..") == 0) | (strcmp(temp, "//") == 0)) {
         printf("Error: Path Out of Bounds\n");
         freemakeargv(tokens);
         return -1;
      }
   }

   //Passed error checks copy the path into filename
   strcpy(filename, tokens[1]);
   
   //Call freemakeargv to free memory
   freemakeargv(tokens);

   //Return success
   return 0;
   // //Remove return -1 once you are done implementing
   // return -1;

}

/**********************************************
 * return_result
   - returns the contents of a file to the requesting client
   - parameters:
      - fd is the file descriptor obtained by accept_connection()
        to where you wish to return the result of a request
      - content_type is a pointer to a string that indicates the
        type of content being returned. possible types include
        "text/html", "text/plain", "image/gif", "image/jpeg" cor-
        responding to .html, .txt, .gif, .jpg files.
      - buf is a pointer to a memory location where the requested
        file has been read into memory (the heap). return_result
        will use this memory location to return the result to the
        user. (remember to use -D_REENTRANT for CFLAGS.) you may
        safely deallocate the memory after the call to
        return_result (if it will not be cached).
      - numbytes is the number of bytes the file takes up in buf
   - returns 0 on success, nonzero on failure.
************************************************/
int return_result(int fd, char *content_type, char *buf, int numbytes) {
   /*
      send headers back to the socketfd, connection: close, content-type, content-length, etc
      then finally send back the resulting file
      then close the connection.

      It is IMPORTANT that you follow the format as described in the writeup while writing to the socket.

   */
   //Send headers
   char* message;
   message = (char *)malloc(128 + numbytes);
   sprintf(message, "HTTP/1.1 200 OK\nContent-Type: %s\nContent-Length: %d\nConnection: Close\n\n", content_type, numbytes);
   strcat(message, "\0");
   if(write(fd, message, strlen(message) + 1) < 0){
      printf("Error sending message\n");
      free(message);
      return -1;
   }
   //send headers and error check
   free(message);

   //Send Result file and error check
   if(write(fd, buf, numbytes) < 0){
      printf("Error sending file\n");
      return -1;
   }

   //Close connection and error check
   if(close(fd) < 0){
      printf("Error closing connection.\n");
      return -1;
   }

   //Return success

   //Remove return -1 once you are done implementing
   return 0;
}


/**********************************************
 * return_error
   - returns an error message in response to a bad request
   - parameters:
      - fd is the file descriptor obtained by accept_connection()
        to where you wish to return the error
      - buf is a pointer to the location of the error text
   - returns 0 on success, nonzero on failure.
************************************************/
int return_error(int fd, char *buf) {
   /*
      send 404 headers back to the socketfd, connection: close, content-type: text/plain, content-length
      send back the error message as a piece of text.
      then close the connection

      It is IMPORTANT that you follow the format as described in the writeup while writing to the socket.
   */
   
   //Send headers and error check
   char *message;
   message = (char *)malloc(200);
   sprintf(message, "HTTP/1.1 404 Not Found\nContent-Type: text/plain\nContent-Length: %d\nConnection: Close\n\nRequested file not found.", (int)strlen(buf));
   if(write(fd, message, strlen(message)) < 1) {
      printf("Error sending error\n");
      free(message);
      return -1;
   }
   free(message);

   //Send Result file and error check
   if(write(fd, buf, strlen(buf)) < 0) {
      printf("Error sending data\n");
      return -1;
   }

   //Close connection and error check
   if(close(fd) < 0) {
      printf("Error closing connection.\n");
      return -1;
   }

   //Return success

   //Remove return -1 once you are done implementing
   return 0;
}
