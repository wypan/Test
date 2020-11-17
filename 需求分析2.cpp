#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<memory.h>
#include<string.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<sys/stat.h>
#include<sys/param.h>
#include<pwd.h>
#include<errno.h>
#include<time.h>
#include<fcntl.h>
#include<dirent.h>
#include<signal.h>
#define MAX_LINE 80
#define MAX_NAME_LEN 100
#define MAX_PATH_LEN 1000


extern char **environ;
char *cmd_array[MAX_LINE/2+1];
int pipe_fd[2];
int cmd_cnt;

void ls();
void cd();
void pwd();
void help();
void exit();

void cat();
void rm();
void echo();
void clear();
void date();



void readcommand(){
	int cnt=0;
	char str[MAX_LINE];
	char* helper;
	memset(cmd_array,0,MAX_LINE/2+1);
	fgets(str,MAX_LINE,stdin);
	if(str[strlen(str)-1]=='\n'){
		str[strlen(str)-1]='\0';
	}
	helper=strtok(str," ");
	while(helper!=NULL){
			cmd_array[cnt]=(char*)malloc(sizeof(*helper));
			strcpy(cmd_array[cnt++],helper);
			helper=strtok(NULL," ");
	}
	cmd_cnt=cnt;
}

int is_internal_cmd(){
	if(cmd_array[0]==NULL){
		return 0;
	}
	else if(strcmp(cmd_array[0],"quit")==0){
		myquit();
	}
	else if(strcmp(cmd_array[0],"exit")==0){
		myexit();
	}
	else if(strcmp(cmd_array[0],"clr")==0){
		myclr();
		return 1;
	}
	else if(strcmp(cmd_array[0],"continue")==0){
		print_continue_info();
		return 1;
	}
	else if(strcmp(cmd_array[0],"pwd")==0){
		mypwd();
		return 1;
	}
	else if(strcmp(cmd_array[0],"echo")==0){
		for(int i=1;i<cmd_cnt;i++){
			if(strcmp(cmd_array[i],">")==0||strcmp(cmd_array[i],">>")==0){
				myecho_redirect();
				return 1;
			}
		}
		myecho();
		return 1;
	}
	else if(strcmp(cmd_array[0],"time")==0){
		mytime();
		return 1;
	}
	else if(strcmp(cmd_array[0],"environ")==0){//environ
		if(cmd_array[1]!=NULL&&(strcmp(cmd_array[1],">")==0||strcmp(cmd_array[1],">>")==0)){
			myenviron_redirect();
			return 1;
		}
		else{
			myenviron();
		}
	}
	else if(strcmp(cmd_array[0],"cd")==0){
		mycd();
		return 1;
	}
	else if(strcmp(cmd_array[0],"help")==0){//help
		if(cmd_array[1]!=NULL&&(strcmp(cmd_array[1],">")==0||strcmp(cmd_array[1],">>")==0)){
			myhelp_redirect();
			return 1;
		}
		else{
			myhelp();
			return 1;
		}
	}
	else if(strcmp(cmd_array[0],"exec")==0){
		myexec();
		return 1;
	}
	else if(strcmp(cmd_array[0],"test")==0){
		mytest();
		return 1;
	}
	else if(strcmp(cmd_array[0],"umask")==0){
		myumask();
		return 1;
	}
	else if(strcmp(cmd_array[0],"jobs")==0){
		myjobs();
		return 1;
	}
	else if(strcmp(cmd_array[0],"fg")==0){
		pid_t pid;
		if(cmd_array[1]!=NULL){
			pid=atoi(cmd_array[1]);
		}
		else{
			printf("myshell: fg: no job assigned\n");
			return 1;
		}
		myfg(pid);
		return 1;
	}
	else if(strcmp(cmd_array[0],"bg")==0){
		pid_t pid;
		if(cmd_array[1]!=NULL){
			pid=atoi(cmd_array[1]);
		}
		else{
			printf("myshell: bg: no job assigned\n");
			return 1;
		}
		mybg(pid);
		return 1;
	}
	else if(strcmp(cmd_array[0],"myshell")==0){
		if(cmd_cnt==1){
			printf("myshell: myshell: too few arguments\n");
			return 1;
		}
		else if(cmd_cnt==2){
			mybatch();
			return 1;
		}
		else{
			printf("myshell: myshell: too many arguments\n");
			return 1;
		}
	}
	else if(strcmp(cmd_array[0],"dir")==0){//dir
		if(cmd_array[1]!=NULL&&(strcmp(cmd_array[1],">")==0||strcmp(cmd_array[1],">>")==0)){
			mydir_redirect();
			return 1;
		}
		else{
			mydir();
			return 1;
		}
	}
	else if(strcmp(cmd_array[0],"set")==0){
		printf("myshell: set: not supported currently\n");
		return 1;
	}
	else if(strcmp(cmd_array[0],"unset")==0){
		printf("myshell: unset: not supported currently\n");
		return 1;
	}//I'll try latter
		printf("myshell: shift: not supported currently\n");
		return 1;
	}
	else{
		return 0;
	}
}

int is_pipe(){
	for(int i=1;i<cmd_cnt;i++){//从第二个字符串开始分析
		if(cmd_array[i]!=NULL&&strcmp(cmd_array[i],"|")==0){
			cmd_array[i]=NULL;//把管道符替换成NULL，因为已经不再需要，避免对命令执行造成影响
			return i+1;//返回下一个命令的位置
		}
	}
	return 0;//没有pipe，返回0
}

void do_redirection(){
	//这个函数仅用来实现外部命令的重定向
	//对于：dir, environ, echo, help命令
	//有专门的函数体执行它们的重定向
	for(int i=1;i<cmd_cnt;i++){
		if(cmd_array[i]!=NULL){
			if(strcmp(cmd_array[i],">")==0){//>:重写文件
				int output=open(cmd_array[i+1],O_WRONLY|O_TRUNC|O_CREAT,0666);//必须用O_TRUNC
				dup2(output,1);//把stdout重定向到output
				close(output);
				cmd_array[i]=NULL;//把>替换成NULL
				i++;
				continue;//跳过
			}
			if(strcmp(cmd_array[i],">>")==0){//>>:在文件内容后追加
				int output=open(cmd_array[i+1],O_WRONLY|O_APPEND|O_CREAT,0666);//必须用O_APPEND
				dup2(output,1);//把stdout重定向到output
				close(output);
				cmd_array[i]=NULL;//用NULL代替>>
				i++;
				continue;//跳过
			}
			if(strcmp(cmd_array[i],"<")==0){//<:输入重定向
				int input=open(cmd_array[i+1],O_CREAT|O_RDONLY,0666);
				dup2(input,0);//把stdin重定向到input
				close(input);
				cmd_array[i]=NULL;//用NULL替换<
				i++;
			}
		}
	}
}

