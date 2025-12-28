# Руководство по использованию SMTP клиента

## Содержание
1. [Установка и сборка](#установка-и-сборка)
2. [Локальное тестирование](#локальное-тестирование)
3. [Использование с реальными SMTP серверами](#использование-с-реальными-smtp-серверами)
4. [Настройка почтовых сервисов](#настройка-почтовых-сервисов)
5. [Решение проблем](#решение-проблем)

## Установка и сборка

### 1. Установите зависимости
```bash
sudo apt-get update
sudo apt-get install libssl-dev
```

### 2. Соберите проект
```bash
cd /mnt/c/Projects/computer_network/lab3
./build.sh
```

### 3. Проверьте сборку
```bash
./smtp_client --help
```

## Локальное тестирование

### 1. Запустите локальный SMTP сервер
```bash
python3 -m smtpd -n -c DebuggingServer localhost:1025
```

### 2. В другом терминале отправьте тестовое письмо
```bash
./smtp_client -s localhost -p 1025 -f sender@test.com -t recipient@test.com -j "Тест" -b "Тестовое сообщение" -v
```

### 3. В первом терминале вы увидите:
```
---------- MESSAGE FOLLOWS ----------
From: <sender@test.com>
To: <recipient@test.com>
Subject: Тест

Тестовое сообщение
------------ END MESSAGE ------------
```

## Использование с реальными SMTP серверами

### ВАЖНО: Для работы с реальными серверами (Gmail, Yandex, Mail.ru) нужно:

1. **Использовать приложение-специфичный пароль**
   - Не обычный пароль от почты
   - Создается в настройках безопасности

2. **Включить доступ для менее безопасных приложений**
   - Требуется для SMTP клиентов

3. **Убедиться, что файлы вложений существуют**
   - Если используете опцию -a

## Настройка почтовых сервисов

### Gmail
1. Войдите в Gmail
2. Перейдите в "Управление аккаунтом" → "Безопасность"
3. Включите "Двухэтапную аутентификацию"
4. Создайте "Пароль приложения"
5. Используйте команду:
```bash
./smtp_client -s smtp.gmail.com -p 587 --tls -l your_email@gmail.com -w "your_app_password" -f your_email@gmail.com -t recipient@example.com -j "Тема" -b "Сообщение"
```

### Yandex
1. Войдите в Yandex
2. Перейдите в "Почту" → "Настройки" → "Почтовый клиент"
3. Включите "Пароль для приложений"
4. Создайте пароль для приложения
5. Используйте команду:
```bash
./smtp_client -s smtp.yandex.ru -p 587 --tls -l ohMyVigor@yandex.ru -w "nksuoqhpdzhmuwgk" -f ohMyVigor@yandex.ru -t ohMyVigor@example.com -j "Тема" -b "Сообщение"
```

### Mail.ru
1. Войдите в Mail.ru
2. Перейдите в "Настройки" → "Почтовые клиенты"
3. Включите IMAP/SMTP
4. Используйте команду:
```bash
./smtp_client -s smtp.mail.ru -p 587 --tls -l your_email@mail.ru -w "your_password" -f your_email@mail.ru -t recipient@example.com -j "Тема" -b "Сообщение"
```

## Отправка с вложениями

### Создайте файлы для вложений:
```bash
echo "Тестовый документ" > document.txt
echo "Изображение" > photo.txt  # в реальности используйте реальные изображения
```

### Отправьте письмо с вложениями:
```bash
./smtp_client -s smtp.gmail.com -p 587 --tls -l your_email@gmail.com -w "your_app_password" -f your_email@gmail.com -t recipient@example.com -j "Файлы" -b "Смотрите вложения" -a document.txt -a photo.txt
```

## Решение проблем

### Ошибка: "SSL routines:ssl3_get_record:wrong version number"
- **Причина**: Неправильная аутентификация с реальным SMTP сервером
- **Решение**: Убедитесь, что вы используете пароль приложения, а не обычный пароль

### Ошибка: "TLS support not compiled in"
- **Причина**: Не установлена библиотека OpenSSL
- **Решение**: 
```bash
sudo apt-get install libssl-dev
./build.sh
```

### Ошибка: "Connection refused"
- **Причина**: Неправильный адрес или порт сервера
- **Решение**: Проверьте правильность адреса сервера и номера порта

### Ошибка: "Authentication failed"
- **Причина**: Неправильный логин или пароль
- **Решение**: Убедитесь, что вы используете правильный адрес электронной почты и пароль приложения

## Проверка функциональности

### 1. Проверьте базовую функциональность:
```bash
./smtp_client -s localhost -p 1025 -f test@localhost -t test@localhost -j "Тест" -b "Тест" -v
```

### 2. Проверьте TLS поддержку:
```bash
./smtp_client --help | grep -i tls
```

### 3. Проверьте MIME вложения:
```bash
echo "Тест" > test_file.txt
./smtp_client -s localhost -p 1025 -f test@localhost -t test@localhost -j "Тест" -b "Тест" -a test_file.txt -v
```

## Полезные команды

### Проверка работоспособности:
```bash
# Проверка всех опций
./smtp_client --help

# Подробный вывод (для отладки)
./smtp_client -v -s localhost -p 1025 -f sender@test.com -t recipient@test.com -j "Тест" -b "Тест"

# Отправка с несколькими вложениями
./smtp_client -s smtp.gmail.com -p 587 --tls -l your_email@gmail.com -w "your_app_password" -f your_email@gmail.com -t recipient@example.com -j "Файлы" -b "Смотрите вложения" -a file1.txt -a file2.txt
```

## Заключение

SMTP клиент полностью функционален и готов к использованию. Для тестирования рекомендуется использовать локальный SMTP сервер. Для отправки реальных писем необходимо настроить почтовый сервис и использовать пароли приложений.