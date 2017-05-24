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

/**
 *	送受信するデータサイズの上限を設定
 *	実行環境下で正常動作する範囲内で、十分大きい値にすること
 */
#define MAX_SIZE 1024

/**
 *	接続するクライアントの上限を設定
 *	実行環境下で正常動作する範囲内で、十分大きい値にすること
 *	MIN:2
 */
#define MAX_CLIENT 5

/**
 *	クライアントに関するデータを格納する構造体
 *	sockfd	ソケット
 *	name	クライアントの名前
 *	isused	1のとき使用中
 */
struct client{
	int sockfd;
	char name[50];
	int isused;
};

/**
 *	クライアントを管理するための配列
 *	send_serverend()のためにglobal変数としている
 */
struct client clients[MAX_CLIENT];
/*
	本来であれば、クライアントはリスト構造などを用いて、上限を特に設定すること無く、無駄なく管理されるべきである。
	しかし、今回はそこまで実装することが困難であったため、配列を用いて管理されている。
*/

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
 *	サーバへLOGOUTコマンドを送信する関数
 *	必ずglobal変数clients[]を初期化してから使用すること
 *	@author		Takuya Kawaguchi
 *	@version	1.0
 */
void send_serverend(){
	int i;
	loop_flg = 0;	//送受信終了
	printf("\x1b[31m\x1b[1mSERVER END\x1b[0m\n");
	for(i = 0; i < MAX_CLIENT; i++) if(clients[i].isused) check_error(write(clients[i].sockfd, "SERVER_END", MAX_SIZE));	//全クライアントにSERVER_ENDコマンドを送信
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
 *	@version	1.0
 *	@return		正常終了時int(0), 異常終了時int(1)
 */
int main(){
	struct sockaddr_in serv_addr;	//サーバデータ
	struct timeval sltime;	//select()待ち時間
	int val, sockfd, used = 0, i, j, acce_flg = 1, maxfd;
	//val:システムコール返り値 sockfd:受付用ソケット used:使用中クライアント数 i,j:カウンタ
	//acce_flg:1のとき新規クライアント受付 maxfd:ファイルディスクリプタの最大値
	//int flag = 1;	//デバッグ時有効化
	char buff[MAX_SIZE], new_buff[MAX_SIZE], reply[60];	//buff, new_buff:送受信データ reply:リプライコマンド操作用
	char *command;	//各コマンド検証用
	fd_set sl;	//select()監視対象

	//クライアントを全て未使用として初期化
	for(i = 0; i < MAX_CLIENT; i++){
		clients[i].isused = 0;
	}

	//ソケット準備->サンプルプログラムと同じため説明省略
	check_error(sockfd = socket(AF_INET, SOCK_STREAM, 0));

	memset(&serv_addr, 0, sizeof(struct sockaddr_in));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(10000);	//受付ポート10000番：適宜変更

	//setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(int));	//デバッグ時有効化
	check_error(bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr_in)));
	check_error(listen(sockfd, 10));

	//select()準備
	sltime.tv_sec = 0;
	sltime.tv_usec = 0;

	//Control + Cが入力された場合、全クライアントへSERVER_ENDコマンドを送信する
	//これ以外での強制終了時も設定するべきだが、今回はControl + Cでのみ強制終了させられるとして、省略する
	if(signal(SIGINT, send_serverend) == SIG_ERR){
		perror("\x1b[31m\x1b[1mERROR\x1b[0m ");
		exit(1);
	}

	printf("\x1b[1m最大接続可能数\x1b[0m : %d\n", MAX_CLIENT);

	while(loop_flg){
		//select()の監視対象を設定
		FD_ZERO(&sl);
		maxfd = 0;
		if(acce_flg){	//受け入れが有効な場合、要求受付用ソケットの監視
			FD_SET(sockfd, &sl);
			if(maxfd < sockfd) maxfd = sockfd;
		}
		for(i = 0; i < MAX_CLIENT; i++){	//全有効クライアントに対して監視
			if(clients[i].isused){
				FD_SET(clients[i].sockfd, &sl);
				if(maxfd < clients[i].sockfd) maxfd = clients[i].sockfd;
			}
		}

		check_error(select(maxfd + 1, &sl, NULL, NULL, &sltime));

		if(FD_ISSET(sockfd, &sl)){	//要求受付ポートに新規要求がある場合受け入れ
			for(i = 0; i < MAX_CLIENT; i++){
				if(clients[i].isused != 1){
					clients[i].isused = 1;	//クライアント使用中に

					strcpy(clients[i].name, "NEW CLIENT");	//使用名の初期設定

					check_error(clients[i].sockfd = accept(sockfd, NULL, NULL));	//受け入れ

					check_error(write(clients[i].sockfd, "\x1b[36m\x1b[1m接続完了\x1b[0m", MAX_SIZE));

					used++;		//使用中クライアント数を増やす

					sprintf(buff, "\x1b[1m%s\t\x1b[0m: \x1b[36m\x1b[1mLOGIN\x1b[0m", clients[i].name);
					printf("%s\n", buff);
					for(j = 0; j < MAX_CLIENT; j++) if(clients[j].isused) check_error(write(clients[j].sockfd, buff, MAX_SIZE));	//全クライアントに送信

					sprintf(buff, "\x1b[1m残り接続可能数\x1b[0m : %d", MAX_CLIENT - used);
					printf("%s\n", buff);
					for(j = 0; j < MAX_CLIENT; j++) if(clients[j].isused) check_error(write(clients[j].sockfd, buff, MAX_SIZE));	//全クライアントに送信

					if(used >= MAX_CLIENT){	//クライアント最大数に達した場合
						acce_flg = 0;	//受付終了
						printf("\x1b[31m\x1b[1m警告\x1b[0m : クライアント数が上限に達しました。\n");
						for(j = 0; j < MAX_CLIENT; j++) if(clients[j].isused) check_error(write(clients[j].sockfd, "\x1b[31m\x1b[1m警告\x1b[0m : クライアント数が上限に達しました。", MAX_SIZE));	//全クライアントに送信
					}

					break;
				}
			}
		}

		for(i = 0; i < MAX_CLIENT; i++){
			if(clients[i].isused && FD_ISSET(clients[i].sockfd, &sl)){	//使用中で、かつ、読み込み可能なデータがある場合
				check_error(read(clients[i].sockfd, buff, MAX_SIZE));

				if(strcmp(buff, "END") == 0){	//ENDコマンドが送られた場合終了
					loop_flg = 0;	//送受信ループを終了する

					sprintf(buff, "\x1b[1m%s\t\x1b[0m: \x1b[31m\x1b[1mEND\x1b[0m", clients[i].name);
					printf("%s\n", buff);
					for(j = 0; j < MAX_CLIENT; j++) if(clients[j].isused) check_error(write(clients[j].sockfd, buff, MAX_SIZE));	//全クライアントに送信

					printf("\x1b[31m\x1b[1mSERVER END\x1b[0m\n");
					for(j = 0; j < MAX_CLIENT; j++) if(clients[j].isused) check_error(write(clients[j].sockfd, "SERVER_END", MAX_SIZE));	//全クライアントにSERVER_ENDコマンドを送信

					break;
				}else if(strcmp(buff, "LOGOUT") == 0){
					check_error(write(clients[i].sockfd, "LOGOUT", MAX_SIZE));	//該当クライアントにLOGOUTコマンドを送信

					//該当クライアントのソケットの使用が終わるまで待機
					while(1){	//read() == 0、つまりクライアントがclose()されるまでループ
						check_error(val = read(clients[i].sockfd, buff, MAX_SIZE));
						if(val == 0) break;
					}

					check_error(close(clients[i].sockfd));

					clients[i].isused = 0;	//該当クライアント使用終了
					used--;	//使用中クライアント数を減らす
					acce_flg = 1;	//必ず新規クライアントが受け入れ可能になるため、要求受付を有効化

					printf("\x1b[1m%s\t\x1b[0m: \x1b[31m\x1b[1mLOGOUT\x1b[0m\n", clients[i].name);
					sprintf(buff, "\x1b[1m%s\t\x1b[0m: \x1b[31m\x1b[1mLOGOUT\x1b[0m", clients[i].name);
					for(j = 0; j < MAX_CLIENT; j++) if(clients[j].isused) check_error(write(clients[j].sockfd, buff, MAX_SIZE));	//全クライアントに対して送信
				}else if((command = strstr(buff, "NAME@")) != NULL && command == buff){	//NAMEコマンドが送られた場合名前変更
					strcpy(new_buff, buff + sizeof("NAME"));	//新規使用名を抽出

					printf("\x1b[1m%s\t\x1b[0m: \x1b[33m\x1b[1mNAME@\x1b[0m%s\n", clients[i].name, new_buff);
					sprintf(buff, "\x1b[1m%s\t\x1b[0m: \x1b[33m\x1b[1mNAME->\x1b[0m %s", clients[i].name, new_buff);
					for(j = 0; j < MAX_CLIENT; j++) if(clients[j].isused) check_error(write(clients[j].sockfd, buff, MAX_SIZE));	//全クライアントに対して送信

					strcpy(clients[i].name, new_buff);	//クライアントの使用名として設定
				}else if((command = strstr(buff, "RE@")) != NULL && command == buff){	//REコマンドが送られた場合リプライ※同一名を使用しているクライアント全てに送信
					printf("\x1b[1m%s\t\x1b[0m: \x1b[32m\x1b[1mRE@\x1b[0m%s\n", clients[i].name, buff + sizeof("RE"));

					//すべての使用中クライアントの使用名と照合
					for(j = 0; j < MAX_CLIENT; j++){
						if(clients[j].isused){
							sprintf(reply, "RE@%s@", clients[j].name);
							if((command = strstr(buff, reply)) != NULL && command == buff){	//使用名で照合
								sprintf(new_buff, "\x1b[32m\x1b[1mReply %s -> %s\t\x1b[0m: %s", clients[i].name, clients[j].name, buff + strlen(reply));
								check_error(write(clients[j].sockfd, new_buff, MAX_SIZE));	//対象宛に送信
								check_error(write(clients[i].sockfd, new_buff, MAX_SIZE));	//送信元宛に送信
							}
						}
					}
				}else{	//コマンドが見つからない場合全クライアント送信
					sprintf(new_buff, "\x1b[1m%s\t\x1b[0m: %s", clients[i].name, buff);
					printf("%s\n", new_buff);
					for(j = 0; j < MAX_CLIENT; j++) if(clients[j].isused) check_error(write(clients[j].sockfd, new_buff, MAX_SIZE));	//全クライアントに対して送信
				}
			}
		}
	}

	//全クライアントのソケットの使用が終わるまで待機
	for(i = 0; i < used; i++){
		if(clients[i].isused){
			while(1){	//read() == 0、つまりクライアントがclose()されるまでループ
				check_error(val = read(clients[i].sockfd, buff, MAX_SIZE));
				if(val == 0) break;
			}
		}
	}

	//全ソケットclose()
	for(i = 0; i < MAX_CLIENT; i++) if(clients[i].isused) check_error(close(clients[i].sockfd));

	check_error(close(sockfd));

	return 0;
}
