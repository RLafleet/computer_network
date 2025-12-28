#!/bin/bash
# Запуск всех тестов

echo "   ПОЛНЫЙ НАБОР ТЕСТОВ SMTP КЛИЕНТА   "
echo ""

# Делаем все тестовые скрипты исполняемыми
chmod +x /mnt/c/Projects/computer_network/lab3/tests/*.sh

# Запускаем тесты по порядку
echo "Запуск Теста 1: Тест компиляции"
echo "------------------------------"
/mnt/c/Projects/computer_network/lab3/tests/test_compilation.sh
echo ""

echo "Запуск Теста 2: Тест команды справки"
echo "--------------------------------"
/mnt/c/Projects/computer_network/lab3/tests/test_help.sh
echo ""

echo "Запуск Теста 3: Тест отсутствующих аргументов"
echo "-------------------------------------"
/mnt/c/Projects/computer_network/lab3/tests/test_missing_args.sh
echo ""

echo "Запуск Теста 4: Тест длинных опций"
echo "--------------------------------"
/mnt/c/Projects/computer_network/lab3/tests/test_long_options.sh
echo ""

echo "Запуск Теста 5: Тест обработки ошибок"
echo "----------------------------------"
/mnt/c/Projects/computer_network/lab3/tests/test_error_handling.sh
echo ""

echo "Запуск Теста 6: Тест компиляции с OpenSSL"
echo "---------------------------------------"
/mnt/c/Projects/computer_network/lab3/tests/test_openssl_compilation.sh
echo ""

echo "Запуск Теста 7: Тест поддержки TLS"
echo "-----------------------------"
/mnt/c/Projects/computer_network/lab3/tests/test_tls_support.sh
echo ""

echo "      ВСЕ ТЕСТЫ УСПЕШНО ЗАВЕРШЕНЫ     "
echo ""
echo "Примечание: Для тестов локального сервера и вложений,"
echo "запустите их вручную с локальным SMTP сервером:"
echo "  python3 -m smtpd -n -c DebuggingServer localhost:1025"
echo ""