# ros2_fault_injection_rviz

`ros2_fault_injection_rviz` provides an RViz panel for viewing and controlling faults from the
`ros2_fault_injection` framework.

The panel is an RViz plugin, not a standalone node. After the package is built and sourced, it appears
in RViz as:

```text
ros2_fault_injection_rviz/FaultInjectionPanel
```

## Features

- Lists configured faults from `/fault_injection/get_fault_status`
- Shows each fault's:
  - fault id
  - injector id
  - active/inactive state
  - formatted details
- Refreshes fault status automatically
- Activates and deactivates faults using `/fault_injection/set_fault_state`
- Reloads the active scenario using `/fault_injection/reload_scenario`
- Displays recent fault events from `/fault_injection/events`
- Displays assertion results from `/fault_injection/assertion_events`
- Edits runtime fault config using `/fault_injection/set_fault_config`
- Loads valid config keys for the selected fault from `/fault_injection/get_fault_schema`
- Loads current runtime config values from `/fault_injection/get_fault_config`

## Expected ROS Interfaces

The core `fault_injector_node` must be running before the panel can populate.

Services:

```text
/fault_injection/get_fault_status
/fault_injection/get_fault_schema
/fault_injection/get_fault_config
/fault_injection/reload_scenario
/fault_injection/set_fault_state
/fault_injection/set_fault_config
```

Topics:

```text
/fault_injection/events
/fault_injection/assertion_events
```

The panel uses message and service types from the `ros2_fault_injection` package:

```text
ros2_fault_injection/msg/FaultEvent
ros2_fault_injection/msg/AssertionEvent
ros2_fault_injection/srv/GetFaultStatus
ros2_fault_injection/srv/GetFaultSchema
ros2_fault_injection/srv/GetFaultConfig
ros2_fault_injection/srv/ReloadScenario
ros2_fault_injection/srv/SetFaultState
ros2_fault_injection/srv/SetFaultConfig
```

## Build

From the workspace root:

```bash
source /opt/ros/jazzy/setup.bash
colcon build --symlink-install --packages-select ros2_fault_injection ros2_fault_injection_rviz
source install/setup.bash
```

If the plugin does not appear in RViz after a build, source the install space again from the same
workspace:

```bash
source install/setup.bash
```

## Open The Panel In RViz

Start RViz from a sourced terminal:

```bash
rviz2
```

Then in RViz:

1. Open `Panels`
2. Select `Add New Panel`
3. Choose `ros2_fault_injection_rviz/FaultInjectionPanel`
4. Click `OK`

The panel will show an empty table until the core fault injector node is running and the status service
is available.

## Typical Workflow

Start the fault injector with a scenario:

```bash
ros2 launch ros2_fault_injection fault_injector.launch.py \
  scenario_file:=/path/to/scenario.yaml
```

Open RViz and add the panel. Fault status refreshes automatically while the panel is open.

Use the per-row button to activate or deactivate a fault. The panel refreshes after a successful state
change.

Use `Reload Scenario` after editing the scenario YAML. The panel asks the running fault injector to reload
the same scenario file it was launched with, then refreshes the table if the reload succeeds.

## Editing Fault Config

Select a row in the fault table to edit that fault's runtime configuration.

When a fault is selected, the panel calls `/fault_injection/get_fault_schema` and fills the config key
dropdown with keys that are valid for that fault's injector type. It then calls
`/fault_injection/get_fault_config` and fills the value field with the current runtime value for the
selected key. Change the value and click `Set Config`.

The backend validates the key and value before applying the update. For example, `drop_probability`
must be a number between `0.0` and `1.0`; invalid values are rejected and the previous config remains
unchanged.

Config edits are runtime-only. To make a change permanent, update the scenario YAML as well.

## Recent Events

The `Recent Events` table subscribes to `/fault_injection/events` and shows the latest fault activity,
including manual state changes, scheduled state changes, scenario reloads, and config updates. The table
keeps the newest events at the top.

## Assertions

The `Assertions` table subscribes to `/fault_injection/assertion_events` and shows assertion results
published by the running fault injector. Rows are inserted newest-first so the latest scenario outcome
is visible at the top of the table.

Assertion rows are color-coded by state:

- `passed` rows are shown in green.
- `failed` rows are shown in red.

Assertion events are not latched. Open the panel before the relevant assertion passes or fails, or reload
the scenario to generate fresh assertion events.

## Troubleshooting

Check that RViz can discover the package:

```bash
ros2 pkg prefix ros2_fault_injection_rviz
```

Check that the plugin XML was installed:

```bash
cat $(ros2 pkg prefix ros2_fault_injection_rviz)/share/ros2_fault_injection_rviz/plugin_description.xml
```

Check that the plugin library was installed:

```bash
ls $(ros2 pkg prefix ros2_fault_injection_rviz)/lib/libros2_fault_injection_rviz.so
```

Check that the fault injector services exist:

```bash
ros2 service list | grep fault_injection
```

Check that config values can be read:

```bash
ros2 service call /fault_injection/get_fault_config ros2_fault_injection/srv/GetFaultConfig \
  "{fault_id: odom_bias}"
```

Check that fault events are being published:

```bash
ros2 topic echo /fault_injection/events
```

Check that assertion events are being published:

```bash
ros2 topic echo /fault_injection/assertion_events
```

If the plugin fails to load, start RViz from a terminal and inspect the pluginlib error:

```bash
rviz2
```

## Next Ideas

- Add filtering by injector id or state
- Add typed editors for numbers, booleans, and bounded values
- Add a clear-events button
