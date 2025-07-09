/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
#include "../../include/csapp.h"

// URL 디코딩 함수
void url_decode(char *dst, const char *src) {
    char a, b;
    while (*src) {
        if ((*src == '%') &&
            ((a = src[1]) && (b = src[2])) &&
            isxdigit(a) && isxdigit(b)) {
            if (a >= 'a') a -= 'a' - 'A';
            if (a >= 'A') a -= ('A' - 10); else a -= '0';
            if (b >= 'a') b -= 'a' - 'A';
            if (b >= 'A') b -= ('A' - 10); else b -= '0';
            *dst++ = 16 * a + b;
            src += 3;
        } else if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

int main(void)
{
    //  STEP 1: 변수 초기화
    char *query = getenv("QUERY_STRING");
    char encoded_url[MAXLINE] = "";
    char decoded_url[MAXLINE] = "";
    char content[MAXLINE];
    sprintf(content, "QUERY_STRING=%s\r\n", query);           // 첫 줄: QUERY_STRING
    sprintf(content + strlen(content), "<h1>Welcome to player.com</h1>\r\n"); // 둘째 줄: welcome 메시지

    // 주소 입력 창
    sprintf(content + strlen(content),
            "<form action=\"/cgi-bin/player\" method=\"get\">\r\n"
            "  <label for=\"video\">Enter video URL:</label>\r\n"
            "  <input type=\"text\" id=\"video\" name=\"video\">\r\n"
            "  <input type=\"submit\" value=\"Play\">\r\n"
            "</form>\r\n");

    //  CASE 1: 입력 arguement 가 있을 때
    if (query && strstr(query, "video=")) {
        char *video_param = strstr(query, "video=") + strlen("video=");
        strncpy(encoded_url, video_param, MAXLINE - 1);
        url_decode(decoded_url, encoded_url);
        sprintf(content + strlen(content),
            "<video width=\"640\" height=\"360\" controls>\r\n"
            "  <source src=\"%s\" type=\"video/mp4\">\r\n"
            "  Your browser does not support the video tag.\r\n"
            "</video>\r\n",
            decoded_url);
        
    }
    // 마지막 줄: 방문 감사 메시지
    sprintf(content + strlen(content), "<p>Thanks for visiting!</p>\r\n");

    //  STEP 4: header 생성 및 client로 전송
    printf("Connection: close\r\n");
    printf("Content-length: %d\r\n", (int)strlen(content));
    printf("Content-type: text/html\r\n\r\n");
    printf("%s", content);
    fflush(stdout);

    exit(0);
}
/* $end adder */
