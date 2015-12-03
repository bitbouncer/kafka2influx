

./kafka2influx --topic collectd.graphite --broker f013-520-kafka --influxdb 10.1.47.16:8086 --template "hostgroup.host...resource.measurement*" --database metrics

