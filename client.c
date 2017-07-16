/**
 *	プロジェクト演習2 Linuxプログラミング
 *	最終課題 通信アプリコース 2
 *	クライアント用プログラム
 *	CY15111 川口 拓哉
 *	2017-05-19
 */
/*
	クライアントのプロセスをControl + Cなどで強制終了させると、ソケットが閉じられ、サーバの挙動が正常でなくなったり、サーバのプロセスが停止される。
	そのため、クライアント側でもサーバと同様に、強制終了時のシグナルを受け取り、終了動作へ遷移させることが理想である。
	しかし、今回のソフトウェアでは、クライアントが配列で管理され、accept()待ちのクライアントが発生する確率が非常に高い。
	accept()済みのクライアントと待ちのクライアントでは、今回の実装では終了方法が全く異なる。
	また、accept()待ちのプロセスが終了された状態でaccept()済みのクライアントをLOGOUTさせると、サーバの挙動が正常でなくなる。
	（通信要求が残っているにもかかわらず、対象のクライアントが終了されているためであると考えられる）
	よって、今回は、accept()済みのクライアントは正常にLOGOUTまたはENDし、accept()待ちのクライアントはaccept()されるまで終了されずに待ち続けるとして考える。
*/
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/**
 *	送受信するデータサイズの上限を設定
 *	実行環境下で正常動作する範囲内で、十分大きい値にすること
 */
#define MAX_SIZE 1024

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
 *	チャットクライアント用プログラム
 *	サーバからSERVER_ENDコマンドを受け取った場合は各プロセスを終了させる
 *	@author		Takuya Kawaguchi
 *	@version	1.0
 *	@return		正常終了時int(0), 異常終了時int(1)
 */
int main(){
	struct sockaddr_in serv_addr;	//サーバデータ
	int sockfd, i;	//sockfd:ソケット通信に使用するソケットのディスクリプタ, i:カウンタ
	char sbuff[MAX_SIZE], kbuff[MAX_SIZE], name[50], serverIP[20];
	//buff:送受信データ name:使用する名前 serverIP:接続するサーバのIPアドレス
	struct timeval sltime;	//select()待ち時間
	fd_set sl;	//select()監視対象

	//ソケット準備->サンプルプログラムと同じ箇所は説明省略
	check_error(sockfd = socket(AF_INET, SOCK_STREAM, 0));

	printf("「\x1b[36m\x1b[1m接続完了\x1b[0m」が表示されるまでメッセージの送受信はできません\n");

	memset(&serv_addr, 0, sizeof(struct sockaddr_in));
	serv_addr.sin_family = AF_INET;
	printf("\x1b[1mサーバのIPアドレス-> \x1b[0m");
	scanf("%s", serverIP);
	serv_addr.sin_addr.s_addr = inet_addr(serverIP);	//入力されたIPアドレスで設定
	serv_addr.sin_port = htons(10000);

	printf("\x1b[1m接続中...\x1b[0m\n");

	check_error(connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr_in)));

	//名前設定
	printf("\x1b[1m名前-> \x1b[0m");
	scanf("%s", name);
	sprintf(kbuff, "NAME@%s", name);
	check_error(write(sockfd, kbuff, MAX_SIZE));

	while(1){
		//select()の監視対象を設定
		FD_ZERO(&sl);
		FD_SET(sockfd, &sl);
		FD_SET(0, &sl);	//stdin == 0

		//select()準備
		sltime.tv_sec = 10;
		sltime.tv_usec = 0;

		check_error(select(sockfd + 1, &sl, NULL, NULL, &sltime));

		if(FD_ISSET(sockfd, &sl)){	//サーバから読み込み可能データが送信されている場合
			//サーバから受信したデータを出力
			check_error(read(sockfd, sbuff, MAX_SIZE));

			//SERVER_ENDコマンドを受け取った場合、終了処理へ遷移
			if(strcmp(sbuff, "SERVER_END") == 0){
				printf("\x1b[1m\x1b[31mSERVER END\x1b[0m\n");
				break;
			}

			//LOGOUTコマンドを受け取った場合、終了処理へ遷移
			if(strcmp(sbuff, "LOGOUT") == 0){
				printf("\x1b[1m\x1b[31mLOGOUT\x1b[0m\n");
				break;
			}

			printf("%s\n", sbuff);
		}

		if(FD_ISSET(0, &sl)){	//キーボードに読み込み可能データがある場合
			fgets(kbuff, MAX_SIZE, stdin);
			if(strcmp(kbuff, "\n") != 0){
				for(i = 0; i < MAX_SIZE && kbuff[i] != '\0'; i++) if(kbuff[i] == '\n') kbuff[i] = '\0';
				/*
					本来であれば、fflush()やlseek()などを用いてキーボードの入力をリセット（ストリームのカーソルを設定）しつつ、キーボードからread()するべきである。
					しかし、実験環境ではfflush()やlseek()が正常に動作せず、正常にread()が行えなかったため、ここではscanf()を用いて、入力を受け付けている。
				*/
				check_error(write(sockfd, kbuff, MAX_SIZE));
			}
		}
	}

	check_error(close(sockfd));

	return 0;
}
