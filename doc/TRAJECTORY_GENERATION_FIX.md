# Trajectory Generation Configuration Fix

## Issue Identified

The trajectory generation system had a **configuration redundancy issue** that prevented config file changes from being properly reflected.

### The Problem

The original flow created a circular override pattern:

1. **Demo.cpp** loads `map_1_config.json` and reads `vehicles.default_count` (e.g., 5)
2. **Demo.cpp** calls Python script with `--vehicles 5` (passing config value as override)
3. **Python script** receives `--vehicles 5` and uses it instead of its own config
4. **Result**: Config file changes were ignored because Python always used C++ overrides

This meant that changing `default_count` in the config file had **no effect** because the demo was always passing the current config value as a command line override.

### Example of the Problem

```bash
# Config file has default_count: 5
# Demo.cpp reads config: n_vehicles = 5  
# Demo.cpp calls: python3 ... --config config.json --vehicles 5
# Python script: args.vehicles=5, so uses 5 (ignores config.json)
```

Even if you changed the config to `default_count: 10`, the system would still use 5 because the demo was passing `--vehicles 5` from the old config reading.

## Solution

**Removed redundant overrides** from demo.cpp to Python script:

### Before (Problematic)
```cpp
if (g_map_config) {
    python_command = "python3 scripts/trajectory_generator_map_1.py";
    python_command += " --config " + config_file;
    python_command += " --vehicles " + std::to_string(n_vehicles);  // ← Redundant override
}
```

### After (Fixed)
```cpp
if (g_map_config) {
    python_command = "python3 scripts/trajectory_generator_map_1.py";
    python_command += " --config " + config_file;
    // Let Python read config independently - no redundant overrides
}
```

## Benefits of the Fix

### ✅ **Configuration Consistency**
- Both C++ and Python read the same config file independently
- Changes to `config/map_1_config.json` are immediately reflected
- No override conflicts between systems

### ✅ **Clear Configuration Source**
Python script now shows where each value comes from:
```
Vehicles: 5 (from config)
Duration: 30 seconds (from config)  
Output directory: tests/logs/trajectories (from config)
```

### ✅ **Explicit Overrides Still Work**
Command line overrides work when explicitly needed:
```bash
python3 scripts/trajectory_generator_map_1.py --config config.json --vehicles 10
# Shows: Vehicles: 10 (from command line)
```

## Testing the Fix

1. **Test default behavior**:
   ```bash
   python3 scripts/trajectory_generator_map_1.py --config config/map_1_config.json
   # Should show: Vehicles: 5 (from config)
   ```

2. **Test config changes**:
   ```bash
   # Change default_count to 8 in config/map_1_config.json
   python3 scripts/trajectory_generator_map_1.py --config config/map_1_config.json  
   # Should show: Vehicles: 8 (from config)
   ```

3. **Test overrides**:
   ```bash
   python3 scripts/trajectory_generator_map_1.py --config config/map_1_config.json --vehicles 12
   # Should show: Vehicles: 12 (from command line)
   ```

## Impact

- **Demo test**: Now properly respects config file changes
- **Trajectory generation**: Works consistently across both C++ and Python
- **Configuration management**: Single source of truth in `config/map_1_config.json`
- **Debugging**: Clear indication of value sources in output 