Imports graphite encoded metrics (from collectd) from kafka (v0.82+) to a database in influxdb. v0.9+

Create the database before running kafka2influx


./kafka2influx --topic collectd.graphite --broker f013-520-kafka --influxdb 10.1.47.16:8086 --template "hostgroup.host...resource.measurement*" --database metrics

Currently no support for storing kafka consumer offsets (missing support in kafka lib) so consumption starts at end of kafka log.

Increasing the messages in batch to 1000 seems to crash influxdb - this is the reason why we're not starting from beginning of log.



