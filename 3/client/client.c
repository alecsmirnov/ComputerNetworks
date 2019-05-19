#define _CRT_SECURE_NO_WARNINGS
#include "client.h"
#include "..\..\clientServerSettings.h"

// Получения адреса хоста по домену
static void hostInit() {
	LPHOSTENT host_ent = gethostbyname("localhost");
	errorCheck(host_ent, NULL, "unable to connect gethostbyname");
}

// Получить клиентский сокет
static SOCKET getClientSocket() {
	SOCKET client_sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	errorCheck(client_sock, SOCKET_ERROR, "unable to create client socket");

	return client_sock;
}

// Подключиться к серверу по указанному IP
static void serverConnect(SOCKET client_sock, char server_ip[IP_LEN]) {
	SOCKADDR_IN server_info;
	server_info.sin_family = PF_INET;
	server_info.sin_addr.S_un.S_addr = inet_addr(server_ip);
	server_info.sin_port = htons(SERVER_PORT);

	int ret_val = connect(client_sock, (LPSOCKADDR)&server_info, sizeof(server_info));
	errorCheck(ret_val, SOCKET_ERROR, "unable to connect");
}

// Функция потока приёма сообщений
static DWORD WINAPI receiveThread(LPVOID client_sock) {
	char msg[MSG_SIZE];

	// Принимать сообщения до тех пор пока не будет получен ответ:
	// Сервер прекратил работу, Сервер перегружен, Соединение завершено
	do {
		msgRecv(*(SOCKET*)client_sock, msg, sizeof(msg));
		printf("%s\n", msg);
	} while (strcmp(msg, "Server shutdown!")  != EQUAL &&
			 strcmp(msg, "Server overflow!")  != EQUAL &&
			 strcmp(msg, "Connection close!") != EQUAL);

	ExitThread(EXIT_SUCCESS);
}

// Функция потока отправки сообщений
static DWORD WINAPI sendThread(LPVOID client_sock) {
	char msg[MSG_SIZE];

	// Возможность отправлять сообщения до тех пор,
	// пока не будет вызвана команда выхода из чата (/exit)
	while (strcmp(msg, "/exit") != EQUAL) {
		readString(msg, MSG_SIZE);

		if (strcmp(msg, "") != EQUAL)
			msgSend(*(SOCKET*)client_sock, msg, sizeof(msg));
	}

	ExitThread(EXIT_SUCCESS);
}

// Запустить клиент
void startClient() {
	winSocketInit();
	hostInit();

	SOCKET client_sock = getClientSocket();

	// Ввести IP адрес сервера
	char server_ip[IP_LEN];
	printf("Enter server IP: ");
	readString(server_ip, IP_LEN);

	// Подключиться к серверу
	serverConnect(client_sock, server_ip);

	char msg[MSG_SIZE];
	msgRecv(client_sock, msg, sizeof(msg));

	// Вывести сообщение о статусе подключения к серверу
	printf("%s\n", msg);

	// Если соединение прошло успешно
	if (strcmp(msg, "Connection made successfully!") == EQUAL) {
		// Ввести имя пользователя
		char name[NAME_LEN];
		printf("Enter user name: ");
		readString(name, NAME_LEN);

		// Отправить имя на сервер
		msgSend(client_sock, name, sizeof(name));

		// Начать отправку и приём сообщений в отдельных потоках
		HANDLE recv_thread = CreateThread(NULL, DEFAULT_STACK_SIZE, receiveThread, &client_sock, IMMEDIATE_START, NULL);
		HANDLE send_thread = CreateThread(NULL, DEFAULT_STACK_SIZE, sendThread, &client_sock, IMMEDIATE_START, NULL);

		// Если сервер завершил работу -- закончить отправку сообщений
		if (WaitForSingleObject(recv_thread, INFINITE) == EXIT_SUCCESS)
			TerminateThread(send_thread, EXIT_SUCCESS);

		CloseHandle(recv_thread);
		CloseHandle(send_thread);
	}

	closesocket(client_sock);
}