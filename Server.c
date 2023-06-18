#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include "Internal.h"
#include "GenericDoubleList.h"
#include "ListFunc.h"
#include "ListItr.h"


#define ADDR "127.0.0.1"
#define PORT 1234
#define BACK_LOG 1000
#define MEGIC_NUMBER 123654789
#define FAIL -1
#define SUCCESS 0
#define NO_BLOCK -2


typedef struct sockaddr_in SocAdd;

typedef struct Server
{
	List *m_list;
	int m_sock;
	size_t m_megicNumber;
	int m_countClient;
	fd_set m_master;
	fd_set m_temp;
	int m_activity;

	
}Serv;

static int NoBlock( int *_sock);

Serv* CreateServer()
{
	SocAdd serverAdd;
	Serv *server;
	int sock = socket(AF_INET, SOCK_STREAM, 0);
	int optval = 1  ;

	if( sock < 0 ) 
	{
		perror("socket failed");
		return NULL;
	}
	memset(&serverAdd, 0, sizeof(serverAdd));
	serverAdd.sin_family = AF_INET;
	serverAdd.sin_addr.s_addr = INADDR_ANY;
	serverAdd.sin_port = htons(PORT);

	if( setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,&optval, sizeof(optval)) < 0) 
	{
		perror("reuse failed");
		return NULL;
	}	
	if( bind(sock, (struct sockaddr  *) &serverAdd, sizeof(serverAdd)) < 0 )
	{
		perror("bind failed");
		return NULL;
	}
	if ( listen(sock, BACK_LOG) < 0) 
	{
		perror("listen failed");
		return NULL;
	}
	if(server->m_countClient >= BACK_LOG)
	{
		close(sock);
		return NULL;
	}
	if( NoBlock( &sock ) < 0 )
	{
		close(sock);
		return NULL;
	}
	if((server = (Serv*)malloc(sizeof(Serv))) == NULL)
	{
		return NULL;
	}
	if((server->m_list = ListCreate()) == NULL)
	{
		free(server);
		return NULL;
	}
	FD_ZERO(&server->m_master);
	FD_SET(sock, &server->m_master);
	server->m_sock = sock;
	server->m_countClient = 0;
	return server;
}

int Connect(Serv* _server)
{
	unsigned int clientAdd_len;
	int client_sock, flags;
	int *  client;
	SocAdd clientAdd;
	
	clientAdd_len = sizeof(clientAdd);
	client_sock = accept(_server->m_sock, (struct sockaddr  *) &clientAdd, &clientAdd_len);
	if( client_sock < 0)
	{
		if( errno != EAGAIN && errno != EWOULDBLOCK )
		{
			perror("accept failed");
			close(client_sock);	
			return FAIL;
		}
		else
		{
			return SUCCESS;
		}
	}
	if( NoBlock( &client_sock ) < 0)
	{
		close(client_sock);
		return FAIL;
	}
	if((client = (int*)malloc(sizeof(int))) == NULL)
	{
		perror("malloc failed");
		close(client_sock);
		return FAIL;
	}
	*client = client_sock;
	if(ListPushHead(_server->m_list, client) != LIST_SUCCESS) 
	{
		free(client);
		return FAIL;
	}
	_server->m_countClient++;
	FD_SET(client_sock, &_server->m_master);
	return client_sock;
}

int Recvfrom(int _sock)
{
	char buffer[64];
	int readBytes;
	readBytes = recv( _sock, buffer, sizeof(buffer), 0 );
	if( readBytes < 0)
	{
		if( errno != EAGAIN && errno != EWOULDBLOCK )
		{
			perror("recv failed");
			return FAIL;
		}
		else
		{
			return NO_BLOCK;
		}
	}
	else if( readBytes == 0 )
	{
		return FAIL;
	}
	buffer[readBytes] = '\0';
	printf("Client: %s \n", buffer);
	return SUCCESS;
}

int Send(int _sock)
{
	int sentBytes;
	char data[] = "Wellcom from server";
	sentBytes = send(_sock, data, strlen(data) , 0);
	if (sentBytes < 0) 
	{
		perror("sendto failed");
		return FAIL;
	}
	/*printf("Message sent.\n");*/
	return SUCCESS;
}

static int CheckCurrClient(int  _sock)
{
	int res;
	if((res  = Recvfrom(_sock)) < 0)
	{
		return res;
	}
	if((res = Send(_sock)) < 0)
	{
		return res;
	}
	return SUCCESS;
}

int ForCheckClients(Serv* _server)
{
	ListItr begin, end, temp;
	int *cliSock;
	
	begin = ListItrBegin(_server->m_list);
	end = ListItrEnd(_server->m_list);
	while(begin != end && _server->m_activity > 0)
	{
		cliSock = (int*)ListItrGet(begin);
		if(FD_ISSET(*cliSock, &_server->m_temp))
		{
			_server->m_activity--;
			if(CheckCurrClient(*cliSock) == FAIL )
			{
				close(*cliSock);
				FD_CLR(*cliSock, &_server->m_master);
				free(cliSock);
				temp = begin;
				begin = ListItrNext(begin);
				ListItrRemove(temp);
				_server->m_countClient--;
				continue;
			}
		}
		begin = ListItrNext(begin);
		
	}
	return SUCCESS;
}

int ServerStart(Serv* _server)
{
	int activity;
	if(_server == NULL)
	{
		return FAIL;
	}
	while(1)
	{
		_server->m_temp = _server->m_master;
		activity = select(1024, &_server->m_temp, NULL, NULL, NULL); 
		if(activity < 0)
		{
			return FAIL;
		}
		else if(activity > 0)
		{
			_server->m_activity = activity;
			if(FD_ISSET(_server->m_sock, &_server->m_temp))
			{
				Connect(_server);
				_server->m_activity--;
			}
			if(_server->m_activity > 0)
			{
				ForCheckClients(_server);
			}
		}
	}
	return SUCCESS;
}
void Destroy( void *_sock)
{
	close(*(int*)_sock);
	free(_sock);
}
void ServerDestroy(Serv* _server)
{
	if(_server == NULL || _server->m_megicNumber != MEGIC_NUMBER)
	{
		return;
	}
	_server->m_megicNumber = 0;
	ListDestroy(&_server->m_list, Destroy);
	close(_server->m_sock);
	free(_server);
}


int main()
{
	int sock, i, con;
	Serv* server;
	server = CreateServer();
	ServerStart( server );
	ServerDestroy(server);
	return SUCCESS;
}

static int NoBlock( int *_sock)
{
	int flags;
	if((flags = fcntl(*_sock, F_GETFL)) < 0)
	{
		perror("error at fcntl. F_GETFL.");
		return FAIL;
	}
	if(fcntl(*_sock, F_SETFL, flags | O_NONBLOCK) < 0)
	{
		perror("error at fcntl. F_SETFL.");
		return FAIL;
	}
	return  SUCCESS;
}

