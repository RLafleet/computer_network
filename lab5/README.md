# Лабораторная работа 5 — RDTP (GBN/SR)

## Сборка

```bash
make
```

## Использование

Отправитель:

```bash
./rdt_sender -alg GBN -d -loss 10 -delay 50 -w 5 receiver_host 8080 file.txt
./rdt_sender -alg SR -d -loss 5 -delay 20 -w 8 receiver_host 8080 file.txt
```

Получатель:

```bash
./rdt_receiver -alg GBN -d -loss 10 -delay 50 -w 5 8080 received.txt
./rdt_receiver -alg SR -d -loss 5 -delay 20 -w 8 8080 received.txt
```

## Опции

- `-alg GBN|SR` — выбор алгоритма (обязательно)
- `-d` — режим отладки (подробные события)
- `-loss pct` — вероятность потери пакета/ACK в процентах
- `-delay ms` — максимальная задержка (джиттер), 0..ms
- `-corrupt pct` — вероятность повреждения пакета (ошибка CRC)
- `-w окно` — размер окна (в пакетах)
- `-cc` — адаптивное окно (медленный старт + избегание перегрузок)
- `-rttlog файл` — запись RTT-логов (sender)
- `-cwndlog файл` — запись изменений окна (sender, при `-cc`)

## Формат RTT/окна

- RTT лог: `seq rtt_ms srtt_ms rto_ms`
- CWND лог: `timestamp_ms cwnd ssthresh event`

## Замечания

- Все сообщения и логи выводятся на русском языке.
- Протокол использует UDP и встроенную симуляцию потерь/задержек.
- Передача начинается с пакета `SYN` (размер файла), завершается `FIN`.
