/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"
#include <stdbool.h>

void doit(int fd);
void read_requesthdrs(rio_t *rp);
bool parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);

int main(int argc, char **argv)
{
    int listenfd, connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    /* Check command line args */
    if (argc != 2)
    {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    listenfd = Open_listenfd(argv[1]);
    while (1)
    {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr,
                        &clientlen); // line:netp:tiny:accept
        Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,
                    0);
        printf("Accepted connection from (%s, %s)\n", hostname, port);
        doit(connfd);  // line:netp:tiny:doit
        Close(connfd); // line:netp:tiny:close
    }
}
/* $end tinymain */

/*
 * doit - handle one HTTP request/response transaction
 */
/* $begin doit */
void doit(int fd)
{
    bool is_static;
    struct stat sbuf;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE], cgiargs[MAXLINE];
    rio_t rio;

    //  STEP 1 : request 읽기
    Rio_readinitb(&rio, fd); //  rio와 소켓 연결

    if (!Rio_readlineb(&rio, buf, MAXLINE)) //  한줄 읽어오기
        return;
    printf("%s", buf);
    sscanf(buf, "%s %s %s", method, uri, version); //   공백 단위로 분리해서 읽기

    if (strcasecmp(method, "GET")) // method가 GET인지 비교
    {                              // line:netp:doit:beginrequesterr
        clienterror(fd, method, "501", "Not Implemented",
                    "Tiny does not implement this method");
        return;
    } // line:netp:doit:endrequesterr

    read_requesthdrs(&rio); //  header 스킵

    //  STEP 2 : query parameter 가져오기
    /* Parse URI from GET request */
    is_static = parse_uri(uri, filename, cgiargs); // line:netp:doit:staticcheck
    if (stat(filename, &sbuf) < 0)
    { // line:netp:doit:beginnotfound
        //  파일 못 찾으면 에러
        clienterror(fd, filename, "404", "Not found",
                    "Tiny couldn't find this file");
        return;
    } // line:netp:doit:endnotfound

    if (is_static) //  CASE 1 : static 요청
    {              /* Serve static content */
        //  STEP 3 : 권한 체크
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode))
        { // line:netp:doit:readable
            clienterror(fd, filename, "403", "Forbidden",
                        "Tiny couldn't read the file");
            return;
        }
        //  STEP 4 : 파일 열어서 전송
        serve_static(fd, filename, sbuf.st_size); // line:netp:doit:servestatic
    }
    else //  CASE 2 : dynamic 요청
    {    /* Serve dynamic content */
        //  STEP 3 : 권한 체크
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode))
        { // line:netp:doit:executable
            clienterror(fd, filename, "403", "Forbidden",
                        "Tiny couldn't run the CGI program");
            return;
        }
        //  STEP 4 : 해당 코드파일 실행해서 결과 전송
        serve_dynamic(fd, filename, cgiargs); // line:netp:doit:servedynamic
    }
}
/* $end doit */

/*
 * read_requesthdrs - read HTTP request headers
 */
/* $begin read_requesthdrs */
void read_requesthdrs(rio_t *rp)
{
    char buf[MAXLINE];

    //  STEP 1 : 한줄 씩 읽어오면서 빈 줄이 들어올 때 까지 print하고 스킵
    //  NOTE : header를 처리해야한다면 아래 Rio_radlineb 밑에 처리할 것
    do
    { // line:netp:readhdrs:checkterm
        Rio_readlineb(rp, buf, MAXLINE);
        printf("%s", buf);
    } while (strcmp(buf, "\r\n"));
    return;
}
/* $end read_requesthdrs */

/*
 * parse_uri - parse URI into filename and CGI args
 *             return false if dynamic content, true if static
 */
/* $begin parse_uri */
bool parse_uri(char *uri, char *filename, char *cgiargs)
{
    char *ptr;
    char *static_dir = "./tiny/static";
    char *bin_dir = "./out/bin";

    //  CASE 1 : static
    if (!strstr(uri, "cgi-bin"))           // cgi-bin이 지정이 안되어있음
    { /* Static content */                 // line:netp:parseuri:isstatic
        strcpy(cgiargs, "");               // line:netp:parseuri:clearcgi
        strcpy(filename, static_dir);         //   파일을 찾는 폴더를 ./out/bin 으로
        strcat(filename, uri);             // line:netp:parseuri:endconvert1
        if (uri[strlen(uri) - 1] == '/')   //   마지막 문자가 '/' 이면 -> 기본값 지정
            strcat(filename, "home.html"); // line:netp:parseuri:appenddefault
        return true;
    }
    else                       // CASE 2 : dynamic
    { /* Dynamic content */    // line:netp:parseuri:isdynamic
        ptr = index(uri, '?'); // line:netp:parseuri:beginextract
        if (ptr)
        {
            strcpy(cgiargs, ptr + 1);
            *ptr = '\0';
        }
        else
            strcpy(cgiargs, "");   // line:netp:parseuri:endextract
        strcpy(filename, bin_dir); //   파일을 찾는 폴더를 ./out/bin 으로
        strcat(filename, uri);     // line:netp:parseuri:endconvert2
        return false;
    }
}
/* $end parse_uri */

/*
 * serve_static - copy a file back to the client
 */
/* $begin serve_static */
void serve_static(int fd, char *filename, int filesize)
{
    int srcfd;
    char *srcp, filetype[MAXLINE], buf[MAXBUF];

    /* Send response headers to client */
    //  STEP 1 : file type 읽어오기
    get_filetype(filename, filetype); // line:netp:servestatic:getfiletype

    //  STEP 2 : 상태 및 헤더 전송
    sprintf(buf, "HTTP/1.0 200 OK\r\n"); // line:netp:servestatic:beginserve
    Rio_writen(fd, buf, strlen(buf));

    sprintf(buf, "Server: Tiny Web Server\r\n");
    Rio_writen(fd, buf, strlen(buf));

    sprintf(buf, "Content-length: %d\r\n", filesize);
    Rio_writen(fd, buf, strlen(buf));

    sprintf(buf, "Content-type: %s\r\n\r\n", filetype);
    Rio_writen(fd, buf, strlen(buf)); // line:netp:servestatic:endserve

    /* Send response body to client */
    //  STEP 3 : body 작성 (data 담아서)
    srcfd = Open(filename, O_RDONLY, 0);                        // line:netp:servestatic:open
    srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0); // line:netp:servestatic:mmap
    Close(srcfd);                                               // line:netp:servestatic:close
    Rio_writen(fd, srcp, filesize);                             // line:netp:servestatic:write
    Munmap(srcp, filesize);                                     // line:netp:servestatic:munmap
}
/* $end serve_static */

/*
 * get_filetype - derive file type from file name
 */
/* $begin get_filetype */
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
    // MP4 추가
    else if (strstr(filename, ".mp4"))
        strcpy(filetype, "video/mp4");
    else
        strcpy(filetype, "text/plain");
}
/* $end get_filetype */

/*
 * serve_dynamic - run a CGI program on behalf of the client
 */
/* $begin serve_dynamic */
void serve_dynamic(int fd, char *filename, char *cgiargs)
{
    char buf[MAXLINE], *emptylist[] = {NULL};

    /* Return first part of HTTP response */
    //  STEP 1 : http 상태 및 헤더 출력
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    Rio_writen(fd, buf, strlen(buf));

    sprintf(buf, "Server: Tiny Web Server\r\n");
    Rio_writen(fd, buf, strlen(buf));

    //  STEP 2 : 자식 만들어서 분기
    if (Fork() == 0)
    { /* Child */ // line:netp:servedynamic:fork
        /* Real server would set all CGI vars here */
        setenv("QUERY_STRING", cgiargs, 1);                         // line:netp:servedynamic:setenv
        Dup2(fd, STDOUT_FILENO); /* Redirect stdout to client */    // line:netp:servedynamic:dup2
        Execve(filename, emptylist, environ); /* Run CGI program */ // line:netp:servedynamic:execve
    }
    Wait(NULL); /* Parent waits for and reaps child */ // line:netp:servedynamic:wait
}
/* $end serve_dynamic */

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

