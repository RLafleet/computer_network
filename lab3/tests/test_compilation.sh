#!/bin/bash
# Тест компиляции

echo "=== Тест компиляции ==="
cd /mnt/c/Projects/computer_network/lab3

echo "Очистка предыдущих сборок..."
make -f Makefile.simple clean

echo "Компиляция без поддержки OpenSSL..."
make -f Makefile.simple

if [ $? -eq 0 ]; then
    echo "Компиляция успешна!"
    echo "Проверка исполняемого файла..."
    ls -la smtp_client
else
    echo "Ошибка компиляции!"
    exit 1
fi

echo ""
echo "=== Тест компиляции завершен ==="