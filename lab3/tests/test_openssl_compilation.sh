#!/bin/bash
# Тест компиляции с OpenSSL

echo "=== Тест компиляции с OpenSSL ==="
cd /mnt/c/Projects/computer_network/lab3

echo "Очистка предыдущих сборок..."
make -f Makefile.simple clean

echo "Попытка компиляции с поддержкой OpenSSL..."
make -f Makefile.simple openssl

if [ $? -eq 0 ]; then
    echo "Компиляция с OpenSSL успешна!"
else
    echo "Компиляция с OpenSSL не удалась (ожидается, если библиотеки разработки OpenSSL не установлены)"
fi

echo ""
echo "=== Тест компиляции с OpenSSL завершен ==="