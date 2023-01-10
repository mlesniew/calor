#ifndef METRICS_H
#define METRICS_H

#include <prometheus.h>

namespace metrics {

extern Prometheus prometheus;

extern PrometheusGauge heating_demand;

extern PrometheusGauge valve_state;

extern PrometheusGauge zone_state;
extern PrometheusGauge zone_desired_temperature;
extern PrometheusGauge zone_desired_temperature_hysteresis;
extern PrometheusGauge zone_actual_temperature;
extern PrometheusGauge zone_valve_state;

}

#endif
