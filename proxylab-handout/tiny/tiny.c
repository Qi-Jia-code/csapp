/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the 
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh 
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

#define MAXLINE  1024

static const char *User_Agent_static = "Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3)
Gecko/20120305 Firefox/10.0.3\r\n";
static const char *Connection_static = "close\r\n";
static const char *Proxy_Connection_static = "close\r\n";

void *thread(void *vargp);
void doit(int fd);
void read_requesthdrs(Rio_t *rp);
int parse_url(uri, char *hostname, int *port, char *path);
reset_req_header(Rio_t Rio_client, char *new_request,char *hostname, char *port);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

typedef struct {
    char host[MAXLINE];
    char port[MAXLINE];
    int path;
}ReqUri;


int main(int argc, char **argv) 
{
    int listenfd, *connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid;

    /* Check command line args */
    if (argc != 2) {
	fprintf(stderr, "usage: %s <port>\n", argv[0]);
	exit(1);
    }

    listenfd = Open_listenfd(argv[1]);
    while (1) {
	clientlen = sizeof(clientaddr);
    connfd = Malloc(sizeof(int));
	*connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); 
    Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    //创建一个主进程来统一的处理连接描述符
    pthread_create(&tid, NULL, thread, connfd);                                  
	Close(connfd);                                           
    }
}
/* $end tinymain */

void *thread(void *vargp) {
    int connfd = *((int *)vargp);
    Pthread_detach(pthread_self());
    Free(vargp);
    doit(connfd);
    close(connfd);
    return NULL;
}
/*
 * doit - handle one HTTP request/response transaction
 */
/* $begin doit */
void doit(int fd) 
{
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char hostname[MAXLINE], path[MAXLINE];
    int connServer;
    struct stat sbuf;
    ReqUri request_uri;
    char filename[MAXLINE], cgiargs[MAXLINE];
    Rio_t Rio_client, Rio_server;
    int port;   
    char portchar[10];
    char new_request[MAXLINE];

    /* Read request line and headers */
    Rio_readinitb(&Rio_client, fd);
    if (!Rio_readlineb(&Rio_client, buf, MAXLINE))  /
        return;
    printf("%s", buf);
    sscanf(buf, "%s %s %s", method, uri, version);       
    parse_url(uri, hostname, port, path);

    //connect to server
    sprintf(portchar, "%d", port);
    strcpy(request_uri.host, hostname);
    strcpy(request_uri.port, portchar);
    connServer = open_clientfd(request_uri.host, request_uri.port);
    if(connServer < 0) {
        printf("connection failed\n");
        return;
    }

    //初始化缓冲池
    Rio_readinitb(&Rio_server, connServer);

    //重新编写请求头
    sprintf(new_request, "GET %s HTTP/1.0\r\n", path);
    reset_req_header(&Rio_client, new_request, request_uri.host, request_uri.port);
    //作为客户端向服务端发送请求
    Rio_writen(connServer, new_request, hostname, portchar);

    int response;
    while(Rio_readlineb(Rio_server, buf, MAXLINE)) {
        response = Rio_readlineb(Rio_server, buf, MAXLINE);
        Rio_writen(fd, buf, response);
    }
    // is_static = parse_uri(uri, filename, cgiargs);       //line:netp:doit:staticcheck
    // if (stat(filename, &sbuf) < 0) {                     //line:netp:doit:beginnotfound
	// clienterror(fd, filename, "404", "Not found",
	// 	    "Tiny couldn't find this file");
	// return;
    // }                                                    //line:netp:doit:endnotfound

    // if (is_static) { /* Serve static content */          
	// if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) { //line:netp:doit:readable
	//     clienterror(fd, filename, "403", "Forbidden",
	// 		"Tiny couldn't read the file");
	//     return;
	// }
	// serve_static(fd, filename, sbuf.st_size);        //line:netp:doit:servestatic
    // }
    // else { /* Serve dynamic content */
	// if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) { //line:netp:doit:executable
	//     clienterror(fd, filename, "403", "Forbidden",
	// 		"Tiny couldn't run the CGI program");
	//     return;
	// }
	// serve_dynamic(fd, filename, cgiargs);            //line:netp:doit:servedynamic
    // }
}
/* $end doit */

/*
 * read_requesthdrs - read HTTP request headers
 */
/* $begin read_requesthdrs */
// void parse_request(int fd, char *hostname, char *path, int *path)  //读请求头
// {
    

//     // Rio_readlineb(rp, buf, MAXLINE);
//     // printf("%s", buf);
//     // while(strcmp(buf, "\r\n")) {          
// 	// Rio_readlineb(rp, buf, MAXLINE);
// 	// printf("%s", buf);
    
//     return;
// }
/* $end read_requesthdrs */

/*
 * parse_uri - parse URI into filename and CGI args
 *             return 0 if dynamic content, 1 if static
 */
/* $begin parse_uri */
int parse_url(uri, char *hostname, int *port, char *path)
{
    char *ptr_hostname;
    ptr_hostname = strstr(uri, "//");
    if(ptr_hostname) {
        ptr_hostname += 2;
    }else {
        ptr_hostname = uri;
    }

    char *ptr_port = strstr(uri, ":");
    if(ptr_port) {
        *ptr_port = '\0';
        strncpy(hostname, ptr_hostname, MAXLINE);
        sscanf(ptr_port+1, "%d%s", port, path);
    }else {
        char *ptr_path = strstr(ptr_hostname, "/");
        *port = 80;
        if(ptr_path) {
            *ptr_path = '\0';
            strncpy(hostname, ptr_hostname, MAXLINE);
            *ptr_path = '/';
            strncpy(path, ptr_path, MAXLINE);
        }else {
            strncpy(hostname, uri, MAXLINE);
            strcpy(path, "");
        }
    }

    // char *ptr;

    // if (!strstr(uri, "cgi-bin")) {  /* Static content */ //line:netp:parseuri:isstatic
	// strcpy(cgiargs, "");                             //line:netp:parseuri:clearcgi
	// strcpy(filename, ".");                           //line:netp:parseuri:beginconvert1
	// strcat(filename, uri);                           //line:netp:parseuri:endconvert1
	// if (uri[strlen(uri)-1] == '/')                   //line:netp:parseuri:slashcheck
	//     strcat(filename, "home.html");               //line:netp:parseuri:appenddefault
	// return 1;
    // }
    // else {  /* Dynamic content */                        //line:netp:parseuri:isdynamic
	// ptr = index(uri, '?');                           //line:netp:parseuri:beginextract
	// if (ptr) {
	//     strcpy(cgiargs, ptr+1);
	//     *ptr = '\0';
	// }
	// else 
	//     strcpy(cgiargs, "");                         //line:netp:parseuri:endextract
	// strcpy(filename, ".");                           //line:netp:parseuri:beginconvert2
	// strcat(filename, uri);                           //line:netp:parseuri:endconvert2
	return;
}
/* $end parse_uri */

void reset_req_header(Rio_t Rio_client, char *new_request,char *hostname, char *port)
{
    char temp[MAXLINE];
    //读取旧的request
    while(Rio_readlineb(Rio_client, temp, MAXLINE) > 0) {
        if(strstr(temp, "\r\n")) break;
        if(strstr(temp, "Host:")) continue;
        if(strstr(temp, "User-Agent:")) continue;
        if(strstr(temp, "Connection:")) continue;
        if(strstr(temp, "Proxy Connection:")) continue;
        //加到新的request中
        sprintf(new_request, "%s%s", new_request, temp);
    }
    sprintf(new_request, "%sHost: %s:%s\r\n", new_request,hostname,port);
    sprintf(new_request, "%s%s%s%s", User_Agent_static, Connection_static, Proxy_Connection);
    sprintf(new_request, "%s\r\n", new_request);
}

/*
 * serve_static - copy a file back to the client 
 */
/* $begin serve_static */
void serve_static(int fd, char *filename, int filesize)
{
    int srcfd;
    char *srcp, filetype[MAXLINE], buf[MAXBUF];

    /* Send response headers to client */
    get_filetype(filename, filetype);    //line:netp:servestatic:getfiletype
    sprintf(buf, "HTTP/1.0 200 OK\r\n"); //line:netp:servestatic:beginserve
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Server: Tiny Web Server\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n", filesize);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: %s\r\n\r\n", filetype);
    Rio_writen(fd, buf, strlen(buf));    //line:netp:servestatic:endserve

    /* Send response body to client */
    srcfd = Open(filename, O_RDONLY, 0); //line:netp:servestatic:open
    srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0); //line:netp:servestatic:mmap
    Close(srcfd);                       //line:netp:servestatic:close
    Rio_writen(fd, srcp, filesize);     //line:netp:servestatic:write
    Munmap(srcp, filesize);             //line:netp:servestatic:munmap
}

/*
 * get_filetype - derive file type from file name
 */
void get_filetype(char *filename, char *filetype) 
{
    if (strstr(filename, ".html"))
	strcpy(filetype, "text/html");
    else if (strstr(filename, ".gif"))
	strcpy(filetype, "image/gif");
    else if (strstr(filename, ".png"))
	strcpy(filetype, "image/png");
    else if (strstr(filename, ".jpg"))
	strcpy(filetype, "image/jpeg");
    else
	strcpy(filetype, "text/plain");
}  
/* $end serve_static */

/*
 * serve_dynamic - run a CGI program on behalf of the client
 */
/* $begin serve_dynamic */
void serve_dynamic(int fd, char *filename, char *cgiargs) 
{
    char buf[MAXLINE], *emptylist[] = { NULL };

    /* Return first part of HTTP response */
    sprintf(buf, "HTTP/1.0 200 OK\r\n"); 
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Server: Tiny Web Server\r\n");
    Rio_writen(fd, buf, strlen(buf));
  
    if (Fork() == 0) { /* Child */ //line:netp:servedynamic:fork
	/* Real server would set all CGI vars here */
	setenv("QUERY_STRING", cgiargs, 1); //line:netp:servedynamic:setenv
	Dup2(fd, STDOUT_FILENO);         /* Redirect stdout to client */ //line:netp:servedynamic:dup2
	Execve(filename, emptylist, environ); /* Run CGI program */ //line:netp:servedynamic:execve
    }
    Wait(NULL); /* Parent waits for and reaps child */ //line:netp:servedynamic:wait
}
/* $end serve_dynamic */

/*
 * clienterror - returns an error message to the client
 */
/* $begin clienterror */
void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg) 
{
    char buf[MAXLINE];

    /* Print the HTTP response headers */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n\r\n");
    Rio_writen(fd, buf, strlen(buf));

    /* Print the HTTP response body */
    sprintf(buf, "<html><title>Tiny Error</title>");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<body bgcolor=""ffffff"">\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "%s: %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<p>%s: %s\r\n", longmsg, cause);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<hr><em>The Tiny Web server</em>\r\n");
    Rio_writen(fd, buf, strlen(buf));
}
/* $end clienterror */
