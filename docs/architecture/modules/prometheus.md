Module - Prometheus
===================

[Prometheus](https://prometheus.io/) is a well known and widely used monitoring and time-series database, often used in
combination with [Grafana](https://grafana.com/).

cachegrand provides a module to expose metrics for Prometheus via an HTTP endpoint that can be configured using the
config file.

## Configuration

To enable the module is enough to add the module configuration to the `modules` section in the config file:
```yaml
    - type: prometheus
      timeout:
        read_ms: 10000
        write_ms: 10000
      bindings:
        - host: 127.0.0.1
          port: 9090
```

The module currently doesn't provide any sort of authentication or https support.

### Metrics

The metrics are exposed on the `/metrics` endpoint.

The available metrics are:

| Name                                               | Description                                   | Type    |
|----------------------------------------------------|-----------------------------------------------|---------|
| cachegrand_network_total_received_packets          | Total amount of received packets              | Counter |
| cachegrand_network_total_received_data             | Total amount of received data (in bytes)      | Counter |
| cachegrand_network_total_sent_packets              | Total amount of sent packets                  | Counter |
| cachegrand_network_total_sent_data                 | Total amount of sent data                     | Counter |
| cachegrand_network_total_accepted_connections      | Total amount of accepted connections          | Counter |
| cachegrand_network_total_active_connections        | Total amount of active connections            | Counter |
| cachegrand_storage_total_written_data              | Total amount of data written to disk          | Counter |
| cachegrand_storage_total_write_iops                | Total amount of write IOPS                    | Counter |
| cachegrand_storage_total_read_data                 | Total amount of read data from disk           | Counter |
| cachegrand_storage_total_read_iops                 | Total amount of read IOPS                     | Counter |
| cachegrand_storage_total_open_files                | Total amount of open files                    | Counter |
| cachegrand_network_per_minute_received_packets     | Per minute amount of received packets         | Counter |
| cachegrand_network_per_minute_received_data        | Per minute amount of received data (in bytes) | Counter |
| cachegrand_network_per_minute_sent_packets         | Per minute amount of sent packets             | Counter |
| cachegrand_network_per_minute_sent_data            | Per minute amount of sent data                | Counter |
| cachegrand_network_per_minute_accepted_connections | Per minute amount of accepted connections     | Counter |
| cachegrand_storage_per_minute_written_data         | Per minute amount of data written to disk     | Counter |
| cachegrand_storage_per_minute_write_iops           | Per minute amount of write IOPS               | Counter |
| cachegrand_storage_per_minute_read_data            | Per minute amount of read data from disk      | Counter |
| cachegrand_storage_per_minute_read_iops            | Per minute amount of read IOPS                | Counter |
| cachegrand_uptime                                  | Uptime in seconds                             | Counter |

### Metrics labels

It is possible to tag the metrics with any amount of arbitrary labels using environment variables.

If one or more environment variables prefixed with `CACHEGRAND_METRIC_ENV_` exist then these will be parsed by
cachegrand and the suffix will be converted to lowercase and used as name of the label and the value of the environment
variable will be used as value.
