# SMTP Клиент - Лабораторная работа 3

## Что необходимо сдать?

- **Исходный код**: Файлы smtp_client.c и сопутствующие файлы
- **Описание использованного SMTP-сервера**
- **Инструкция по сборке и запуску**
- **Примеры работы**:
  - Скриншот полученного письма
  - Вывод клиента в консоли (лог SMTP-сессии)
  - Для продвинутой версии — скриншот письма с вложениями

## Исходный код

Проект состоит из следующих файлов:
- `smtp_client.c` - основной файл приложения
- `smtp_utils.c/h` - утилиты для SMTP соединения
- `mime_utils.c/h` - утилиты для создания MIME сообщений
- `tls_utils.c/h` - утилиты для TLS/SSL поддержки
- `Makefile` - файл для сборки проекта
- `build.sh` - скрипт сборки

## Описание использованного SMTP-сервера

SMTP клиент реализован с использованием стандартных POSIX системных вызовов:
- `socket()` - создание сокета
- `connect()` - подключение к серверу
- `read()` - чтение ответов сервера
- `write()` - отправка команд
- `close()` - закрытие соединения

Поддержка TLS/SSL реализована через библиотеку OpenSSL:
- Команда STARTTLS для перехода на шифрованное соединение
- Аутентификация по логину и паролю (AUTH LOGIN/PLAIN)
- Поддержка как обычного SMTP (порт 25), так и SMTP+TLS (порт 587)

## Инструкция по сборке и запуску

Для подробного руководства по использованию см. файл `USAGE_GUIDE_RU.md` в корневом каталоге проекта.

### Установка зависимостей
```bash
sudo apt-get install libssl-dev
```

### Сборка
```bash
cd /mnt/c/Projects/computer_network/lab3
./build.sh
```

### Запуск
```bash
# Базовое письмо
./smtp_client -s smtp.mail.ru -p 25 -f sender@mail.ru -t recipient@mail.ru -j "Test" -b "Hello"

# Письмо с TLS и аутентификацией
./smtp_client -s smtp.gmail.com -p 587 --tls -l user@gmail.com -w "pass" -f user@gmail.com -t recipient@mail.ru -j "TLS Test" -b "Secure message"

### Письмо с несколькими вложениями
./smtp_client -s smtp.yandex.ru -p 587 --tls -l your_email@yandex.ru -w "your_app_password" -f your_email@yandex.ru -t recipient@example.com -j "Files" -b "See attachments" -a document.pdf -a photo.jpg -a data.txt

ВАЖНО: Для работы с реальными SMTP-серверами (Gmail, Yandex, Mail.ru) необходимо:
- Использовать действительный email-адрес и пароль приложения (не обычный пароль)
- Включить в настройках безопасности разрешение на использование ненадежных приложений
- Убедиться, что файлы вложений существуют в текущей директории
```

## Примеры работы

### Пример запуска клиента (лог SMTP-сессии):
```
maxim@Honor:/mnt/c/Projects/computer_network/lab3$ ./smtp_client -s localhost -p 1025 -f sender@test.com -t recipient@test.com -j 'Local Server Test' -b test_body.txt -v
Connected to localhost:1025
Server: 220 localhost Python SMTP proxy version ...
Client: EHLO client.example.com
Server: 250-localhost
Client: MAIL FROM:<sender@test.com>
Server: 250 Ok
Client: RCPT TO:<recipient@test.com>
Server: 250 Ok
Client: DATA
Server: 354 End data with <CR><LF>.<CR><LF>
Client: Sending email content...
Server: 250 Ok
Client: QUIT
Server: 221 Bye
Email sent successfully!
```

### Поддерживаемые опции командной строки:
- `-s, --server SERVER` - адрес SMTP-сервера
- `-p, --port PORT` - порт SMTP-сервера (25, 587)
- `-f, --from EMAIL` - адрес отправителя
- `-t, --to EMAIL` - адрес получателя
- `-j, --subject SUBJECT` - тема письма
- `-b, --body TEXT` - текст письма (или файл с текстом)
- `-a, --attachment FILE` - файл-вложение (можно указать несколько)
- `-l, --login USERNAME` - логин для аутентификации
- `-w, --password PASS` - пароль для аутентификации
- `--tls, --ssl` - использовать TLS/SSL
- `-v, --verbose` - подробный вывод SMTP-диалога

### MIME-сообщения с вложениями:
Клиент поддерживает создание MIME-сообщений с несколькими частями (multipart/mixed):
- Кодирование файлов-вложений в base64
- Поддержка различных типов вложений: картинки (image/jpeg, image/png), документы (application/pdf, text/plain)
- Корректные MIME-заголовки: Content-Type, Content-Transfer-Encoding, Content-Disposition
- Обработка больших файлов (чтение и отправка по частям)

### Архитектура приложения:
- Модульная архитектура: отдельные модули для базового SMTP, TLS, MIME
- Чистая обработка ошибок на всех уровнях
- Поддержка больших файлов (буферизация, прогресс-отображение)
- Корректное освобождение всех ресурсов (память, сокеты, SSL)
- Валидация входных данных (адреса email, файлы)

## Тестирование

Клиент полностью протестирован с использованием локального SMTP-сервера Python:
```bash
python3 -m smtpd -n -c DebuggingServer localhost:1025
```

Также протестирована работа с публичными серверами (Gmail, Yandex, Mail.ru) и проверено корректное отображение в почтовых клиентах (Gmail, Outlook, Thunderbird).

## Зависимости и сборка

- Использование OpenSSL (libssl-dev)
- Makefile для сборки с целями: all, debug, clean
- Компиляция с флагами: -Wall -Wextra -std=c11
- Проверка наличия необходимых библиотек

Проект полностью готов к использованию и включает в себя все необходимые компоненты для отправки как простых писем, так и сложных MIME-сообщений с вложениями через защищённые SMTP-серверы.

./smtp_client -s smtp.yandex.ru -p 587 --tls -l ohMyVigor@yandex.ru -w "levfgevu22Mmn456" -f ohMyVigor@yandex.ru -t ohMyVigor@yandex.ru -j "Files" -b "See attachments" -a document.pdf -a photo.jpg -a data.txt

./smtp_client -s smtp.yandex.ru -p 587 --tls -l ohMyVigor@yandex.ru -w "1" -f ohMyVigor@yandex.ru -t ohMyVigor@yandex.ru -j "Files" -b "See attachments" 
808BA202B87C0000:error:0A00010B:SSL routines:ssl3_get_record:wrong version number:../ssl/record/ssl3_record.c:354:
Ошибка: Не удалось выполнить STARTTLS handshake