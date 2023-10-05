#include "metrics.h"

namespace metrics {

Prometheus prometheus;

PrometheusGauge heating_demand(prometheus, "heating_demand", "Burner heat demand state");

PrometheusGauge zone_state(prometheus, "zone_state", "Zone state enum");
PrometheusGauge zone_desired_temperature(prometheus, "zone_temperature_desired", "Zone's desired temperature");
PrometheusGauge zone_desired_temperature_hysteresis(prometheus, "zone_temperature_desired_hysteresis",
        "Zone's desired temperature hysteresis");
PrometheusGauge zone_actual_temperature(prometheus, "zone_temperature_actual", "Zone's actual temperature");
PrometheusGauge zone_valve_state(prometheus, "zone_valve_state", "Zone's valve state enum");

}
