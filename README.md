

kafka2influx --topic collectd.graphite --broker f013-520-kafka --influxdb 10.1.47.16:8086 --template "hostgroup.host...resource.measurement*" 


curl -G 'http://10.1.47.16:8086/query?pretty=true' --data-urlencode "db=metrics" --data-urlencode "q=SELECT value FROM cpu_load_short"

