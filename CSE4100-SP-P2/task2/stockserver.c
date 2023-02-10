/* 
 * echoserveri.c - An iterative echo server 
 */ 
/* $begin echoserverimain */
#include "csapp.h"
#define NTHREADS 1000
#define SBUFSIZE 1000

typedef struct item{
    int ID;
    int left_stock;
    int price;
    struct item *left;
    struct item *right;
} item;
item* root;

typedef struct {
    int *buf; 
    int n; 
    int front; 
    int rear; 
    sem_t mutex; 
    sem_t slots; 
    sem_t items; 
} sbuf_t;

sbuf_t sbuf;

item* insert(item* root, int id, int left_stock, int price);
void *thread(void *vargp);
void sbuf_init(sbuf_t *sp, int n);
void sbuf_insert(sbuf_t *sp, int item);
int sbuf_remove(sbuf_t *sp);
static void init_echo_cnt(void);
void echo_cnt(int connfd);
void print(item* root, char* buf);

int byte_cnt = 0;
static sem_t mutex; 
char input[MAXLINE];

item* insert(item* root, int id, int left_stock, int price) { 
    if(root == NULL) { 
        root = (item*)malloc(sizeof(item)); 
        root->right = root->left = NULL; 
        root->ID = id;
        root->left_stock = left_stock;
        root->price = price; 
        return root; 
        } 
    else { 
        if(id < root->ID) root->left = insert(root->left, id, left_stock, price);
        else root->right = insert(root->right, id, left_stock, price);
        }
    return root;
}

void *thread(void *vargp){
    Pthread_detach(pthread_self());
    while (1) {
        int connfd = sbuf_remove(&sbuf); 
        echo_cnt(connfd); 
        Close(connfd);
        
        memset(input, 0, sizeof(input));
        print(root, input);
        FILE* fp = fopen("stock.txt", "w");
		fprintf(fp,"%s\n", input);
        fclose(fp);
    }
}

void sbuf_init(sbuf_t *sp, int n){
    sp->buf = Calloc(n, sizeof(int));
    sp->n = n; 
    sp->front = sp->rear = 0; 
    Sem_init(&sp->mutex, 0, 1); 
    Sem_init(&sp->slots, 0, n); 
    Sem_init(&sp->items, 0, 0); 
}

void sbuf_insert(sbuf_t *sp, int item){
    P(&sp->slots); 
    P(&sp->mutex); 
    sp->buf[(++sp->rear)%(sp->n)] = item; 
    V(&sp->mutex); 
    V(&sp->items); 
}

int sbuf_remove(sbuf_t *sp){
    int item;
    P(&sp->items); 
    P(&sp->mutex); 
    item = sp->buf[(++sp->front)%(sp->n)]; 
    V(&sp->mutex); 
    V(&sp->slots); 
    return item;
}

static void init_echo_cnt(void) {
    Sem_init(&mutex, 0, 1);
    byte_cnt = 0;
 }

 void echo_cnt(int connfd){
    int n;
    char buf[MAXLINE];
    rio_t rio;
    static pthread_once_t once = PTHREAD_ONCE_INIT;

    Pthread_once(&once, init_echo_cnt);
    Rio_readinitb(&rio, connfd);

    while((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {
        P(&mutex);
        char command[MAXLINE];
        int id, amount;
        sscanf(buf, "%s %d %d", command, &id, &amount);
        byte_cnt += n;
        printf("Server received %d bytes\n", n);

        if(!strcmp(command, "show")){
            memset(input, 0, sizeof(input));
			print(root, input);
        }

        else if(!strcmp(command, "buy")){
            memset(input, 0, sizeof(input));
            item *buy = root;
	        while(buy){
                if(id == buy->ID) break;
		        else if(id < buy->ID) buy = buy->left;
		        else buy = buy->right;
            }

            if(amount > buy->left_stock){
                strcpy(input, "Not enough left stock\n");
                }

            else{
                    buy->left_stock -= amount;
                    strcpy(input, "[buy] success\n");
                }
        }

        else if(!strcmp(command, "sell")){
            memset(input, 0, sizeof(input));
            item *sell = root;
	        while(sell){
                if(id == sell->ID) break;
                else if(id < sell->ID) sell = sell->left;
		        else sell = sell->right;
	        }

			sell->left_stock += amount;
	    	strcpy(input, "[sell] successs\n");
		}

        else{
            memset(input, 0, sizeof(input)); 
            strcpy(input, "wrong command\n");
            }
        V(&mutex);
        Rio_writen(connfd, input, MAXLINE);
    }
 }

 void print(item* root, char* buf){
    char temp[MAXLINE];

	if(root == NULL) return;
	sprintf(temp,"%d %d %d \n", root->ID, root->left_stock, root->price);
	strcat(buf, temp);

    print(root->left, buf);
    print(root->right, buf);
	return;
 }

int main(int argc, char **argv) 
{
    int listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;  
    char client_hostname[MAXLINE], client_port[MAXLINE];
    pthread_t tid;

    if (argc != 2) {
	    fprintf(stderr, "usage: %s <port>\n", argv[0]);
	    exit(0);
    }

    FILE* fp = fopen("stock.txt", "r");
    int id, left, price;

    while(fscanf(fp, "%d %d %d", &id, &left, &price ) != EOF){
        root = insert(root, id, left, price);
    }
     fclose(fp);

    listenfd = Open_listenfd(argv[1]);
    sbuf_init(&sbuf, SBUFSIZE);

    for (int i = 0; i < NTHREADS; i++)
        Pthread_create(&tid, NULL, thread, NULL);

    while (1) {
        clientlen = sizeof(struct sockaddr_storage); 
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
        printf("Connected to (%s, %s)\n", client_hostname, client_port);
        sbuf_insert(&sbuf, connfd);
    }
    exit(0);
}
/* $end echoserverimain */
