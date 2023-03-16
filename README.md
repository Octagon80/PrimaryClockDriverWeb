**Первичные часы с web-интерфейсом**

Статус: не рабочая версия, отладка.

В СССР существовала система часофикации: первичные часы определяли точное время и ежеминутно (ежесекундно) давали импульс всем вторичным часам.

![Вторичные часы Стрела. Вторичные часы как в фильме Довод](https://raw.githubusercontent.com/Octagon80/PrimaryClockDriverWeb/main/clock_secondary_strela.jpg|width=100)

Таким образом, вторичные часы для своей работы требуют наличие первичных. В данном проекте реализуется первичные часы. 
У меня вторичные часы висят высоко и от лени ставить стул, чтобы скорректировать показания вторичных часов, разработал данный проект.

Проект реализован с использованием:
- ESP8266 (Node MCU)
- модуля L298N
- блок питание 24В

Для модуля ESP8266 в данном репозитории представлена программа имеющая функции:
- генерирует минутные импульсы на вторичные часы;
- предосталение web-интерфейса корректировки показания часов.

