#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "server.h"
#include "..\..\clientServerSettings.h"

#include <stdint.h>

#define ONE_SECOND 1000			// Одна секунда в миллисекундах

// Информация о клиенте 
typedef struct ClientInfo {
	char name[NAME_LEN];		// Имя

	SOCKET sock;				// Сокет клинета
	SOCKADDR_IN addr;			// Адрес подключения
	HANDLE handle;				// Хэндл потока для работы с клиентом
} ClientInfo;

// Информация о сервере
typedef struct ServerInfo {
	SOCKET serv_sock;						// Серверный сокет

	ClientInfo client[CLIENT_COUNT_MAX];	// Клиентские сокеты
	uint8_t client_count;					// Кол-во клиентнов

	bool active;							// Статус работы сервера
} ServerInfo;

// Информация для работы с потоком
typedef struct ThreadInfo {
	ServerInfo* serv;						// Информация о сервере
	uint8_t client_num;						// Номер клиента
} ThreadInfo;

// Получить сокет сервера
static SOCKET getServerSocket() {
	SOCKET serv_sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	errorCheck(serv_sock, SOCKET_ERROR, "unable to create server socket");

	// Адрес и порт сокета
	SOCKADDR_IN server_info;						// Параметры адреса для серверного сокета
	server_info.sin_addr.s_addr = INADDR_ANY;		// Привязка ко всем локальным адресам
	server_info.sin_family = PF_INET;				// Протокол / тип адреса, номера портов ТCP, UDP
	server_info.sin_port = htons(SERVER_PORT);		// Порт и его перевод в TCP представление
	
	// Привязать сокет к адресу и порту сервера
	int ret_val = bind(serv_sock, (LPSOCKADDR)&server_info, sizeof(server_info));
	errorCheck(ret_val, SOCKET_ERROR, "unable to bind");

	char host_name[NAME_LEN];							// Имя хоста

	if (!gethostname(host_name, NAME_LEN)) {			// Получаем имя хоста
		LPHOSTENT host_ent = gethostbyname(host_name);	// Получаем адрес сервера
		if (host_ent) {
			char host_ip[IP_LEN];						// IP хоста
			strcpy(host_ip, inet_ntoa(*((IN_ADDR*)host_ent->h_addr_list[0])));

			printf("Server started at %s, port %d\n", host_ip, htons(server_info.sin_port));
		}
	}

	return serv_sock;
}

// Инициализировать информацию о сервере
static void serverInfoInit(ServerInfo* serv_info) {
	serv_info->client_count = 0;
	serv_info->active = true;

	serv_info->serv_sock = getServerSocket();

	for (uint8_t i = 0; i != CLIENT_COUNT_MAX; ++i)
		serv_info->client[i].sock = SOCKET_ERROR;
}

// Сформировать и отправить ответ для нового клиента
static void responceToNewClient(ClientInfo* client) {
	char msg[MSG_SIZE];

	strcpy(msg, "Connection made successfully!");
	msgSend(client->sock, msg, sizeof(msg));

	msgRecv(client->sock, client->name, sizeof(client->name));

	printf("New connection from: %s, port %d\n", inet_ntoa(client->addr.sin_addr), htons(client->addr.sin_port));
	printf("User name: %s\n", client->name);
}

// Сформировать и отправить ответ для постороннего
// (в случае отсутствия свободных на сервере мест)
static void responceToOutsider(ClientInfo* client) {
	char msg[MSG_SIZE];

	strcpy(msg, "Server overflow!");
	msgSend(client->sock, msg, sizeof(msg));
}

// Получить входящее подключение (информацию о новом клиенте)
static ClientInfo getClientConnection(SOCKET serv_sock) {
	ClientInfo client;

	// Указываем размер очереди и готовимся к подключениям
	int ret_val = listen(serv_sock, CLIENT_COUNT_MAX);
	errorCheck(ret_val, SOCKET_ERROR, "unable to listen");

	// Извлекаем запрос из очереди
	int addr_len = sizeof(client.addr);
	client.sock = accept(serv_sock, (struct sockaddr*)&client.addr, &addr_len);
	errorCheck(client.sock, SOCKET_ERROR, "unable to accept");

	return client;
}

// Функция потока обмена сообщениями с клиентом
static DWORD WINAPI clientCommunicationThread(LPVOID thread_info) {
	ThreadInfo* info = (ThreadInfo*)thread_info;	// Информация для работы
	uint8_t client_num = info->client_num;			// Номер клиента

	char msg[MSG_SIZE];
	char name_msg[MSG_SIZE];

	do {
		// Принимаем сообщение от клиента
		msgRecv(info->serv->client[client_num].sock, msg, sizeof(msg));
		printf("%s: %s\n", info->serv->client[client_num].name, msg);

		// Если сообщение не является командой выхода из чата --
		// Оправить остальным пользователям Имя клиента и Сообщение
		if (strcmp(msg, "/exit") != EQUAL)
			for (uint8_t i = 0; i != CLIENT_COUNT_MAX; ++i)
				if (client_num != i && info->serv->client[i].sock != SOCKET_ERROR) {
					sprintf(name_msg, "%s: %s", info->serv->client[client_num].name, msg);
					msgSend(info->serv->client[i].sock, name_msg, sizeof(name_msg));
				}
	} while (strcmp(msg, "/exit") != EQUAL);

	// Если клиент вышел из чата -- 
	// отправить остальным пользователям соответствующее сообщение
	for (uint8_t i = 0; i != CLIENT_COUNT_MAX; ++i)
		if (client_num != i && info->serv->client[i].sock != SOCKET_ERROR) {
			sprintf(msg, "User %s has left the chat!", info->serv->client[client_num].name);
			msgSend(info->serv->client[i].sock, msg, sizeof(msg));
		}

	// Отправить пользователю сигнал о закрытом подключении
	strcpy(msg, "Connection close!");
	msgSend(info->serv->client[client_num].sock, msg, sizeof(msg));

	ExitThread(EXIT_SUCCESS);
}

// Функция потока ожидания новых подключений
static DWORD WINAPI listenThread(LPVOID serv_info) {
	ServerInfo* info = (ServerInfo*)serv_info;

	char msg[MSG_SIZE];

	// Пока статус работы сервера -- активен
	while (info->active) {
		// Ожидать и получить новое подключение
		ClientInfo new_connection = getClientConnection(info->serv_sock);

		// Если кол-во клиентов меньше максимального числа пользователей
		if (info->client_count < CLIENT_COUNT_MAX) {
			// Отправить ответ для нового пользователя
			responceToNewClient(&new_connection);

			// Найти свободное место в списке пользователей
			uint8_t client_num = info->client_count;
			bool search = true;
			for (uint8_t i = 0; i != CLIENT_COUNT_MAX && search; ++i)
				if (info->client[i].sock == SOCKET_ERROR) {
					client_num = i;
					search = false;
				}

			// Записать пользователя в список
			info->client[client_num] = new_connection;

			// Отправить остальным пользователям сообщение о новом пользователе
			for (uint8_t i = 0; i != CLIENT_COUNT_MAX; ++i)
				if (client_num != i && info->client[i].sock != SOCKET_ERROR) {
					sprintf(msg, "User %s has joined the chat!", info->client[client_num].name);
					msgSend(info->client[i].sock, msg, sizeof(msg));
				}

			// Увеличить кол-вое пользователей
			++info->client_count;

			// Начать приём сообщений от нового пользователя в отдельном потоке
			ThreadInfo thread_info = {info, client_num};
			info->client[client_num].handle = CreateThread(NULL, DEFAULT_STACK_SIZE,
				clientCommunicationThread, &thread_info, IMMEDIATE_START, NULL);
		}
		else {
			// Если на сервере нет свободных мест --
			// отправить соответствующее сообщение пользователю
			responceToOutsider(&new_connection);
			closesocket(new_connection.sock);
		}
	}

	ExitThread(EXIT_SUCCESS);
}

// Функция потока завершения работы пользователей
static DWORD WINAPI clientCommunicationWaitThread(LPVOID serv_info) {
	ServerInfo* info = (ServerInfo*)serv_info;

	// Пока статус работы сервера -- активен
	while (info->active)
		for (uint8_t i = 0; i != CLIENT_COUNT_MAX; ++i)
			if (info->client[i].sock != SOCKET_ERROR)
				if (WaitForSingleObject(info->client[i].handle, ONE_SECOND) == EXIT_SUCCESS) {
					// Если клиент завершил свою работу --
					// очистить место в списке пользователей и уменьшить кол-во пользователей
					info->client[i].sock = SOCKET_ERROR;
					--info->client_count;
				}

	ExitThread(EXIT_SUCCESS);
}

// Запустить сервер
void startServer() {
	winSocketInit();

	// Инициализация информации о сервере
	ServerInfo serv_info;
	serverInfoInit(&serv_info);

	// Начать ожидание входящих подключений в отдельном потоке
	HANDLE listen_thread = CreateThread(NULL, DEFAULT_STACK_SIZE, 
						   listenThread, &serv_info, IMMEDIATE_START, NULL);
	// Начать ожидание завершения работы клиентов в отдельном потоке
	HANDLE client_wait_thread = CreateThread(NULL, DEFAULT_STACK_SIZE, 
								clientCommunicationWaitThread, &serv_info, IMMEDIATE_START, NULL);

	char msg[MSG_SIZE];

	// Пока статус работы сервера -- активен
	// Ожидать ввода команды для завершения работы сервера (/close)
	while (serv_info.active) {
		char command[MSG_SIZE];
		readString(command, MSG_SIZE);

		if (strcmp(command, "/close") == EQUAL) {
			for (uint8_t i = 0; i != CLIENT_COUNT_MAX; ++i)
				if (serv_info.client[i].sock != SOCKET_ERROR) {
					strcpy(msg, "Server shutdown!");
					msgSend(serv_info.client[i].sock, msg, sizeof(msg));
				}

			serv_info.active = false;
		}
	}

	// Завершить работу потока ожидания входящих сообщений
	TerminateThread(listen_thread, EXIT_SUCCESS);

	// Завершить работу потоков приёма сообщений от пользователей
	for (uint8_t i = 0; i != CLIENT_COUNT_MAX; ++i)
		if (serv_info.client[i].sock != SOCKET_ERROR) {
			TerminateThread(serv_info.client[i].handle, EXIT_SUCCESS);
			CloseHandle(serv_info.client[i].handle);
		}

	// Завершить работу потока ожидания завершения работы клиента
	TerminateThread(client_wait_thread, EXIT_SUCCESS);

	CloseHandle(listen_thread);
	CloseHandle(client_wait_thread);

	closesocket(serv_info.serv_sock);
	for (uint8_t i = 0; i != CLIENT_COUNT_MAX; ++i)
		if (serv_info.client[i].sock != SOCKET_ERROR)
			closesocket(serv_info.client[i].sock);

	WSACleanup();
}