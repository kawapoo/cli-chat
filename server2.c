/**
*	プロジェクト演習2 Linuxプログラミング
*	最終課題 通信アプリコース 2
*	サーバ用プログラム
*	CY15111 川口 拓哉
*	2017-05-19
*/
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <time.h>

/**
*	送受信するデータサイズの上限を設定
*	実行環境下で正常動作する範囲内で、十分大きい値にすること
*/
#define MAX_SIZE 1024

/**
*	クライアントに関するデータを格納する構造体
*	sockfd	ソケット
*	name	クライアントの名前
*	isused	1のとき使用中
*/
struct client{
	int sockfd;
	char name[50];
	struct client* next;
};

/**
*	send_serverend()用クライアントリスト
*	クライアントリストの先頭のポインタを指すポインタを必ず入れておくこと
*	send_serverend()のためにglobal変数としている
*/
struct client** send_serverend_list;

/**
*	クライアントとの送受信ループの実行フラグ
*	1のときクライアントと送受信し、1以外に設定された場合にプログラムの終了へ遷移する
*	send_serverend()のためにglobal変数としている
*/
int loop_flg = 1;

/**
*	エラーチェックをする関数
*	異常終了時の返り値が負数であり、errnoが設定されるシステムコールにのみ使用可能
*	エラー時は1を返してプログラムを異常終了する
*	@author		Takuya Kawaguchi
*	@version	1.0
*	@param		val	システムコールの返り値
*/
void check_error(int val){
	if(val < 0){
		perror("\x1b[31m\x1b[1mERROR\x1b[0m ");
		exit(1);
	}
	return;
}

/**
*	クライアント名が使用可能かチェックする関数
*	@author		Takuya Kawaguchi
*	@version	1.0
*	@param		list	検索を開始するclient構造体を指すポインタ
*	@param		name	検索対象とする名前文字列
*	@return		名前使用時int(1), 名前未使用時int(0)
*/
int is_name_used(struct client* list, char* name){
	struct client* tlist = list;
	while(tlist != 0){
		if(strcmp(tlist->name, name) == 0) return 1;
		tlist = tlist->next;
	}
	return 0;
}

/**
*	クライアントリストにクライアントを追加する関数
*	@author		Takuya Kawaguchi
*	@version	1.0
*	@param		list		追加先リスト先頭のclient構造体を指すポインタ
*	@param		new_client	追加するclient構造体を指すポインタ
*	@return		追加されたclient構造体を指すポインタ
*/
struct client* add_client(struct client* list, int sockfd, char* name){
	struct client* new_cli;
	if((new_cli = (struct client*)malloc(sizeof(struct client))) == NULL){
		printf("\x1b[31m\x1b[1mERROR\x1b[0m :new client has not been added.\n");
	}
	new_cli->sockfd = sockfd;
	strcpy(new_cli->name, name);
	new_cli->next = list;
	return new_cli;
}

/**
*	クライアントリストから直前のクライアントを検索する関数
*	@author		Takuya Kawaguchi
*	@version	1.0
*	@param		list	検索先リスト先頭のclient構造体を指すポインタ
*	@param		target	検索対象が指すclient構造体を指すポインタ
*	@param		flag	リストの先頭を帰す場合、flagの値は1になる
*	@return		直前のクライアントを指すポインタを返す
*/
struct client* search_prev(struct client* list, struct client* target, int *flag){
	struct client* old_p = 0, * cur_p = list;
	*flag = 1;
	while(cur_p != 0){
		if(cur_p == target)
			return old_p;
		old_p = cur_p;
		*flag = 0;
		cur_p = cur_p->next;
	}
	*flag = 0;
	return 0;
}

/**
*	クライアントリストからクライアントを削除する関数
*	@author		Takuya Kawaguchi
*	@version	1.0
*	@param		list	削除元リスト先頭のclient構造体を指すポインタのポインタ
*	@param		target	削除するclient構造体を指すポインタ
*	@return		新リストの先頭のclient構造体
*/
struct client* del_client(struct client* list, struct client* target){
	struct client* prep, * temp_p = 0;
	int flag = 0;
	prep = search_prev(list, target, &flag);
	if(prep == 0 && flag == 0) return list;
	else if(prep == 0 && flag == 1){
		temp_p = list->next;
		free(list);
		return temp_p;
	}else{
		temp_p = prep->next->next;
		free(prep->next);
		prep->next = temp_p;
		return list;
	}
	return list;
}

/**
*	指定クライアントにメッセージを送信する関数
*	@author		Takuya Kawaguchi
*	@version	1.0
*	@param		target	送信先クライアントのclient構造体を指すポインタ
*	@param		buff	送信する文字列
*/
void send_2_a_client(struct client* target, char* buff){
	check_error(write(target->sockfd, buff, MAX_SIZE));
	return;
}

/**
*	全クライアントにメッセージを送信する関数
*	@author		Takuya Kawaguchi
*	@version	1.0
*	@param		list	送信先リスト先頭のclient構造体を指すポインタ
*	@param		buff	送信する文字列
*/
void send_2_all_client(struct client* list, char* buff){
	struct client* tlist = list;
	while(tlist != 0){
		send_2_a_client(tlist, buff);
		tlist = tlist->next;
	}
	return;
}

/**
*	全クライアントの使用名を指定クライアントに送信する関数
*	@author		Takuya Kawaguchi
*	@version	1.0
*	@param		list	リスト先頭のclient構造体を指すポインタ
*	@param		target	送信先クライアントを指すポインタ
*/
void send_name_list(struct client* list, struct client* target){
	struct client* tlist = list;
	char buff[50];
	send_2_a_client(target, "\x1b[35m\x1b[1mLIST\x1b[0m");
	while(tlist != 0){
		sprintf(buff, "\t+ %s", tlist->name);
		send_2_a_client(target, buff);
		tlist = tlist->next;
	}
	return;
}

/**
*	全クライアントへSERVER_ENDコマンドを送信する関数
*	対象となるリストをあらかじめglobal変数send_serverend_listに登録しておくこと
*	@author		Takuya Kawaguchi
*	@version	2.0
*/
void send_serverend(){
	loop_flg = 0;	//送受信終了
	printf("\x1b[31m\x1b[1mSERVER END\x1b[0m\n");
	send_2_all_client(*send_serverend_list, "SERVER_END");
	return;
}

/**
*	チャットサーバ用プログラム
*	以下の各種コマンドをクライアントから受信した場合、特定の動作をする
*		END				全クライアント、サーバを終了する
*		LOGOUT			クライアントはログアウトし、プロセスを終了する
*		NAME@hogehoge	hogehogeに名前を変更する
*		RE@hoge@message	hogeに対してのみmessageを送信
*	@author		Takuya Kawaguchi
*	@version	2.0
*	@return		正常終了時int(0), 異常終了時int(1)
*/
int main(){
	struct sockaddr_in serv_addr;	//サーバデータ
	struct timeval sltime;	//select()待ち時間
	int val, sockfd, maxfd, tsockfd;
	//val:システムコール返り値 sockfd:受付用ソケット
	//maxfd:ファイルディスクリプタの最大値 tsockfd:ソケット登録用
	//int flag = 1;	//デバッグ時有効化
	char buff[MAX_SIZE], new_buff[MAX_SIZE], reply[60], name[50];	//buff, new_buff:送受信データ reply:リプライコマンド操作用 name:使用名登録用
	char *command;	//各コマンド検証用
	fd_set sl;	//select()監視対象
	struct client* clients = 0, * target_cli, * ttarget_cli;	//clients:クライアントリスト, target_cli:クライアント操作用, ttarget_cli:クライアント操作用
	send_serverend_list = &clients;

	//ソケット準備->サンプルプログラムと同じため説明省略
	check_error(sockfd = socket(AF_INET, SOCK_STREAM, 0));

	memset(&serv_addr, 0, sizeof(struct sockaddr_in));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(10000);	//受付ポート10000番：適宜変更

	//setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(int));	//デバッグ時有効化
	check_error(bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr_in)));
	check_error(listen(sockfd, 10));

	//Control + Cが入力された場合、全クライアントへSERVER_ENDコマンドを送信する
	//これ以外での強制終了時も設定するべきだが、今回はControl + Cでのみ強制終了させられるとして、省略する
	if(signal(SIGINT, send_serverend) == SIG_ERR){
		perror("\x1b[31m\x1b[1mERROR\x1b[0m ");
		exit(1);
	}

	while(loop_flg){
		//select()の監視対象を設定
		FD_ZERO(&sl);
		maxfd = 0;
		FD_SET(sockfd, &sl);
		if(maxfd < sockfd) maxfd = sockfd;
		target_cli = clients;
		while(target_cli != 0){
			FD_SET(target_cli->sockfd, &sl);
			if(maxfd < target_cli->sockfd) maxfd = target_cli->sockfd;
			target_cli = target_cli->next;
		}

		//select()準備
		sltime.tv_sec = 10;
		sltime.tv_usec = 0;

		check_error(select(maxfd + 1, &sl, NULL, NULL, &sltime));

		if(FD_ISSET(sockfd, &sl)){	//要求受付ポートに新規要求がある場合受け入れ
			do sprintf(name, "NEW CLIENT:%d", (int)time(NULL));
			while(is_name_used(clients, name));
			check_error(tsockfd = accept(sockfd, NULL, NULL));	//受け入れ
			clients = add_client(clients, tsockfd, name);
			send_2_a_client(clients, "\x1b[36m\x1b[1m接続完了\x1b[0m");
			sprintf(buff, "\x1b[1m%s\t\x1b[0m: \x1b[36m\x1b[1mLOGIN\x1b[0m", clients->name);
			printf("%s\n", buff);
			send_2_all_client(clients, buff);
		}

		target_cli = clients;
		while(target_cli != 0){
			if(FD_ISSET(target_cli->sockfd, &sl)){	//使用中で、かつ、読み込み可能なデータがある場合
				check_error(read(target_cli->sockfd, buff, MAX_SIZE));
				if(strcmp(buff, "END") == 0){	//ENDコマンドが送られた場合終了
					loop_flg = 0;	//送受信ループを終了する
					sprintf(buff, "\x1b[1m%s\t\x1b[0m: \x1b[31m\x1b[1mEND\x1b[0m", target_cli->name);
					printf("%s\n", buff);
					send_2_all_client(clients, buff);	//全クライアントに送信
					printf("\x1b[31m\x1b[1mSERVER END\x1b[0m\n");
					send_2_all_client(clients, "SERVER_END");	//全クライアントにSERVER_ENDコマンドを送信
					break;
				}else if(strcmp(buff, "LOGOUT") == 0){
					check_error(write(target_cli->sockfd, "LOGOUT", MAX_SIZE));	//該当クライアントにLOGOUTコマンドを送信
					//該当クライアントのソケットの使用が終わるまで待機
					while(1){	//read() == 0、つまりクライアントがclose()されるまでループ
						check_error(val = read(target_cli->sockfd, buff, MAX_SIZE));
						if(val == 0) break;
					}
					check_error(close(target_cli->sockfd));
					clients = del_client(clients, target_cli);	//該当クライアント使用終了
					sprintf(buff, "\x1b[1m%s\t\x1b[0m: \x1b[31m\x1b[1mLOGOUT\x1b[0m", target_cli->name);
					printf("%s\n", buff);
					send_2_all_client(clients, buff);	//全クライアントに対して送信
				}else if((command = strstr(buff, "NAME@")) != NULL && command == buff){	//NAMEコマンドが送られた場合名前変更
					strcpy(new_buff, buff + sizeof("AME@"));	//新規使用名を抽出
					if(is_name_used(clients, new_buff)){
						sprintf(buff, "\x1b[1m%s\t\x1b[0m: \x1b[33m\x1b[1mTHE NAME\x1b[0m %s \x1b[33m\x1b[1mIS ALREADY EXISTS.\x1b[0m", target_cli->name, new_buff);
						printf("%s\n", buff);
						send_2_a_client(target_cli, buff);
					}else{
						sprintf(buff, "\x1b[1m%s\t\x1b[0m: \x1b[33m\x1b[1mNAME->\x1b[0m %s", target_cli->name, new_buff);
						printf("%s\n", buff);
						send_2_all_client(clients, buff);	//全クライアントに対して送信
						strcpy(target_cli->name, new_buff);	//クライアントの使用名として設定
					}
				}else if((command = strstr(buff, "RE@")) != NULL && command == buff){	//REコマンドが送られた場合リプライ
					printf("\x1b[1m%s\t\x1b[0m: \x1b[32m\x1b[1mRE@\x1b[0m%s\n", target_cli->name, buff + sizeof("RE"));
					//すべての使用中クライアントの使用名と照合
					ttarget_cli = clients;
					while(ttarget_cli != 0){
						sprintf(reply, "RE@%s@", ttarget_cli->name);
						if((command = strstr(buff, reply)) != NULL && command == buff){	//使用名で照合
							sprintf(new_buff, "\x1b[32m\x1b[1mReply %s -> %s\t\x1b[0m: %s", target_cli->name, ttarget_cli->name, buff + strlen(reply));
							send_2_a_client(ttarget_cli, new_buff);	//対象宛に送信
							send_2_a_client(target_cli, new_buff);	//送信元宛に送信
						}
						ttarget_cli = ttarget_cli->next;
					}
				}else if(strcmp(buff, "LIST") == 0){	//LISTコマンドが送られた場合クライアントリストを送信
					printf("\x1b[1m%s\t\x1b[0m: \x1b[35m\x1b[1mLIST\x1b[0m\n", target_cli->name);
					send_name_list(clients, target_cli);
				}else{	//コマンドが見つからない場合全クライアント送信
					sprintf(new_buff, "\x1b[1m%s\t\x1b[0m: %s", target_cli->name, buff);
					printf("%s\n", new_buff);
					send_2_all_client(clients, new_buff);	//全クライアントに対して送信
				}
			}
			target_cli = target_cli->next;
		}
	}

	//全クライアントのソケットの使用が終わるまで待機
	target_cli = clients;
	while(target_cli != 0){
			while(1){	//read() == 0、つまりクライアントがclose()されるまでループ
				check_error(val = read(target_cli->sockfd, buff, MAX_SIZE));
				if(val == 0) break;
			}
			target_cli = target_cli->next;
		}

	//全ソケットclose()
	target_cli = clients;
	while(target_cli != 0){
		check_error(close(target_cli->sockfd));
		target_cli = target_cli->next;
	}

	check_error(close(sockfd));

	return 0;
}
