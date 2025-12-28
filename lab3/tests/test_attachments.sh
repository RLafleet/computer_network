#!/bin/bash
# Тест отправки письма с вложениями

echo "=== Тест отправки письма с вложениями ==="
cd /mnt/c/Projects/computer_network/lab3

# Создаем тестовые файлы
echo "Это тестовый документ 1" > test_doc1.txt
echo "Это тестовый документ 2" > test_doc2.txt

echo "Запуск: ./smtp_client -s localhost -p 1025 -f sender@test.com -t recipient@test.com -j 'Тест вложений' -b 'Тестирование вложений' -a test_doc1.txt -a test_doc2.txt -v"
echo "Ожидается: Обработка вложений и отправка MIME multipart сообщения"
echo ""

./smtp_client -s localhost -p 1025 -f sender@test.com -t recipient@test.com -j 'Тест вложений' -b 'Тестирование вложений' -a test_doc1.txt -a test_doc2.txt -v

echo ""
echo "=== Тест вложений завершен ==="