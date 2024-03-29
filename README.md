# yellow-balancer
 1C:Enterprise process balancer
 
При эксплуатации высоконагруженных систем на базе 1С:Предприятие на серверах, имеющих больше 64 процессоров и имеющих более одной группы NUMA столкнулись с неравномерным распределением процессов 1С по группам NUMA. Для начала пробовали силами дежурной смены менять привязку процессов 1С:Предприятие к группам NUMA через Task Manager. Но это было возможно в основном только один раз. При повторной попытке через некоторое время возникала ошибка "Отказано в доступе". Было принято решение, использую API OS Windows, попробовать самим управлять распределением процессов и потоков по группам NUMA. В результате было создано приложение - служба windows.
При запуске программы создаются файл settings.json с настройками по умолчанию

    { 
      "switching_frequency_in_seconds" : 10,
      "cpu_analysis_period_in_seconds" : 60,
      "log_storage_duration_in_hours" : 24,
      "maximum_cpu_value" : 70,
      "delta_cpu_values" : 30,
      "processes" : ["rphost.exe"]
    }

switching_frequency_in_seconds - частота анализа выполнения балансировки, если необходимо (в секундах)
cpu_analysis_period_in_seconds - период скользящего окна загрузки CPU numa групп и USER_TIME процессов (в секундах)
log_storage_duration_in_hours - период хранения логов (в часах)
maximum_cpu_value - максимальное значение CPU любой numa группы, при котором принимается решение о балансировке (в процентах)
delta_cpu_values - разница потребления CPU между самой загруженной numa группой и самой незагруженной, при котором принимается решение о балансировке (в процентах)
processes - процессы, которые необходимо привязывать к numa группам.

Алгоритм балансировки:
1. Скользящим окном (длительность в параметре cpu_analysis_period_in_seconds) собирается загрузка CPU по numa группам. Показания cpu собираются раз в секунду.
2. Периодически (параметр switching_frequency_in_seconds) анализируются процессы, подлежащие балансировке (указанные в processes). По ним собирается потребление USER_TIME. Так же анализируется средняя загрузка CPU по каждой numa группе.
3. Если среднее значение CPU максимально загруженной numa группы превышает значение параметра maximum_cpu_value и разница CPU между самой загруженной numa группой и самой не загруженной превышает значение, указанное в параметре delta_cpu_values, то принимается решение о необходимости балансировки.
4. Процессы, подлежащие балансировке, сортируются по убыванию среднего USER_TIME и привязываются к numa группам по round robin.
