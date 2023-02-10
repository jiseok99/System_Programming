/* 
 * echoserveri.c - An iterative echo server 
 */ 
/* $begin echoserverimain */
#include "csapp.h"

typedef struct item{
    int ID;
    int left_stock;
    int price;
    struct item *left;
    struct item *right;
} item;
item* root;

typedef struct{
    int maxfd;
    fd_set read_set;
    fd_set ready_set;
    int nready;
    int maxi;
    int clientfd[FD_SETSIZE];
    rio_t clientrio[FD_SETSIZE];
} pool;

int byte_cnt = 0;
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

void init_pool(int listenfd, pool *p){
    int i;
    p->maxi = -1;
    for (i=0; i< FD_SETSIZE; i++)
        p->clientfd[i] = -1;
    p->maxfd = listenfd;
    FD_ZERO(&p->read_set);
    FD_SET(listenfd, &p->read_set);
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

void add_client(int connfd, pool *p){
    int i;
    p->nready--;
    for (i = 0; i < FD_SETSIZE; i++) {
        if (p->clientfd[i] < 0) {
            p->clientfd[i] = connfd;
            Rio_readinitb(&p->clientrio[i], connfd);
            
            FD_SET(connfd, &p->read_set);

            if (connfd > p->maxfd)
                p->maxfd = connfd;
            if (i > p->maxi)
                p->maxi = i;
            break;
        }
    }
    if (i == FD_SETSIZE) 
        app_error("add_client error: Too many clients");
 }

void check_clients(pool *p){
    int i, connfd, n;
    char buf[MAXLINE];
    rio_t rio;

    for (i = 0; (i <= p->maxi) && (p->nready > 0); i++) {
        connfd = p->clientfd[i];
        rio = p->clientrio[i];

        if ((connfd > 0) && (FD_ISSET(connfd, &p->ready_set))) {
            p->nready--;

            if ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {
                char command[MAXLINE];
                int id, amount;
                sscanf(buf, "%s %d %d", command, &id, &amount);
                byte_cnt += n;
                //memset(buf, 0, sizeof(buf));
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
               Rio_writen(connfd, input, MAXLINE);
            }

            else {
                memset(input, 0, sizeof(buf));
                print(root, input);
                FILE* fp = fopen("stock.txt", "w");
				fprintf(fp,"%s\n", input);
                fclose(fp);
    
                Close(connfd);
                FD_CLR(connfd, &p->read_set);
                p->clientfd[i] = -1;
            }
        }
    }
}

void echo(int connfd);

int main(int argc, char **argv) 
{
    int listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;  /* Enough space for any address */  //line:netp:echoserveri:sockaddrstorage
    char client_hostname[MAXLINE], client_port[MAXLINE];
    static pool pool;

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
    init_pool(listenfd, &pool);
    while (1) {
        pool.ready_set = pool.read_set;
        pool.nready = Select(pool.maxfd+1, &pool.ready_set, NULL, NULL, NULL);

        if (FD_ISSET(listenfd, &pool.ready_set)) {
            clientlen = sizeof(struct sockaddr_storage); 
            connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
            Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
            add_client(connfd, &pool);
            printf("Connected to (%s, %s)\n", client_hostname, client_port);
        }

        check_clients(&pool);
    }
    exit(0);
}
/* $end echoserverimain */
