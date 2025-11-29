#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <string>
#pragma comment(lib, "ws2_32.lib")
#include <Windows.h>

int main() {
    SetConsoleCP(1251);
    SetConsoleOutputCP(1251);

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed\n";
        return 1;
    }

    // Основний цикл
    while (true) {
        std::string query;
        std::cout << "\nВведіть пошукову фразу (E - вихід): ";
        std::getline(std::cin, query);
        if (query == "E" || query == "e") break;

        SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
        if (s == INVALID_SOCKET) { std::cerr << "Socket creation failed\n"; break; }

        sockaddr_in serverAddr{};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(8080);
        serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

        if (connect(s, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            std::cerr << "Connect failed\n";
            closesocket(s);
            break;
        }

        // надсилаємо пошукову фразу
        std::string cmd = query + "\n";
        send(s, cmd.c_str(), (int)cmd.size(), 0);

        // отримуємо список файлів
        char buf[8192];
        int n = recv(s, buf, sizeof(buf)-1, 0);
        if (n > 0) {
            buf[n] = '\0';
            std::string serverReply(buf);
            std::cout << serverReply << std::endl;

            // якщо сервер повернув помилку
            if (serverReply.rfind("ERROR", 0) == 0) {
                closesocket(s);
                continue;
            }

            // якщо сервер надіслав список файлів
            std::string filename;
            std::cout << "Введіть назву файлу для читання: ";
            std::getline(std::cin, filename);
            filename += "\n";
            send(s, filename.c_str(), (int)filename.size(), 0);

            // отримуємо повний текст файлу у циклі
            std::string response;
            int n2;
            while ((n2 = recv(s, buf, sizeof(buf)-1, 0)) > 0) {
                buf[n2] = '\0';
                response += buf;
            }
            std::cout << response << std::endl;
        }
        closesocket(s);
    }

    WSACleanup();
    return 0;
}
