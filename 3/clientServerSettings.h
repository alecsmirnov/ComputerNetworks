#ifndef CLIENTSERVERSETTINGS_H
#define CLIENTSERVERSETTINGS_H

#pragma comment(lib, "Ws2_32.lib")
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <winsock.h>

#define CLIENT_COUNT_MAX 2		// Максимально число пользователей чата

#define SERVER_PORT 2006		// Порт сервера

#define IP_LEN   16				// Длина IP адреса
#define NAME_LEN 32				// Длина имени пользователя
#define MSG_SIZE 512			// Размер сообщения

#define EQUAL 0					// Признак равенства двух строк

#define DEFAULT_PROTOCOL   0	// Стандартного протокола
#define DEFAULT_STACK_SIZE 0	// Стандартный размер стека потока
#define IMMEDIATE_START    0	// Немедленный старт работы потока

// Функция проверки ошибок и вывода результата
#define errorCheck(value, error_value, error_msg) do {	\
	if (value == error_value) {							\
		fprintf(stderr, "Error: %s!\n", error_msg);		\
		WSACleanup();									\
		exit(EXIT_FAILURE);								\
	}													\
} while (false)

// Инициализация WinSock
static inline void winSocketInit() {
	WSADATA wsa_data;
	WSAStartup(MAKEWORD(2, 2), (LPWSADATA)&wsa_data);
}

// Функция отправки сообщения
static inline void msgSend(SOCKET client_sock, void* msg, size_t msg_size) {
	int ret_val = send(client_sock, msg, msg_size, DEFAULT_PROTOCOL);
	errorCheck(ret_val, SOCKET_ERROR, "unable to send");
}

// Функция приёма сообщения
static inline void msgRecv(SOCKET client_sock, void* msg, size_t msg_size) {
	int ret_val = recv(client_sock, msg, msg_size, DEFAULT_PROTOCOL);
	errorCheck(ret_val, SOCKET_ERROR, "unable to recv");
}

// Функция для ввода строк, содержащих символы пробела.
// Окончанием ввода строки интерпретируется символ конца строки '\n'
static inline void readString(char* str, size_t str_size) {
	fgets(str, str_size, stdin);

	if (0 < strlen(str) && str[strlen(str) - 1] == '\n')
		str[strlen(str) - 1] = '\0';
}

#endif