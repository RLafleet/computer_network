#!/bin/bash
# Сборщик SMTP клиента

echo "Компиляция SMTP клиента с поддержкой OpenSSL..."
gcc -Wall -Wextra -std=c11 -O2 -DUSE_OPENSSL -o smtp_client smtp_client.c smtp_utils.c mime_utils.c tls_utils.c -lssl -lcrypto

if [ $? -eq 0 ]; then
    echo "Компиляция успешна!"
    echo "Поддержка TLS/SSL включена"
    echo ""
    echo "Использование: ./smtp_client [опции]"
    echo "Основные опции:"
    echo "  -s, --server СЕРВЕР      Адрес SMTP сервера"
    echo "  -p, --port ПОРТ          Порт SMTP сервера (25, 587)"
    echo "  -f, --from EMAIL         Адрес отправителя"
    echo "  -t, --to EMAIL           Адрес получателя"
    echo "  -j, --subject ТЕМА       Тема письма"
    echo "  -b, --body ТЕКСТ         Текст письма или путь к файлу"
    echo "  -a, --attachment ФАЙЛ    Вложение (можно использовать несколько раз)"
    echo "  -l, --login ИМЯ          Имя пользователя для аутентификации"
    echo "  -w, --password ПАРОЛЬ    Пароль для аутентификации"
    echo "      --tls, --ssl         Использовать TLS/SSL шифрование"
    echo "  -v, --verbose            Подробный вывод"
    echo "  -h, --help               Показать справку"
    echo ""
    echo "Примеры:"
    echo "  ./smtp_client -s smtp.mail.ru -p 25 -f sender@mail.ru -t recipient@mail.ru -j \"Тест\" -b \"Привет\""
    echo "  ./smtp_client -s smtp.gmail.com -p 587 --tls -l user@gmail.com -w \"пароль\" -f user@gmail.com -t recipient@mail.ru -j \"TLS тест\" -b \"Защищенное сообщение\""
else
    echo "Ошибка компиляции!"
    echo "Убедитесь, что установлен libssl-dev: sudo apt-get install libssl-dev"
    exit 1
fi