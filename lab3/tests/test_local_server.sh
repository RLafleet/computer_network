#!/bin/bash
# Тест подключения к локальному SMTP серверу

echo "=== Тест подключения к локальному SMTP серверу ==="
cd /mnt/c/Projects/computer_network/lab3

echo "Содержимое тестового сообщения для проверки локального сервера" > test_body.txt

echo "Запуск: ./smtp_client -s localhost -p 1025 -f sender@test.com -t recipient@test.com -j 'Тест локального сервера' -b test_body.txt -v"
echo "Ожидается: Подключиться к localhost:1025 и показать подробный SMTP диалог"
echo ""

./smtp_client -s localhost -p 1025 -f sender@test.com -t recipient@test.com -j 'Тест локального сервера' -b test_body.txt -v

echo ""
echo "=== Тест локального сервера завершен ==="