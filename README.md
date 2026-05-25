# ros2_fault_injection_rviz

`ros2_fault_injection_rviz` provides an RViz panel for viewing and controlling faults from the `ros2_fault_injection` framework.

The panel is an RViz plugin, not a standalone node. It appears in RViz under the panel plugin name:

```text
ros2_fault_injection_rviz/FaultInjectionPanel
```

## Current Features

- Lists faults from `/fault_injection/get_fault_status`
- Displays:
  - fault id
  - injector id
  - active/inactive state
  - fault details
- Provides a refresh button
- Provides a scenario reload button using `/fault_injection/reload_scenario`
- Provides per-fault Activate/Deactivate buttons using `/fault_injection/set_fault_state`
- Shows a short status message for service calls

## Expected Services

The core fault injector node should be running and providing:

```text
/fault_injection/get_fault_status
/fault_injection/reload_scenario
/fault_injection/set_fault_state
```

The panel uses service types from the `ros2_fault_injection` package:

```text
ros2_fault_injection/srv/GetFaultStatus
ros2_fault_injection/srv/ReloadScenario
ros2_fault_injection/srv/SetFaultState
```

## Build

From the workspace root:

```bash
cd /home/reece/fault_injection_ws
source /opt/ros/jazzy/setup.bash
colcon build --symlink-install --packages-select ros2_fault_injection ros2_fault_injection_rviz
source install/setup.bash
```

If the plugin does not appear in RViz after a build, source the install space again:

```bash
source /home/reece/fault_injection_ws/install/setup.bash
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

The panel will show an empty table until the core fault injector node is running and the status service is available.

## Typical Workflow

Start the fault injector:

```bash
ros2 launch ros2_fault_injection fault_injector.launch.py \
  scenario_file:=/home/reece/fault_injection_ws/install/ros2_fault_injection/share/ros2_fault_injection/config/multi_injector_faults.yaml
```

Open RViz, add the panel, then use `Refresh` to load the current faults.

Use the per-row button to activate or deactivate a fault. The panel refreshes after a successful state change.

Use `Reload Scenario` after editing the scenario YAML. The panel asks the running fault injector to reload the same scenario file it was launched with, then refreshes the table if the reload succeeds.

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

If the panel appears but does not populate, check that the fault injector services exist:

```bash
ros2 service list | grep fault_injection
```

If the plugin fails to load, start RViz from a terminal and inspect the pluginlib error:

```bash
rviz2
```

## Next Ideas

- Auto-refresh fault status
- Subscribe to `/fault_injection/events`
- Add runtime config editing through `/fault_injection/set_fault_config`
- Add filtering by injector id or state
