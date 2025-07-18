/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
#include "../../include/csapp.h"

int main(void)
{
    //  STEP 1: 변수 초기화
    char *buf, *p;
    char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
    int n1 = 0, n2 = 0;

    //  STEP 2: QUERY_STRING 환경변수에서 문자열 받아와서 파싱
    if ((buf = getenv("QUERY_STRING")) != NULL)
    {
        p = strchr(buf, '&');
        *p = '\0';
        strcpy(arg1, buf);
        strcpy(arg2, p + 1);
        n1 = atoi(arg1);
        n2 = atoi(arg2);
        *p = '&';
    }
    // //일부러 무한루프
    // while(1){
    //     int a = 10;
    //     a+1;
    // }

    //  STEP 3: response 생성
    sprintf(content, "QUERY_STRING=%s\r\n<p>", buf);
    sprintf(content, "%sWelcome to add.com: ", content);
    sprintf(content, "%sTHE Internet addition portal.\r\n<p>", content);
    sprintf(content, "%sThe answer is: %d + %d = %d\r\n<p>",
            content, n1, n2, n1 + n2);
    sprintf(content, "%sThanks for visiting!\r\n", content);

    //  STEP 4: header 생성 및 client로 전송
    printf("Connection: close\r\n");
    printf("Content-length: %d\r\n", (int)strlen(content));
    printf("Content-type: text/html\r\n\r\n");
    printf("%s", content);
    fflush(stdout);

    exit(0);
}
/* $end adder */
