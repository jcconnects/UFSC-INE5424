#!/usr/bin/env python3
"""
Trajectory Generator for 2-RSU REQ-RESP Test Map
Generates specific trajectories for testing REQ-RESP key discovery functionality.

Scenario:
- RSU0 (ID 1000) on LEFT at (-501, 0) and RSU1 (ID 1001) on RIGHT at (501, 0) 
- Non-overlapping collision domains (1002m apart, 500m radius each)
- Vehicle 1: Stationary near RSU0 (left side)
- Vehicle 2: Stationary near RSU0 (left side) 
- Vehicle 3: Mobile - drives from far right (past RSU1) to far left (past RSU0)

Usage:
    python3 trajectory_generator_map_2rsu.py [--config <config_file>] [--duration <seconds>] [--output-dir <path>]
"""

import argparse
import csv
import json
import math
import os
from typing import Tuple, List, Dict, Any

class Map2RSUConfig:
    """Configuration loader for 2-RSU test scenario."""
    
    def __init__(self, config_file: str):
        with open(config_file, 'r') as f:
            self.config = json.load(f)
    
    @property
    def rsus(self) -> List[Dict[str, Any]]:
        return self.config['rsus']
    
    @property
    def vehicles(self) -> Dict[str, Any]:
        return self.config['vehicles']
    
    @property
    def simulation(self) -> Dict[str, Any]:
        return self.config['simulation']
    
    @property
    def waypoints(self) -> List[Dict[str, Any]]:
        return self.config['waypoints']
    
    @property
    def routes(self) -> List[Dict[str, Any]]:
        return self.config['routes']
    
    @property
    def logging(self) -> Dict[str, str]:
        return self.config['logging']

class Map2RSUTrajectoryGenerator:
    """Generates trajectories for 2-RSU test scenario."""
    
    def __init__(self, config: Map2RSUConfig, duration_seconds: int = None, update_interval_ms: int = None):
        self.config = config
        self.duration_ms = (duration_seconds or config.simulation['duration_s']) * 1000
        self.update_interval_ms = update_interval_ms or config.simulation['update_interval_ms']
        self.timestamps = list(range(0, self.duration_ms + 1, self.update_interval_ms))
        
        # Build waypoint lookup
        self.waypoints = {wp['name']: wp for wp in config.waypoints}
        
        # Vehicle configuration
        self.vehicle_speed_kmh = config.vehicles['speed_kmh']
        self.vehicle_speed_ms = self.vehicle_speed_kmh / 3.6  # Convert to m/s
        
    def generate_vehicle_trajectory(self, vehicle_id: int) -> List[Tuple[int, float, float]]:
        """Generate trajectory based on vehicle configuration."""
        trajectory = []
        
        # Find vehicle config
        vehicle_config = None
        for v_config in self.config.vehicles['configs']:
            if v_config['id'] == vehicle_id:
                vehicle_config = v_config
                break
        
        if not vehicle_config:
            raise ValueError(f"No configuration found for vehicle {vehicle_id}")
        
        if vehicle_config['type'] == 'stationary':
            # Stationary vehicle - same position for all timestamps
            x = vehicle_config['position']['x']
            y = vehicle_config['position']['y']
            
            for timestamp in self.timestamps:
                trajectory.append((timestamp, x, y))
                
        elif vehicle_config['type'] == 'mobile':
            # Mobile vehicle - linear movement from start to end
            start_x = vehicle_config['start_position']['x']
            start_y = vehicle_config['start_position']['y']
            end_x = vehicle_config['end_position']['x']
            end_y = vehicle_config['end_position']['y']
            
            # Calculate total distance and time needed
            total_distance = self._cartesian_distance(start_x, start_y, end_x, end_y)
            total_time_needed_ms = (total_distance / self.vehicle_speed_ms) * 1000
            
            # Calculate movement per timestamp
            x_diff = end_x - start_x
            y_diff = end_y - start_y
            
            # Generate trajectory points
            for timestamp in self.timestamps:
                if total_time_needed_ms > 0:
                    progress = min(timestamp / total_time_needed_ms, 1.0)
                else:
                    progress = 1.0  # Instant movement for very short distances
                
                current_x = start_x + (x_diff * progress)
                current_y = start_y + (y_diff * progress)
                
                trajectory.append((timestamp, current_x, current_y))
                
                # Once destination is reached, stay there
                if progress >= 1.0:
                    # Fill remaining timestamps with end position
                    while len(trajectory) < len(self.timestamps):
                        trajectory.append((self.timestamps[len(trajectory)], end_x, end_y))
                    break
        else:
            raise ValueError(f"Unknown vehicle type: {vehicle_config['type']}")
            
        return trajectory
    
    def generate_rsu_trajectory(self, rsu_id: int) -> List[Tuple[int, float, float]]:
        """Generate static trajectory for RSU (same position for all timestamps)."""
        trajectory = []
        
        # Find RSU config
        rsu_config = None
        for r_config in self.config.rsus:
            if r_config['id'] == rsu_id:
                rsu_config = r_config
                break
        
        if not rsu_config:
            raise ValueError(f"No configuration found for RSU {rsu_id}")
        
        for timestamp in self.timestamps:
            trajectory.append((timestamp, rsu_config['position']['x'], rsu_config['position']['y']))
            
        return trajectory
    
    def save_trajectory_csv(self, trajectory: List[Tuple[int, float, float]], 
                           filename: str, entity_type: str, entity_id: int):
        """Save trajectory to CSV file."""
        os.makedirs(os.path.dirname(filename), exist_ok=True)
        
        with open(filename, 'w', newline='') as csvfile:
            writer = csv.writer(csvfile)
            # Write header
            writer.writerow(['timestamp_ms', 'x', 'y'])
            # Write trajectory data
            for timestamp, x, y in trajectory:
                writer.writerow([timestamp, f"{x:.2f}", f"{y:.2f}"])
        
        print(f"Generated {entity_type} {entity_id} trajectory: {filename} ({len(trajectory)} points)")
    
    def calculate_distance_between_rsus(self):
        """Calculate and display distance between RSUs for verification."""
        if len(self.config.rsus) >= 2:
            rsu0 = None
            rsu1 = None
            
            for rsu in self.config.rsus:
                if rsu['id'] == 1000:
                    rsu0 = rsu
                elif rsu['id'] == 1001:
                    rsu1 = rsu
            
            if rsu0 and rsu1:
                distance = self._cartesian_distance(
                    rsu0['position']['x'], rsu0['position']['y'],
                    rsu1['position']['x'], rsu1['position']['y']
                )
                radius = self.config.simulation['default_transmission_radius_m']
                overlap = max(0, 2 * radius - distance)
                
                print(f"RSU Distance Analysis:")
                print(f"  RSU0 (ID {rsu0['id']}) LEFT position: ({rsu0['position']['x']:.1f}, {rsu0['position']['y']:.1f})")
                print(f"  RSU1 (ID {rsu1['id']}) RIGHT position: ({rsu1['position']['x']:.1f}, {rsu1['position']['y']:.1f})")
                print(f"  Distance between RSUs: {distance:.1f}m")
                print(f"  Transmission radius: {radius}m each")
                print(f"  Combined coverage: {2 * radius}m")
                print(f"  Domain overlap: {overlap:.1f}m {'(NO OVERLAP - PERFECT FOR REQ-RESP TEST)' if overlap == 0 else '(OVERLAP - RECONSIDER POSITIONS)'}")
                
                # Calculate Vehicle 3's journey
                vehicle3_distance = 1700  # 500 to -1200
                vehicle3_time_needed = vehicle3_distance / self.vehicle_speed_ms
                print(f"\nVehicle 3 Journey Analysis:")
                print(f"  Start: (500, 0) - Far right, past RSU1")
                print(f"  End: (-1200, 0) - Far left, past RSU0") 
                print(f"  Total distance: {vehicle3_distance}m")
                print(f"  Speed: {self.vehicle_speed_kmh} km/h ({self.vehicle_speed_ms:.1f} m/s)")
                print(f"  Time needed: {vehicle3_time_needed:.1f}s")
                print(f"  Simulation duration: {self.duration_ms/1000:.1f}s")
                print(f"  {'✓ Vehicle 3 will complete journey' if vehicle3_time_needed <= self.duration_ms/1000 else '✗ Vehicle 3 will NOT complete journey - increase duration or speed'}")
    
    @staticmethod
    def _cartesian_distance(x1: float, y1: float, x2: float, y2: float) -> float:
        """Calculate distance between two points using simple Euclidean formula."""
        dx = x2 - x1
        dy = y2 - y1
        return math.sqrt(dx * dx + dy * dy)

def main():
    parser = argparse.ArgumentParser(description='Generate trajectories for 2-RSU REQ-RESP test scenario')
    parser.add_argument('--config', type=str, default='config/map_2rsu_config.json',
                       help='Configuration file path (default: config/map_2rsu_config.json)')
    parser.add_argument('--duration', type=int, 
                       help='Simulation duration in seconds (overrides config default)')
    parser.add_argument('--output-dir', type=str, 
                       help='Output directory for trajectory files (overrides config default)')
    parser.add_argument('--update-interval', type=int,
                       help='Update interval in milliseconds (overrides config default)')
    
    args = parser.parse_args()
    
    # Load configuration
    if not os.path.exists(args.config):
        print(f"Error: Configuration file not found: {args.config}")
        print("Make sure the config file exists. You can use config/map_2rsu_config.json")
        return 1
    
    config = Map2RSUConfig(args.config)
    
    # Override config values with command line arguments if provided
    n_vehicles = config.vehicles['default_count']
    duration = args.duration if args.duration is not None else config.simulation['duration_s']
    output_dir = args.output_dir if args.output_dir is not None else config.logging['trajectory_dir']
    update_interval = args.update_interval if args.update_interval is not None else config.simulation['update_interval_ms']
    
    print(f"Generating trajectories for 2-RSU REQ-RESP test scenario:")
    print(f"  Config file: {args.config}")
    print(f"  RSUs: {len(config.rsus)} (RSU0 LEFT, RSU1 RIGHT)")
    print(f"  Vehicles: {n_vehicles}")
    print(f"  Duration: {duration} seconds" + (" (from config)" if args.duration is None else " (from command line)"))
    print(f"  Update interval: {update_interval} ms" + (" (from config)" if args.update_interval is None else " (from command line)"))
    print(f"  Output directory: {output_dir}" + (" (from config)" if args.output_dir is None else " (from command line)"))
    print(f"  Vehicle speed: {config.vehicles['speed_kmh']} km/h")
    print(f"  Transmission radius: {config.simulation['default_transmission_radius_m']}m")
    
    generator = Map2RSUTrajectoryGenerator(config, duration, update_interval)
    
    # Verify RSU positioning and vehicle journey
    generator.calculate_distance_between_rsus()
    print()
    
    # Generate RSU trajectories (static)
    for rsu_config in config.rsus:
        rsu_id = rsu_config['id']
        rsu_trajectory = generator.generate_rsu_trajectory(rsu_id)
        rsu_filename = os.path.join(output_dir, f"rsu_{rsu_id}_trajectory.csv")
        generator.save_trajectory_csv(rsu_trajectory, rsu_filename, "RSU", rsu_id)
    
    # Generate vehicle trajectories based on their individual configs
    for vehicle_config in config.vehicles['configs']:
        vehicle_id = vehicle_config['id']
        vehicle_trajectory = generator.generate_vehicle_trajectory(vehicle_id)
        vehicle_filename = os.path.join(output_dir, f"vehicle_{vehicle_id}_trajectory.csv")
        generator.save_trajectory_csv(vehicle_trajectory, vehicle_filename, "Vehicle", vehicle_id)
        
        print(f"  → {vehicle_config['description']}")
    
    print(f"\nTrajectory generation completed!")
    print(f"Files saved to: {output_dir}")
    print(f"Total files: {len(config.rsus) + len(config.vehicles['configs'])} (RSUs + vehicles)")
    
    # Print test scenario information
    print(f"\nTest Scenario: {config.config['test_scenario']['name']}")
    print(f"Description: {config.config['test_scenario']['description']}")
    print(f"\nExpected Behavior:")
    for i, behavior in enumerate(config.config['test_scenario']['expected_behavior'], 1):
        print(f"  {i}. {behavior}")
    
    return 0

if __name__ == "__main__":
    exit(main()) 